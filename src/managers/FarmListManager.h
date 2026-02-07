#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>

class TravianDataFetcher;

/**
 * @brief Per-farm-list configuration and independent timer manager
 *
 * Each farm list has its own independent timer and config.
 * When the timer fires, that specific list is executed via TravianDataFetcher.
 */
class FarmListManager : public QObject {
  Q_OBJECT

public:
  struct FarmConfig {
    int listId = 0;           // Farm list ID
    int villageId = 0;        // Which village sends the raid
    QString listName;         // Farm list name (for display)
    int intervalMinutes = 30; // Raid interval in minutes
    bool enabled = false;     // Whether auto-raid is active

    QJsonObject toJson() const;
    static FarmConfig fromJson(int listId, const QJsonObject &obj);
  };

  explicit FarmListManager(QObject *parent = nullptr);

  // Config persistence
  void loadConfig(const QString &path);
  void saveConfig();

  // Per-list config management
  void setListConfig(int listId, int villageId, const QString &listName,
                     int intervalMinutes);
  void removeListConfig(int listId);
  void setListEnabled(int listId, bool enabled);
  FarmConfig getListConfig(int listId) const;
  QList<int> getConfiguredLists() const;
  QList<int> getConfiguredVillages() const;

  // Data for QML
  QVariantList allConfigs() const;

  // Timer management
  void startTimers();
  void stopTimers();
  void executeListNow(int listId, TravianDataFetcher *fetcher,
                      const QVariantMap &allData);

  // Process all configured lists (start timers)
  void processAllFarms(TravianDataFetcher *fetcher,
                       const QVariantMap &allData);

  // Timer remaining seconds for a specific list
  int remainingSeconds(int listId) const;

signals:
  void configChanged();
  void farmExecutionStarted(int villageId, int listId);
  void timerTick(int listId, int remainingSeconds);

private slots:
  void onTimer();

private:
  void startListTimer(int listId);
  void stopListTimer(int listId);

  QMap<int, FarmConfig> m_configs;       // listId -> config
  QMap<int, int> m_remainingSeconds;     // listId -> countdown
  QTimer *m_tickTimer = nullptr;         // 1-second tick for countdowns
  QString m_configPath;

  TravianDataFetcher *m_fetcher = nullptr;
  QVariantMap m_lastAllData;
};
