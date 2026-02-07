#include "src/managers/BuildQueueManager.h"
#include "src/network/TravianDataFetcher.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

BuildQueueManager::BuildQueueManager(QObject *parent) : QObject(parent) {}

QJsonObject BuildQueueManager::BuildTask::toJson() const {
  QJsonObject obj;
  obj["villageId"] = villageId;
  obj["slotId"] = slotId;
  obj["currentLevel"] = currentLevel;
  obj["targetLevel"] = targetLevel;
  obj["buildingName"] = buildingName;
  obj["priority"] = priority;
  return obj;
}

BuildQueueManager::BuildTask
BuildQueueManager::BuildTask::fromJson(const QJsonObject &obj) {
  BuildTask task;
  task.villageId = obj["villageId"].toInt();
  task.slotId = obj["slotId"].toInt();
  task.currentLevel = obj["currentLevel"].toInt(0);
  task.targetLevel = obj["targetLevel"].toInt();
  task.buildingName = obj["buildingName"].toString();
  task.priority = obj["priority"].toInt();
  return task;
}

void BuildQueueManager::loadQueue(const QString &filePath) {
  m_queueFilePath = filePath;
  QFile file(filePath);

  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (!doc.isObject()) {
    return;
  }

  QJsonObject root = doc.object();
  m_queues.clear();

  // New format: "queues" object with villageId keys
  if (root.contains("queues")) {
    QJsonObject queuesObj = root["queues"].toObject();
    for (auto it = queuesObj.begin(); it != queuesObj.end(); ++it) {
      int villageId = it.key().toInt();
      QJsonArray arr = it.value().toArray();
      QList<BuildTask> tasks;
      for (const QJsonValue &val : arr) {
        tasks.append(BuildTask::fromJson(val.toObject()));
      }
      if (!tasks.isEmpty()) {
        m_queues[villageId] = tasks;
      }
    }
  }
  // Old format: flat "queue" array — migrate by grouping on villageId
  else if (root.contains("queue")) {
    QJsonArray queueArray = root["queue"].toArray();
    for (const QJsonValue &val : queueArray) {
      BuildTask task = BuildTask::fromJson(val.toObject());
      m_queues[task.villageId].append(task);
    }
    // Save in new format immediately
    if (!m_queueFilePath.isEmpty()) {
      saveQueue(m_queueFilePath);
    }
  }
}

void BuildQueueManager::saveQueue(const QString &filePath) {
  QJsonObject queuesObj;
  for (auto it = m_queues.constBegin(); it != m_queues.constEnd(); ++it) {
    QJsonArray arr;
    for (const BuildTask &task : it.value()) {
      arr.append(task.toJson());
    }
    queuesObj[QString::number(it.key())] = arr;
  }

  QJsonObject root;
  root["queues"] = queuesObj;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return;
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  file.close();
}

void BuildQueueManager::addTask(const BuildTask &task) {
  m_queues[task.villageId].append(task);

  // Sort this village's queue by priority
  auto &list = m_queues[task.villageId];
  std::sort(list.begin(), list.end(),
            [](const BuildTask &a, const BuildTask &b) {
              return a.priority < b.priority;
            });

  if (!m_queueFilePath.isEmpty()) {
    saveQueue(m_queueFilePath);
  }

  emit queueChanged();
}

void BuildQueueManager::removeTask(int villageId, int index) {
  if (!m_queues.contains(villageId))
    return;

  auto &list = m_queues[villageId];
  if (index >= 0 && index < list.size()) {
    list.removeAt(index);

    // Remove village entry if empty
    if (list.isEmpty()) {
      m_queues.remove(villageId);
    }

    if (!m_queueFilePath.isEmpty()) {
      saveQueue(m_queueFilePath);
    }

    emit queueChanged();
  }
}

QList<BuildQueueManager::BuildTask>
BuildQueueManager::getQueue(int villageId) const {
  return m_queues.value(villageId);
}

QList<int> BuildQueueManager::getVillageIds() const {
  return m_queues.keys();
}

QList<BuildQueueManager::BuildTask> BuildQueueManager::getAllTasks() const {
  QList<BuildTask> all;
  for (auto it = m_queues.constBegin(); it != m_queues.constEnd(); ++it) {
    all.append(it.value());
  }
  return all;
}

int BuildQueueManager::totalTaskCount() const {
  int count = 0;
  for (auto it = m_queues.constBegin(); it != m_queues.constEnd(); ++it) {
    count += it.value().size();
  }
  return count;
}

bool BuildQueueManager::isBuilderFree(const QVariantMap &villageData) const {
  QVariantMap dorf1Data = villageData["dorf1"].toMap();
  QVariantList constructionQueue = dorf1Data["constructionQueue"].toList();
  return constructionQueue.isEmpty();
}

int BuildQueueManager::getBuilderRemainingTime(
    const QVariantMap &villageData) const {
  QVariantMap dorf1Data = villageData["dorf1"].toMap();
  QVariantList constructionQueue = dorf1Data["constructionQueue"].toList();

  if (constructionQueue.isEmpty()) {
    return 0;
  }

  QVariantMap firstItem = constructionQueue.first().toMap();
  QString remainingTime = firstItem["remainingTime"].toString();

  QStringList parts = remainingTime.split(':');
  int seconds = 0;

  if (parts.size() == 3) {
    seconds =
        parts[0].toInt() * 3600 + parts[1].toInt() * 60 + parts[2].toInt();
  } else if (parts.size() == 2) {
    seconds = parts[0].toInt() * 60 + parts[1].toInt();
  }

  return seconds;
}

int BuildQueueManager::getCurrentLevel(const QVariantMap &villageData,
                                       int slotId) const {
  QVariantMap dorf1Data = villageData["dorf1"].toMap();
  QVariantList resourceFields = dorf1Data["resourceFields"].toList();

  for (const QVariant &field : resourceFields) {
    QVariantMap fieldMap = field.toMap();
    if (fieldMap["slotId"].toInt() == slotId) {
      return fieldMap["level"].toInt();
    }
  }

  QVariantMap dorf2Data = villageData["dorf2"].toMap();
  QVariantList buildings = dorf2Data["buildings"].toList();

  for (const QVariant &building : buildings) {
    QVariantMap buildingMap = building.toMap();
    if (buildingMap["slotId"].toInt() == slotId) {
      return buildingMap["level"].toInt();
    }
  }

  return 0;
}

bool BuildQueueManager::canAffordBuilding(const QVariantMap &villageData,
                                          int slotId, int currentLevel) const {
  Q_UNUSED(slotId);
  Q_UNUSED(currentLevel);
  QVariantMap dorf1Data = villageData["dorf1"].toMap();

  int lumber = dorf1Data["lumber"].toString().remove('.').toInt();
  int clay = dorf1Data["clay"].toString().remove('.').toInt();
  int iron = dorf1Data["iron"].toString().remove('.').toInt();
  int crop = dorf1Data["crop"].toString().remove('.').toInt();

  return (lumber >= 100 && clay >= 100 && iron >= 100 && crop >= 100);
}

void BuildQueueManager::processQueue(TravianDataFetcher *fetcher,
                                     const QVariantMap &allData) {
  if (!fetcher || m_queues.isEmpty()) {
    return;
  }

  // Process each village independently
  QList<int> villageIds = m_queues.keys();
  for (int villageId : villageIds) {
    QString villageKey = QString("village_%1").arg(villageId);
    if (!allData.contains(villageKey)) {
      continue;
    }

    QVariantMap villageData = allData[villageKey].toMap();
    QList<BuildTask> &tasks = m_queues[villageId];

    if (tasks.isEmpty()) {
      continue;
    }

    // Check if builder is free for this village
    if (!isBuilderFree(villageData)) {
      int remainingSec = getBuilderRemainingTime(villageData);
      emit builderBusy(villageId, remainingSec);
      continue; // Try next village instead of returning
    }

    // Process first task in this village's queue
    for (int i = 0; i < tasks.size(); ++i) {
      const BuildTask &task = tasks[i];

      // Check current level
      int currentLevel = getCurrentLevel(villageData, task.slotId);
      if (currentLevel >= task.targetLevel) {
        int vid = task.villageId;
        int sid = task.slotId;
        tasks.removeAt(i);
        if (tasks.isEmpty()) {
          m_queues.remove(villageId);
        }
        if (!m_queueFilePath.isEmpty()) {
          saveQueue(m_queueFilePath);
        }
        emit queueChanged();
        emit taskCompleted(vid, sid);
        i--;
        continue;
      }

      // Check resources
      if (!canAffordBuilding(villageData, task.slotId, currentLevel)) {
        emit insufficientResources(villageId, task.buildingName);
        break; // Try next village
      }

      // Start upgrade for this village
      fetcher->upgradeBuilding(task.villageId, task.slotId);
      emit taskStarted(task.villageId, task.slotId, task.buildingName);

      // Only one upgrade per village per cycle
      break;
    }
  }
}
