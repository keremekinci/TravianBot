#include "src/managers/TroopQueueManager.h"
#include "src/network/TravianDataFetcher.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

TroopQueueManager::TroopQueueManager(QObject *parent) : QObject(parent) {}

QJsonObject TroopQueueManager::TroopConfig::toJson() const {
  QJsonObject obj;
  obj["troopId"] = troopId;
  obj["troopName"] = troopName;
  obj["building"] = building;
  return obj;
}

TroopQueueManager::TroopConfig
TroopQueueManager::TroopConfig::fromJson(int villageId,
                                          const QJsonObject &obj) {
  TroopConfig config;
  config.villageId = villageId;
  config.troopId = obj["troopId"].toString();
  config.troopName = obj["troopName"].toString();
  config.building = obj["building"].toString();
  return config;
}

void TroopQueueManager::setVillageTroop(int villageId, const QString &troopId,
                                         const QString &troopName,
                                         const QString &building) {
  TroopConfig config;
  config.villageId = villageId;
  config.troopId = troopId;
  config.troopName = troopName;
  config.building = building;

  m_configs[villageId] = config;

  if (!m_configFilePath.isEmpty()) {
    saveConfig(m_configFilePath);
  }

  emit configChanged();
}

void TroopQueueManager::removeVillageTroop(int villageId) {
  if (!m_configs.contains(villageId))
    return;

  m_configs.remove(villageId);

  if (!m_configFilePath.isEmpty()) {
    saveConfig(m_configFilePath);
  }

  emit configChanged();
}

TroopQueueManager::TroopConfig
TroopQueueManager::getVillageTroop(int villageId) const {
  return m_configs.value(villageId);
}

QList<int> TroopQueueManager::getConfiguredVillages() const {
  return m_configs.keys();
}

bool TroopQueueManager::hasConfig(int villageId) const {
  return m_configs.contains(villageId);
}

void TroopQueueManager::loadConfig(const QString &filePath) {
  m_configFilePath = filePath;
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
  m_configs.clear();

  QJsonObject villages = root["villages"].toObject();
  for (auto it = villages.begin(); it != villages.end(); ++it) {
    int villageId = it.key().toInt();
    TroopConfig config = TroopConfig::fromJson(villageId, it.value().toObject());
    m_configs[villageId] = config;
  }
}

void TroopQueueManager::saveConfig(const QString &filePath) {
  QJsonObject villages;
  for (auto it = m_configs.constBegin(); it != m_configs.constEnd(); ++it) {
    villages[QString::number(it.key())] = it.value().toJson();
  }

  QJsonObject root;
  root["villages"] = villages;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return;
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  file.close();
}

QVariantList TroopQueueManager::allConfigs() const {
  QVariantList result;
  for (auto it = m_configs.constBegin(); it != m_configs.constEnd(); ++it) {
    QVariantMap item;
    item["villageId"] = it.key();
    item["troopId"] = it.value().troopId;
    item["troopName"] = it.value().troopName;
    item["building"] = it.value().building;
    result.append(item);
  }
  return result;
}

int TroopQueueManager::findMilitarySlot(const QVariantMap &villageData,
                                         const QString &building) const {
  // GID mapping: barracks=19, stable=20, workshop=21
  int targetGid = 0;
  if (building == "barracks")
    targetGid = 19;
  else if (building == "stable")
    targetGid = 20;
  else if (building == "workshop")
    targetGid = 21;
  else
    return -1;

  QVariantMap dorf2Data = villageData["dorf2"].toMap();
  QVariantList buildings = dorf2Data["buildings"].toList();

  for (const QVariant &b : buildings) {
    QVariantMap bMap = b.toMap();
    if (bMap["gid"].toInt() == targetGid) {
      return bMap["slotId"].toInt();
    }
  }

  return -1;
}

void TroopQueueManager::processTraining(TravianDataFetcher *fetcher,
                                         const QVariantMap &allData) {
  if (!fetcher || m_configs.isEmpty()) {
    return;
  }

  QList<int> villageIds = m_configs.keys();
  for (int villageId : villageIds) {
    QString villageKey = QString("village_%1").arg(villageId);
    if (!allData.contains(villageKey)) {
      continue;
    }

    QVariantMap villageData = allData[villageKey].toMap();
    const TroopConfig &config = m_configs[villageId];

    // Find the military building slot
    int slotId = findMilitarySlot(villageData, config.building);
    if (slotId < 0) {
      emit trainingFailed(villageId,
                          QString("%1 binası bulunamadı").arg(config.building));
      continue;
    }

    // Trigger training via fetcher (pass troopName from config)
    fetcher->trainTroops(villageId, slotId, config.troopId, config.troopName);
  }
}
