// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "amstoremodel.h"
#include "logging.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>
#include <KLocalizedString>
#include <algorithm>
#include "appimagereader.h"
#include "appimagemanager.h"
#include "appsettings.h"
#include "releasechecker.h"
#include "updatesource.h"
#include <QSysInfo>

static bool isFileFresh(const QString &path, qint64 maxAgeHours = 24)
{
    const QFileInfo fi(path);
    if (!fi.exists()) return false;
    return fi.lastModified().secsTo(QDateTime::currentDateTime()) < maxAgeHours * 3600LL;
}

// The CPU the pkgforge and AM lists are published for: x86_64 or aarch64 (empty for others).
static QString catalogArch()
{
    const QString cpu = QSysInfo::currentCpuArchitecture();
    if (cpu == QLatin1String("x86_64"))
        return QStringLiteral("x86_64");
    if (cpu == QLatin1String("arm64"))
        return QStringLiteral("aarch64");
    return {};
}

// pkgforge and AM publish one list per CPU: a stored address of either CPU's list is pointed
// at this computer's (so a list set up on another computer stays right). Empty when there is
// no list for this CPU.
static QString effectiveUrl(const StoreSource &src)
{
    static const QRegularExpression perCpu(QStringLiteral(R"((?:/pkgcache/|/programs/)(x86_64|aarch64)(?:-Linux\.json|-appimages)$)"));
    QString url = src.url;
    const QRegularExpressionMatch m = perCpu.match(url);
    if (!m.hasMatch())
        return url;
    const QString arch = catalogArch();
    if (arch.isEmpty())
        return {};
    return url.replace(m.capturedStart(1), m.capturedLength(1), arch);
}

static bool amInstalledOnSystem()
{
    return !QStandardPaths::findExecutable(QStringLiteral("am")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("appman")).isEmpty()
        || QFile::exists(QDir::homePath() + QStringLiteral("/.local/bin/appman"));
}

static StoreSource pkgforgeSource()
{
    StoreSource src;
    src.id = QStringLiteral("pkgforge");
    src.name = QStringLiteral("pkgforge");
    src.url = QStringLiteral("https://meta.pkgforge.dev/pkgcache/x86_64-Linux.json");
    src.type = QStringLiteral("pkgforge");
    src.enabled = true;
    return src;
}

static QString normalizeForMatch(const QString &name)
{
    static const QRegularExpression nonAlnum(QStringLiteral("[^a-z0-9]"));
    return name.toLower().remove(nonAlnum);
}

AMStoreModel::AMStoreModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_downloadReply(nullptr)
    , m_downloadFile(nullptr)
    , m_pendingSourcesCount(0)
    , m_loading(false)
    , m_categoryFilter(QStringLiteral("All"))
    , m_sortAscending(true)
    , m_storeSource(0)
    , m_isInstalling(false)
    , m_installStage(0)
    , m_process(nullptr)
    , m_amInstalled(amInstalledOnSystem())
    , m_initialized(false)
    , m_filterGeneration(0)
{
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(m_cacheDir);

    QSettings s(QStringLiteral("io.github.appimagemanager"), QStringLiteral("AppImageManager"));
    m_storeSource = s.value(QStringLiteral("Store/source"), 0).toInt();
    loadSources();
}

AMStoreModel::~AMStoreModel()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished();
    }
    for (auto *reply : m_activeReplies.keys()) {
        reply->abort();
        reply->deleteLater();
    }
    m_activeReplies.clear();
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->deleteLater();
    }
}

int AMStoreModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_apps.size();
}

QVariant AMStoreModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_apps.size())
        return {};

    const auto &app = m_apps[index.row()];
    switch (role) {
    case PackageNameRole: return app.packageName;
    case DisplayNameRole: return app.displayName;
    case DescriptionRole: return app.description;
    case CategoriesRole:  return app.categories;
    case IconSourceRole:  return app.iconSource;
    case HomepageRole:    return app.homepage;
    case DownloadUrlRole: return app.downloadUrl;
    case AuthorRole:      return app.author;
    case LicenseRole:     return app.license;
    case HasAmScriptRole: return app.hasAmScript;
    case HasFeedDataRole: return app.hasFeedData;
    case SectionCategoryRole: {
        const QString first = app.categories.section(QLatin1Char(';'), 0, 0, QString::SectionSkipEmpty);
        return first.isEmpty() ? i18nc("list section for apps without a category", "Other") : first;
    }
    }
    return {};
}

QHash<int, QByteArray> AMStoreModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[PackageNameRole] = "packageName";
    roles[DisplayNameRole] = "displayName";
    roles[DescriptionRole] = "description";
    roles[CategoriesRole]  = "categories";
    roles[IconSourceRole]  = "iconSource";
    roles[HomepageRole]    = "homepage";
    roles[DownloadUrlRole] = "downloadUrl";
    roles[AuthorRole]      = "author";
    roles[LicenseRole]     = "license";
    roles[HasAmScriptRole] = "hasAmScript";
    roles[HasFeedDataRole] = "hasFeedData";
    roles[SectionCategoryRole] = "sectionCategory";
    return roles;
}

void AMStoreModel::setFilterText(const QString &text)
{
    if (m_filterText == text) return;
    m_filterText = text;
    Q_EMIT filterTextChanged();
    applyFilter();
}

void AMStoreModel::setCategoryFilter(const QString &category)
{
    if (m_categoryFilter == category) return;
    m_categoryFilter = category;
    Q_EMIT categoryFilterChanged();
    applyFilter();
}

void AMStoreModel::setSortAscending(bool ascending)
{
    if (m_sortAscending == ascending) return;
    m_sortAscending = ascending;
    Q_EMIT sortAscendingChanged();
    applyFilter();
}

void AMStoreModel::setStoreSource(int source)
{
    if (m_storeSource == source) return;
    m_storeSource = source;
    Q_EMIT storeSourceChanged();

    QSettings s(QStringLiteral("io.github.appimagemanager"), QStringLiteral("AppImageManager"));
    s.setValue(QStringLiteral("Store/source"), m_storeSource);

    updateAvailableCategories();
    applyFilter();
}

QVariantMap AMStoreModel::itemData(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_apps.size()) return map;

    const auto &app = m_apps[row];
    map[QStringLiteral("packageName")] = app.packageName;
    map[QStringLiteral("displayName")] = app.displayName;
    map[QStringLiteral("description")] = app.description;
    map[QStringLiteral("categories")]  = app.categories;
    map[QStringLiteral("iconSource")]  = app.iconSource;
    map[QStringLiteral("homepage")]    = app.homepage;
    map[QStringLiteral("downloadUrl")] = app.downloadUrl;
    map[QStringLiteral("author")]      = app.author;
    map[QStringLiteral("license")]     = app.license;
    map[QStringLiteral("hasAmScript")] = app.hasAmScript;
    map[QStringLiteral("hasFeedData")] = app.hasFeedData;
    map[QStringLiteral("sourceIds")]   = app.sourceIds;
    map[QStringLiteral("preferAm")]    = app.preferAm;
    return map;
}

// ── Loading ────────────────────────────────────────────────────────────────

void AMStoreModel::initialize()
{
    if (m_initialized) return;
    m_initialized = true;
    load(false);
}

void AMStoreModel::sync()
{
    m_initialized = true;
    load(true);
}

QString AMStoreModel::amScriptBase() const
{
    return QStringLiteral("https://github.com/ivan-hc/AM/blob/main/programs/%1/")
        .arg(catalogArch().isEmpty() ? QStringLiteral("x86_64") : catalogArch());
}

QVariantList AMStoreModel::sources() const
{
    QVariantList list;
    for (const auto &src : m_sources) {
        QVariantMap map;
        map[QStringLiteral("id")] = src.id;
        map[QStringLiteral("name")] = src.name;
        const QString url = effectiveUrl(src);
        map[QStringLiteral("url")] = url.isEmpty() ? src.url : url;
        map[QStringLiteral("type")] = src.type;
        map[QStringLiteral("enabled")] = src.enabled;
        map[QStringLiteral("available")] = !url.isEmpty();
        QString note;
        if (url.isEmpty())
            note = i18n("Not available for this computer's processor.");
        else if (src.type == QStringLiteral("appimagehub"))
            note = i18n("More apps, from each project's releases. Some projects no longer publish AppImages, so an install can fail.");
        else if (src.type == QStringLiteral("am-database") && !m_amInstalled)
            note = i18n("Needs AM or AppMan installed (github.com/ivan-hc/AM). Without it, only apps also on AppImage Hub can be installed.");
        map[QStringLiteral("note")] = note;
        list.append(map);
    }
    return list;
}

void AMStoreModel::loadSources()
{
    m_sources.clear();
    QSettings s(QStringLiteral("io.github.appimagemanager"), QStringLiteral("AppImageManager"));
    const QString jsonStr = s.value(QStringLiteral("Store/sourcesJson")).toString();
    if (!jsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const auto &val : arr) {
                const QJsonObject obj = val.toObject();
                StoreSource src;
                src.id = obj.value(QStringLiteral("id")).toString();
                src.name = obj.value(QStringLiteral("name")).toString();
                src.url = obj.value(QStringLiteral("url")).toString();
                src.type = obj.value(QStringLiteral("type")).toString();
                src.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
                if (!src.id.isEmpty() && !src.name.isEmpty() && !src.url.isEmpty()) {
                    m_sources.append(src);
                }
            }
        }
    }

    if (m_sources.isEmpty()) {
        StoreSource hub;
        hub.id = QStringLiteral("appimage-hub");
        hub.name = QStringLiteral("AppImage Hub");
        hub.url = QStringLiteral("https://appimage.github.io/feed.json");
        hub.type = QStringLiteral("appimagehub");
        hub.enabled = false; // an option for more apps; pkgforge is the default

        StoreSource am;
        am.id = QStringLiteral("am-database");
        am.name = QStringLiteral("AM Database");
        am.url = QStringLiteral("https://raw.githubusercontent.com/ivan-hc/AM/main/programs/x86_64-appimages");
        am.type = QStringLiteral("am-database");
        am.enabled = false; // needs AM or AppMan installed

        // pkgforge first and on: direct downloads that just work. Existing lists are kept.
        m_sources.append(pkgforgeSource());
        m_sources.append(hub);
        m_sources.append(am);
        s.setValue(QStringLiteral("Store/addedPkgforge"), true);

        saveSources();
    } else if (!s.value(QStringLiteral("Store/addedPkgforge"), false).toBool()) {
        // pkgforge became a built-in catalog in 2.3: added once to existing lists (after the
        // others, on), and not again if the user removes it.
        s.setValue(QStringLiteral("Store/addedPkgforge"), true);
        const bool present = std::any_of(m_sources.cbegin(), m_sources.cend(),
                                         [](const StoreSource &src) { return src.type == QStringLiteral("pkgforge"); });
        if (!present) {
            m_sources.append(pkgforgeSource());
            saveSources();
        }
    }
    Q_EMIT sourcesChanged();
}

void AMStoreModel::saveSources()
{
    QJsonArray arr;
    for (const auto &src : m_sources) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = src.id;
        obj[QStringLiteral("name")] = src.name;
        obj[QStringLiteral("url")] = src.url;
        obj[QStringLiteral("type")] = src.type;
        obj[QStringLiteral("enabled")] = src.enabled;
        arr.append(obj);
    }
    QJsonDocument doc(arr);
    QSettings s(QStringLiteral("io.github.appimagemanager"), QStringLiteral("AppImageManager"));
    s.setValue(QStringLiteral("Store/sourcesJson"), QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    Q_EMIT sourcesChanged();
}

void AMStoreModel::addSource(const QString &name, const QString &url, const QString &type, bool enabled)
{
    const QString trimmedUrl = url.trimmed();
    QUrl qurl(trimmedUrl);
    if (name.trimmed().isEmpty() || !qurl.isValid() || qurl.host().isEmpty() ||
        (qurl.scheme() != QStringLiteral("http") && qurl.scheme() != QStringLiteral("https"))) {
        return;
    }

    StoreSource src;
    src.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    src.name = name.trimmed();
    src.url = trimmedUrl;
    src.type = type.trimmed();
    src.enabled = enabled;

    m_sources.append(src);
    saveSources();
    if (m_initialized) sync(); // otherwise the lazy initialize() loads them
}

void AMStoreModel::updateSource(const QString &id, const QString &name, const QString &url, const QString &type, bool enabled)
{
    const QString trimmedUrl = url.trimmed();
    QUrl qurl(trimmedUrl);
    if (name.trimmed().isEmpty() || !qurl.isValid() || qurl.host().isEmpty() ||
        (qurl.scheme() != QStringLiteral("http") && qurl.scheme() != QStringLiteral("https"))) {
        return;
    }

    for (auto &src : m_sources) {
        if (src.id == id) {
            src.name = name.trimmed();
            src.url = trimmedUrl;
            src.type = type.trimmed();
            src.enabled = enabled;
            saveSources();
            if (m_initialized) sync(); // otherwise the lazy initialize() loads them
            return;
        }
    }
}

void AMStoreModel::removeSource(const QString &id)
{
    for (int i = 0; i < m_sources.size(); ++i) {
        if (m_sources[i].id == id) {
            const QString cacheFile = m_cacheDir + QStringLiteral("/source_") + id +
                (m_sources[i].type == QStringLiteral("appimagehub") ? QStringLiteral(".json") : QStringLiteral(".txt"));
            QFile::remove(cacheFile);

            m_sources.removeAt(i);
            // storeSource is a 1-based position in m_sources: keep it on the same catalog.
            if (m_storeSource == i + 1)
                setStoreSource(0);
            else if (m_storeSource > i + 1)
                setStoreSource(m_storeSource - 1);
            saveSources();
            if (m_initialized) sync(); // otherwise the lazy initialize() loads them
            return;
        }
    }
}

void AMStoreModel::resetSourcesToDefault()
{
    for (const auto &src : m_sources) {
        const QString cacheFile = m_cacheDir + QStringLiteral("/source_") + src.id +
            (src.type == QStringLiteral("appimagehub") ? QStringLiteral(".json") : QStringLiteral(".txt"));
        QFile::remove(cacheFile);
    }

    m_sources.clear();
    QSettings s(QStringLiteral("io.github.appimagemanager"), QStringLiteral("AppImageManager"));
    s.remove(QStringLiteral("Store/sourcesJson"));
    setStoreSource(0);
    loadSources();
    if (m_initialized) sync(); // otherwise the lazy initialize() loads them
}

void AMStoreModel::setSourcesOrder(const QStringList &idOrder)
{
    // storeSource is a 1-based position in m_sources: keep it on the same catalog.
    const QString filteredId = m_storeSource > 0 && m_storeSource <= m_sources.size()
        ? m_sources.at(m_storeSource - 1).id : QString();

    QList<StoreSource> reordered;
    reordered.reserve(m_sources.size());
    for (const QString &id : idOrder) {
        for (const auto &src : m_sources) {
            if (src.id == id) {
                reordered.append(src);
                break;
            }
        }
    }
    for (const auto &src : m_sources) {
        if (!idOrder.contains(src.id)) {
            reordered.append(src);
        }
    }
    m_sources = reordered;
    saveSources();
    if (!filteredId.isEmpty()) {
        for (int i = 0; i < m_sources.size(); ++i) {
            if (m_sources.at(i).id == filteredId)
                setStoreSource(i + 1);
        }
    }
    checkAllLoaded();
}

void AMStoreModel::load(bool forceNetwork)
{
    if (m_loading) return;

    m_loading = true;
    Q_EMIT loadingChanged();

    for (auto *reply : m_activeReplies.keys()) {
        reply->abort();
        reply->deleteLater();
    }
    m_activeReplies.clear();
    m_parsedSourceApps.clear();

    QList<StoreSource> enabledSources;
    for (const auto &src : m_sources) {
        if (src.enabled && !effectiveUrl(src).isEmpty()) {
            enabledSources.append(src);
        }
    }

    if (enabledSources.isEmpty()) {
        m_allApps.clear();
        m_availableCategories.clear();
        Q_EMIT availableCategoriesChanged();
        m_loading = false;
        Q_EMIT loadingChanged();
        applyFilter();
        return;
    }

    m_pendingSourcesCount = enabledSources.size();

    for (const auto &src : enabledSources) {
        const QString cacheExt = (src.type == QStringLiteral("appimagehub")) ? QStringLiteral(".json") : QStringLiteral(".txt");
        const QString cachePath = m_cacheDir + QStringLiteral("/source_") + src.id + cacheExt;

        if (!forceNetwork && isFileFresh(cachePath)) {
            QFile f(cachePath);
            if (f.open(QIODevice::ReadOnly)) {
                const QByteArray data = f.readAll();
                auto *w = new QFutureWatcher<QVector<StoreApp>>(this);
                connect(w, &QFutureWatcher<QVector<StoreApp>>::finished, this, [this, w, src]() {
                    onSourceParsed(src.id, w->result());
                    w->deleteLater();
                });
                w->setFuture(QtConcurrent::run([data, src]() -> QVector<StoreApp> {
                    if (src.type == QStringLiteral("appimagehub")) {
                        return AMStoreModel::parseFeedJsonSync(data);
                    } else if (src.type == QStringLiteral("universal")) {
                        return AMStoreModel::parseUniversalJsonSync(data);
                    } else if (src.type == QStringLiteral("pkgforge")) {
                        return AMStoreModel::parsePkgforgeJsonSync(data);
                    } else {
                        return AMStoreModel::parseAmDatabaseSync(QString::fromUtf8(data));
                    }
                }));
                continue;
            }
        }

        QNetworkRequest req((QUrl(effectiveUrl(src))));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = m_nam->get(req);
        m_activeReplies.insert(reply, src.id);
        connect(reply, &QNetworkReply::finished, this, &AMStoreModel::onReplyFinished);
    }
}

void AMStoreModel::onReplyFinished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    const QString sourceId = m_activeReplies.take(reply);
    if (sourceId.isEmpty()) {
        reply->deleteLater();
        return;
    }

    StoreSource src;
    for (const auto &s : m_sources) {
        if (s.id == sourceId) {
            src = s;
            break;
        }
    }

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(AIM_LOG) << "Failed to fetch source" << src.name << ":" << reply->errorString();
        reply->deleteLater();
        onSourceParsed(sourceId, {});
        return;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    const QString cacheExt = (src.type == QStringLiteral("appimagehub")) ? QStringLiteral(".json") : QStringLiteral(".txt");
    const QString cachePath = m_cacheDir + QStringLiteral("/source_") + src.id + cacheExt;
    QFile f(cachePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(data);
    }

    auto *w = new QFutureWatcher<QVector<StoreApp>>(this);
    connect(w, &QFutureWatcher<QVector<StoreApp>>::finished, this, [this, w, sourceId]() {
        onSourceParsed(sourceId, w->result());
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([data, src]() -> QVector<StoreApp> {
        if (src.type == QStringLiteral("appimagehub")) {
            return AMStoreModel::parseFeedJsonSync(data);
        } else if (src.type == QStringLiteral("universal")) {
            return AMStoreModel::parseUniversalJsonSync(data);
        } else if (src.type == QStringLiteral("pkgforge")) {
            return AMStoreModel::parsePkgforgeJsonSync(data);
        } else {
            return AMStoreModel::parseAmDatabaseSync(QString::fromUtf8(data));
        }
    }));
}

void AMStoreModel::onSourceParsed(const QString &sourceId, QVector<StoreApp> apps)
{
    m_parsedSourceApps[sourceId] = std::move(apps);
    m_pendingSourcesCount--;

    if (m_pendingSourcesCount <= 0) {
        m_pendingSourcesCount = 0;
        checkAllLoaded();
    }
}

void AMStoreModel::checkAllLoaded()
{
    QList<std::pair<StoreSource, QVector<StoreApp>>> sourcesData;
    for (const auto &src : m_sources) {
        if (src.enabled && m_parsedSourceApps.contains(src.id)) {
            sourcesData.append({src, m_parsedSourceApps[src.id]});
        }
    }

    using Result = std::pair<QVector<StoreApp>, QStringList>;
    auto *w = new QFutureWatcher<Result>(this);
    connect(w, &QFutureWatcher<Result>::finished, this, [this, w]() {
        auto [merged, cats] = w->result();
        m_allApps             = std::move(merged);
        m_availableCategories = std::move(cats);
        Q_EMIT availableCategoriesChanged();
        m_loading = false;
        Q_EMIT loadingChanged();
        applyFilter();
        w->deleteLater();
    });

    w->setFuture(QtConcurrent::run([sourcesData]() -> Result {
        QVector<StoreApp> mergedList;
        QHash<QString, int> nameToIndex;

        auto sortedSourcesData = sourcesData;
        std::stable_sort(sortedSourcesData.begin(), sortedSourcesData.end(), [](const auto &a, const auto &b) {
            if (a.first.type == QStringLiteral("am-database") && b.first.type != QStringLiteral("am-database"))
                return true;
            return false;
        });

        for (const auto &pair : sortedSourcesData) {
            const auto &source = pair.first;
            const auto &apps = pair.second;

            for (auto app : apps) {
                QString norm = normalizeForMatch(app.displayName);
                if (!nameToIndex.contains(norm)) {
                    app.sourceIds = { source.id };
                    if (source.type == QStringLiteral("am-database")) {
                        app.hasAmScript = true;
                    } else if (source.type == QStringLiteral("appimagehub")) {
                        app.hasFeedData = true;
                    }
                    mergedList.append(app);
                    nameToIndex.insert(norm, mergedList.size() - 1);
                } else {
                    int idx = nameToIndex[norm];
                    StoreApp &existing = mergedList[idx];
                    if (!existing.sourceIds.contains(source.id)) {
                        existing.sourceIds.append(source.id);
                    }
                    if (source.type == QStringLiteral("am-database") || app.hasAmScript) {
                        existing.hasAmScript = true;
                    }
                    if (source.type == QStringLiteral("appimagehub") || source.type == QStringLiteral("universal") || app.hasFeedData || !app.downloadUrl.isEmpty()) {
                        existing.hasFeedData = true;
                        if (existing.description.length() < app.description.length())
                            existing.description = app.description;
                        if (!app.categories.isEmpty()) existing.categories = app.categories;
                        if (existing.author.isEmpty()) existing.author = app.author;
                        if (existing.license.isEmpty()) existing.license = app.license;
                        if (!app.homepage.isEmpty()) existing.homepage = app.homepage;
                        if (!app.downloadUrl.isEmpty()) {
                            existing.downloadUrl = app.downloadUrl;
                            existing.repoUrl = app.repoUrl;
                        }
                        if (!app.iconSource.isEmpty()) existing.iconSource = app.iconSource;
                        if (existing.packageName.isEmpty()) existing.packageName = app.packageName;
                    }
                }
            }
        }

        // Per source: normalized name → whether the app it lists first has an AM script
        // (one lookup per app instead of a scan of the whole source).
        QHash<QString, QHash<QString, bool>> hasAmBySource;
        for (const auto &pair : sourcesData) {
            if (pair.first.type == QStringLiteral("am-database"))
                continue;
            auto &byName = hasAmBySource[pair.first.id];
            for (const auto &sa : pair.second) {
                const QString norm = normalizeForMatch(sa.displayName);
                if (!byName.contains(norm))
                    byName.insert(norm, sa.hasAmScript);
            }
        }

        for (auto &app : mergedList) {
            const QString norm = normalizeForMatch(app.displayName);
            for (const auto &pair : sourcesData) {
                const auto &source = pair.first;
                if (app.sourceIds.contains(source.id)) {
                    app.preferAm = source.type == QStringLiteral("am-database")
                                || hasAmBySource.value(source.id).value(norm, false);
                    break;
                }
            }
        }

        QSet<QString> cats;
        for (const StoreApp &app : std::as_const(mergedList)) {
            for (const auto &cat : app.categories.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                const QString t = cat.trimmed();
                if (!t.isEmpty()) cats.insert(t);
            }
        }
        QStringList catList(cats.cbegin(), cats.cend());
        std::sort(catList.begin(), catList.end());

        // Theme icon names are not checked here: 10,000 hasThemeIcon() calls took half a
        // minute. The page's Kirigami.Icon falls back to a generic icon for names the
        // theme lacks, and only looks up the rows on screen.
        return {std::move(mergedList), std::move(catList)};
    }));
}

// ── Parsing — pure static functions, safe on worker threads ───────────────

QVector<StoreApp> AMStoreModel::parseAmDatabaseSync(const QString &rawText)
{
    QVector<StoreApp> apps;
    const QStringList lines = rawText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    apps.reserve(lines.size());

    for (const auto &line : lines) {
        if (!line.startsWith(QString::fromUtf8("◆"))) continue;

        const QString cleaned = line.mid(1).trimmed();
        const int sep = cleaned.indexOf(QStringLiteral(" : "));
        if (sep == -1) continue;

        StoreApp app;
        app.packageName = cleaned.left(sep).trimmed();
        app.displayName = cleanName(app.packageName);
        app.description = cleaned.mid(sep + 3).trimmed();
        app.iconSource  = app.packageName;
        app.homepage    = QStringLiteral("https://github.com/ivan-hc/AM/blob/main/programs/%1/%2")
                              .arg(catalogArch().isEmpty() ? QStringLiteral("x86_64") : catalogArch(), app.packageName);
        app.hasAmScript = true;
        apps.append(std::move(app));
    }

    return apps;
}

QVector<StoreApp> AMStoreModel::parseFeedJsonSync(const QByteArray &data)
{
    QVector<StoreApp> apps;

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return apps;

    const QJsonArray items = doc.object().value(QStringLiteral("items")).toArray();
    apps.reserve(items.size());

    for (const auto &itemVal : items) {
        const QJsonObject item = itemVal.toObject();

        const QString name = item.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) continue;

        StoreApp app;
        app.displayName = name;
        app.packageName = name.toLower().replace(QLatin1Char(' '), QLatin1Char('-'));
        app.description = item.value(QStringLiteral("description")).toString();
        app.hasFeedData = true;

        const QJsonArray cats = item.value(QStringLiteral("categories")).toArray();
        QStringList catList;
        catList.reserve(cats.size());
        for (const auto &c : cats) {
            const QString cat = c.toString();
            if (!cat.isEmpty()) catList.append(cat);
        }
        app.categories = catList.join(QLatin1Char(';'));

        const QJsonArray authors = item.value(QStringLiteral("authors")).toArray();
        if (!authors.isEmpty())
            app.author = authors.first().toObject().value(QStringLiteral("name")).toString();

        app.license = item.value(QStringLiteral("license")).toString();

        const QJsonArray links = item.value(QStringLiteral("links")).toArray();
        for (const auto &linkVal : links) {
            const QJsonObject link = linkVal.toObject();
            const QString type = link.value(QStringLiteral("type")).toString();
            const QString url  = link.value(QStringLiteral("url")).toString();
            if (type == QStringLiteral("Download") && !url.isEmpty())
                app.downloadUrl = url;
            else if (type == QStringLiteral("GitHub") && !url.isEmpty() && app.homepage.isEmpty())
                app.homepage = QStringLiteral("https://github.com/") + url;
        }

        // Only use raster icon URLs — SVGs with CSS mesh gradients crash Qt's renderer.
        const QJsonArray icons = item.value(QStringLiteral("icons")).toArray();
        for (const auto &iconVal : icons) {
            const QString iconPath = iconVal.toString();
            if (iconPath.isEmpty()) continue;
            const bool isSafe = iconPath.endsWith(QStringLiteral(".png"),  Qt::CaseInsensitive)
                             || iconPath.endsWith(QStringLiteral(".jpg"),  Qt::CaseInsensitive)
                             || iconPath.endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive)
                             || iconPath.endsWith(QStringLiteral(".webp"), Qt::CaseInsensitive);
            if (!isSafe) continue;
            app.iconSource = QStringLiteral("https://appimage.github.io/database/") + iconPath;
            break;
        }

        apps.append(std::move(app));
    }

    return apps;
}

QVector<StoreApp> AMStoreModel::parseUniversalJsonSync(const QByteArray &data)
{
    QVector<StoreApp> apps;
    const QJsonDocument doc = QJsonDocument::fromJson(data);

    QJsonArray array;
    if (doc.isArray()) {
        array = doc.array();
    } else if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QJsonValue val = obj.value(QStringLiteral("apps"));
        if (val.isArray()) {
            array = val.toArray();
        } else {
            const QJsonValue itemsVal = obj.value(QStringLiteral("items"));
            if (itemsVal.isArray()) {
                array = itemsVal.toArray();
            }
        }
    }

    if (array.isEmpty()) return apps;
    apps.reserve(array.size());

    for (const auto &itemVal : array) {
        const QJsonObject item = itemVal.toObject();
        QString name = item.value(QStringLiteral("displayName")).toString();
        if (name.isEmpty()) {
            name = item.value(QStringLiteral("name")).toString();
        }
        if (name.isEmpty()) continue;

        StoreApp app;
        app.displayName = name;
        app.packageName = item.value(QStringLiteral("packageName")).toString();
        if (app.packageName.isEmpty()) {
            app.packageName = name.toLower().replace(QLatin1Char(' '), QLatin1Char('-'));
        }
        app.description = item.value(QStringLiteral("description")).toString();
        app.categories = item.value(QStringLiteral("categories")).toString();
        app.iconSource = item.value(QStringLiteral("iconSource")).toString();
        app.homepage = item.value(QStringLiteral("homepage")).toString();
        app.downloadUrl = item.value(QStringLiteral("downloadUrl")).toString();
        app.author = item.value(QStringLiteral("author")).toString();
        app.license = item.value(QStringLiteral("license")).toString();
        app.hasAmScript = item.value(QStringLiteral("hasAmScript")).toBool(false);
        app.hasFeedData = !app.downloadUrl.isEmpty();

        apps.append(std::move(app));
    }
    return apps;
}

QVector<StoreApp> AMStoreModel::parsePkgforgeJsonSync(const QByteArray &data)
{
    // A JSON array of packages; pkg_type "appimage" are AppImages. Flags are strings ("false").
    const auto flag = [](const QJsonValue &v) { return v.isBool() ? v.toBool() : v.toString() == QLatin1String("true"); };
    const auto first = [](const QJsonValue &v) {
        if (!v.isArray())
            return v.toString();
        const QJsonArray a = v.toArray();
        return a.isEmpty() ? QString() : a.at(0).toString();
    };

    QVector<StoreApp> apps;
    QHash<QString, qsizetype> byName;
    QSet<QString> prerelease;
    const QJsonArray packages = QJsonDocument::fromJson(data).array();
    for (const QJsonValue &value : packages) {
        const QJsonObject pkg = value.toObject();
        if (pkg.value(QLatin1String("pkg_type")).toString() != QLatin1String("appimage")
            || flag(pkg.value(QLatin1String("_disabled"))) || flag(pkg.value(QLatin1String("deprecated")))
            || !flag(pkg.value(QLatin1String("installable"))))
            continue;
        const QString name = pkg.value(QLatin1String("pkg_name")).toString();
        const QString url = pkg.value(QLatin1String("download_url")).toString();
        if (name.isEmpty() || url.isEmpty())
            continue;

        StoreApp app;
        app.packageName = name;
        app.displayName = cleanName(name);
        app.description = pkg.value(QLatin1String("description")).toString();
        QStringList categories;
        for (const QJsonValue &c : pkg.value(QLatin1String("category")).toArray())
            categories << c.toString();
        app.categories = categories.join(QLatin1Char(';'));
        app.homepage = first(pkg.value(QLatin1String("homepage")));
        app.license = first(pkg.value(QLatin1String("license")));
        app.downloadUrl = url;
        app.hasFeedData = true;
        // Copied from the project's own releases: those releases are its update source.
        bool upstream = false;
        for (const QJsonValue &note : pkg.value(QLatin1String("note")).toArray())
            upstream = upstream || note.toString().contains(QLatin1String("Fetched from Upstream"), Qt::CaseInsensitive);
        const QString source = first(pkg.value(QLatin1String("src_url")));
        if (upstream && (source.startsWith(QLatin1String("https://github.com/")) || source.startsWith(QLatin1String("https://codeberg.org/"))))
            app.repoUrl = source;
        // Raster icons only: SVGs with CSS mesh gradients crash Qt's renderer (see the Hub).
        const QString icon = pkg.value(QLatin1String("icon")).toString();
        if (!icon.contains(QLatin1String(".svg"), Qt::CaseInsensitive))
            app.iconSource = icon;

        // The same app may come as a stable and a prerelease build: keep the stable one.
        const bool isPrerelease = pkg.value(QLatin1String("pkg_id")).toString().contains(QLatin1String("prerelease"));
        const auto it = byName.constFind(name);
        if (it != byName.cend()) {
            if (prerelease.contains(name) && !isPrerelease) {
                apps[*it] = app;
                prerelease.remove(name);
            }
            continue;
        }
        byName.insert(name, apps.size());
        if (isPrerelease)
            prerelease.insert(name);
        apps.append(std::move(app));
    }
    return apps;
}

// ── Filter + sort — dispatched to worker thread ───────────────────────────

void AMStoreModel::applyFilter()
{
    if (m_allApps.isEmpty()) return;

    const int gen = ++m_filterGeneration;

    QString targetSourceId;
    if (m_storeSource > 0 && m_storeSource - 1 < m_sources.size()) {
        targetSourceId = m_sources[m_storeSource - 1].id;
    }

    // Snapshot state — captured by value into lambda
    const auto allApps       = m_allApps;
    const auto filterText    = m_filterText;
    const auto categoryFilter = m_categoryFilter;
    const auto sortAscending = m_sortAscending;

    auto *w = new QFutureWatcher<QVector<StoreApp>>(this);
    connect(w, &QFutureWatcher<QVector<StoreApp>>::finished, this, [this, w, gen]() {
        if (gen == m_filterGeneration) {
            beginResetModel();
            m_apps = w->result();
            endResetModel();
        }
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run(
        [allApps, filterText, categoryFilter, sortAscending, targetSourceId]() -> QVector<StoreApp> {
            QVector<StoreApp> apps;
            apps.reserve(allApps.size());
            for (const StoreApp &app : allApps) {
                if (!targetSourceId.isEmpty() && !app.sourceIds.contains(targetSourceId)) continue;

                const bool matchSearch = filterText.isEmpty()
                    || app.displayName.contains(filterText, Qt::CaseInsensitive)
                    || app.description.contains(filterText, Qt::CaseInsensitive);
                const bool matchCategory = categoryFilter.isEmpty()
                    || categoryFilter == QStringLiteral("All")
                    || app.categories.contains(categoryFilter, Qt::CaseInsensitive);

                if (matchSearch && matchCategory)
                    apps.append(app);
            }
            // Without a category filter: grouped by first category (the list shows category
            // sections; apps without one go last), then by name in the chosen direction.
            auto primary = [](const StoreApp &app) {
                return app.categories.section(QLatin1Char(';'), 0, 0, QString::SectionSkipEmpty);
            };
            const bool grouped = categoryFilter.isEmpty() || categoryFilter == QStringLiteral("All");
            std::sort(apps.begin(), apps.end(), [&primary, sortAscending, grouped](const StoreApp &a, const StoreApp &b) {
                const QString ca = grouped ? primary(a) : QString();
                const QString cb = grouped ? primary(b) : QString();
                if (ca != cb) {
                    if (ca.isEmpty() || cb.isEmpty())
                        return cb.isEmpty();
                    return ca.localeAwareCompare(cb) < 0;
                }
                const int byName = a.displayName.localeAwareCompare(b.displayName);
                return sortAscending ? byName < 0 : byName > 0;
            });
            return apps;
        }));
}

void AMStoreModel::updateAvailableCategories()
{
    QString targetSourceId;
    if (m_storeSource > 0 && m_storeSource - 1 < m_sources.size()) {
        targetSourceId = m_sources[m_storeSource - 1].id;
    }

    QSet<QString> cats;
    for (const StoreApp &app : std::as_const(m_allApps)) {
        if (!targetSourceId.isEmpty() && !app.sourceIds.contains(targetSourceId)) continue;
        for (const auto &cat : app.categories.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            const QString t = cat.trimmed();
            if (!t.isEmpty()) cats.insert(t);
        }
    }
    QStringList list(cats.cbegin(), cats.cend());
    std::sort(list.begin(), list.end());

    if (m_availableCategories != list) {
        m_availableCategories = list;
        Q_EMIT availableCategoriesChanged();
    }
}

QString AMStoreModel::cleanName(const QString &packageName)
{
    QStringList parts = packageName.split(QLatin1Char('-'));
    for (auto &part : parts)
        if (!part.isEmpty()) part[0] = part[0].toUpper();
    return parts.join(QLatin1Char(' '));
}

// ── Installation ───────────────────────────────────────────────────────────

void AMStoreModel::setInstallStage(int stage)
{
    if (m_installStage == stage) return;
    m_installStage = stage;
    Q_EMIT installStageChanged();
}

void AMStoreModel::cancelInstallation()
{
    if (m_process) {
        Q_EMIT logReceived(m_activePackage, i18n("Installation cancelled."));
        m_process->kill();
    } else if (m_downloadReply) {
        Q_EMIT logReceived(m_activePackage, i18n("Download cancelled."));
        m_downloadReply->abort();
    }
}

void AMStoreModel::installApp(const QString &packageName, bool forceDownload)
{
    if (m_process || m_downloadReply) return;

    m_activePackage = packageName;
    m_isInstalling  = true;
    Q_EMIT isInstallingChanged();
    setInstallStage(1);

    StoreApp app;
    bool found = false;
    for (const auto &item : m_apps) {
        if (item.packageName == packageName) {
            app = item;
            found = true;
            break;
        }
    }
    if (!found) {
        for (const auto &item : m_allApps) {
            if (item.packageName == packageName) {
                app = item;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        Q_EMIT logReceived(packageName, i18n("Error: Package metadata not found."));
        m_isInstalling = false;
        Q_EMIT isInstallingChanged();
        setInstallStage(0);
        Q_EMIT installationFinished(packageName, false);
        return;
    }

    QString appmanPath = QStandardPaths::findExecutable(QStringLiteral("appman"));
    if (appmanPath.isEmpty()) {
        const QString local = QDir::homePath() + QStringLiteral("/.local/bin/appman");
        if (QFile::exists(local)) appmanPath = local;
    }
    const bool amAvailable = !appmanPath.isEmpty() || !QStandardPaths::findExecutable(QStringLiteral("am")).isEmpty();

    bool useAm = app.hasAmScript && !forceDownload;
    if (useAm && !amAvailable) {
        if (!app.downloadUrl.isEmpty()) {
            Q_EMIT logReceived(packageName, i18n("AM/AppMan is not installed. Falling back to direct AppImage download..."));
            useAm = false;
        } else {
            Q_EMIT logReceived(packageName, i18n("Error: AM/AppMan is required but not installed on your system."));
            m_isInstalling = false;
            Q_EMIT isInstallingChanged();
            setInstallStage(0);
            Q_EMIT installationFinished(packageName, false);
            return;
        }
    }

    if (useAm) {
        QString cmd = QStringLiteral("appman");
        if (!QStandardPaths::findExecutable(QStringLiteral("am")).isEmpty() && appmanPath.isEmpty())
            cmd = QStringLiteral("am");
        else if (!appmanPath.isEmpty())
            cmd = appmanPath;

        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("PATH"), QDir::homePath() + QStringLiteral("/.local/bin:") + env.value(QStringLiteral("PATH")));
        m_process->setProcessEnvironment(env);

        connect(m_process, &QProcess::readyReadStandardOutput, this, &AMStoreModel::readProcessOutput);
        connect(m_process, &QProcess::finished, this, &AMStoreModel::onProcessFinished);

        Q_EMIT logReceived(packageName, i18n("Running: %1 -i %2", cmd, packageName));
        m_process->start(cmd, {QStringLiteral("-i"), packageName});
    } else {
        if (app.downloadUrl.isEmpty()) {
            Q_EMIT logReceived(packageName, i18n("Error: No direct download URL available for this application."));
            m_isInstalling = false;
            Q_EMIT isInstallingChanged();
            setInstallStage(0);
            Q_EMIT installationFinished(packageName, false);
            return;
        }

        resolveAndDownload(app);
    }
}

// Ends an install that failed before or during the download.
void AMStoreModel::failInstall(const QString &packageName, const QString &message)
{
    Q_EMIT logReceived(packageName, message);
    m_isInstalling = false;
    Q_EMIT isInstallingChanged();
    setInstallStage(0);
    Q_EMIT installationFinished(packageName, false);
}

// Catalog links are mostly not files: AppImage Hub's "Download" link is nearly always the
// project's GitHub releases page. A GitHub (or Codeberg/Forgejo/Gitea) repository link is
// resolved to its latest release's AppImage for this CPU; anything else is downloaded and
// checked (a web page is refused).
void AMStoreModel::resolveAndDownload(const StoreApp &app)
{
    const QString packageName = app.packageName;
    const QUrl link(app.downloadUrl);
    if (link.path().endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive)) {
        startDownload(packageName, link, app.repoUrl);
        return;
    }
    static const QRegularExpression repoLink(QStringLiteral(R"(^https?://([^/]+)/([^/]+)/([^/#?]+))"));
    const QRegularExpressionMatch m = repoLink.match(app.downloadUrl);
    const QString host = m.captured(1).toLower();
    const bool github = host == QLatin1String("github.com");
    // Forgejo/Gitea hosts: only links to a release page, so arbitrary sites aren't probed.
    const bool forgejo = !github && m.hasMatch() && app.downloadUrl.contains(QLatin1String("/releases"));
    if (!m.hasMatch() || (!github && !forgejo)) {
        startDownload(packageName, link, app.repoUrl);
        return;
    }
    const QString owner = m.captured(2);
    const QString repo = m.captured(3).endsWith(QLatin1String(".git")) ? m.captured(3).chopped(4) : m.captured(3);
    const QString repoPage = QStringLiteral("https://%1/%2/%3").arg(host, owner, repo);
    const QUrl api(github ? QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest").arg(owner, repo)
                          : QStringLiteral("https://%1/api/v1/repos/%2/%3/releases/latest").arg(host, owner, repo));

    Q_EMIT logReceived(packageName, i18n("Looking for the latest release of %1…", repoPage));
    QNetworkRequest request(api);
    request.setRawHeader("User-Agent", "AppImageManager/1.0");
    request.setRawHeader("Accept", github ? "application/vnd.github+json" : "application/json");
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, packageName, repoPage]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            failInstall(packageName, i18n("Error: Could not read the releases of %1: %2", repoPage,
                                          ReleaseChecker::describeReplyError(reply, true)));
            return;
        }
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString arch = QSysInfo::currentCpuArchitecture();
        QJsonArray appImages;
        for (const QJsonValue &asset : release.value(QLatin1String("assets")).toArray()) {
            const QString name = asset.toObject().value(QLatin1String("name")).toString();
            if (name.endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive)
                && ReleaseChecker::matchesArchitecture(name, arch))
                appImages.append(asset);
        }
        if (appImages.isEmpty()) {
            failInstall(packageName, i18n("Error: The latest release of %1 has no AppImage for this computer.", repoPage));
            return;
        }
        // Several fitting files: the one named like the app, else the first.
        QString url = ReleaseChecker::pickAsset(appImages, {}, packageName + QStringLiteral(".AppImage"), arch);
        if (url.isEmpty())
            url = appImages.first().toObject().value(QLatin1String("browser_download_url")).toString();
        startDownload(packageName, QUrl(url), repoPage);
    });
}

// repoPage: the repository the file came from (it becomes the update source when the
// AppImage doesn't name one itself), or empty.
void AMStoreModel::startDownload(const QString &packageName, const QUrl &url, const QString &repoPage)
{
        Q_EMIT logReceived(packageName, i18n("Downloading %1", url.toDisplayString()));
        setInstallStage(2);

        const QString applicationsDir = AppSettings::instance()->applicationsPath();
        QDir().mkpath(applicationsDir);
        const QString targetPath = applicationsDir + QLatin1Char('/') + packageName + QStringLiteral(".AppImage");
        const QString tempPath = targetPath + QStringLiteral(".new");

        m_downloadFile = new QFile(tempPath, this);
        if (!m_downloadFile->open(QIODevice::WriteOnly)) {
            m_downloadFile->deleteLater();
            m_downloadFile = nullptr;
            failInstall(packageName, i18n("Error: Failed to open temporary file for writing: %1", tempPath));
            return;
        }

        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", "AppImageManager/1.0");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        m_downloadReply = m_nam->get(request);

        connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
            if (!m_downloadReply || !m_downloadFile)
                return;
            // A web page (a download page, an error page) is never an AppImage: stop at once.
            if (m_downloadFile->pos() == 0 && ReleaseChecker::isNonBinaryContentType(
                    m_downloadReply->header(QNetworkRequest::ContentTypeHeader).toString())) {
                m_downloadReply->setProperty("webPage", true);
                m_downloadReply->abort();
                return;
            }
            m_downloadFile->write(m_downloadReply->readAll());
        });

        connect(m_downloadReply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
                static int lastPercent = -1;
                if (percent / 5 != lastPercent / 5) {
                    lastPercent = percent;
                    Q_EMIT logReceived(m_activePackage, i18n("Downloading: %1%", percent));
                }
            }
        });

        connect(m_downloadReply, &QNetworkReply::finished, this, [this, targetPath, tempPath, packageName, url, repoPage]() {
            if (!m_downloadReply) return;
            
            QNetworkReply *reply = m_downloadReply;
            m_downloadReply = nullptr;
            reply->deleteLater();

            if (m_downloadFile) {
                m_downloadFile->close();
                m_downloadFile->deleteLater();
                m_downloadFile = nullptr;
            }

            if (reply->property("webPage").toBool()) {
                QFile::remove(tempPath);
                failInstall(packageName, i18n("Error: %1 is a web page, not an AppImage. Download it from there in your browser.",
                                              url.toDisplayString()));
                return;
            }
            if (reply->error() != QNetworkReply::NoError) {
                QFile::remove(tempPath);
                failInstall(packageName, reply->error() == QNetworkReply::OperationCanceledError
                                             ? i18n("Download cancelled.")
                                             : i18n("Download failed: %1", reply->errorString()));
                return;
            }

            // Only an AppImage goes into Applications: an ELF file with the AppImage mark ("AI").
            QFile check(tempPath);
            const QByteArray head = check.open(QIODevice::ReadOnly) ? check.read(10) : QByteArray();
            check.close();
            if (!head.startsWith("\x7f" "ELF") || head.mid(8, 2) != "AI") {
                QFile::remove(tempPath);
                failInstall(packageName, i18n("Error: The downloaded file is not an AppImage (%1).", url.toDisplayString()));
                return;
            }

            Q_EMIT logReceived(packageName, i18n("Download complete. Integrating application..."));
            setInstallStage(4);

            if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
                QFile::remove(tempPath);
                failInstall(packageName, i18n("Error: Could not overwrite existing file: %1", targetPath));
                return;
            }

            if (!QFile::rename(tempPath, targetPath)) {
                QFile::remove(tempPath);
                failInstall(packageName, i18n("Error: Failed to finalize file renaming."));
                return;
            }

            QFile::setPermissions(targetPath,
                QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                QFile::ReadGroup | QFile::ExeGroup |
                QFile::ReadOther | QFile::ExeOther);

            auto *watcher = new QFutureWatcher<AppImageInfo>(this);
            connect(watcher, &QFutureWatcher<AppImageInfo>::finished, this, [this, watcher, targetPath, packageName, repoPage]() {
                watcher->deleteLater();
                const AppImageInfo info = watcher->result();
                if (!info.isValid) {
                    failInstall(packageName, i18n("Error: Extracted AppImage metadata is invalid or file is corrupted."));
                    return;
                }

                // Found through its repository: updates come from there when the AppImage
                // itself doesn't say (and the user hasn't set a source already).
                if (!repoPage.isEmpty() && info.updateInfo.isEmpty()
                    && loadUpdateSource(targetPath).type == UpdateSourceType::None) {
                    const UpdateSource source = parseUpdateSourceInput(repoPage, UpdateSourceType::None);
                    if (source.type != UpdateSourceType::None) {
                        saveUpdateSource(targetPath, source);
                        Q_EMIT logReceived(packageName, i18n("Updates will come from %1.", repoPage));
                    }
                }

                if (!AppImageManager::createDesktopLink(targetPath, info)) {
                    Q_EMIT logReceived(packageName, i18n("Warning: AppImage installed, but desktop integration failed."));
                } else {
                    Q_EMIT logReceived(packageName, i18n("Desktop entry and icons created."));
                }

                Q_EMIT logReceived(packageName, i18n("Installation completed successfully."));
                setInstallStage(5);
                m_isInstalling = false;
                Q_EMIT isInstallingChanged();
                Q_EMIT installationFinished(packageName, true);
            });

            watcher->setFuture(QtConcurrent::run([targetPath]() {
                AppImageReader reader(targetPath);
                return reader.read();
            }));
        });
}

void AMStoreModel::readProcessOutput()
{
    if (!m_process) return;
    while (m_process->canReadLine()) {
        const QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        if (line.isEmpty()) continue;
        Q_EMIT logReceived(m_activePackage, line);

        const QString lower = line.toLower();
        if (lower.contains(QStringLiteral("download")))
            setInstallStage(2);
        else if (lower.contains(QStringLiteral("extract")))
            setInstallStage(3);
        else if (lower.contains(QStringLiteral("install")) || lower.contains(QStringLiteral("integrat")))
            setInstallStage(4);
    }
}

void AMStoreModel::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    m_process->deleteLater();
    m_process = nullptr;

    m_isInstalling = false;
    Q_EMIT isInstallingChanged();
    setInstallStage(exitCode == 0 ? 5 : 0);

    const QString pkg = m_activePackage;
    m_activePackage.clear();
    Q_EMIT installationFinished(pkg, exitCode == 0);
}
