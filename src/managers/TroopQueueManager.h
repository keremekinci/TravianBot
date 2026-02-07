#ifndef TROOPQUEUEMANAGER_H
#define TROOPQUEUEMANAGER_H

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantMap>

class TravianDataFetcher;

class TroopQueueManager : public QObject {
  Q_OBJECT

public:
  struct TroopConfig {
    int villageId;
    QString troopId;  // "t1", "t2", ...
    QString troopName;
    QString building; // "barracks", "stable", "workshop"

    QJsonObject toJson() const;
    static TroopConfig fromJson(int villageId, const QJsonObject &obj);
  };

  explicit TroopQueueManager(QObject *parent = nullptr);

  void setVillageTroop(int villageId, const QString &troopId,
                       const QString &troopName, const QString &building);
  void removeVillageTroop(int villageId);
  TroopConfig getVillageTroop(int villageId) const;
  QList<int> getConfiguredVillages() const;
  bool hasConfig(int villageId) const;

  void loadConfig(const QString &filePath);
  void saveConfig(const QString &filePath);

  void processTraining(TravianDataFetcher *fetcher, const QVariantMap &allData);

  // For UI
  QVariantList allConfigs() const;

signals:
  void configChanged();
  void trainingStarted(int villageId, const QString &troopName, int count);
  void trainingFailed(int villageId, const QString &reason);

private:
  int findMilitarySlot(const QVariantMap &villageData,
                       const QString &building) const;

  QMap<int, TroopConfig> m_configs; // villageId -> config
  QString m_configFilePath;
};

#endif // TROOPQUEUEMANAGER_H
