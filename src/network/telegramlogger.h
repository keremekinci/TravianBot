#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTimer>

class TelegramLogger : public QObject {
  Q_OBJECT
public:
  explicit TelegramLogger(QObject *parent = nullptr);

  void setCredentials(const QString &botToken, const QString &chatId);
  void setEnabled(bool enabled);
  void addLog(const QString &message);

private slots:
  void flush();

private:
  QString m_botToken;
  QString m_chatId;
  bool m_enabled = false;

  QStringList m_buffer;
  QTimer *m_flushTimer;
  QNetworkAccessManager *m_manager;

  void sendToTelegram(const QString &text);
};
