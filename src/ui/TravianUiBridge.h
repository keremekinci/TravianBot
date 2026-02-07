#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class TravianDataFetcher;
class BuildQueueManager;
class TroopQueueManager;
class FarmListManager;
class Account;

class TravianUiBridge : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantMap allData READ allData NOTIFY allDataChanged)
  Q_PROPERTY(QVariantList villages READ villages NOTIFY villagesChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
  Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY isLoggedInChanged)
  Q_PROPERTY(bool autoRefreshEnabled READ autoRefreshEnabled WRITE
                 setAutoRefreshEnabled NOTIFY autoRefreshEnabledChanged)
  Q_PROPERTY(QString refreshMode READ refreshMode WRITE setRefreshMode NOTIFY
                 refreshModeChanged)
  Q_PROPERTY(int nextRefreshIn READ nextRefreshIn NOTIFY nextRefreshInChanged)
  Q_PROPERTY(QVariantList buildQueue READ buildQueue NOTIFY buildQueueChanged)
  Q_PROPERTY(QVariantList farmConfigs READ farmConfigs NOTIFY farmConfigsChanged)
  Q_PROPERTY(QVariantList availableFarmLists READ availableFarmLists NOTIFY
                 availableFarmListsChanged)
  Q_PROPERTY(Account *account READ account CONSTANT)
  Q_PROPERTY(int currentVillageId READ currentVillageId WRITE
                 setCurrentVillageId NOTIFY currentVillageIdChanged)
  Q_PROPERTY(
      QVariantList activityLog READ activityLog NOTIFY activityLogChanged)
  Q_PROPERTY(QString botMode READ botMode WRITE setBotMode NOTIFY botModeChanged)
  Q_PROPERTY(QVariantList troopConfigs READ troopConfigs NOTIFY troopConfigsChanged)

public:
  explicit TravianUiBridge(QObject *parent = nullptr);

  QVariantMap allData() const { return m_allData; }
  QVariantList villages() const { return m_villages; }

  bool loading() const { return m_loading; }
  QString statusText() const { return m_statusText; }
  bool isLoggedIn() const { return m_isLoggedIn; }

  bool autoRefreshEnabled() const { return m_autoRefreshEnabled; }
  QString refreshMode() const { return m_refreshMode; }
  int nextRefreshIn() const { return m_nextRefreshIn; }
  QVariantList buildQueue() const;
  QVariantList farmConfigs() const;
  QVariantList availableFarmLists() const { return m_availableFarmLists; }

  // Account model
  Account *account() const { return m_account; }
  int currentVillageId() const;
  void setCurrentVillageId(int villageId);

  Q_INVOKABLE void startFetch();
  Q_INVOKABLE void performLogin();
  Q_INVOKABLE void upgradeBuilding(int villageId, int slotId);
  Q_INVOKABLE void setAutoRefreshEnabled(bool enabled);
  Q_INVOKABLE void setRefreshMode(const QString &mode);
  Q_INVOKABLE void addToBuildQueue(int villageId, int slotId, int targetLevel,
                                   const QString &buildingName);
  Q_INVOKABLE void removeFromBuildQueue(int villageId, int index);
  Q_INVOKABLE void fetchFarmLists(int villageId);
  Q_INVOKABLE QVariantList farmListsForVillage(int villageId) const;
  Q_INVOKABLE void setFarmListConfig(int listId, int villageId,
                                      const QString &listName,
                                      int intervalMinutes);
  Q_INVOKABLE void removeFarmListConfig(int listId);
  Q_INVOKABLE void setFarmListEnabled(int listId, bool enabled);
  Q_INVOKABLE void executeFarmListNow(int listId);
  Q_INVOKABLE int farmRemainingSeconds(int listId) const;
  Q_INVOKABLE void selectVillage(int villageId);

  // Bot mode
  QString botMode() const { return m_botMode; }
  Q_INVOKABLE void setBotMode(const QString &mode);
  QVariantList troopConfigs() const;
  Q_INVOKABLE void setVillageTroop(int villageId, const QString &troopId,
                                    const QString &troopName,
                                    const QString &building);
  Q_INVOKABLE void removeVillageTroop(int villageId);

  // Activity log
  QVariantList activityLog() const { return m_activityLog; }
  Q_INVOKABLE void logActivity(const QString &message,
                               const QString &type = "info");

signals:
  void allDataChanged();
  void villagesChanged();
  void loadingChanged();
  void statusTextChanged();
  void isLoggedInChanged();
  void autoRefreshEnabledChanged();
  void refreshModeChanged();
  void nextRefreshInChanged();
  void buildQueueChanged();
  void farmConfigsChanged();
  void farmTimerTick();
  void availableFarmListsChanged();
  void currentVillageIdChanged();
  void activityLogChanged();
  void botModeChanged();
  void troopConfigsChanged();

private:
  void setLoading(bool v);
  void setStatus(const QString &s);
  void loadSettings();

private slots:
  void onRefreshTimer();
  void onCountdownTimer();

private:
  void scheduleNextRefresh();
  int getRandomInterval() const;
  int getConstructionTimeRemaining() const;

private:
  TravianDataFetcher *m_fetcher = nullptr;
  BuildQueueManager *m_buildQueueManager = nullptr;
  TroopQueueManager *m_troopQueueManager = nullptr;
  FarmListManager *m_farmListManager = nullptr;
  Account *m_account = nullptr;

  QVariantMap m_allData;
  QVariantList m_villages;

  bool m_loading = false;
  QString m_statusText = "Hazır";
  bool m_isLoggedIn = false;

  // Auto-refresh
  QTimer *m_refreshTimer = nullptr;
  QTimer *m_countdownTimer = nullptr;
  bool m_autoRefreshEnabled = true;
  QString m_refreshMode = "long"; // "short" (4-5s) or "long" (5-10min)
  int m_nextRefreshIn = 0;        // seconds
  bool m_buildQueueScheduledRefresh =
      false; // True if build queue set custom refresh

  // Farm lists (per-village)
  QVariantList m_availableFarmLists; // union of all village lists
  QMap<int, QVariantList> m_villageFarmLists; // villageId -> farm lists

  // Settings
  QString m_baseUrl;
  QString m_username;
  QString m_password;

  // Bot mode
  QString m_botMode = "build"; // "build", "troop", "mixed"

  // Farm list auto-fetch flag (only once on startup)
  bool m_farmListsFetched = false;

  // Upgrade in-progress flag (prevents re-processing after upgrade)
  bool m_upgradeInProgress = false;

  // Activity log
  QVariantList m_activityLog;
};
