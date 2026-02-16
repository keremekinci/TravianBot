#ifndef TRAVIANDATAFETCHER_H
#define TRAVIANDATAFETCHER_H

#include "src/parsers/VillageParser.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QRegularExpression>
#include <QTimer>
#include <QVariantMap>

/**
 * @brief Main data fetcher for Travian game data
 *
 * This class manages HTTP requests to Travian servers,
 * handles cookies for authentication, and extracts data
 * from HTML pages based on JSON configuration.
 */
class TravianDataFetcher : public QObject {
  Q_OBJECT

public:
  explicit TravianDataFetcher(QObject *parent = nullptr);
  ~TravianDataFetcher();

  // Configuration
  bool loadConfig(const QString &configPath);
  bool loadCookies(const QString &cookiePath);

  // Authentication
  void performLogin(const QString &username, const QString &password);
  void setBaseUrl(const QString &url) { m_baseUrl = url; }
  void setCookieCachePath(const QString &path) { m_cookieCachePath = path; }
  bool tryLoadSavedCookies(const QString &cookiePath);
  void saveCookiesToFile(const QString &cookiePath);

  // Session management
  void checkSessionHealth();

  // Data fetching
  void fetchAllVillagesData();
  void fetchVillageData(int villageId, const QString &villageName = QString());
  void fetchAllData();
  void fetchPage(const QString &pageName, int villageId = -1);

  // Actions
  void upgradeBuilding(int villageId, int slotId);
  void fetchFarmLists(int villageId);
  void executeFarmList(int villageId, int listId);
  void sendFarmListPost(int villageId, int listId,
                         const QJsonArray &slotIds);
  void trainTroops(int villageId, int slotId, const QString &troopId,
                   const QString &troopName = QString());
  void fetchIncomingAttacks(int villageId);

  // Data access
  QVariantMap getAllData() const { return m_collectedData; }
  QList<VillageInfo> getVillages() const { return m_villages; }
  QVariantMap getVillageData(int villageId) const;

signals:
  void villagesDiscovered(const QList<VillageInfo> &villages);
  void villageDataUpdated(int villageId, const QString &villageName,
                          const QVariantMap &data);
  void dataUpdated(const QString &pageName, const QVariantMap &data);
  void allDataFetched(const QVariantMap &allData);
  void fetchError(const QString &pageName, const QString &error);
  void fetchProgress(int current, int total, const QString &pageName);
  void loginSuccess();
  void loginFailed(const QString &error);
  void sessionExpiredAutoLogin();
  void upgradeStarted(int villageId, int slotId, const QString &buildingName);
  void upgradeFailed(int villageId, int slotId, const QString &error);
  void farmListsFetched(int villageId, const QVariantList &lists);
  void farmListExecuted(int villageId, int listId, bool success,
                        const QString &message);
  void troopTrainingResult(int villageId, bool success,
                           const QString &troopName, int count,
                           const QString &message);
  void incomingAttacksFetched(int villageId, const QVariantList &attacks);
  void sessionHealthCheckResult(bool isValid);

private slots:
  void onRequestFinished(QNetworkReply *reply);
  void onLoginFinished(QNetworkReply *reply);
  void onUpgradeFinished(QNetworkReply *reply);
  void onFarmListFinished(QNetworkReply *reply);
  void onTrainTroopFinished(QNetworkReply *reply);
  void onSessionCheckFinished(QNetworkReply *reply);
  void onIncomingAttacksFinished(QNetworkReply *reply);
  void processNextRequest();

private:
  // Request structure
  struct PendingRequest {
    QString pageName;
    QString url;
    QJsonObject pageConfig;
    int villageId = -1;
    QString villageName;
    bool isVillageListRequest = false;
  };

  // Helpers
  int getRandomDelay() const;
  QString getRandomUserAgent() const;
  QString buildVillageUrl(const QString &baseUrl, int villageId) const;
  void enqueuePageRequests(int villageId, const QString &villageName);
  void enqueueMilitaryBuildingRequests(int villageId,
                                       const QString &villageName,
                                       const QVariantMap &buildingsData);
  void handleVillageListResponse(const QString &html);
  void handlePageResponse(const QString &html, const PendingRequest &req);
  void storeVillageData(int villageId, const QString &villageName,
                        const QString &pageName, const QVariantMap &data);
  void logPageData(const QString &pageName, const QVariantMap &data);

  // Connection stability helpers
  void refreshCookiesFromResponse(QNetworkReply *reply);
  QByteArray decompressGzip(const QByteArray &data);
  bool shouldRetryNetworkError(QNetworkReply::NetworkError error) const;
  void resetNetworkManager();

  // Network
  QNetworkAccessManager *m_networkManager;

  // Configuration
  QJsonObject m_config;
  QString m_baseUrl;
  int m_delayMin;
  int m_delayMax;

  // Request queue
  QQueue<PendingRequest> m_requestQueue;
  bool m_isProcessing;
  QString m_currentPageName;

  // Village data
  QList<VillageInfo> m_villages;
  int m_currentVillageIndex;

  // Collected data
  QVariantMap m_collectedData;

  // Statistics
  int m_totalRequests;
  int m_completedRequests;

  // Anti-bot protection
  QStringList m_userAgents;
  QString m_sessionUserAgent; // Oturum boyunca sabit
  QString m_lastReferer;

  // Farm list retry mechanism
  struct FarmRetryInfo {
    int villageId;
    int listId;
    int retryCount = 0;
  };
  QMap<QString, FarmRetryInfo> m_farmRetries; // key: "villageId_listId"
  static constexpr int MAX_FARM_RETRIES = 3;
  static constexpr int FARM_RETRY_DELAY_MS = 2000;

  // Cookie auto-refresh
  QString m_cookieCachePath;
  QDateTime m_lastCookieSaveTime;
  QString m_lastJwtValue;

  // Network retry
  int m_currentRetryCount = 0;
  static constexpr int MAX_REQUEST_RETRIES = 3;

  // Connection pool management
  int m_consecutiveNetworkErrors = 0;
  static constexpr int MAX_CONSECUTIVE_ERRORS_BEFORE_RESET = 5;
};

#endif // TRAVIANDATAFETCHER_H
