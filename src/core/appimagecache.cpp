// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#include "appimagecache.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QMutex>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QVariant>

// Bump whenever adding, removing, or renaming a persisted AppImageInfo field.
// A version mismatch silently discards the entry — one cold re-read per app.
// Version history:
//   1 — initial fields
//   2 — added comment, description
//   3 — added developerName, homepage
//   4 — migrated storage from QSettings INI to SQLite
static constexpr int kCacheVersion = 4;

static QString cacheDbPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dir))
        qWarning() << "AppImageCache: failed to create cache directory:" << dir;
    return dir + QStringLiteral("/cache.db");
}

static QString keyFor(const QString &path)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex());
}

// Per-thread connection: each thread gets its own QSqlDatabase connection so we
// satisfy Qt's "one connection per thread" rule. SQLite WAL mode handles
// file-level concurrency between connections safely.
// A mutex guards the rare initial addDatabase() call (Qt's registry is not
// documented as thread-safe for additions from multiple threads simultaneously).
static QMutex s_dbInitMutex;

static QSqlDatabase connectionForCurrentThread(const QString &dbPath)
{
    const auto tid = reinterpret_cast<quintptr>(QThread::currentThread());
    const QString name = QStringLiteral("aim_cache_%1").arg(tid, 0, 16);

    {
        QMutexLocker lock(&s_dbInitMutex);
        if (!QSqlDatabase::contains(name)) {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            db.setDatabaseName(dbPath);
            if (!db.open()) {
                qWarning() << "AppImageCache: cannot open database:" << db.lastError().text();
                return db;
            }

            QObject::connect(QThread::currentThread(), &QThread::finished, QThread::currentThread(), [name]() {
                QSqlDatabase::removeDatabase(name);
            });

            QSqlQuery q(db);
            q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
            q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
            q.exec(QStringLiteral(R"(
                CREATE TABLE IF NOT EXISTS metadata (
                    key           TEXT    PRIMARY KEY,
                    mtime         INTEGER NOT NULL,
                    cache_version INTEGER NOT NULL,
                    original_name TEXT,
                    clean_name    TEXT,
                    app_id        TEXT,
                    app_name      TEXT,
                    version       TEXT,
                    categories    TEXT,
                    comment       TEXT,
                    description   TEXT,
                    developer_name TEXT,
                    homepage      TEXT,
                    exec_args     TEXT,
                    file_size     INTEGER,
                    icon_ext      TEXT,
                    update_info   TEXT,
                    icon_data     BLOB,
                    is_valid      INTEGER DEFAULT 0
                )
            )"));
        }
    }
    return QSqlDatabase::database(name, false);
}

// ── Singleton ─────────────────────────────────────────────────────────────────

AppImageCache &AppImageCache::instance()
{
    static AppImageCache s;
    return s;
}

AppImageCache::AppImageCache()
    : m_dbPath(cacheDbPath())
{
}

// ── Public API ────────────────────────────────────────────────────────────────

AppImageInfo AppImageCache::load(const QString &path, qint64 mtime)
{
    QSqlDatabase db = connectionForCurrentThread(m_dbPath);
    if (!db.isOpen()) return {};

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT cache_version, mtime, original_name, clean_name, app_id, app_name, "
        "       version, categories, comment, description, developer_name, homepage, "
        "       exec_args, file_size, icon_ext, update_info, icon_data, is_valid "
        "FROM metadata WHERE key = ?"));
    q.addBindValue(keyFor(path));
    if (!q.exec() || !q.next()) return {};

    if (q.value(0).toInt() != kCacheVersion) return {};  // schema version mismatch
    if (q.value(1).toLongLong() != mtime)    return {};  // file was replaced

    AppImageInfo info;
    info.originalName  = q.value(2).toString();
    info.cleanName     = q.value(3).toString();
    info.appId         = q.value(4).toString();
    info.appName       = q.value(5).toString();
    info.version       = q.value(6).toString();
    info.categories    = q.value(7).toString();
    info.comment       = q.value(8).toString();
    info.description   = q.value(9).toString();
    info.developerName = q.value(10).toString();
    info.homepage      = q.value(11).toString();
    info.execArgs      = q.value(12).toString();
    info.fileSize      = q.value(13).toLongLong();
    info.iconExt       = q.value(14).toString();
    info.updateInfo    = q.value(15).toString();
    info.iconData      = q.value(16).toByteArray();   // stored as BLOB, no base64 needed
    info.isValid       = q.value(17).toBool();
    return info;
}

void AppImageCache::store(const QString &path, qint64 mtime, const AppImageInfo &info)
{
    QSqlDatabase db = connectionForCurrentThread(m_dbPath);
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO metadata "
        "(key, mtime, cache_version, original_name, clean_name, app_id, app_name, "
        " version, categories, comment, description, developer_name, homepage, "
        " exec_args, file_size, icon_ext, update_info, icon_data, is_valid) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(keyFor(path));
    q.addBindValue(mtime);
    q.addBindValue(kCacheVersion);
    q.addBindValue(info.originalName);
    q.addBindValue(info.cleanName);
    q.addBindValue(info.appId);
    q.addBindValue(info.appName);
    q.addBindValue(info.version);
    q.addBindValue(info.categories);
    q.addBindValue(info.comment);
    q.addBindValue(info.description);
    q.addBindValue(info.developerName);
    q.addBindValue(info.homepage);
    q.addBindValue(info.execArgs);
    q.addBindValue(info.fileSize);
    q.addBindValue(info.iconExt);
    q.addBindValue(info.updateInfo);
    q.addBindValue(info.iconData);   // QByteArray → BLOB, no base64 overhead
    q.addBindValue(info.isValid ? 1 : 0);
    if (!q.exec())
        qWarning() << "AppImageCache: store failed:" << q.lastError().text();
}
