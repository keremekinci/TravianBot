#include "src/managers/TroopQueueManager.h"
#include "src/network/TravianDataFetcher.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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

  // Insert or update the config for this specific building in this village
  m_configs[villageId][building] = config;

  if (!m_configFilePath.isEmpty()) {
    saveConfig(m_configFilePath);
  }

  emit configChanged();
}

void TroopQueueManager::removeVillageTroop(int villageId,
                                           const QString &building) {
  if (!m_configs.contains(villageId))
    return;

  // Remove specific building config
  if (m_configs[villageId].contains(building)) {
    m_configs[villageId].remove(building);

    // If no more configs for this village, remove the village entry
    if (m_configs[villageId].isEmpty()) {
      m_configs.remove(villageId);
    }

    if (!m_configFilePath.isEmpty()) {
      saveConfig(m_configFilePath);
    }

    emit configChanged();
  }
}

QList<TroopQueueManager::TroopConfig>
TroopQueueManager::getVillageTroops(int villageId) const {
  if (!m_configs.contains(villageId)) {
    return {};
  }
  return m_configs[villageId].values();
}

QList<int> TroopQueueManager::getConfiguredVillages() const {
  return m_configs.keys();
}

bool TroopQueueManager::hasConfig(int villageId,
                                  const QString &building) const {
  return m_configs.contains(villageId) &&
         m_configs[villageId].contains(building);
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
    QJsonObject updatedBuildingMap = it.value().toObject();

    // Check if it's the old format (directly a config object with "troopId")
    // or new format (map of building -> config)
    if (updatedBuildingMap.contains("troopId")) {
      // OLD FORMAT: Convert to new format
      TroopConfig config = TroopConfig::fromJson(villageId, updatedBuildingMap);
      m_configs[villageId][config.building] = config;
    } else {
      // NEW FORMAT: Map of building -> config
      for (auto bIt = updatedBuildingMap.begin();
           bIt != updatedBuildingMap.end(); ++bIt) {
        QString building = bIt.key();
        TroopConfig config =
            TroopConfig::fromJson(villageId, bIt.value().toObject());
        m_configs[villageId][building] = config;
      }
    }
  }
}

void TroopQueueManager::saveConfig(const QString &filePath) {
  QJsonObject villages;

  // Iterate all villages
  for (auto it = m_configs.constBegin(); it != m_configs.constEnd(); ++it) {
    int villageId = it.key();
    QJsonObject buildingMap;

    // Iterate all buildings for this village
    const auto &innerMap = it.value();
    for (auto bIt = innerMap.constBegin(); bIt != innerMap.constEnd(); ++bIt) {
      buildingMap[bIt.key()] = bIt.value().toJson();
    }

    villages[QString::number(villageId)] = buildingMap;
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
    const auto &innerMap = it.value();
    for (auto bIt = innerMap.constBegin(); bIt != innerMap.constEnd(); ++bIt) {
      QVariantMap item;
      item["villageId"] = it.key();
      item["troopId"] = bIt.value().troopId;
      item["troopName"] = bIt.value().troopName;
      item["building"] = bIt.value().building;
      result.append(item);
    }
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

    // Iterate through all configured buildings for this village
    const auto &innerMap = m_configs[villageId];
    for (const TroopConfig &config : innerMap) {

      // Find the military building slot
      int slotId = findMilitarySlot(villageData, config.building);
      if (slotId < 0) {
        emit trainingFailed(
            villageId, QString("%1 binası bulunamadı").arg(config.building));
        continue;
      }

      // Trigger training via fetcher (pass troopName from config)
      fetcher->trainTroops(villageId, slotId, config.troopId, config.troopName);
    }
  }
}
