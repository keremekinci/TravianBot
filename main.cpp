#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

#include "src/ui/TravianUiBridge.h"

static QFile *g_logFile = nullptr;
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
  QString logLine =
      QString("[%1] [%2] %3\n").arg(timestamp, level, msg);

  // Write to log file
  if (g_logFile && g_logFile->isOpen()) {
    QTextStream stream(g_logFile);
    stream << logLine;
    stream.flush();
  }

  // Also write to stderr
  fprintf(stderr, "%s", logLine.toLocal8Bit().constData());
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
           << "[" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
           << "] === UYGULAMA BASLATILDI ===\n"
           << QString("=").repeated(80) << "\n";
    stream.flush();
  }

  QGuiApplication app(argc, argv);

  qInfo() << "Travian Bot başlatılıyor...";

  QQmlApplicationEngine engine;

  auto *model = new TravianUiBridge(&engine);
  engine.rootContext()->setContextProperty("TravianModel", model);

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
