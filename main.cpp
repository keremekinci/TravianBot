#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

#include "src/network/telegramlogger.h"
#include "src/network/telegramnotifier.h"
#include "src/ui/TravianUiBridge.h"

static QFile *g_logFile = nullptr;
static TelegramLogger *g_telegramLogger = nullptr;
static QMutex g_logMutex;

void customMessageHandler(QtMsgType type, const QMessageLogContext &context,
                          const QString &msg) {
  QMutexLocker locker(&g_logMutex);

  QString level;
  switch (type) {
  case QtDebugMsg:
    level = "DEBUG";
    break;
  case QtInfoMsg:
    level = "INFO";
    break;
  case QtWarningMsg:
    level = "WARN";
    break;
  case QtCriticalMsg:
    level = "ERROR";
    break;
  case QtFatalMsg:
    level = "FATAL";
    break;
  }

  QString timestamp =
      QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
  QString logLine = QString("[%1] [%2] %3\n").arg(timestamp, level, msg);

  // Write to log file
  if (g_logFile && g_logFile->isOpen()) {
    QTextStream stream(g_logFile);
    stream << logLine;
    stream.flush();
  }

  // Also write to stderr
  fprintf(stderr, "%s", logLine.toLocal8Bit().constData());

  // Send to Telegram Logger
  if (g_telegramLogger) {
    // Filter logs for Telegram - only send important events
    bool important = false;
    if (logLine.contains("Sonraki yenileme", Qt::CaseInsensitive) ||
        logLine.contains("Asker eğitimi", Qt::CaseInsensitive) ||
        logLine.contains("Yağma listesi", Qt::CaseInsensitive) ||
        logLine.contains("Oturum", Qt::CaseInsensitive) ||
        logLine.contains("Giriş", Qt::CaseInsensitive) ||
        logLine.contains("Saldırı", Qt::CaseInsensitive) ||
        logLine.contains("Hata", Qt::CaseInsensitive) ||
        logLine.contains("ERROR", Qt::CaseInsensitive) ||
        logLine.contains("FATAL", Qt::CaseInsensitive)) {
      important = true;
    }

    if (important) {
      g_telegramLogger->addLog(logLine);
    }
  }
}

int main(int argc, char *argv[]) {
  // Setup log file
  QString logDir = "/Users/kekinci/Desktop/test/config";
  QDir().mkpath(logDir);
  QString logPath = logDir + "/travian_bot.log";

  g_logFile = new QFile(logPath);
  if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Append |
                      QIODevice::Text)) {
    qInstallMessageHandler(customMessageHandler);

    // Log separator for new session
    QTextStream stream(g_logFile);
    stream << "\n"
           << QString("=").repeated(80) << "\n"
           << "["
           << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
           << "] === UYGULAMA BASLATILDI ===\n"
           << QString("=").repeated(80) << "\n";
    stream.flush();
  }

  QGuiApplication app(argc, argv);

  // Setup Telegram Logger
  g_telegramLogger = new TelegramLogger(&app);

  // Load logger settings
  QSettings settings("/Users/kekinci/Desktop/test/config/settings.ini",
                     QSettings::IniFormat);
  QString logBotToken =
      settings
          .value("TelegramLogger/botToken",
                 "7345411384:AAGvgscYXfFpBqqJy0PPqJqXcJdOasZGQpg")
          .toString()
          .trimmed();
  QString logChatId =
      settings.value("TelegramLogger/chatId", "").toString().trimmed();
  bool logEnabled = settings.value("TelegramLogger/enabled", false).toBool();

  if (!logBotToken.isEmpty() && !logChatId.isEmpty()) {
    g_telegramLogger->setCredentials(logBotToken, logChatId);
    g_telegramLogger->setEnabled(logEnabled);
    qInfo() << "Telegram Remote Logger initialized (Enabled:" << logEnabled
            << ")";
  }

  qInfo() << "Travian Bot başlatılıyor...";

  QQmlApplicationEngine engine;

  auto *model = new TravianUiBridge(&engine);
  engine.rootContext()->setContextProperty("TravianModel", model);

  TelegramNotifier *telegramNotifier = new TelegramNotifier(&app);
  engine.rootContext()->setContextProperty("telegram", telegramNotifier);

  engine.loadFromModule("TravianChecker", "Main");

  if (engine.rootObjects().isEmpty()) {
    qCritical() << "QML engine yüklenemedi!";
    return -1;
  }

  qInfo() << "UI başarıyla yüklendi";

  int result = app.exec();

  // Cleanup
  if (g_logFile) {
    qInfo() << "Uygulama kapatılıyor...";
    g_logFile->close();
    delete g_logFile;
    g_logFile = nullptr;
  }

  return result;
}
