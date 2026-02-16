#include "telegramlogger.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

TelegramLogger::TelegramLogger(QObject *parent)
    : QObject(parent), m_manager(new QNetworkAccessManager(this)) {
  m_flushTimer = new QTimer(this);
  m_flushTimer->setInterval(3000); // Flush every 3 seconds
  connect(m_flushTimer, &QTimer::timeout, this, &TelegramLogger::flush);
  m_flushTimer->start();
}

void TelegramLogger::setCredentials(const QString &botToken,
                                    const QString &chatId) {
  m_botToken = botToken;
  m_chatId = chatId;
}

void TelegramLogger::setEnabled(bool enabled) { m_enabled = enabled; }

void TelegramLogger::addLog(const QString &message) {
  if (!m_enabled)
    return;
  m_buffer.append(message);
}

void TelegramLogger::flush() {
  if (m_buffer.isEmpty() || !m_enabled || m_botToken.isEmpty() ||
      m_chatId.isEmpty()) {
    return;
  }

  // Join messages with newlines
  QString fullText = m_buffer.join("");
  m_buffer.clear();

  // Telegram message limit is 4096 chars
  // Split if too long
  const int MAX_LEN = 4000;
  for (int i = 0; i < fullText.length(); i += MAX_LEN) {
    sendToTelegram(fullText.mid(i, MAX_LEN));
  }
}

void TelegramLogger::sendToTelegram(const QString &text) {
  QUrl url("https://api.telegram.org/bot" + m_botToken + "/sendMessage");
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject json;
  json["chat_id"] = m_chatId;
  json["text"] = "`" + text + "`"; // Code block format
  json["parse_mode"] = "Markdown";

  QJsonDocument doc(json);
  QByteArray data = doc.toJson();

  QNetworkReply *reply = m_manager->post(request, data);
  // Fire and forget, but delete later
  connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
