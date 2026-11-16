// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "updatedaemon.h"
#include "updatesource.h"
#include "checkresults.h"
#include "appimageinfo.h"
#include "logging.h"
#include "appsettings.h"
#include "appimagereader.h"
#include "appimagemanagersettings.h"
#include "../gui/appimageupdate.h"
#include "../gui/downloadwatcher.h"
#include "../gui/traymodel.h"

#include <KConfigGroup>
#include <KDirWatch>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KNotification>
#include <KSharedConfig>
#include <KStatusNotifierItem>

#ifdef HAVE_LAYERSHELLQT
#include <LayerShellQt/Window>
#endif

#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkInformation>
#include <QPainter>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

using namespace std::chrono_literals;

namespace {

constexpr auto kTickInterval   = 1min;   // the schedule is re-evaluated this often
constexpr auto kFirstTick      = 30s;    // after login: let the network and panel settle
constexpr auto kRetryAfterFail = 30min;  // scheduled check failed (offline server, rate limit)
constexpr auto kReadSettle     = 2s;     // a changed AppImage is read again after this
constexpr auto kTrayIconDelay  = 8s;     // at login, give the tray widget time to register

KConfigGroup stateGroup(const QString &name)
{
    // ~/.local/state/appimagemanagerstaterc: runtime state, not settings (see checkresults.h).
    return KSharedConfig::openStateConfig(QStringLiteral("appimagemanagerstaterc"))->group(name);
}

bool isAppImageFile(const QString &path)
{
    return path.endsWith(QLatin1String(".appimage"), Qt::CaseInsensitive);
}

QString iconIdFor(const QString &path)
{
    return QStringLiteral("tray-") + QString::number(qHash(path));
}

QString displayNameFor(const QString &path, const AppImageInfo &info)
{
    if (!info.appName.isEmpty())
        return info.appName;
    return info.cleanName.isEmpty() ? QFileInfo(path).fileName() : info.cleanName;
}

void launchDashboard(const QStringList &args)
{
    // Same binary: works for installed and development builds alike. The dashboard is
    // single-instance, so a running one receives the arguments and switches page.
    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), args))
        qCWarning(AIM_LOG) << "Failed to start AppImage Manager with" << args;
}

// Our icon with the update count in a badge at the bottom right.
QIcon badgedIcon(int count)
{
    const QIcon base = QIcon::fromTheme(QStringLiteral("appimagemanager"),
                                        QIcon::fromTheme(QStringLiteral("application-x-executable")));
    const QPalette palette = QApplication::palette();
    const QString text = count > 99 ? QStringLiteral("99+") : QString::number(count);
    QIcon icon;
    for (int size : { 16, 22, 24, 32, 48, 64 }) {
        QPixmap pixmap = base.pixmap(QSize(size, size));
        if (pixmap.isNull())
            continue;
        pixmap.setDevicePixelRatio(1.0);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QFont font = QApplication::font();
        font.setBold(true);
        font.setPixelSize(qMax(7, size * 9 / 20));
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const int h = metrics.height();
        const int w = qMax(h, metrics.horizontalAdvance(text) + h / 2);
        const QRect badge(size - w, size - h, w, h);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette.color(QPalette::Highlight));
        painter.drawRoundedRect(badge, h / 2.0, h / 2.0);
        painter.setPen(palette.color(QPalette::HighlightedText));
        painter.drawText(badge, Qt::AlignCenter, text);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

// ── Schedule ──────────────────────────────────────────────────────────────────

int UpdateDaemon::intervalDays(int frequency, int customDays)
{
    switch (frequency) {
    case 1:  return 1;
    case 2:  return 7;
    case 3:  return 30;
    case 4:  return qMax(1, customDays);
    default: return 0; // Never
    }
}

QDateTime UpdateDaemon::nextCheck(const QDateTime &now, const QDateTime &lastCheck, int frequency, int customDays)
{
    const int days = intervalDays(frequency, customDays);
    if (days == 0)
        return {};
    if (!lastCheck.isValid())
        return now;
    return lastCheck.addDays(days);
}

bool UpdateDaemon::isDue(const QDateTime &now, const QDateTime &lastCheck, int frequency, int customDays)
{
    const QDateTime next = nextCheck(now, lastCheck, frequency, customDays);
    return next.isValid() && now >= next;
}

// ── Setup ─────────────────────────────────────────────────────────────────────

UpdateDaemon::UpdateDaemon(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_downloadWatcher(new DownloadWatcher(this))
    , m_updates(new AppImageUpdateManager(m_networkManager, this))
    , m_model(new TrayModel(this))
    , m_tick(new QTimer(this))
    , m_readTimer(new QTimer(this))
    , m_dirWatch(new KDirWatch(this))
    , m_stateTimer(new QTimer(this))
    , m_viewWatcher(new QDBusServiceWatcher(this))
{
    m_readerPool.setMaxThreadCount(qBound(1, QThread::idealThreadCount() / 2, 4));
    m_updates->setCompletionNotifications(false); // one summary for Update All instead

    m_tick->setInterval(kTickInterval);
    connect(m_tick, &QTimer::timeout, this, &UpdateDaemon::tick);

    m_readTimer->setSingleShot(true);
    m_readTimer->setInterval(kReadSettle);
    connect(m_readTimer, &QTimer::timeout, this, [this]() {
        const QSet<QString> paths = std::exchange(m_pendingReads, {});
        bool removed = false;
        for (const QString &path : paths) {
            if (QFileInfo::exists(path)) {
                readItem(path); // async; applyMetadata picks the stored result for its version
            } else {
                m_model->remove(path);
                m_recheck.remove(path);
                // Only this entry: the rest of the file may hold newer results from the main window.
                saveCheckResult(QFileInfo(path).fileName(), {});
                removed = true;
            }
        }
        if (removed)
            Q_EMIT UpdatesChanged();
    });

    // Single source of truth for download detection. Mirror the user's
    // watchDownloads pref into the watcher; the dashboard's own watcher
    // skips notification when the daemon's D-Bus name is registered.
    // Settings changed in the main window (another process) arrive through reloadSettings().
    m_downloadWatcher->setEnabled(AppSettings::instance()->watchDownloads());
    connect(m_downloadWatcher, &DownloadWatcher::appImageFound,
            this, &UpdateDaemon::onDownloadAppeared);

    // ── Update manager ────────────────────────────────────────────────────────
    connect(m_updates, &AppImageUpdateManager::updateFound, this,
            [this](const QString &path, const QString &version, const QString &url) {
        m_model->setState(path, TrayModel::UpdateAvailable, version, url);
    });
    connect(m_updates, &AppImageUpdateManager::itemChecked, this,
            [this](const QString &path, AppImageUpdateManager::CheckResult result, const QString &reason) {
        if (result == AppImageUpdateManager::CheckResult::UpToDate)
            m_model->setState(path, TrayModel::UpToDate);
        else if (result == AppImageUpdateManager::CheckResult::Failed)
            m_model->setState(path, TrayModel::Failed, {}, {}, reason);
    });
    connect(m_updates, &AppImageUpdateManager::checkFinished, this, &UpdateDaemon::onCheckFinished);
    connect(m_updates, &AppImageUpdateManager::downloadProgress, m_model, &TrayModel::setProgress);
    connect(m_updates, &AppImageUpdateManager::downloadFinished, this, &UpdateDaemon::onDownloadFinished);

    connect(m_model, &TrayModel::summaryChanged, this, &UpdateDaemon::updateTray);

    // The tray widget reads State() again on StateChanged; progress updates are coalesced.
    m_stateTimer->setSingleShot(true);
    m_stateTimer->setInterval(250ms);
    connect(m_stateTimer, &QTimer::timeout, this, &UpdateDaemon::StateChanged);
    const auto stateChanged = [this]() { m_stateTimer->start(); };
    connect(m_model, &TrayModel::summaryChanged, this, stateChanged);
    connect(m_model, &TrayModel::changed, this, stateChanged);

    // A registered tray widget lives in plasmashell; if that leaves the bus, our icon returns.
    m_viewWatcher->setConnection(QDBusConnection::sessionBus());
    m_viewWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_viewWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &name) {
        m_viewWatcher->removeWatchedService(name);
        m_views.remove(name);
        viewsChanged();
    });

    // KDirWatch: per-file events, no full re-scans (see AppImageListModel).
    const auto changed = [this](const QString &path) {
        if (path == m_watchedDir || !isAppImageFile(path))
            return;
        m_pendingReads.insert(path);
        m_readTimer->start();
    };
    connect(m_dirWatch, &KDirWatch::created, this, changed);
    connect(m_dirWatch, &KDirWatch::dirty, this, changed);
    connect(m_dirWatch, &KDirWatch::deleted, this, changed);
}

UpdateDaemon::~UpdateDaemon() = default;

void UpdateDaemon::start()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QStringLiteral("io.github.appimagemanager.Daemon")))
        qCWarning(AIM_LOG) << "Failed to register D-Bus service io.github.appimagemanager.Daemon";
    if (!bus.registerObject(QStringLiteral("/Daemon"), this,
                            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals))
        qCWarning(AIM_LOG) << "Failed to register D-Bus object /Daemon";

    m_lastCheck = loadLastCheck();

    // Offline: scheduled checks wait; back online: check if one is due.
    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, this,
                [this](QNetworkInformation::Reachability reachability) {
            if (reachability == QNetworkInformation::Reachability::Online)
                QTimer::singleShot(5s, this, &UpdateDaemon::tick);
        });
    }

    // Our own tray icon only when no tray widget registers in the meantime.
    QTimer::singleShot(kTrayIconDelay, this, [this]() {
        m_started = true;
        viewsChanged();
    });
    watchFolder();
    scanFolder();
    updateSchedule();
    m_tick->start();
    QTimer::singleShot(kFirstTick, this, &UpdateDaemon::tick);
}

bool UpdateDaemon::isOnline() const
{
    // Without a backend (unusual) we cannot tell, so we try.
    const QNetworkInformation *info = QNetworkInformation::instance();
    return !info || info->reachability() == QNetworkInformation::Reachability::Online
        || info->reachability() == QNetworkInformation::Reachability::Unknown;
}

void UpdateDaemon::updateSchedule()
{
    const AppSettings *settings = AppSettings::instance();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime next = nextCheck(now, m_lastCheck, settings->updateFrequency(), settings->customUpdateDays());
    if (next.isValid() && next < now)
        next = now; // overdue: checked within a minute (or once back online)
    m_model->setSchedule(m_lastCheck, next);
}

// The main window changes settings in its own process; this one re-reads them each tick.
void UpdateDaemon::reloadSettings()
{
    const AppSettings *settings = AppSettings::instance();
    const QString applicationsPath = settings->applicationsPath();
    const bool watchDownloads = settings->watchDownloads();
    AppImageManagerSettings::self()->load();
    if (settings->applicationsPath() != applicationsPath) {
        watchFolder();
        scanFolder();
    }
    if (settings->watchDownloads() != watchDownloads)
        m_downloadWatcher->setEnabled(settings->watchDownloads());
}

void UpdateDaemon::tick()
{
    reloadSettings();
    updateSchedule();
    const AppSettings *settings = AppSettings::instance();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (m_model->checking() || m_model->updatingCount() > 0 || !isOnline())
        return;
    if (m_retryAt.isValid() && now < m_retryAt)
        return;
    if (!m_recheck.isEmpty()) {
        runCheck(QStringList(m_recheck.cbegin(), m_recheck.cend()), false);
        m_recheck.clear();
        return;
    }
    if (isDue(now, m_lastCheck, settings->updateFrequency(), settings->customUpdateDays()))
        runCheck({}, true);
}

// ── Installed AppImages ───────────────────────────────────────────────────────

void UpdateDaemon::watchFolder()
{
    if (!m_watchedDir.isEmpty())
        m_dirWatch->removeDir(m_watchedDir);
    m_watchedDir = AppSettings::instance()->applicationsPath();
    m_dirWatch->addDir(m_watchedDir, KDirWatch::WatchFiles);
}

void UpdateDaemon::scanFolder()
{
    const QFileInfoList files = QDir(m_watchedDir).entryInfoList(kAppImageFilters(), QDir::Files);
    QSet<QString> present;
    for (const QFileInfo &fi : files)
        present.insert(fi.absoluteFilePath());
    for (const TrayModel::Item &item : QList<TrayModel::Item>(m_model->items())) {
        if (!present.contains(item.path))
            m_model->remove(item.path);
    }
    for (const QString &path : std::as_const(present))
        readItem(path);
}

void UpdateDaemon::readItem(const QString &path)
{
    // Cached reads are cheap; a changed or new file is read once off the main thread.
    auto *watcher = new QFutureWatcher<AppImageInfo>(this);
    connect(watcher, &QFutureWatcher<AppImageInfo>::finished, this, [this, watcher, path]() {
        watcher->deleteLater();
        if (QFileInfo::exists(path))
            applyMetadata(path, watcher->result());
    });
    watcher->setFuture(QtConcurrent::run(&m_readerPool, [path]() { return AppImageReader(path).read(); }));
}

void UpdateDaemon::applyMetadata(const QString &path, const AppImageInfo &info)
{
    TrayModel::Item item;
    item.path = path;
    item.name = displayNameFor(path, info);
    item.version = normalizeVersion(info.version);
    item.appIconId = info.appId;
    item.updateInfo = info.updateInfo;
    if (!info.appId.isEmpty() && QIcon::hasThemeIcon(info.appId)) {
        item.icon = info.appId;
    } else if (!info.iconData.isEmpty()) {
        // Written to the cache: the tray views (also the widget in plasmashell) load it from there.
        const QString iconFile = iconFileFor(path, info.iconExt);
        QFile file(iconFile);
        if (QFileInfo(iconFile).size() != info.iconData.size()
            && QDir().mkpath(QFileInfo(iconFile).absolutePath()) && file.open(QIODevice::WriteOnly))
            file.write(info.iconData);
        item.icon = QUrl::fromLocalFile(iconFile).toString();
    } else {
        item.icon = QStringLiteral("application-x-executable");
    }

    const TrayModel::Item *existing = m_model->item(path);
    if (existing && existing->version == item.version) {
        // Same file version: keep what we know.
        item.state = existing->state;
        item.newVersion = existing->newVersion;
        item.url = existing->url;
        item.reason = existing->reason;
        item.progress = existing->progress;
    } else {
        // New row (startup, a new install) or a new version (updated here or in the main
        // window): the stored result for this version, if any.
        const auto results = loadCheckResults();
        const auto it = results.constFind(QFileInfo(path).fileName());
        if (it != results.cend() && it->versionAtCheck == item.version) {
            item.state = static_cast<TrayModel::State>(it->state);
            item.newVersion = it->newVersion;
            item.url = it->url;
            item.reason = it->reason;
        } else if (existing && existing->state == TrayModel::UpdateAvailable) {
            // A pending update was replaced by something else (by hand): check it again.
            m_recheck.insert(path);
            QTimer::singleShot(kReadSettle, this, &UpdateDaemon::tick);
        }
    }
    if (!isSupportedUpdateInfo(effectiveUpdateInfo(info.updateInfo, loadUpdateSource(path))))
        item.state = TrayModel::NoSource;
    else if (item.state == TrayModel::NoSource)
        item.state = TrayModel::NotChecked; // a source was set since

    m_model->upsert(item);
}

// ── Checking ──────────────────────────────────────────────────────────────────

void UpdateDaemon::checkNow()
{
    if (m_model->checking() || m_model->updatingCount() > 0)
        return;
    m_recheck.clear();
    runCheck({}, true);
}

void UpdateDaemon::runCheck(const QStringList &paths, bool full)
{
    QList<AppImageUpdateManager::CheckItem> items;
    m_checkedCount = 0;
    for (const TrayModel::Item &item : m_model->items()) {
        if (!paths.isEmpty() && !paths.contains(item.path))
            continue;
        if (isSupportedUpdateInfo(effectiveUpdateInfo(item.updateInfo, loadUpdateSource(item.path))))
            ++m_checkedCount;
        items.append({ item.path, item.updateInfo, item.version, item.name, item.appIconId });
    }
    m_fullCheck = full;
    m_model->setChecking(true);
    m_updates->checkForUpdates(items); // drops items without a source; finishes at once if none
}

void UpdateDaemon::onCheckFinished(int updatesFound, int networkFailures)
{
    m_model->setChecking(false);
    const bool failed = updatesFound < 0 || (m_checkedCount > 0 && networkFailures >= m_checkedCount);
    if (m_fullCheck) {
        if (failed) {
            m_retryAt = QDateTime::currentDateTimeUtc().addSecs(std::chrono::seconds(kRetryAfterFail).count());
        } else {
            m_retryAt = {};
            m_lastCheck = QDateTime::currentDateTimeUtc();
            saveLastCheck(m_lastCheck);
        }
    }
    updateSchedule();
    saveResults();
    notifyNewUpdates();
    updateTray();
    Q_EMIT UpdatesChanged();
}

void UpdateDaemon::saveResults() const
{
    QHash<QString, CheckResult> results;
    for (const TrayModel::Item &item : m_model->items())
        results.insert(QFileInfo(item.path).fileName(),
                       { static_cast<UpdateState>(item.state), item.newVersion, item.url, item.version, item.reason });
    saveCheckResults(results); // keeps only the stored states
}

void UpdateDaemon::ReloadState()
{
    // The main window checked or updated: take over its results (they are in the file).
    m_lastCheck = loadLastCheck();
    m_retryAt = {};
    const auto results = loadCheckResults();
    KConfigGroup announced = stateGroup(QStringLiteral("TrayAnnounced"));
    for (const TrayModel::Item &item : QList<TrayModel::Item>(m_model->items())) {
        if (item.state == TrayModel::Updating || item.state == TrayModel::NoSource)
            continue;
        const QString key = QFileInfo(item.path).fileName();
        const auto it = results.constFind(key);
        if (it == results.cend() || it->versionAtCheck != item.version)
            continue; // not checked there, or our row has not seen the new file yet
        m_model->setState(item.path, static_cast<TrayModel::State>(it->state), it->newVersion, it->url, it->reason);
        // Seen in the main window: no notification for it later.
        if (it->state == UpdateState::UpdateAvailable)
            announced.writeEntry(key, it->newVersion);
    }
    announced.sync();
    updateSchedule();
    updateTray();
}

void UpdateDaemon::notifyNewUpdates()
{
    // One notification, only for versions not announced before.
    KConfigGroup announced = stateGroup(QStringLiteral("TrayAnnounced"));
    QStringList lines;
    for (const TrayModel::Item &item : m_model->items()) {
        if (item.state != TrayModel::UpdateAvailable)
            continue;
        const QString key = QFileInfo(item.path).fileName();
        if (announced.readEntry(key, QString()) == item.newVersion)
            continue;
        announced.writeEntry(key, item.newVersion);
        lines.append(item.version.isEmpty() ? i18nc("app, new version", "%1 %2", item.name, item.newVersion)
                                            : i18nc("app, old → new version", "%1 %2 → %3", item.name, item.version, item.newVersion));
    }
    announced.sync();
    if (lines.isEmpty())
        return;

    constexpr int kShown = 4;
    const int more = static_cast<int>(lines.size()) - kShown;
    if (more > 0) {
        lines = lines.mid(0, kShown);
        lines.append(i18np("and 1 more", "and %1 more", more));
    }
    auto *n = new KNotification(QStringLiteral("updateAvailable"), KNotification::CloseOnTimeout, this);
    n->setTitle(i18np("1 update available", "%1 updates available", m_model->updateCount()));
    n->setText(lines.join(QLatin1Char('\n')));
    n->setIconName(QStringLiteral("appimagemanager"));
    auto *updateAction = n->addAction(i18n("Update All"));
    connect(updateAction, &KNotificationAction::activated, this, &UpdateDaemon::updateAll);
    auto *openAction = n->addDefaultAction(i18n("Open AppImage Manager"));
    connect(openAction, &KNotificationAction::activated, this, [this]() { openManager(); });
    n->sendEvent();
}

// ── Updating ──────────────────────────────────────────────────────────────────

void UpdateDaemon::updateAll()
{
    if (m_model->checking())
        return;
    for (const TrayModel::Item &item : QList<TrayModel::Item>(m_model->items())) {
        if (item.state != TrayModel::UpdateAvailable || item.url.isEmpty())
            continue;
        m_model->setState(item.path, TrayModel::Updating, item.newVersion, item.url);
        m_updates->downloadUpdate(item.path, item.url, item.name, item.appIconId);
    }
    updateTray();
}

void UpdateDaemon::onDownloadFinished(const QString &path, bool success)
{
    const TrayModel::Item *item = m_model->item(path);
    if (!item)
        return;
    const QString name = item->name;
    const QString newVersion = item->newVersion;
    if (success) {
        // As in the dashboard: remember the installed release so version-less AppImages stop
        // showing it.
        if (isReleaseUpdateInfo(effectiveUpdateInfo(item->updateInfo, loadUpdateSource(path))))
            saveInstalledTag(path, newVersion);
        m_model->setState(path, TrayModel::UpToDate);
        m_updatedNames.append(name);
        readItem(path); // new version, icon
    } else {
        m_model->setState(path, TrayModel::UpdateAvailable, newVersion, item->url);
        auto *n = new KNotification(QStringLiteral("error"), KNotification::CloseOnTimeout, this);
        n->setTitle(i18n("Update Failed"));
        n->setText(i18n("%1 could not be updated. It was left as it was.", name));
        n->setIconName(QStringLiteral("appimagemanager"));
        n->sendEvent();
    }

    if (m_model->updatingCount() == 0 && !m_updatedNames.isEmpty()) {
        auto *n = new KNotification(QStringLiteral("updateDownloaded"), KNotification::CloseOnTimeout, this);
        n->setTitle(i18np("Updated 1 AppImage", "Updated %1 AppImages", m_updatedNames.size()));
        n->setText(m_updatedNames.join(QStringLiteral(", ")));
        n->setIconName(QStringLiteral("appimagemanager"));
        n->sendEvent();
        m_updatedNames.clear();
    }
    saveResults();
    updateTray();
    Q_EMIT UpdatesChanged();
}

QString UpdateDaemon::State() const
{
    QJsonArray items;
    for (const TrayModel::Item &item : m_model->items()) {
        items.append(QJsonObject{
            { QStringLiteral("path"),       item.path },
            { QStringLiteral("name"),       item.name },
            { QStringLiteral("icon"),       item.icon },
            { QStringLiteral("version"),    item.version },
            { QStringLiteral("state"),      static_cast<int>(item.state) },
            { QStringLiteral("newVersion"), item.newVersion },
            { QStringLiteral("progress"),   item.progress },
            { QStringLiteral("reason"),     item.reason },
        });
    }
    const QJsonObject state{
        { QStringLiteral("updateCount"),   m_model->updateCount() },
        { QStringLiteral("updatingCount"), m_model->updatingCount() },
        { QStringLiteral("checking"),      m_model->checking() },
        { QStringLiteral("schedule"),      m_model->scheduleText() },
        { QStringLiteral("lastChecked"),   m_model->lastCheckText() },
        { QStringLiteral("items"),         items },
    };
    return QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
}

void UpdateDaemon::RegisterView()
{
    if (!calledFromDBus())
        return;
    const QString name = message().service();
    m_views.insert(name);
    m_viewWatcher->addWatchedService(name);
    viewsChanged();
}

void UpdateDaemon::UnregisterView()
{
    if (!calledFromDBus())
        return;
    const QString name = message().service();
    m_views.remove(name);
    m_viewWatcher->removeWatchedService(name);
    viewsChanged();
}

void UpdateDaemon::viewsChanged()
{
    if (!m_started)
        return;
    if (m_views.isEmpty()) {
        if (!m_trayIcon)
            setupTrayIcon();
    } else {
        removeTrayIcon();
    }
}

QString UpdateDaemon::iconFileFor(const QString &path, const QString &ext) const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/tray-icons/")
         + iconIdFor(path) + ext;
}

// ── Tray icon and window ──────────────────────────────────────────────────────

void UpdateDaemon::setupTrayIcon()
{
    m_trayIcon = new KStatusNotifierItem(QStringLiteral("appimagemanager-updates"), this);
    m_trayIcon->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_trayIcon->setTitle(i18n("AppImage Manager"));
    m_trayIcon->setToolTipIconByName(QStringLiteral("appimagemanager"));
    m_trayIcon->setToolTipTitle(i18n("AppImage Manager"));

    // Left click: the overview window. Middle click: check now. Right click: the menu.
    connect(m_trayIcon, &KStatusNotifierItem::activateRequested, this,
            [this](bool, const QPoint &pos) { toggleTrayWindow(pos); });
    connect(m_trayIcon, &KStatusNotifierItem::secondaryActivateRequested, this, &UpdateDaemon::checkNow);

    if (QMenu *menu = m_trayIcon->contextMenu()) {
        QAction *open = menu->addAction(QIcon::fromTheme(QStringLiteral("appimagemanager")),
                                        i18n("Open AppImage Manager"));
        connect(open, &QAction::triggered, this, [this]() { openManager(); });
        QAction *check = menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                                         i18n("Check for Updates"));
        connect(check, &QAction::triggered, this, &UpdateDaemon::checkNow);
        connect(m_model, &TrayModel::summaryChanged, check, [this, check]() {
            check->setEnabled(!m_model->checking() && m_model->updatingCount() == 0);
        });
        QAction *settings = menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), i18n("Settings…"));
        connect(settings, &QAction::triggered, this, &UpdateDaemon::openSettings);
    }
    updateTray();
}

void UpdateDaemon::removeTrayIcon()
{
    hideTrayWindow();
    delete m_trayIcon;
    m_trayIcon = nullptr;
}

void UpdateDaemon::updateTray()
{
    if (!m_trayIcon)
        return;
    const int count = m_model->updateCount();
    const int updating = m_model->updatingCount();

    if (count > 0)
        m_trayIcon->setIconByPixmap(badgedIcon(count));
    else
        m_trayIcon->setIconByName(QStringLiteral("appimagemanager"));
    // Passive moves the icon into the tray's hidden items when there is nothing to see.
    m_trayIcon->setStatus(count > 0 || updating > 0 || m_model->checking()
                          ? KStatusNotifierItem::Active : KStatusNotifierItem::Passive);

    QString status;
    if (m_model->checking())
        status = i18n("Checking for updates…");
    else if (updating > 0)
        status = i18np("Updating 1 AppImage…", "Updating %1 AppImages…", updating);
    else if (count > 0)
        status = i18np("1 update available", "%1 updates available", count);
    else
        status = i18n("No updates available");
    m_trayIcon->setToolTipSubTitle(status + QLatin1Char('\n') + m_model->scheduleText());
}

bool UpdateDaemon::createTrayWindow()
{
    if (m_trayWindow)
        return true;
    m_engine = new QQmlApplicationEngine(this);
    KLocalization::setupLocalizedContext(m_engine);
    m_engine->rootContext()->setContextProperty(QStringLiteral("trayModel"), m_model);
    m_engine->rootContext()->setContextProperty(QStringLiteral("daemon"), this);
    m_engine->load(QUrl(QStringLiteral("qrc:/appimagemanager/TrayWindow.qml")));
    const QList<QObject *> roots = m_engine->rootObjects();
    m_trayWindow = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
    if (!m_trayWindow) {
        qCWarning(AIM_LOG) << "Failed to load TrayWindow.qml";
        return false;
    }
    return true;
}

void UpdateDaemon::toggleTrayWindow(const QPoint &pos)
{
    if (m_trayWindow && m_trayWindow->isVisible()) {
        m_trayWindow->hide();
        return;
    }
    if (!createTrayWindow())
        return;

    QScreen *screen = QGuiApplication::screenAt(pos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect area = screen->availableGeometry();
    const int gap = 8;
    // The tray is usually in a corner: open towards the screen's middle from the click.
    const bool bottom = pos.isNull() || pos.y() > area.center().y();
    const bool right = pos.isNull() || pos.x() > area.center().x();
    const int w = m_trayWindow->width();
    const int h = m_trayWindow->height();

#ifdef HAVE_LAYERSHELLQT
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        // Wayland windows cannot place themselves; a layer surface anchored to a screen
        // corner can. The compositor keeps it clear of panels.
        auto *layer = LayerShellQt::Window::get(m_trayWindow);
        layer->setScope(QStringLiteral("appimagemanager-tray"));
        layer->setLayer(LayerShellQt::Window::LayerTop);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
        layer->setActivateOnShow(true);
        layer->setWantsToBeOnActiveScreen(false);
        layer->setScreen(screen);
        LayerShellQt::Window::Anchors anchors;
        anchors |= bottom ? LayerShellQt::Window::AnchorBottom : LayerShellQt::Window::AnchorTop;
        anchors |= right ? LayerShellQt::Window::AnchorRight : LayerShellQt::Window::AnchorLeft;
        layer->setAnchors(anchors);
        // Under the click along the panel, clamped to the screen.
        const QRect geometry = screen->geometry();
        int side = gap;
        if (!pos.isNull()) {
            side = right ? geometry.right() - pos.x() - w / 2 : pos.x() - geometry.left() - w / 2;
            side = qBound(gap, side, qMax(gap, geometry.width() - w - gap));
        }
        layer->setMargins(QMargins(right ? 0 : side, bottom ? 0 : gap, right ? side : 0, bottom ? gap : 0));
    } else
#endif
    {
        int x = pos.isNull() ? area.right() - w - gap : pos.x() - w / 2;
        x = qBound(area.left() + gap, x, area.right() - w - gap);
        const int y = bottom ? area.bottom() - h - gap : area.top() + gap;
        m_trayWindow->setPosition(x, y);
    }
    m_trayWindow->show();
    m_trayWindow->raise();
    m_trayWindow->requestActivate();
}

void UpdateDaemon::hideTrayWindow()
{
    if (m_trayWindow)
        m_trayWindow->hide();
}

void UpdateDaemon::openManager(const QString &selectPath)
{
    hideTrayWindow();
    launchDashboard(selectPath.isEmpty() ? QStringList() : QStringList{ QStringLiteral("--select"), selectPath });
}

void UpdateDaemon::openSettings()
{
    hideTrayWindow();
    launchDashboard({ QStringLiteral("--settings") });
}

// ── Downloads folder ──────────────────────────────────────────────────────────

void UpdateDaemon::onDownloadAppeared(const QString &path, const QString &displayName)
{
    auto *n = new KNotification(QStringLiteral("downloaded"), KNotification::Persistent, this);
    n->setTitle(i18n("%1 downloaded", displayName));
    n->setIconName(QStringLiteral("application-x-executable"));
    auto *action = n->addAction(i18n("Manage"));
    connect(action, &KNotificationAction::activated, this, [path]() {
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), { path }))
            qCWarning(AIM_LOG) << "Failed to launch appimagemanager for" << path;
    });
    n->sendEvent();
}
