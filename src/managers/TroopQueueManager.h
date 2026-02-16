#ifndef TROOPQUEUEMANAGER_H
#define TROOPQUEUEMANAGER_H

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QRandomGenerator>
#include <QVariantMap>

class TravianDataFetcher;

class TroopQueueManager : public QObject {
  Q_OBJECT

public:
  struct TroopConfig {
    int villageId;
    QString troopId; // "u1", "u11", ...
    QString troopName;
    QString building; // "barracks", "stable", "workshop"
    int intervalMinutes = 5; // Training check interval
    bool enabled = false; // Auto-training enabled

    QJsonObject toJson() const;
    static TroopConfig fromJson(int villageId, const QJsonObject &obj);
  };

  explicit TroopQueueManager(QObject *parent = nullptr);

  // Updated to include building type for specific removal/setting
  void setVillageTroop(int villageId, const QString &troopId,
                       const QString &troopName, const QString &building,
                       int intervalMinutes);
  void removeVillageTroop(int villageId, const QString &building);
  void setVillageTroopEnabled(int villageId, const QString &building, bool enabled);

  // Returns list of configs for a village (can be empty)
  QList<TroopConfig> getVillageTroops(int villageId) const;

  QList<int> getConfiguredVillages() const;
  bool hasConfig(int villageId, const QString &building) const;

  void loadConfig(const QString &filePath);
  void saveConfig(const QString &filePath);

  void processTraining(TravianDataFetcher *fetcher, const QVariantMap &allData);

  // Execute training for specific village+building
  void executeTrainingNow(int villageId, const QString &building,
                         TravianDataFetcher *fetcher, const QVariantMap &allData);

  // Timer management
  void startTimers();
  void stopTimers();

  // Get remaining seconds for a village+building timer
  int remainingSeconds(int villageId, const QString &building) const;

  // For UI - Returns flattened list of all configs
  QVariantList allConfigs() const;

signals:
  void configChanged();
  void trainingStarted(int villageId, const QString &troopName, int count);
  void trainingFailed(int villageId, const QString &reason);
  void timerTick(int villageId, const QString &building, int remainingSeconds);

private slots:
  void onTimer();

private:
  int findMilitarySlot(const QVariantMap &villageData,
                       const QString &building) const;
  void startVillageTimer(int villageId, const QString &building);
  void stopVillageTimer(int villageId, const QString &building);
  QString makeTimerKey(int villageId, const QString &building) const;

  // Key: VillageID -> Key: BuildingName -> Config
  QMap<int, QMap<QString, TroopConfig>> m_configs;

  // Timer management: "villageId:building" -> remaining seconds
  QMap<QString, int> m_remainingSeconds;
  QTimer *m_tickTimer = nullptr;

  QString m_configFilePath;

  TravianDataFetcher *m_fetcher = nullptr;
  QVariantMap m_lastAllData;
};

#endif // TROOPQUEUEMANAGER_H
