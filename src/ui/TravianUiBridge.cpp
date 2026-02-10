#include "src/ui/TravianUiBridge.h"
#include "src/managers/BuildQueueManager.h"
#include "src/managers/FarmListManager.h"
#include "src/managers/TroopQueueManager.h"
#include "src/models/Account.h"
#include "src/network/TravianDataFetcher.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QRandomGenerator>
#include <QSet>
#include <QSettings>
#include <QThread>
#include <QTimer>

static const QString COOKIE_CACHE_PATH =
    "/Users/kekinci/Desktop/test/config/cookie_cache.json";

TravianUiBridge::TravianUiBridge(QObject *parent) : QObject(parent) {
  m_fetcher = new TravianDataFetcher(this);

  // Initialize build queue manager
  m_buildQueueManager = new BuildQueueManager(this);
  m_buildQueueManager->loadQueue(
      "/Users/kekinci/Desktop/test/config/build_queue.json");

  // Initialize troop queue manager
  m_troopQueueManager = new TroopQueueManager(this);
  m_troopQueueManager->loadConfig(
      "/Users/kekinci/Desktop/test/config/troop_config.json");

  connect(m_troopQueueManager, &TroopQueueManager::configChanged, this,
          &TravianUiBridge::troopConfigsChanged);

  // Initialize farm list manager
  m_farmListManager = new FarmListManager(this);
  m_farmListManager->loadConfig(
      "/Users/kekinci/Desktop/test/config/farm_config.json");

  connect(m_farmListManager, &FarmListManager::configChanged, this,
          &TravianUiBridge::farmConfigsChanged);

  connect(m_farmListManager, &FarmListManager::timerTick, this,
          [this](int listId, int remaining) {
            Q_UNUSED(listId);
            Q_UNUSED(remaining);
            emit farmTimerTick();
          });

  connect(m_farmListManager, &FarmListManager::farmExecutionStarted, this,
          [this](int villageId, int listId) {
            logActivity(QString("Yağma listesi başlatılıyor (Köy %1, Liste %2)")
                            .arg(villageId)
                            .arg(listId),
                        "info");
          });

  // Initialize account model
  m_account = new Account(this);

  connect(m_buildQueueManager, &BuildQueueManager::queueChanged, this,
          &TravianUiBridge::buildQueueChanged);
  connect(m_buildQueueManager, &BuildQueueManager::taskStarted, this,
          [this](int villageId, int slotId, const QString &buildingName) {
            setStatus(QString("🏗️ Auto: %1 yükseltiliyor (Köy %2, Slot %3)")
                          .arg(buildingName)
                          .arg(villageId)
                          .arg(slotId));
            logActivity(QString("%1 yükseltme başlatıldı (Köy %2)")
                            .arg(buildingName)
                            .arg(villageId),
                        "success");
          });

  // Task completed - building reached target level
  connect(
      m_buildQueueManager, &BuildQueueManager::taskCompleted, this,
      [this](int villageId, int slotId) {
        Q_UNUSED(slotId);
        logActivity(
            QString(
                "Kuyruk görevi tamamlandı (Köy %1) - hedef seviyeye ulaşıldı")
                .arg(villageId),
            "success");
      });

  // Builder busy - wait until construction finishes + 15 seconds
  connect(
      m_buildQueueManager, &BuildQueueManager::builderBusy, this,
      [this](int villageId, int remainingSec) {
        Q_UNUSED(villageId);
        int waitTime = remainingSec + 15; // Wait until finished + 15 sec buffer
        setStatus(
            QString("⏳ İnşaat devam ediyor, %1 saniye sonra tekrar denenecek")
                .arg(waitTime));
        logActivity(QString("Kuyrukta inşaat mevcut, inşaatçı meşgul. %1 "
                            "saniye sonra tekrar denenecek")
                        .arg(waitTime),
                    "warning");

        // Override next refresh time
        m_refreshTimer->stop();
        m_refreshTimer->start(waitTime * 1000);
        m_nextRefreshIn = waitTime;
        m_buildQueueScheduledRefresh = true; // Mark that we set custom refresh
        emit nextRefreshInChanged();
      });

  // Insufficient resources - wait random 2-5 minutes
  connect(
      m_buildQueueManager, &BuildQueueManager::insufficientResources, this,
      [this](int villageId, const QString &buildingName) {
        Q_UNUSED(villageId);
        int waitTime = QRandomGenerator::global()->bounded(120, 301); // 2-5 min
        setStatus(QString("💰 %1 için kaynak yetersiz, %2 saniye bekleniyor")
                      .arg(buildingName)
                      .arg(waitTime));
        logActivity(
            QString("%1 için kaynak yetersiz, %2 saniye sonra tekrar denenecek")
                .arg(buildingName)
                .arg(waitTime),
            "warning");

        // Override next refresh time
        m_refreshTimer->stop();
        m_refreshTimer->start(waitTime * 1000);
        m_nextRefreshIn = waitTime;
        m_buildQueueScheduledRefresh = true; // Mark that we set custom refresh
        emit nextRefreshInChanged();
      });

  // Initialize timers
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setSingleShot(true);
  connect(m_refreshTimer, &QTimer::timeout, this,
          &TravianUiBridge::onRefreshTimer);

  m_countdownTimer = new QTimer(this);
  m_countdownTimer->setInterval(1000); // Update every second
  connect(m_countdownTimer, &QTimer::timeout, this,
          &TravianUiBridge::onCountdownTimer);

  // Settings'den ayarları yükle
  loadSettings();

  // Hardcoded config'i yükle
  m_fetcher->loadConfig(""); // Path unused, config is hardcoded

  // BaseUrl'i settings'den al
  m_fetcher->setBaseUrl(m_baseUrl);

  setStatus("✅ Hazır");
  logActivity("Uygulama başlatıldı", "info");

  // Login signals
  connect(m_fetcher, &TravianDataFetcher::loginSuccess, this, [this]() {
    m_isLoggedIn = true;
    emit isLoggedInChanged();
    setStatus("✅ Giriş başarılı!");
    logActivity("Giriş başarılı", "success");
    setLoading(false);

    // Cookie'leri kaydet (gelecek seferler için)
    m_fetcher->saveCookiesToFile(COOKIE_CACHE_PATH);

    // Cookie'lerin event loop'ta işlenmesi için kısa delay sonra veri çek
    QTimer::singleShot(500, this, [this]() {
      // Cookie'leri yeniden yükle - bu login sonrası cookie sync sorununu çözer
      m_fetcher->tryLoadSavedCookies(COOKIE_CACHE_PATH);
      QMetaObject::invokeMethod(this, "startFetch", Qt::QueuedConnection);
    });
  });

  connect(m_fetcher, &TravianDataFetcher::loginFailed, this,
          [this](const QString &error) {
            m_isLoggedIn = false;
            emit isLoggedInChanged();
            setStatus("❌ Giriş başarısız: " + error);
            logActivity("Giriş başarısız: " + error, "error");
            setLoading(false);
          });

  connect(m_fetcher, &TravianDataFetcher::villagesDiscovered, this,
          [this](const QList<VillageInfo> &villages) {
            setStatus(QString("🏘️ %1 köy bulundu").arg(villages.size()));
            logActivity(QString("%1 köy bulundu, bilgiler çekiliyor...")
                            .arg(villages.size()),
                        "info");
          });

  connect(m_fetcher, &TravianDataFetcher::fetchError, this,
          [this](const QString &pageName, const QString &error) {
            // Session expired olabilir, tekrar login dene
            if (error.contains("302") || error.contains("redirect") ||
                error.contains("401") || error.contains("unauthorized")) {
              setStatus("⚠️ Session expired, tekrar giriş yapılıyor...");
              logActivity("Oturum süresi doldu, tekrar giriş yapılıyor...",
                          "warning");
              m_isLoggedIn = false;
              QMetaObject::invokeMethod(this, "performLogin",
                                        Qt::QueuedConnection);
              return;
            }
            setStatus(QString("❌ %1: %2").arg(pageName, error));
            logActivity(QString("Hata: %1 - %2").arg(pageName, error), "error");
            setLoading(false);
          });

  connect(
      m_fetcher, &TravianDataFetcher::allDataFetched, this,
      [this](const QVariantMap &allData) {
        // ham data
        m_allData = allData;
        emit allDataChanged();

        // QML için düzgün villages listesi üret
        QVariantList vlist;
        const QList<VillageInfo> v = m_fetcher->getVillages();

        for (const VillageInfo &vi : v) {
          QVariantMap one;
          one["id"] = vi.id;
          one["name"] = vi.name;
          one["data"] = vi.data;
          vlist.append(one);
        }

        m_villages = vlist;
        emit villagesChanged();

        setStatus(QString("✅ %1 köy verisi yüklendi").arg(v.size()));
        logActivity(QString("%1 köy verisi başarıyla yüklendi").arg(v.size()),
                    "success");
        setLoading(false);

        // Reset flag before processing queue
        m_buildQueueScheduledRefresh = false;

        // Upgrade sonrası flag'i sıfırla
        if (m_upgradeInProgress) {
          m_upgradeInProgress = false;
          qDebug() << "[UI] Upgrade sonrası veri geldi";
        }

        // Process build queue - kuyrukta görev varsa her zaman çalışır
        // NOT: Sonsuz döngü tehlikesi yok çünkü:
        // 1) upgradeStarted artık anında startFetch çağırmıyor (10 sn bekliyor)
        // 2) isBuilderFree() inşaat devam ediyorsa yeni upgrade başlatmıyor
        if (m_buildQueueManager->totalTaskCount() > 0) {
          logActivity(QString("İnşaat kuyruğu işleniyor (%1 görev)")
                          .arg(m_buildQueueManager->totalTaskCount()),
                      "info");
          m_buildQueueManager->processQueue(m_fetcher, allData);
        }

        // Process troop training - config varsa her zaman çalışır
        if (!m_troopQueueManager->getConfiguredVillages().isEmpty()) {
          logActivity("Asker eğitimi işleniyor...", "info");
          m_troopQueueManager->processTraining(m_fetcher, allData);
        }

        // Process farm lists (keep timers running) - her zaman çalışır
        if (!m_farmListManager->getConfiguredLists().isEmpty()) {
          m_farmListManager->processAllFarms(m_fetcher, allData);
        }

        // Auto-fetch farm lists - sadece ilk yüklemede bir kez çalışır
        if (!m_farmListsFetched) {
          m_farmListsFetched = true;
          qDebug() << "[UI] İlk yükleme: farm listeleri otomatik çekiliyor ("
                   << v.size() << "köy)";
          for (int i = 0; i < v.size(); ++i) {
            int vid = v[i].id;
            QTimer::singleShot(1000 + i * 2000, this, [this, vid]() {
              m_fetcher->fetchFarmLists(vid);
            });
          }
        }

        // Start auto-refresh if enabled (but not if build queue already
        // scheduled)
        if (m_autoRefreshEnabled && !m_buildQueueScheduledRefresh) {
          scheduleNextRefresh();
        }
      });

  // Upgrade signals
  connect(m_fetcher, &TravianDataFetcher::upgradeStarted, this,
          [this](int villageId, int slotId, const QString &buildingName) {
            Q_UNUSED(villageId);
            Q_UNUSED(slotId);
            setStatus("🔨 " + buildingName + " yükseltiliyor...");
            logActivity(buildingName + " yükseltme başlatıldı", "success");
            // Upgrade sonrası verileri yenile — ama hemen değil, 10 saniye
            // bekle Bu sayede sonsuz döngü engellenir (anında çağırınca döngü
            // oluşuyordu)
            m_upgradeInProgress = true;
            m_refreshTimer->stop();
            m_refreshTimer->start(10000); // 10 saniye sonra yenile
            m_nextRefreshIn = 10;
            m_countdownTimer->start();
            emit nextRefreshInChanged();
            logActivity("10 saniye sonra veri yenilenecek", "info");
          });

  connect(m_fetcher, &TravianDataFetcher::upgradeFailed, this,
          [this](int villageId, int slotId, const QString &error) {
            Q_UNUSED(villageId);
            Q_UNUSED(slotId);
            setStatus("❌ Yükseltme başarısız: " + error);
            logActivity("Yükseltme başarısız: " + error, "error");
          });

  // Farm list results
  connect(m_fetcher, &TravianDataFetcher::farmListsFetched, this,
          [this](int villageId, const QVariantList &lists) {
            // Store per-village farm lists
            m_villageFarmLists[villageId] = lists;

            // Also update the global list (union of all village lists)
            QVariantList allLists;
            QSet<int> seenIds;
            for (auto it = m_villageFarmLists.begin();
                 it != m_villageFarmLists.end(); ++it) {
              for (const QVariant &v : it.value()) {
                int id = v.toMap()["id"].toInt();
                if (!seenIds.contains(id)) {
                  seenIds.insert(id);
                  allLists.append(v);
                }
              }
            }
            m_availableFarmLists = allLists;
            emit availableFarmListsChanged();

            logActivity(QString("Köy %1 için %2 yağma listesi bulundu")
                            .arg(villageId)
                            .arg(lists.size()),
                        "info");
          });

  connect(
      m_fetcher, &TravianDataFetcher::farmListExecuted, this,
      [this](int villageId, int listId, bool success, const QString &message) {
        if (success) {
          logActivity(QString("Yağma listesi başlatıldı (Köy %1, Liste %2)")
                          .arg(villageId)
                          .arg(listId),
                      "success");
        } else {
          logActivity(QString("Yağma listesi hatası (Köy %1, Liste %2): %3")
                          .arg(villageId)
                          .arg(listId)
                          .arg(message),
                      "error");
        }
      });

  // Troop training results
  connect(m_fetcher, &TravianDataFetcher::troopTrainingResult, this,
          [this](int villageId, bool success, const QString &troopName,
                 int count, const QString &message) {
            if (success) {
              logActivity(QString("Asker eğitimi başlatıldı (Köy %1): %2x %3")
                              .arg(villageId)
                              .arg(count)
                              .arg(troopName),
                          "success");
            } else {
              logActivity(QString("Asker eğitimi hatası (Köy %1): %2")
                              .arg(villageId)
                              .arg(message),
                          "error");
            }
          });

  // Başlangıçta önce kaydedilmiş cookie'yi dene, yoksa login yap
  if (m_fetcher->tryLoadSavedCookies(COOKIE_CACHE_PATH)) {
    // Cookie'ler yüklendi, direkt veri çekmeye başla
    m_isLoggedIn = true;
    emit isLoggedInChanged();
    setStatus("✅ Kaydedilmiş oturum kullanılıyor");
    logActivity("Kaydedilmiş oturum bulundu, veri çekiliyor...", "info");
    QMetaObject::invokeMethod(this, "startFetch", Qt::QueuedConnection);
  } else if (!m_username.isEmpty() && !m_password.isEmpty()) {
    // Cookie yok/expired, login yap
    setStatus("🔐 Yeni giriş yapılıyor...");
    logActivity("Hesaba giriş yapılıyor...", "info");
    QMetaObject::invokeMethod(this, "performLogin", Qt::QueuedConnection);
  } else {
    setStatus("⚠️ settings.ini'de kullanıcı bilgileri yok");
    logActivity("Kullanıcı bilgileri bulunamadı (settings.ini)", "error");
  }
}

void TravianUiBridge::loadSettings() {
  QString settingsPath = "/Users/kekinci/Desktop/test/config/settings.ini";

  QSettings settings(settingsPath, QSettings::IniFormat);

  // Server settings
  m_baseUrl =
      settings.value("Server/baseUrl", "https://ts30.x3.europe.travian.com")
          .toString();

  // Credentials
  m_username = settings.value("Credentials/username", "").toString().trimmed();
  m_password = settings.value("Credentials/password", "").toString().trimmed();

  // Bot mode
  m_botMode = settings.value("BotMode/mode", "build").toString();
}

void TravianUiBridge::setLoading(bool v) {
  if (m_loading == v)
    return;
  m_loading = v;
  emit loadingChanged();
}

void TravianUiBridge::setStatus(const QString &s) {
  if (m_statusText == s)
    return;
  m_statusText = s;
  emit statusTextChanged();
}

void TravianUiBridge::performLogin() {
  if (m_loading)
    return;

  if (m_username.isEmpty() || m_password.isEmpty()) {
    setStatus("⚠️ Kullanıcı adı veya şifre boş!");
    return;
  }

  setLoading(true);
  setStatus("🔐 Giriş yapılıyor...");
  logActivity("Hesaba giriş yapılıyor...", "info");
  m_fetcher->performLogin(m_username, m_password);
}

void TravianUiBridge::startFetch() {
  if (m_loading)
    return;
  setLoading(true);
  setStatus("⏳ Veri çekiliyor...");
  logActivity("Köy bilgileri çekiliyor...", "info");
  m_fetcher->fetchAllVillagesData();
}

void TravianUiBridge::upgradeBuilding(int villageId, int slotId) {
  setStatus("⏳ Yükseltme başlatılıyor...");
  m_fetcher->upgradeBuilding(villageId, slotId);
}

// Auto-refresh methods
void TravianUiBridge::setAutoRefreshEnabled(bool enabled) {
  if (m_autoRefreshEnabled == enabled)
    return;

  m_autoRefreshEnabled = enabled;
  emit autoRefreshEnabledChanged();

  if (enabled) {
    logActivity("Otomatik yenileme aktif edildi", "info");
    scheduleNextRefresh();
    m_countdownTimer->start();
  } else {
    logActivity("Otomatik yenileme devre dışı bırakıldı", "info");
    m_refreshTimer->stop();
    m_countdownTimer->stop();
    m_nextRefreshIn = 0;
    emit nextRefreshInChanged();
  }
}

void TravianUiBridge::setRefreshMode(const QString &mode) {
  if (m_refreshMode == mode)
    return;

  m_refreshMode = mode;
  emit refreshModeChanged();

  // If auto-refresh is active, reschedule with new mode
  if (m_autoRefreshEnabled) {
    scheduleNextRefresh();
  }
}

void TravianUiBridge::onRefreshTimer() {
  if (!m_autoRefreshEnabled)
    return;

  startFetch();

  // Schedule next refresh
  scheduleNextRefresh();
}

void TravianUiBridge::onCountdownTimer() {
  if (m_nextRefreshIn > 0) {
    m_nextRefreshIn--;
    emit nextRefreshInChanged();
  }
}

void TravianUiBridge::scheduleNextRefresh() {
  int intervalMs = getRandomInterval();
  m_nextRefreshIn = intervalMs / 1000; // Convert to seconds

  m_refreshTimer->start(intervalMs);

  logActivity(QString("Sonraki yenileme %1 saniye sonra (%2 modu)")
                  .arg(m_nextRefreshIn)
                  .arg(m_refreshMode),
              "info");

  emit nextRefreshInChanged();
}

int TravianUiBridge::getRandomInterval() const {
  if (m_refreshMode == "smart") {
    // Smart mode: Calculate based on construction queue
    int constructionTimeLeft = getConstructionTimeRemaining();

    if (constructionTimeLeft > 0) {
      // If building is in progress, check shortly after completion (add
      // 10-30sec buffer)
      int buffer = QRandomGenerator::global()->bounded(10, 31) * 1000;
      int interval = constructionTimeLeft + buffer;
      return interval;
    } else {
      // No construction, check every 2-5 minutes
      int interval = QRandomGenerator::global()->bounded(120, 301) * 1000;
      return interval;
    }
  } else if (m_refreshMode == "short") {
    // Short mode: 30-60 seconds (less aggressive than 4-5sec)
    return QRandomGenerator::global()->bounded(30, 61) * 1000;
  } else {
    // Long mode: 5-10 minutes
    return QRandomGenerator::global()->bounded(300, 601) * 1000;
  }
}

int TravianUiBridge::getConstructionTimeRemaining() const {
  // Parse construction queue from village data to get remaining time
  for (const QVariant &villageVar : m_villages) {
    QVariantMap village = villageVar.toMap();
    QVariantMap villageData = village["data"].toMap();
    QVariantMap dorf1 = villageData["dorf1"].toMap();
    QVariantList constructionQueue = dorf1["constructionQueue"].toList();

    if (!constructionQueue.isEmpty()) {
      // Get first construction item
      QVariantMap construction = constructionQueue.first().toMap();
      QString remainingTime = construction["remainingTime"].toString();

      // Parse time format "HH:MM:SS"
      QStringList parts = remainingTime.split(":");
      if (parts.size() == 3) {
        int hours = parts[0].toInt();
        int minutes = parts[1].toInt();
        int seconds = parts[2].toInt();
        int totalSeconds = hours * 3600 + minutes * 60 + seconds;
        return totalSeconds * 1000; // Convert to milliseconds
      }
    }
  }

  return 0; // No construction in progress
}

// Build queue methods
QVariantList TravianUiBridge::buildQueue() const {
  QVariantList result;
  const QList<BuildQueueManager::BuildTask> queue =
      m_buildQueueManager->getAllTasks();

  for (const BuildQueueManager::BuildTask &task : queue) {
    QVariantMap item;
    item["villageId"] = task.villageId;
    item["slotId"] = task.slotId;
    item["currentLevel"] = task.currentLevel;
    item["targetLevel"] = task.targetLevel;
    item["buildingName"] = task.buildingName;
    item["priority"] = task.priority;
    result.append(item);
  }

  return result;
}

void TravianUiBridge::addToBuildQueue(int villageId, int slotId,
                                      int targetLevel,
                                      const QString &buildingName) {
  // Get current level from village data
  QString villageKey = QString("village_%1").arg(villageId);
  int currentLevel = 0;

  if (m_allData.contains(villageKey)) {
    QVariantMap villageData = m_allData[villageKey].toMap();
    currentLevel = m_buildQueueManager->getCurrentLevel(villageData, slotId);
  }

  BuildQueueManager::BuildTask task;
  task.villageId = villageId;
  task.slotId = slotId;
  task.currentLevel = currentLevel;
  task.targetLevel = targetLevel;
  task.buildingName = buildingName;
  task.priority = m_buildQueueManager->totalTaskCount() + 1; // Add to end

  m_buildQueueManager->addTask(task);

  setStatus(QString("📋 Kuyruğa eklendi: %1 (Seviye %2 → %3)")
                .arg(buildingName)
                .arg(currentLevel)
                .arg(targetLevel));
  logActivity(QString("Kuyruğa eklendi: %1 (Seviye %2 → %3)")
                  .arg(buildingName)
                  .arg(currentLevel)
                  .arg(targetLevel),
              "info");
}

void TravianUiBridge::removeFromBuildQueue(int villageId, int index) {
  const auto queue = m_buildQueueManager->getQueue(villageId);
  QString name =
      (index >= 0 && index < queue.size()) ? queue[index].buildingName : "?";
  m_buildQueueManager->removeTask(villageId, index);
  logActivity(
      QString("Kuyruktan silindi: %1 (Köy %2)").arg(name).arg(villageId),
      "info");
}
// Farm list methods
QVariantList TravianUiBridge::farmConfigs() const {
  return m_farmListManager->allConfigs();
}

void TravianUiBridge::fetchFarmLists(int villageId) {
  logActivity(
      QString("Köy %1 için yağma listeleri çekiliyor...").arg(villageId),
      "info");
  m_fetcher->fetchFarmLists(villageId);
}

QVariantList TravianUiBridge::farmListsForVillage(int villageId) const {
  // Köy bazlı farm listesi döndür (ownerVillageId'ye göre filtrele)
  QVariantList result;
  for (const QVariant &v : m_availableFarmLists) {
    QVariantMap item = v.toMap();
    int ownerId = item["ownerVillageId"].toInt();
    // ownerVillageId eşleşiyorsa veya 0 ise (bilinmiyor) göster
    if (ownerId == villageId || ownerId == 0) {
      result.append(v);
    }
  }
  return result;
}

void TravianUiBridge::setFarmListConfig(int listId, int villageId,
                                        const QString &listName,
                                        int intervalMinutes) {
  m_farmListManager->setListConfig(listId, villageId, listName,
                                   intervalMinutes);
  logActivity(QString("Yağma listesi ayarlandı: %1 (Liste %2, %3 dk)")
                  .arg(listName)
                  .arg(listId)
                  .arg(intervalMinutes),
              "info");
}

void TravianUiBridge::removeFarmListConfig(int listId) {
  m_farmListManager->removeListConfig(listId);
  logActivity(QString("Yağma listesi ayarı kaldırıldı (Liste %1)").arg(listId),
              "info");
}

void TravianUiBridge::setFarmListEnabled(int listId, bool enabled) {
  m_farmListManager->setListEnabled(listId, enabled);
  logActivity(QString("Yağma listesi %1 %2")
                  .arg(listId)
                  .arg(enabled ? "aktif edildi" : "devre dışı bırakıldı"),
              "info");
}

void TravianUiBridge::executeFarmListNow(int listId) {
  logActivity(QString("Yağma listesi %1 manuel başlatılıyor").arg(listId),
              "info");

  // Manuel gönderimde config olmasa da çalışmalı
  // Seçili köyden gönder
  int vid = currentVillageId();
  if (vid <= 0 && !m_villages.isEmpty()) {
    vid = m_villages.first().toMap()["id"].toInt();
  }

  // Config varsa oradan villageId al
  auto config = m_farmListManager->getListConfig(listId);
  if (config.listId > 0 && config.villageId > 0) {
    vid = config.villageId;
  }

  qDebug() << "[UI] Manual farm list send - listId:" << listId
           << "villageId:" << vid;
  m_fetcher->executeFarmList(vid, listId);
}

int TravianUiBridge::farmRemainingSeconds(int listId) const {
  return m_farmListManager->remainingSeconds(listId);
}

// Account model methods
int TravianUiBridge::currentVillageId() const {
  return m_account ? m_account->currentVillageId() : -1;
}

void TravianUiBridge::setCurrentVillageId(int villageId) {
  if (m_account) {
    m_account->setCurrentVillageId(villageId);
  }
}

void TravianUiBridge::selectVillage(int villageId) {
  setCurrentVillageId(villageId);

  // Trigger data refresh for selected village
  if (m_isLoggedIn) {
    startFetch();
  }
}

// Activity log implementation
void TravianUiBridge::logActivity(const QString &message, const QString &type) {
  QVariantMap entry;
  entry["timestamp"] =
      QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss");
  entry["message"] = message;
  entry["type"] = type; // "info", "success", "warning", "error"

  m_activityLog.prepend(entry); // Newest first

  // Keep only last 100 entries
  if (m_activityLog.size() > 100) {
    m_activityLog.removeLast();
  }

  // Also log to file via Qt message system
  QString logPrefix = "[ACTIVITY/" + type.toUpper() + "]";
  if (type == "error") {
    qWarning().noquote() << logPrefix << message;
  } else if (type == "warning") {
    qWarning().noquote() << logPrefix << message;
  } else {
    qInfo().noquote() << logPrefix << message;
  }

  emit activityLogChanged();
}

// Bot mode methods
void TravianUiBridge::setBotMode(const QString &mode) {
  if (m_botMode == mode)
    return;

  m_botMode = mode;
  emit botModeChanged();

  QString modeName;
  if (mode == "build")
    modeName = "İnşaat";
  else if (mode == "troop")
    modeName = "Asker";
  else
    modeName = "Karma";

  logActivity(QString("Bot modu değiştirildi: %1").arg(modeName), "info");

  // Save to settings
  QSettings settings("/Users/kekinci/Desktop/test/config/settings.ini",
                     QSettings::IniFormat);
  settings.setValue("BotMode/mode", mode);
}

QVariantList TravianUiBridge::troopConfigs() const {
  return m_troopQueueManager->allConfigs();
}

void TravianUiBridge::setVillageTroop(int villageId, const QString &troopId,
                                      const QString &troopName,
                                      const QString &building) {
  m_troopQueueManager->setVillageTroop(villageId, troopId, troopName, building);
  logActivity(QString("Köy %1 için asker ayarlandı: %2 (%3)")
                  .arg(villageId)
                  .arg(troopName)
                  .arg(building),
              "info");
}

void TravianUiBridge::removeVillageTroop(int villageId,
                                         const QString &building) {
  m_troopQueueManager->removeVillageTroop(villageId, building);
  logActivity(QString("Köy %1 için otomatik asker basma kaldırıldı (%2)")
                  .arg(villageId)
                  .arg(building),
              "info");
}
