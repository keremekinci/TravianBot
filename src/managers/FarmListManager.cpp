#include "src/managers/FarmListManager.h"
#include "src/network/TravianDataFetcher.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

// ============================================================================
// FarmConfig JSON serialization
// ============================================================================

QJsonObject FarmListManager::FarmConfig::toJson() const {
  QJsonObject obj;
  obj["villageId"] = villageId;
  obj["listName"] = listName;
  obj["intervalMinutes"] = intervalMinutes;
  obj["enabled"] = enabled;
  return obj;
}

FarmListManager::FarmConfig
FarmListManager::FarmConfig::fromJson(int listId, const QJsonObject &obj) {
  FarmConfig c;
  c.listId = listId;
  c.villageId = obj["villageId"].toInt();
  c.listName = obj["listName"].toString();
  c.intervalMinutes = obj["intervalMinutes"].toInt(30);
  c.enabled = obj["enabled"].toBool(false);
  return c;
}

// ============================================================================
// Constructor
// ============================================================================

FarmListManager::FarmListManager(QObject *parent) : QObject(parent) {
  // 1-second tick timer for countdown updates
  m_tickTimer = new QTimer(this);
  m_tickTimer->setInterval(1000);
  connect(m_tickTimer, &QTimer::timeout, this, &FarmListManager::onTimer);
}

// ============================================================================
// Config persistence
// ============================================================================

void FarmListManager::loadConfig(const QString &path) {
  m_configPath = path;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qDebug() << "[FARM_MGR] No config file found:" << path;
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  QJsonObject root = doc.object();
  QJsonObject lists = root["lists"].toObject();

  for (auto it = lists.begin(); it != lists.end(); ++it) {
    int listId = it.key().toInt();
    FarmConfig config = FarmConfig::fromJson(listId, it.value().toObject());
    m_configs[listId] = config;
    qDebug() << "[FARM_MGR] Loaded config for list" << listId
             << "(" << config.listName << ")"
             << "village:" << config.villageId
             << "interval:" << config.intervalMinutes << "min"
             << "enabled:" << config.enabled;
  }
}

void FarmListManager::saveConfig() {
  if (m_configPath.isEmpty())
    return;

  QJsonObject lists;
  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    lists[QString::number(it.key())] = it.value().toJson();
  }

  QJsonObject root;
  root["lists"] = lists;

  QFile file(m_configPath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
  }
}

// ============================================================================
// Config management
// ============================================================================

void FarmListManager::setListConfig(int listId, int villageId,
                                     const QString &listName,
                                     int intervalMinutes) {
  FarmConfig &config = m_configs[listId];
  config.listId = listId;
  config.villageId = villageId;
  config.listName = listName;
  config.intervalMinutes = intervalMinutes;

  saveConfig();
  emit configChanged();

  qDebug() << "[FARM_MGR] Config set for list" << listId
           << "(" << listName << ")"
           << "village:" << villageId
           << "interval:" << intervalMinutes;
}

void FarmListManager::removeListConfig(int listId) {
  if (!m_configs.contains(listId))
    return;

  stopListTimer(listId);
  m_configs.remove(listId);
  m_remainingSeconds.remove(listId);

  saveConfig();
  emit configChanged();

  qDebug() << "[FARM_MGR] Config removed for list" << listId;
}

void FarmListManager::setListEnabled(int listId, bool enabled) {
  if (!m_configs.contains(listId))
    return;

  m_configs[listId].enabled = enabled;
  saveConfig();
  emit configChanged();

  if (enabled) {
    startListTimer(listId);
    qDebug() << "[FARM_MGR] Farm enabled for list" << listId;
  } else {
    stopListTimer(listId);
    m_remainingSeconds[listId] = 0;
    emit timerTick(listId, 0);
    qDebug() << "[FARM_MGR] Farm disabled for list" << listId;
  }
}

FarmListManager::FarmConfig
FarmListManager::getListConfig(int listId) const {
  return m_configs.value(listId);
}

QList<int> FarmListManager::getConfiguredLists() const {
  return m_configs.keys();
}

QList<int> FarmListManager::getConfiguredVillages() const {
  QSet<int> villages;
  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    villages.insert(it.value().villageId);
  }
  return villages.values();
}

QVariantList FarmListManager::allConfigs() const {
  QVariantList result;
  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    QVariantMap item;
    item["listId"] = it.key();
    item["villageId"] = it.value().villageId;
    item["listName"] = it.value().listName;
    item["enabled"] = it.value().enabled;
    item["intervalMinutes"] = it.value().intervalMinutes;
    item["remainingSeconds"] = m_remainingSeconds.value(it.key(), 0);
    result.append(item);
  }
  return result;
}

// ============================================================================
// Timer management
// ============================================================================

void FarmListManager::startTimers() {
  if (!m_tickTimer->isActive()) {
    m_tickTimer->start();
  }

  for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
    if (it.value().enabled) {
      startListTimer(it.key());
    }
  }
}

void FarmListManager::stopTimers() {
  m_tickTimer->stop();
  for (int listId : m_remainingSeconds.keys()) {
    stopListTimer(listId);
  }
}

void FarmListManager::startListTimer(int listId) {
  if (!m_configs.contains(listId))
    return;

  const FarmConfig &config = m_configs[listId];
  m_remainingSeconds[listId] = config.intervalMinutes * 60;

  // Ensure tick timer is running
  if (!m_tickTimer->isActive()) {
    m_tickTimer->start();
  }
}

void FarmListManager::stopListTimer(int listId) {
  m_remainingSeconds.remove(listId);
}

void FarmListManager::onTimer() {
  // Countdown tick for all active lists
  QList<int> expiredLists;

  for (auto it = m_remainingSeconds.begin(); it != m_remainingSeconds.end();
       ++it) {
    int listId = it.key();
    it.value()--;
    emit timerTick(listId, it.value());

    if (it.value() <= 0) {
      expiredLists.append(listId);
    }
  }

  // Execute farm lists for expired timers
  for (int listId : expiredLists) {
    if (m_fetcher && m_configs.contains(listId) &&
        m_configs[listId].enabled) {
      executeListNow(listId, m_fetcher, m_lastAllData);
      // Reset timer
      startListTimer(listId);
    }
  }
}

void FarmListManager::executeListNow(int listId, TravianDataFetcher *fetcher,
                                      const QVariantMap &allData) {
  m_fetcher = fetcher;
  m_lastAllData = allData;

  if (!m_configs.contains(listId)) {
    qDebug() << "[FARM_MGR] No config for list" << listId;
    return;
  }

  const FarmConfig &config = m_configs[listId];

  qDebug() << "[FARM_MGR] Executing farm list" << listId
           << "(" << config.listName << ")"
           << "from village" << config.villageId;

  emit farmExecutionStarted(config.villageId, listId);
  fetcher->executeFarmList(config.villageId, listId);
}

void FarmListManager::processAllFarms(TravianDataFetcher *fetcher,
                                       const QVariantMap &allData) {
  m_fetcher = fetcher;
  m_lastAllData = allData;

  // Start timers if not already running
  startTimers();
}

int FarmListManager::remainingSeconds(int listId) const {
  return m_remainingSeconds.value(listId, 0);
}
