#include "telegramnotifier.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

TelegramNotifier::TelegramNotifier(QObject *parent)
    : QObject(parent), m_manager(new QNetworkAccessManager(this)) {}

void TelegramNotifier::setCredentials(const QString &botToken,
                                      const QString &chatId) {
  m_botToken = botToken;
  m_chatId = chatId;
}

void TelegramNotifier::sendNotification(const QString &message) {
  if (m_botToken.isEmpty() || m_chatId.isEmpty()) {
    qWarning() << "Telegram credentials not set!";
    return;
  }

  QUrl url("https://api.telegram.org/bot" + m_botToken + "/sendMessage");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject json;
  json["chat_id"] = m_chatId;
  json["text"] = message;

  QJsonDocument doc(json);
  QByteArray data = doc.toJson();

  QNetworkReply *reply = m_manager->post(request, data);

  connect(reply, &QNetworkReply::finished, [reply]() {
    if (reply->error() == QNetworkReply::NoError) {
      qDebug() << "Telegram notification sent successfully!";
    } else {
      QByteArray errorBody = reply->readAll();
      qWarning() << "Failed to send Telegram notification:"
                 << reply->errorString();
      qWarning() << "Telegram response:" << errorBody;
    }
    reply->deleteLater();
  });
}
