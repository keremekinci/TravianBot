#ifndef BUILDQUEUEMANAGER_H
#define BUILDQUEUEMANAGER_H

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantMap>

class TravianDataFetcher;

class BuildQueueManager : public QObject {
  Q_OBJECT

public:
  struct BuildTask {
    int villageId;
    int slotId;
    int currentLevel;
    int targetLevel;
    QString buildingName;
    int priority;

    QJsonObject toJson() const;
    static BuildTask fromJson(const QJsonObject &obj);
  };

  explicit BuildQueueManager(QObject *parent = nullptr);

  void loadQueue(const QString &filePath);
  void saveQueue(const QString &filePath);
  void addTask(const BuildTask &task);
  void removeTask(int villageId, int index);

  // Per-village queue access
  QList<BuildTask> getQueue(int villageId) const;
  QList<int> getVillageIds() const;

  // All tasks (flat list)
  QList<BuildTask> getAllTasks() const;
  int totalTaskCount() const;

  int getCurrentLevel(const QVariantMap &villageData, int slotId) const;

  void processQueue(TravianDataFetcher *fetcher, const QVariantMap &allData);

  // Returns remaining construction time in seconds, or 0 if builder is free
  int getBuilderRemainingTime(const QVariantMap &villageData) const;

signals:
  void queueChanged();
  void taskStarted(int villageId, int slotId, const QString &buildingName);
  void taskCompleted(int villageId, int slotId);
  void builderBusy(int villageId, int remainingSeconds);
  void insufficientResources(int villageId, const QString &buildingName);

private:
  QMap<int, QList<BuildTask>> m_queues; // villageId -> tasks
  QString m_queueFilePath;

  bool isBuilderFree(const QVariantMap &villageData) const;
  bool canAffordBuilding(const QVariantMap &villageData, int slotId,
                         int currentLevel) const;
};

#endif // BUILDQUEUEMANAGER_H
