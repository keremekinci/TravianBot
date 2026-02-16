#ifndef TELEGRAMNOTIFIER_H
#define TELEGRAMNOTIFIER_H

#include <QNetworkAccessManager>
#include <QObject>

class TelegramNotifier : public QObject {
  Q_OBJECT
public:
  explicit TelegramNotifier(QObject *parent = nullptr);

  Q_INVOKABLE void sendNotification(const QString &message);
  Q_INVOKABLE void setCredentials(const QString &botToken,
                                  const QString &chatId);

private:
  QNetworkAccessManager *m_manager;
  QString m_botToken;
  QString m_chatId;
};

#endif // TELEGRAMNOTIFIER_H
