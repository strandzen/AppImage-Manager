// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 AppImage Manager Contributors
#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QVector>

struct StoreApp {
    QString packageName;
    QString displayName;
    QString description;
    QString categories;   // semicolon-separated AppStream categories
    QString iconSource;   // theme icon name, https:// URL, or package name fallback
    QString homepage;
    QString downloadUrl;
    QString author;
    QString license;
    QString repoUrl;      // the project's releases, when the download is a copy of them (update source)
    bool hasAmScript = false;
    bool hasFeedData = false;
    QStringList sourceIds;
    bool preferAm = false;
};

struct StoreSource {
    QString id;
    QString name;
    QString url;
    QString type; // "appimagehub" or "am-database"
    bool enabled = true;
};


class AMStoreModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool isInstalling READ isInstalling NOTIFY isInstallingChanged)
    Q_PROPERTY(int installStage READ installStage NOTIFY installStageChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY categoryFilterChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY sortAscendingChanged)
    Q_PROPERTY(int storeSource READ storeSource WRITE setStoreSource NOTIFY storeSourceChanged)
    Q_PROPERTY(QStringList availableCategories READ availableCategories NOTIFY availableCategoriesChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    // AM or AppMan is installed (looked up at start): AM-only apps need it.
    Q_PROPERTY(bool amInstalled READ amInstalled CONSTANT)
    // Where AM's install scripts for this computer's CPU are browsable.
    Q_PROPERTY(QString amScriptBase READ amScriptBase CONSTANT)

public:
    enum StoreRoles {
        PackageNameRole = Qt::UserRole + 1,
        DisplayNameRole,
        DescriptionRole,
        CategoriesRole,
        IconSourceRole,
        HomepageRole,
        DownloadUrlRole,
        AuthorRole,
        LicenseRole,
        HasAmScriptRole,
        HasFeedDataRole,
        SectionCategoryRole,  // first category, or "Other"; the list is grouped by it
    };

    explicit AMStoreModel(QObject *parent = nullptr);
    ~AMStoreModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const { return m_loading; }
    bool isInstalling() const { return m_isInstalling; }
    int installStage() const { return m_installStage; }

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);

    QString categoryFilter() const { return m_categoryFilter; }
    void setCategoryFilter(const QString &category);

    bool sortAscending() const { return m_sortAscending; }
    void setSortAscending(bool ascending);

    int storeSource() const { return m_storeSource; }
    void setStoreSource(int source);

    QStringList availableCategories() const { return m_availableCategories; }
    QVariantList sources() const;
    bool amInstalled() const { return m_amInstalled; }
    QString amScriptBase() const;

    Q_INVOKABLE void initialize();   // call once when Discover page first opens
    Q_INVOKABLE void sync();
    Q_INVOKABLE void installApp(const QString &packageName, bool forceDownload = false);
    Q_INVOKABLE void cancelInstallation();
    Q_INVOKABLE QVariantMap itemData(int row) const;

    Q_INVOKABLE void addSource(const QString &name, const QString &url, const QString &type, bool enabled);
    Q_INVOKABLE void updateSource(const QString &id, const QString &name, const QString &url, const QString &type, bool enabled);
    Q_INVOKABLE void removeSource(const QString &id);
    Q_INVOKABLE void resetSourcesToDefault();
    Q_INVOKABLE void setSourcesOrder(const QStringList &idOrder);

Q_SIGNALS:
    void loadingChanged();
    void isInstallingChanged();
    void installStageChanged();
    void filterTextChanged();
    void categoryFilterChanged();
    void sortAscendingChanged();
    void storeSourceChanged();
    void availableCategoriesChanged();
    void sourcesChanged();
    void logReceived(const QString &packageName, const QString &line);
    void installationFinished(const QString &packageName, bool success);

private Q_SLOTS:
    void onReplyFinished();
    void readProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void failInstall(const QString &packageName, const QString &message);
    void resolveAndDownload(const StoreApp &app);
    void startDownload(const QString &packageName, const QUrl &url, const QString &repoPage);
    void load(bool forceNetwork);
    void loadSources();
    void saveSources();
    void onSourceParsed(const QString &sourceId, QVector<StoreApp> apps);
    void checkAllLoaded();
    void applyFilter();
    void updateAvailableCategories();
    void setInstallStage(int stage);

    // Pure functions — safe to call from worker threads
    static QVector<StoreApp> parseAmDatabaseSync(const QString &rawText);
    static QVector<StoreApp> parseFeedJsonSync(const QByteArray &data);
    static QVector<StoreApp> parseUniversalJsonSync(const QByteArray &data);
    // pkgforge's package list (the catalog behind Soar): its AppImages only.
    static QVector<StoreApp> parsePkgforgeJsonSync(const QByteArray &data);
    static QString cleanName(const QString &packageName);

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_downloadReply;
    QFile *m_downloadFile;

    QList<StoreSource> m_sources;
    QHash<QString, QVector<StoreApp>> m_parsedSourceApps;
    QVector<StoreApp> m_allApps;
    QVector<StoreApp> m_apps;

    int m_pendingSourcesCount;
    QHash<QNetworkReply*, QString> m_activeReplies;
    bool m_loading;

    QString m_filterText;
    QString m_categoryFilter;
    bool m_sortAscending;
    int m_storeSource;
    QStringList m_availableCategories;

    bool m_isInstalling;
    int m_installStage;
    QProcess *m_process;
    bool m_amInstalled;
    QString m_activePackage;

    QString m_cacheDir;
    bool m_initialized;
    int m_filterGeneration;
};
