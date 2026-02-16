#include "src/managers/TroopQueueManager.h"
#include "src/network/TravianDataFetcher.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

TroopQueueManager::TroopQueueManager(QObject *parent) : QObject(parent) {
  // 1-second tick timer for countdown updates
  m_tickTimer = new QTimer(this);
  m_tickTimer->setInterval(1000);
  connect(m_tickTimer, &QTimer::timeout, this, &TroopQueueManager::onTimer);
}

QJsonObject TroopQueueManager::TroopConfig::toJson() const {
  QJsonObject obj;
  obj["troopId"] = troopId;
  obj["troopName"] = troopName;
  obj["building"] = building;
  obj["intervalMinutes"] = intervalMinutes;
  obj["enabled"] = enabled;
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
  config.intervalMinutes = obj["intervalMinutes"].toInt(5);
  config.enabled = obj["enabled"].toBool(false);
  return config;
}

void TroopQueueManager::setVillageTroop(int villageId, const QString &troopId,
                                        const QString &troopName,
                                        const QString &building,
                                        int intervalMinutes) {
  TroopConfig config;
  config.villageId = villageId;
  config.troopId = troopId;
  config.troopName = troopName;
  config.building = building;
  config.intervalMinutes = intervalMinutes;

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
    // Stop timer first
    stopVillageTimer(villageId, building);

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
    int villageId = it.key();
    const auto &innerMap = it.value();
    for (auto bIt = innerMap.constBegin(); bIt != innerMap.constEnd(); ++bIt) {
      const TroopConfig &config = bIt.value();
      QVariantMap item;
      item["villageId"] = villageId;
      item["troopId"] = config.troopId;
      item["troopName"] = config.troopName;
      item["building"] = config.building;
      item["intervalMinutes"] = config.intervalMinutes;
      item["enabled"] = config.enabled;
      item["remainingSeconds"] = remainingSeconds(villageId, config.building);
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

void TroopQueueManager::setVillageTroopEnabled(int villageId,
                                                const QString &building,
                                                bool enabled) {
  if (!m_configs.contains(villageId) ||
      !m_configs[villageId].contains(building)) {
    return;
  }

  m_configs[villageId][building].enabled = enabled;

  if (!m_configFilePath.isEmpty()) {
    saveConfig(m_configFilePath);
  }

  emit configChanged();

  if (enabled) {
    startVillageTimer(villageId, building);
    qDebug() << "[TROOP_MGR] Training enabled for village" << villageId
             << building;
  } else {
    stopVillageTimer(villageId, building);
    QString key = makeTimerKey(villageId, building);
    m_remainingSeconds[key] = 0;
    emit timerTick(villageId, building, 0);
    qDebug() << "[TROOP_MGR] Training disabled for village" << villageId
             << building;
  }
}

QString TroopQueueManager::makeTimerKey(int villageId,
                                        const QString &building) const {
  return QString("%1:%2").arg(villageId).arg(building);
}

void TroopQueueManager::startTimers() {
  qDebug() << "[TROOP_MGR] startTimers called, configs count:" << m_configs.size();

  if (!m_tickTimer->isActive()) {
    m_tickTimer->start();
    qDebug() << "[TROOP_MGR] Tick timer started";
  }

  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    int villageId = it.key();
    const auto &innerMap = it.value();
    for (auto bIt = innerMap.begin(); bIt != innerMap.end(); ++bIt) {
      qDebug() << "[TROOP_MGR] Checking village" << villageId << bIt.key()
               << "enabled:" << bIt.value().enabled;
      if (bIt.value().enabled) {
        startVillageTimer(villageId, bIt.key());
      }
    }
  }
}

void TroopQueueManager::stopTimers() {
  m_tickTimer->stop();
  m_remainingSeconds.clear();
}

void TroopQueueManager::startVillageTimer(int villageId,
                                          const QString &building) {
  if (!m_configs.contains(villageId) ||
      !m_configs[villageId].contains(building)) {
    return;
  }

  const TroopConfig &config = m_configs[villageId][building];
  int baseSeconds = config.intervalMinutes * 60;

  // +/- %20 random jitter
  int jitterRange = baseSeconds / 5;
  int jitter = QRandomGenerator::global()->bounded(-jitterRange, jitterRange + 1);
  int finalSeconds = qMax(30, baseSeconds + jitter);

  QString key = makeTimerKey(villageId, building);
  m_remainingSeconds[key] = finalSeconds;

  qDebug() << "[TROOP_MGR] Timer set for village" << villageId << building
           << "base:" << baseSeconds << "s, jitter:" << jitter
           << "s, total:" << finalSeconds << "s";

  // Ensure tick timer is running
  if (!m_tickTimer->isActive()) {
    m_tickTimer->start();
  }
}

void TroopQueueManager::stopVillageTimer(int villageId,
                                         const QString &building) {
  QString key = makeTimerKey(villageId, building);
  m_remainingSeconds.remove(key);
}

void TroopQueueManager::onTimer() {
  // Countdown tick for all active timers
  QList<QPair<int, QString>> expiredTimers;

  for (auto it = m_remainingSeconds.begin(); it != m_remainingSeconds.end();
       ++it) {
    it.value()--;

    // Extract villageId and building from key "villageId:building"
    QStringList parts = it.key().split(":");
    if (parts.size() != 2)
      continue;

    int villageId = parts[0].toInt();
    QString building = parts[1];

    emit timerTick(villageId, building, it.value());

    if (it.value() <= 0) {
      qDebug() << "[TROOP_MGR] Timer expired for village" << villageId << building;
      expiredTimers.append(qMakePair(villageId, building));
    }
  }

  // Execute training for expired timers
  for (const auto &pair : expiredTimers) {
    int villageId = pair.first;
    QString building = pair.second;

    if (m_configs.contains(villageId) &&
        m_configs[villageId].contains(building) &&
        m_configs[villageId][building].enabled) {
      executeTrainingNow(villageId, building, m_fetcher, m_lastAllData);
      // Reset timer
      startVillageTimer(villageId, building);
    }
  }
}

void TroopQueueManager::executeTrainingNow(int villageId,
                                           const QString &building,
                                           TravianDataFetcher *fetcher,
                                           const QVariantMap &allData) {
  qDebug() << "[TROOP_MGR] executeTrainingNow called for village" << villageId << building;

  m_fetcher = fetcher;
  m_lastAllData = allData;

  if (!fetcher) {
    qDebug() << "[TROOP_MGR] No fetcher available";
    return;
  }

  if (!m_configs.contains(villageId)) {
    qDebug() << "[TROOP_MGR] Village" << villageId << "not in configs";
    return;
  }

  if (!m_configs[villageId].contains(building)) {
    qDebug() << "[TROOP_MGR] Building" << building << "not configured for village" << villageId;
    return;
  }

  QString villageKey = QString("village_%1").arg(villageId);
  if (!allData.contains(villageKey)) {
    qDebug() << "[TROOP_MGR] Village data not found:" << villageKey;
    emit trainingFailed(villageId,
                       QString("Köy verisi bulunamadı: %1").arg(villageId));
    return;
  }

  QVariantMap villageData = allData[villageKey].toMap();
  const TroopConfig &config = m_configs[villageId][building];

  // Find the military building slot
  int slotId = findMilitarySlot(villageData, config.building);
  if (slotId < 0) {
    emit trainingFailed(
        villageId, QString("%1 binası bulunamadı").arg(config.building));
    return;
  }

  qDebug() << "[TROOP_MGR] Executing training for village" << villageId
           << building << "troop:" << config.troopName;

  // Trigger training via fetcher
  fetcher->trainTroops(villageId, slotId, config.troopId, config.troopName);
}

int TroopQueueManager::remainingSeconds(int villageId,
                                        const QString &building) const {
  QString key = makeTimerKey(villageId, building);
  return m_remainingSeconds.value(key, 0);
}

void TroopQueueManager::processTraining(TravianDataFetcher *fetcher,
                                        const QVariantMap &allData) {
  qDebug() << "[TROOP_MGR] processTraining called, active timers:" << m_remainingSeconds.size();
  m_fetcher = fetcher;
  m_lastAllData = allData;

  // Start timers if not already running
  startTimers();
}
