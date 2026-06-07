// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "amstoremodel.h"
#include "logging.h"
#include <QDateTime>
#include <QIcon>
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

static bool isFileFresh(const QString &path, qint64 maxAgeHours = 24)
{
    const QFileInfo fi(path);
    if (!fi.exists()) return false;
    return fi.lastModified().secsTo(QDateTime::currentDateTime()) < maxAgeHours * 3600LL;
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

QVariantList AMStoreModel::sources() const
{
    QVariantList list;
    for (const auto &src : m_sources) {
        QVariantMap map;
        map[QStringLiteral("id")] = src.id;
        map[QStringLiteral("name")] = src.name;
        map[QStringLiteral("url")] = src.url;
        map[QStringLiteral("type")] = src.type;
        map[QStringLiteral("enabled")] = src.enabled;
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
        hub.enabled = true;

        StoreSource am;
        am.id = QStringLiteral("am-database");
        am.name = QStringLiteral("AM Database");
        am.url = QStringLiteral("https://raw.githubusercontent.com/ivan-hc/AM/main/programs/x86_64-appimages");
        am.type = QStringLiteral("am-database");
        am.enabled = true;

        m_sources.append(hub);
        m_sources.append(am);

        saveSources();
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
    sync();
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
            sync();
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
            if (m_storeSource == i + 1) {
                m_storeSource = 0;
                Q_EMIT storeSourceChanged();
            } else if (m_storeSource > i + 1) {
                m_storeSource--;
                Q_EMIT storeSourceChanged();
            }
            saveSources();
            sync();
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
    m_storeSource = 0;
    Q_EMIT storeSourceChanged();
    loadSources();
    sync();
}

void AMStoreModel::moveSource(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_sources.size() ||
        toIndex < 0 || toIndex >= m_sources.size() ||
        fromIndex == toIndex) {
        return;
    }
    m_sources.move(fromIndex, toIndex);
    saveSources();
    checkAllLoaded();
}

void AMStoreModel::setSourcesOrder(const QStringList &idOrder)
{
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
        if (src.enabled) {
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
                    } else {
                        return AMStoreModel::parseAmDatabaseSync(QString::fromUtf8(data));
                    }
                }));
                continue;
            }
        }

        QNetworkRequest req(QUrl(src.url));
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

        // Main-thread pass: clear iconSource when it's not a URL and not a real
        // theme icon. Without this, the package name falls through to the QML
        // fallback which resolves to the system's AppImage file-type icon.
        for (auto &app : m_allApps) {
            if (!app.iconSource.startsWith(QStringLiteral("http")) &&
                !QIcon::hasThemeIcon(app.iconSource)) {
                app.iconSource.clear();
            }
        }

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
                        if (!app.downloadUrl.isEmpty()) existing.downloadUrl = app.downloadUrl;
                        if (!app.iconSource.isEmpty()) existing.iconSource = app.iconSource;
                        if (existing.packageName.isEmpty()) existing.packageName = app.packageName;
                    }
                }
            }
        }

        for (auto &app : mergedList) {
            for (const auto &pair : sourcesData) {
                const auto &source = pair.first;
                if (app.sourceIds.contains(source.id)) {
                    bool sourceHasAm = false;
                    if (source.type == QStringLiteral("am-database")) {
                        sourceHasAm = true;
                    } else {
                        // Check if the original parsed app from this source had hasAmScript
                        for (const auto &sa : pair.second) {
                            if (normalizeForMatch(sa.displayName) == normalizeForMatch(app.displayName)) {
                                if (sa.hasAmScript) {
                                    sourceHasAm = true;
                                }
                                break;
                            }
                        }
                    }
                    app.preferAm = sourceHasAm;
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
        app.homepage    = QStringLiteral("https://github.com/ivan-hc/AM/blob/main/programs/x86_64/%1").arg(app.packageName);
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
        // QIcon::hasThemeIcon() is NOT called here (not main-thread-safe in all configs);
        // theme check happens in mergeDatabases() on the main thread.
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
            if (sortAscending) {
                std::sort(apps.begin(), apps.end(), [](const StoreApp &a, const StoreApp &b) {
                    return a.displayName.localeAwareCompare(b.displayName) < 0;
                });
            } else {
                std::sort(apps.begin(), apps.end(), [](const StoreApp &a, const StoreApp &b) {
                    return a.displayName.localeAwareCompare(b.displayName) > 0;
                });
            }
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

QString AMStoreModel::mapCategory(const QString &packageName, const QString &description)
{
    const QString s = (packageName + QLatin1Char(' ') + description).toLower();

    if (s.contains(QStringLiteral("game")) || s.contains(QStringLiteral("arcade"))
        || s.contains(QStringLiteral("emulator")))
        return QStringLiteral("Game");
    if (s.contains(QStringLiteral("editor")) || s.contains(QStringLiteral(" ide "))
        || s.contains(QStringLiteral("code")) || s.contains(QStringLiteral("compiler")))
        return QStringLiteral("Development");
    if (s.contains(QStringLiteral("image")) || s.contains(QStringLiteral("paint"))
        || s.contains(QStringLiteral("graphics")) || s.contains(QStringLiteral("3d")))
        return QStringLiteral("Graphics");
    if (s.contains(QStringLiteral("browser")) || s.contains(QStringLiteral("chat"))
        || s.contains(QStringLiteral("internet")) || s.contains(QStringLiteral(" web ")))
        return QStringLiteral("Network");
    if (s.contains(QStringLiteral("office")) || s.contains(QStringLiteral("document"))
        || s.contains(QStringLiteral("pdf")))
        return QStringLiteral("Office");
    if (s.contains(QStringLiteral("audio")) || s.contains(QStringLiteral("video"))
        || s.contains(QStringLiteral("media")) || s.contains(QStringLiteral("music")))
        return QStringLiteral("AudioVideo");

    return QStringLiteral("Utility");
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

        Q_EMIT logReceived(packageName, i18n("Running: %1 -i %2").arg(cmd, packageName));
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

        Q_EMIT logReceived(packageName, i18n("Starting direct download from Hub..."));
        setInstallStage(2);

        const QString applicationsDir = AppSettings::instance()->applicationsPath();
        QDir().mkpath(applicationsDir);
        const QString targetPath = applicationsDir + QLatin1Char('/') + app.packageName + QStringLiteral(".AppImage");
        const QString tempPath = targetPath + QStringLiteral(".new");

        m_downloadFile = new QFile(tempPath, this);
        if (!m_downloadFile->open(QIODevice::WriteOnly)) {
            Q_EMIT logReceived(packageName, i18n("Error: Failed to open temporary file for writing: %1").arg(tempPath));
            m_downloadFile->deleteLater();
            m_downloadFile = nullptr;
            m_isInstalling = false;
            Q_EMIT isInstallingChanged();
            setInstallStage(0);
            Q_EMIT installationFinished(packageName, false);
            return;
        }

        QNetworkRequest request(QUrl(app.downloadUrl));
        request.setRawHeader("User-Agent", "AppImageManager/1.0");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        m_downloadReply = m_nam->get(request);

        connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
            if (m_downloadReply && m_downloadFile) {
                m_downloadFile->write(m_downloadReply->readAll());
            }
        });

        connect(m_downloadReply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
                static int lastPercent = -1;
                if (percent / 5 != lastPercent / 5) {
                    lastPercent = percent;
                    Q_EMIT logReceived(m_activePackage, i18n("Downloading: %1%").arg(percent));
                }
            }
        });

        connect(m_downloadReply, &QNetworkReply::finished, this, [this, targetPath, tempPath, packageName]() {
            if (!m_downloadReply) return;
            
            QNetworkReply *reply = m_downloadReply;
            m_downloadReply = nullptr;
            reply->deleteLater();

            if (m_downloadFile) {
                m_downloadFile->close();
                m_downloadFile->deleteLater();
                m_downloadFile = nullptr;
            }

            if (reply->error() != QNetworkReply::NoError) {
                QFile::remove(tempPath);
                if (reply->error() == QNetworkReply::OperationCanceledError) {
                    Q_EMIT logReceived(packageName, i18n("Download cancelled."));
                } else {
                    Q_EMIT logReceived(packageName, i18n("Download failed: %1").arg(reply->errorString()));
                }
                m_isInstalling = false;
                Q_EMIT isInstallingChanged();
                setInstallStage(0);
                Q_EMIT installationFinished(packageName, false);
                return;
            }

            Q_EMIT logReceived(packageName, i18n("Download complete. Integrating application..."));
            setInstallStage(4);

            if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
                Q_EMIT logReceived(packageName, i18n("Error: Could not overwrite existing file: %1").arg(targetPath));
                QFile::remove(tempPath);
                m_isInstalling = false;
                Q_EMIT isInstallingChanged();
                setInstallStage(0);
                Q_EMIT installationFinished(packageName, false);
                return;
            }

            if (!QFile::rename(tempPath, targetPath)) {
                Q_EMIT logReceived(packageName, i18n("Error: Failed to finalize file renaming."));
                QFile::remove(tempPath);
                m_isInstalling = false;
                Q_EMIT isInstallingChanged();
                setInstallStage(0);
                Q_EMIT installationFinished(packageName, false);
                return;
            }

            QFile::setPermissions(targetPath,
                QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                QFile::ReadGroup | QFile::ExeGroup |
                QFile::ReadOther | QFile::ExeOther);

            auto *watcher = new QFutureWatcher<AppImageInfo>(this);
            connect(watcher, &QFutureWatcher<AppImageInfo>::finished, this, [this, watcher, targetPath, packageName]() {
                watcher->deleteLater();
                const AppImageInfo info = watcher->result();
                if (!info.isValid) {
                    Q_EMIT logReceived(packageName, i18n("Error: Extracted AppImage metadata is invalid or file is corrupted."));
                    m_isInstalling = false;
                    Q_EMIT isInstallingChanged();
                    setInstallStage(0);
                    Q_EMIT installationFinished(packageName, false);
                    return;
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
