#include "src/network/TravianDataFetcher.h"
#include "src/parsers/HtmlParser.h"
#include "src/parsers/VillageParser.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QRandomGenerator>
#include <QUrlQuery>
#include <zlib.h>

// ============================================================================
// Constructor / Destructor
// ============================================================================

TravianDataFetcher::TravianDataFetcher(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this)),
      m_isProcessing(false), m_delayMin(500) // Anti-bot: increased from 200
      ,
      m_delayMax(2000) // Anti-bot: increased from 300
      ,
      m_totalRequests(0), m_completedRequests(0), m_currentVillageIndex(0) {
  connect(m_networkManager, &QNetworkAccessManager::finished, this,
          &TravianDataFetcher::onRequestFinished);

  // Ignore SSL errors for development
  connect(m_networkManager, &QNetworkAccessManager::sslErrors,
          [](QNetworkReply *reply, const QList<QSslError> &) {
            reply->ignoreSslErrors();
          });

  // Initialize user-agent list for anti-bot protection
  // Güncel User-Agent listesi — oturum başında bir tanesi seçilir ve sabit kalır
  m_userAgents = {
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
      "like Gecko) Chrome/133.0.0.0 Safari/537.36",
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
      "like Gecko) Chrome/132.0.0.0 Safari/537.36",
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36",
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 "
      "(KHTML, like Gecko) Version/18.3 Safari/605.1.15",
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:134.0) Gecko/20100101 "
      "Firefox/134.0",
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36 Edg/133.0.0.0",
      "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/133.0.0.0 Safari/537.36"};

  // Oturum boyunca sabit UA kullan (her request'te farklı UA göndermek şüpheli)
  m_sessionUserAgent = getRandomUserAgent();

  // Cookie auto-refresh tracking
  m_lastCookieSaveTime = QDateTime::currentDateTime();
}

TravianDataFetcher::~TravianDataFetcher() = default;

// ============================================================================
// Connection Stability Helpers
// ============================================================================

QByteArray TravianDataFetcher::decompressGzip(const QByteArray &data) {
  if (data.size() < 2)
    return data;
  // Check gzip magic bytes (0x1f 0x8b)
  if ((unsigned char)data[0] != 0x1f || (unsigned char)data[1] != 0x8b)
    return data;

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  // 15 + 32 tells zlib to auto-detect gzip or zlib format
  if (inflateInit2(&strm, 15 + 32) != Z_OK)
    return data;

  strm.avail_in = data.size();
  strm.next_in = (Bytef *)data.constData();

  QByteArray output;
  char buffer[32768];
  int ret;
  do {
    strm.avail_out = sizeof(buffer);
    strm.next_out = (Bytef *)buffer;
    ret = inflate(&strm, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END)
      break;
    output.append(buffer, sizeof(buffer) - strm.avail_out);
  } while (ret != Z_STREAM_END);

  inflateEnd(&strm);

  if (output.isEmpty()) {
    qWarning() << "[GZIP] Decompression failed, returning raw data ("
               << data.size() << "bytes)";
    return data;
  }
  qDebug() << "[GZIP] Decompressed" << data.size() << "->" << output.size()
           << "bytes";
  return output;
}

void TravianDataFetcher::refreshCookiesFromResponse(QNetworkReply *reply) {
  Q_UNUSED(reply);
  if (m_cookieCachePath.isEmpty())
    return;

  // Qt's cookie jar is already updated automatically from set-cookie headers
  // We just need to check if we should persist to disk
  QList<QNetworkCookie> cookies =
      m_networkManager->cookieJar()->cookiesForUrl(QUrl(m_baseUrl));
  QString currentJwt;
  for (const QNetworkCookie &c : cookies) {
    if (c.name() == "JWT") {
      currentJwt = QString::fromUtf8(c.value());
      break;
    }
  }

  bool jwtChanged = (!currentJwt.isEmpty() && currentJwt != m_lastJwtValue);
  bool timeElapsed =
      m_lastCookieSaveTime.secsTo(QDateTime::currentDateTime()) >= 600;

  if (jwtChanged || timeElapsed) {
    m_lastJwtValue = currentJwt;
    m_lastCookieSaveTime = QDateTime::currentDateTime();
    saveCookiesToFile(m_cookieCachePath);
    qDebug() << "[COOKIE] Auto-saved cookies (jwtChanged:" << jwtChanged
             << "timeElapsed:" << timeElapsed << ")";
  }
}

bool TravianDataFetcher::shouldRetryNetworkError(
    QNetworkReply::NetworkError error) const {
  switch (error) {
  case QNetworkReply::RemoteHostClosedError:       // GOAWAY / shutdown
  case QNetworkReply::ConnectionRefusedError:      // Connection refused
  case QNetworkReply::TimeoutError:                // Request timeout
  case QNetworkReply::TemporaryNetworkFailureError: // Temporary failure
  case QNetworkReply::NetworkSessionFailedError:   // Session failed
  case QNetworkReply::InternalServerError:         // 500
  case QNetworkReply::ServiceUnavailableError:     // 503
  case QNetworkReply::UnknownNetworkError:         // Unknown
    return true;
  default:
    return false;
  }
}

void TravianDataFetcher::resetNetworkManager() {
  qWarning()
      << "[NET] Resetting QNetworkAccessManager to clear stale connections";

  // Save current cookies before destroying the old manager
  QList<QNetworkCookie> cookies =
      m_networkManager->cookieJar()->cookiesForUrl(QUrl(m_baseUrl));

  // Disconnect old manager
  disconnect(m_networkManager, nullptr, this, nullptr);
  m_networkManager->deleteLater();

  // Create new manager
  m_networkManager = new QNetworkAccessManager(this);

  // Re-establish the finished connection
  connect(m_networkManager, &QNetworkAccessManager::finished, this,
          &TravianDataFetcher::onRequestFinished);

  // Re-establish SSL error ignoring
  connect(m_networkManager, &QNetworkAccessManager::sslErrors,
          [](QNetworkReply *reply, const QList<QSslError> &) {
            reply->ignoreSslErrors();
          });

  // Restore cookies
  m_networkManager->cookieJar()->setCookiesFromUrl(cookies, QUrl(m_baseUrl));

  m_consecutiveNetworkErrors = 0;
  qDebug() << "[NET] Network manager reset complete, cookies restored";
}

// ============================================================================
// Session Health Check
// ============================================================================

void TravianDataFetcher::checkSessionHealth() {
  qDebug() << "[SESSION] Performing session health check";

  QNetworkRequest request;
  request.setUrl(QUrl(m_baseUrl + "/dorf1.php"));
  request.setRawHeader("User-Agent", m_sessionUserAgent.toUtf8());
  request.setRawHeader("Accept-Encoding", "identity");

  QNetworkReply *reply = m_networkManager->get(request);
  reply->setProperty("isSessionCheck", true);

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onSessionCheckFinished(reply); });
}

void TravianDataFetcher::onSessionCheckFinished(QNetworkReply *reply) {
  refreshCookiesFromResponse(reply);

  if (reply->error() != QNetworkReply::NoError) {
    qWarning() << "[SESSION] Health check network error:"
               << reply->errorString();
    emit sessionHealthCheckResult(false);
    reply->deleteLater();
    return;
  }

  QByteArray rawData = reply->readAll();
  rawData = decompressGzip(rawData);
  QString html = QString::fromUtf8(rawData);
  reply->deleteLater();

  // Check for valid session indicators
  bool isValid = html.contains("villageList") &&
                 !html.contains("class=\"login\"") &&
                 !html.contains("id=\"loginScene\"");

  qDebug() << "[SESSION] Health check result:"
           << (isValid ? "VALID" : "EXPIRED") << "(response:" << html.size()
           << "bytes)";
  emit sessionHealthCheckResult(isValid);
}

// ============================================================================
// Configuration Loading
// ============================================================================

bool TravianDataFetcher::loadConfig(const QString &configPath) {
  Q_UNUSED(configPath);

  // Hardcoded config - embedded in code for portability
  const char *configJson = R"CONFIG({
    "baseUrl": "https://ts30.x3.europe.travian.com",
    "requestDelay": {
        "min": 500,
        "max": 2000
    },
    "pages": {
        "dorf1": {
            "url": "/dorf1.php",
            "description": "Kaynak alanlari ve uretim",
            "fields": {
                "tribe": {
                    "selector": "resourceFieldContainer\"[^>]*class=\"[^\"]*tribe(\\d+)",
                    "type": "single"
                },
                "villageName": {
                    "selector": "villageName\"[^>]*value=\"([^\"]+)\"",
                    "type": "single"
                },
                "lumber": {
                    "selector": "id=\"l1\"[^>]*>&#x202d;([\\d.]+)&#x202c;",
                    "type": "single"
                },
                "clay": {
                    "selector": "id=\"l2\"[^>]*>&#x202d;([\\d.]+)&#x202c;",
                    "type": "single"
                },
                "iron": {
                    "selector": "id=\"l3\"[^>]*>&#x202d;([\\d.]+)&#x202c;",
                    "type": "single"
                },
                "crop": {
                    "selector": "id=\"l4\"[^>]*>&#x202d;([\\d.]+)&#x202c;",
                    "type": "single"
                },
                "warehouseCapacity": {
                    "selector": "class=\"warehouse\"[^>]*>[\\s\\S]*?class=\"value\">&#x202d;([\\d.]+)&#x202c;",
                    "type": "single"
                },
                "granaryCapacity": {
                    "selector": "class=\"granary\"[^>]*>[\\s\\S]*?class=\"value\">&#x202d;([\\d.]+)&#x202c;",
                    "type": "single"
                },
                "productionLumber": {
                    "selector": "resource1[^>]*title=\"[^|]*\\|\\|[^:]*retim: (\\d+)",
                    "type": "single"
                },
                "productionClay": {
                    "selector": "resource2[^>]*title=\"[^|]*\\|\\|[^:]*retim: (\\d+)",
                    "type": "single"
                },
                "productionIron": {
                    "selector": "resource3[^>]*title=\"[^|]*\\|\\|[^:]*retim: (\\d+)",
                    "type": "single"
                },
                "productionCrop": {
                    "selector": "resource4[^>]*title=\"[^|]*\\|\\|[^:]*retim[^:]*: (-?\\d+)",
                    "type": "single"
                },
                "resourceFields": {
                    "selector": "resourceField\\s+gid(\\d+)\\s+buildingSlot(\\d+)[^\"]*level(\\d+)\"[^>]*data-aid=\"(\\d+)\"\\s+data-gid=\"\\d+\"[^>]*title=\"([^&<]+)",
                    "type": "list",
                    "fields": ["gid", "slotId", "level", "aid", "name"]
                },
                "constructionQueue": {
                    "selector": "<div class=\"name\">\\s*([^<]+?)\\s*<span class=\"lvl\">Seviye\\s*(\\d+)</span>[\\s\\S]*?<span\\s+class=\"timer\"[^>]*>(\\d+:\\d+:\\d+)</span>",
                    "type": "list",
                    "fields": ["buildingName", "level", "remainingTime"]
                },
                "troops": {
                    "selector": "unit (u\\w+)[^>]*alt=\"([^\"]+)\"[\\s\\S]*?<td class=\"num\">(\\d+)</td>[\\s\\S]*?<td class=\"un\">([^<]+)</td>",
                    "type": "list",
                    "fields": ["unitClass", "unitName", "count", "displayName"]
                }
            }
        },
        "dorf2": {
            "url": "/dorf2.php",
            "description": "Koy merkezi binalari",
            "fields": {
                "buildings": {
                    "selector": "buildingSlot\\s+a(\\d+)\\s+g(\\d+)[^\"]*\"[^>]*data-aid=\"(\\d+)\"\\s+data-gid=\"(\\d+)\"[^>]*data-name=\"([^\"]*)\"[^>]*>[^<]*<a[^>]*data-level=\"(\\d+)\"",
                    "type": "list",
                    "fields": ["slotId", "gidClass", "aid", "gid", "name", "level"]
                },
                "buildingLevels": {
                    "selector": "data-level=\"(\\d+)\"[^>]*><div class=\"labelLayer\">(\\d+)</div>",
                    "type": "list",
                    "fields": ["level", "label"]
                }
            }
        },
        "barracks": {
            "url": "/build.php",
            "description": "Kisla - asker egitimi",
            "fields": {
                "trainableTroops": {
                    "selector": "innerTroopWrapper\\s+troopt(\\d+)[^\"]*\"[^>]*data-troop(?:id|ID)=\"(t\\d+)\"[\\s\\S]*?alt=\"([^\"]+)\"",
                    "type": "list",
                    "fields": ["troopNum", "troopId", "name"]
                },
                "lockedTroops": {
                    "selector": "action troop troopt(\\d+) empty",
                    "type": "list",
                    "fields": ["troopNum"]
                }
            }
        },
        "stable": {
            "url": "/build.php",
            "description": "Ahir - atli asker egitimi",
            "fields": {
                "trainableTroops": {
                    "selector": "innerTroopWrapper\\s+troopt(\\d+)[^\"]*\"[^>]*data-troop(?:id|ID)=\"(t\\d+)\"[\\s\\S]*?alt=\"([^\"]+)\"",
                    "type": "list",
                    "fields": ["troopNum", "troopId", "name"]
                },
                "lockedTroops": {
                    "selector": "action troop troopt(\\d+) empty",
                    "type": "list",
                    "fields": ["troopNum"]
                }
            }
        },
        "workshop": {
            "url": "/build.php",
            "description": "Atokye - kusatma araclari",
            "fields": {
                "trainableTroops": {
                    "selector": "innerTroopWrapper\\s+troopt(\\d+)[^\"]*\"[^>]*data-troop(?:id|ID)=\"(t\\d+)\"[\\s\\S]*?alt=\"([^\"]+)\"",
                    "type": "list",
                    "fields": ["troopNum", "troopId", "name"]
                },
                "lockedTroops": {
                    "selector": "action troop troopt(\\d+) empty",
                    "type": "list",
                    "fields": ["troopNum"]
                }
            }
        }
    },
    "villageList": {
        "selector": "villageList\":\\s*\\[([^\\]]+)\\]",
        "itemSelector": "\\{[^}]*\"villageId\":(\\d+)[^}]*\"name\":\"([^\"]+)\"[^}]*\\}",
        "fields": ["villageId", "name"]
    }
})CONFIG";

  QJsonDocument doc = QJsonDocument::fromJson(QByteArray(configJson));

  if (doc.isNull() || !doc.isObject()) {
    return false;
  }

  m_config = doc.object();
  // baseUrl will be set from settings.ini via setBaseUrl()

  QJsonObject delay = m_config["requestDelay"].toObject();
  m_delayMin = delay["min"].toInt(500);
  m_delayMax = delay["max"].toInt(2000);

  return true;
}

bool TravianDataFetcher::loadCookies(const QString &cookiePath) {
  QFile file(cookiePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (doc.isNull()) {
    return false;
  }

  QJsonArray cookieArray = doc.object()["cookies"].toArray();
  QList<QNetworkCookie> cookies;

  for (const QJsonValue &val : cookieArray) {
    QJsonObject obj = val.toObject();
    QNetworkCookie cookie;
    cookie.setName(obj["name"].toString().toUtf8());
    cookie.setValue(obj["value"].toString().toUtf8());
    cookie.setDomain(obj["domain"].toString());
    cookie.setPath(obj["path"].toString("/"));
    cookies.append(cookie);
  }

  m_networkManager->cookieJar()->setCookiesFromUrl(cookies, QUrl(m_baseUrl));

  return true;
}

// ============================================================================
// Authentication - Cookie Management
// ============================================================================

bool TravianDataFetcher::tryLoadSavedCookies(const QString &cookiePath) {
  QFile file(cookiePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();

  if (doc.isNull() || !doc.isObject()) {
    return false;
  }

  QJsonObject root = doc.object();
  QJsonArray cookieArray = root["cookies"].toArray();

  if (cookieArray.isEmpty()) {
    return false;
  }

  // Expiry kontrolü
  qint64 expiry = root["expiry"].toVariant().toLongLong();
  qint64 now = QDateTime::currentSecsSinceEpoch();

  if (expiry > 0 && now >= expiry) {
    return false;
  }

  QList<QNetworkCookie> cookies;
  bool hasJwt = false;

  // m_baseUrl'den domain çıkar (örn: ts30.x3.europe.travian.com)
  QUrl baseUrlObj(m_baseUrl);
  QString targetDomain = baseUrlObj.host();

  for (const QJsonValue &val : cookieArray) {
    QJsonObject obj = val.toObject();
    QNetworkCookie cookie;
    cookie.setName(obj["name"].toString().toUtf8());
    cookie.setValue(obj["value"].toString().toUtf8());

    // Cookie domain'ini hedef sunucuya uygun hale getir
    QString savedDomain = obj["domain"].toString();
    if (savedDomain.contains("travian.com")) {
      // Domain'i mevcut sunucu için ayarla
      cookie.setDomain(targetDomain);
    } else {
      cookie.setDomain(savedDomain);
    }

    cookie.setPath(obj["path"].toString("/"));
    cookies.append(cookie);

    if (obj["name"].toString() == "JWT") {
      hasJwt = true;
    }
  }

  if (!hasJwt) {
    return false;
  }

  // Önce mevcut cookie'leri temizle
  QList<QNetworkCookie> existingCookies =
      m_networkManager->cookieJar()->cookiesForUrl(QUrl(m_baseUrl));
  for (const QNetworkCookie &c : existingCookies) {
    QNetworkCookie expiredCookie = c;
    expiredCookie.setExpirationDate(QDateTime::currentDateTime().addDays(-1));
    m_networkManager->cookieJar()->setCookiesFromUrl({expiredCookie},
                                                     QUrl(m_baseUrl));
  }

  // Yeni cookie'leri ekle
  m_networkManager->cookieJar()->setCookiesFromUrl(cookies, QUrl(m_baseUrl));

  return true;
}

void TravianDataFetcher::saveCookiesToFile(const QString &cookiePath) {
  QList<QNetworkCookie> cookies =
      m_networkManager->cookieJar()->cookiesForUrl(QUrl(m_baseUrl));

  if (cookies.isEmpty()) {
    return;
  }

  QJsonArray cookieArray;
  qint64 minExpiry = 0;

  for (const QNetworkCookie &c : cookies) {
    QJsonObject obj;
    obj["name"] = QString::fromUtf8(c.name());
    obj["value"] = QString::fromUtf8(c.value());
    obj["domain"] = c.domain();
    obj["path"] = c.path();

    // JWT için expiry'yi hesapla
    if (c.name() == "JWT") {
      // JWT genelde 24 saat geçerli, 20 saat olarak kaydedelim güvenli tarafta
      // olmak için
      minExpiry = QDateTime::currentSecsSinceEpoch() + (20 * 60 * 60);
    }

    cookieArray.append(obj);
  }

  QJsonObject root;
  root["cookies"] = cookieArray;
  root["expiry"] = minExpiry;
  root["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

  QFile file(cookiePath);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
  } else {
  }
}

// ============================================================================
// Authentication - Auto Login
// ============================================================================

void TravianDataFetcher::performLogin(const QString &username,
                                      const QString &password) {

  // Travian Legends modern API login endpoint
  QString loginUrl = m_baseUrl + "/api/v1/auth/login";

  // Create JSON payload
  QJsonObject payload;
  payload["name"] = username;
  payload["password"] = password;
  payload["w"] = "1440:900"; // Screen resolution
  payload["mobileOptimizations"] = false;

  QJsonDocument doc(payload);
  QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

  QNetworkRequest request;
  request.setUrl(QUrl(loginUrl));
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setRawHeader("User-Agent",
                       "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                       "AppleWebKit/537.36 (KHTML, like Gecko) "
                       "Chrome/120.0.0.0 Safari/537.36");
  request.setRawHeader("X-Requested-With", "XMLHttpRequest");
  request.setRawHeader("Accept-Encoding", "identity");

  QNetworkReply *reply = m_networkManager->post(request, jsonData);

  // Mark this as login request
  reply->setProperty("isLoginRequest", true);
  reply->setProperty("loginStep", "postLogin");

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onLoginFinished(reply); });
}

void TravianDataFetcher::onLoginFinished(QNetworkReply *reply) {
  QString loginStep = reply->property("loginStep").toString();

  if (reply->error() != QNetworkReply::NoError) {
    QString error = reply->errorString();
    emit loginFailed(error);
    reply->deleteLater();
    return;
  }

  QString response = QString::fromUtf8(reply->readAll());
  int statusCode =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  reply->deleteLater();

  if (loginStep == "postLogin") {

    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
      emit loginFailed("Invalid response from server");
      return;
    }

    QJsonObject responseObj = doc.object();

    // Check for errors in response
    if (responseObj.contains("error")) {
      QString errorMsg = responseObj["error"].toString();
      emit loginFailed(errorMsg);
      return;
    }

    // Check status code
    if (statusCode == 200) {

      // Check if there's a redirectTo URL (OAuth-like flow)
      if (responseObj.contains("redirectTo")) {
        QString redirectUrl = responseObj["redirectTo"].toString();

        QNetworkRequest redirectRequest;
        redirectRequest.setUrl(QUrl(m_baseUrl + redirectUrl));
        redirectRequest.setRawHeader(
            "User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                          "AppleWebKit/537.36 (KHTML, like Gecko) "
                          "Chrome/120.0.0.0 Safari/537.36");

        QNetworkReply *redirectReply = m_networkManager->get(redirectRequest);
        redirectReply->setProperty("isLoginRequest", true);
        redirectReply->setProperty("loginStep", "followRedirect");

        connect(redirectReply, &QNetworkReply::finished, this,
                [this, redirectReply]() { onLoginFinished(redirectReply); });
        return;
      }

      // Check if we got JWT cookie directly
      QList<QNetworkCookie> cookies =
          m_networkManager->cookieJar()->cookiesForUrl(QUrl(m_baseUrl));

      bool hasJwt = false;
      for (const QNetworkCookie &c : cookies) {
        if (c.name() == "JWT") {
          hasJwt = true;
        }
      }

      if (hasJwt) {

        // Save cookies to file for next time
        QString cookiePath =
            QCoreApplication::applicationDirPath() + "/cookies.json";
        saveCookiesToFile(cookiePath);

        emit loginSuccess();
        return;
      }
    } else {
      emit loginFailed("Login failed - status code: " +
                       QString::number(statusCode));
      return;
    }
  }

  if (loginStep == "followRedirect") {

    // Check if we got JWT cookie after redirect
    QList<QNetworkCookie> cookies =
        m_networkManager->cookieJar()->cookiesForUrl(QUrl(m_baseUrl));

    bool hasJwt = false;
    for (const QNetworkCookie &c : cookies) {
      if (c.name() == "JWT") {
        hasJwt = true;
      }
    }

    if (hasJwt) {

      // Save cookies to file for next time
      QString cookiePath =
          QCoreApplication::applicationDirPath() + "/cookies.json";
      saveCookiesToFile(cookiePath);

      emit loginSuccess();
      return;
    } else {
      // Try navigating to dorf1.php as final attempt

      QNetworkRequest serverRequest;
      serverRequest.setUrl(QUrl(m_baseUrl + "/dorf1.php"));
      serverRequest.setRawHeader(
          "User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                        "AppleWebKit/537.36 (KHTML, like Gecko) "
                        "Chrome/120.0.0.0 Safari/537.36");

      QNetworkReply *serverReply = m_networkManager->get(serverRequest);
      serverReply->setProperty("isLoginRequest", true);
      serverReply->setProperty("loginStep", "checkServer");

      connect(serverReply, &QNetworkReply::finished, this,
              [this, serverReply]() { onLoginFinished(serverReply); });
      return;
    }
  }

  if (loginStep == "checkServer") {

    // JWT cookie var mı kontrol et
    QList<QNetworkCookie> cookies =
        m_networkManager->cookieJar()->cookiesForUrl(QUrl(m_baseUrl));

    bool hasJwt = false;
    for (const QNetworkCookie &c : cookies) {
      if (c.name() == "JWT") {
        hasJwt = true;
      }
    }

    // Sayfa içeriğinde login formu var mı (giriş yapılamamış demek)
    if (response.contains("loginForm") || response.contains("name=\"login\"")) {
      emit loginFailed("Login failed - credentials may be incorrect");
      return;
    }

    // Village bilgisi var mı kontrol et (giriş yapılmış demek)
    if (response.contains("villageList") || response.contains("dorf1")) {

      // Save cookies to file for next time
      QString cookiePath =
          QCoreApplication::applicationDirPath() + "/cookies.json";
      saveCookiesToFile(cookiePath);

      emit loginSuccess();
      return;
    }

    if (hasJwt) {

      // Save cookies to file for next time
      QString cookiePath =
          QCoreApplication::applicationDirPath() + "/cookies.json";
      saveCookiesToFile(cookiePath);

      emit loginSuccess();
      return;
    }

    emit loginFailed("Could not verify login status");
  }
}

// ============================================================================
// Helper Methods
// ============================================================================

int TravianDataFetcher::getRandomDelay() const {
  return QRandomGenerator::global()->bounded(m_delayMin, m_delayMax + 1);
}

QString TravianDataFetcher::getRandomUserAgent() const {
  if (m_userAgents.isEmpty()) {
    return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
           "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
  }
  int index = QRandomGenerator::global()->bounded(m_userAgents.size());
  return m_userAgents[index];
}

QString TravianDataFetcher::buildVillageUrl(const QString &pageUrl,
                                            int villageId) const {
  if (villageId <= 0) {
    return m_baseUrl + pageUrl;
  }

  QString separator = pageUrl.contains("?") ? "&" : "?";
  return m_baseUrl + pageUrl + separator +
         "newdid=" + QString::number(villageId);
}

void TravianDataFetcher::enqueuePageRequests(int villageId,
                                             const QString &villageName) {
  QJsonObject pages = m_config["pages"].toObject();

  for (auto it = pages.begin(); it != pages.end(); ++it) {
    QString pageName = it.key();
    QJsonObject page = it.value().toObject();

    PendingRequest req;
    req.pageName = pageName;
    req.villageId = villageId;
    req.villageName = villageName;
    req.isVillageListRequest = false;
    req.pageConfig = page;
    req.url = buildVillageUrl(page["url"].toString(), villageId);

    m_requestQueue.enqueue(req);
  }
}

void TravianDataFetcher::storeVillageData(int villageId,
                                          const QString &villageName,
                                          const QString &pageName,
                                          const QVariantMap &data) {
  QString villageKey = "village_" + QString::number(villageId);
  QVariantMap villageData = m_collectedData.value(villageKey).toMap();

  villageData[pageName] = data;
  villageData["villageName"] = villageName;
  villageData["villageId"] = villageId;

  m_collectedData[villageKey] = villageData;

  // Update village list as well
  for (int i = 0; i < m_villages.size(); ++i) {
    if (m_villages[i].id == villageId) {
      m_villages[i].data = villageData;
      break;
    }
  }
}

void TravianDataFetcher::logPageData(const QString &pageName,
                                     const QVariantMap &data) {
  for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
    if (it.value().typeId() == QMetaType::QVariantList) {
    } else if (it.value().typeId() == QMetaType::QVariantMap) {
    } else {
    }
  }
}

// ============================================================================
// Data Fetching - Public Methods
// ============================================================================

void TravianDataFetcher::fetchAllVillagesData() {
  m_villages.clear();
  m_currentVillageIndex = 0;
  m_collectedData.clear();
  m_requestQueue.clear();

  // First, fetch village list from dorf1.php
  PendingRequest req;
  req.pageName = "_villageList";
  req.url = m_baseUrl + "/dorf1.php";
  req.isVillageListRequest = true;
  req.villageId = -1;

  m_totalRequests = 1;
  m_completedRequests = 0;
  m_requestQueue.enqueue(req);

  processNextRequest();
}

void TravianDataFetcher::fetchVillageData(int villageId,
                                          const QString &villageName) {
  enqueuePageRequests(villageId, villageName);
}

void TravianDataFetcher::fetchAllData() {
  QJsonObject pages = m_config["pages"].toObject();

  m_totalRequests = pages.size();
  m_completedRequests = 0;
  m_requestQueue.clear();

  for (auto it = pages.begin(); it != pages.end(); ++it) {
    QString pageName = it.key();
    QJsonObject page = it.value().toObject();

    PendingRequest req;
    req.pageName = pageName;
    req.url = m_baseUrl + page["url"].toString();
    req.pageConfig = page;
    req.villageId = -1;
    req.isVillageListRequest = false;

    m_requestQueue.enqueue(req);
  }

  processNextRequest();
}

void TravianDataFetcher::fetchPage(const QString &pageName, int villageId) {
  QJsonObject pages = m_config["pages"].toObject();

  if (!pages.contains(pageName)) {
    return;
  }

  QJsonObject page = pages[pageName].toObject();

  m_totalRequests = 1;
  m_completedRequests = 0;
  m_requestQueue.clear();

  PendingRequest req;
  req.pageName = pageName;
  req.pageConfig = page;
  req.villageId = villageId;
  req.isVillageListRequest = false;
  req.url = buildVillageUrl(page["url"].toString(), villageId);

  m_requestQueue.enqueue(req);
  processNextRequest();
}

QVariantMap TravianDataFetcher::getVillageData(int villageId) const {
  QString key = "village_" + QString::number(villageId);
  return m_collectedData.value(key).toMap();
}

// ============================================================================
// Building Upgrade
// ============================================================================

void TravianDataFetcher::upgradeBuilding(int villageId, int slotId) {

  // Önce bina sayfasını aç (upgrade butonunu ve gerekli parametreleri almak
  // için)
  QString buildUrl = m_baseUrl + "/build.php?id=" + QString::number(slotId);
  if (villageId > 0) {
    buildUrl += "&newdid=" + QString::number(villageId);
  }

  QNetworkRequest request;
  request.setUrl(QUrl(buildUrl));
  request.setRawHeader("User-Agent",
                       "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                       "AppleWebKit/537.36 (KHTML, like Gecko) "
                       "Chrome/120.0.0.0 Safari/537.36");
  request.setRawHeader("Accept-Encoding", "identity");

  QNetworkReply *reply = m_networkManager->get(request);
  reply->setProperty("isUpgradeRequest", true);
  reply->setProperty("upgradeStep", "getBuildPage");
  reply->setProperty("villageId", villageId);
  reply->setProperty("slotId", slotId);

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onUpgradeFinished(reply); });
}

void TravianDataFetcher::onUpgradeFinished(QNetworkReply *reply) {
  QString upgradeStep = reply->property("upgradeStep").toString();
  int villageId = reply->property("villageId").toInt();
  int slotId = reply->property("slotId").toInt();

  if (reply->error() != QNetworkReply::NoError) {
    QString error = reply->errorString();
    emit upgradeFailed(villageId, slotId, error);
    reply->deleteLater();
    return;
  }

  QByteArray rawData = reply->readAll();
  rawData = decompressGzip(rawData);
  QString response = QString::fromUtf8(rawData);
  int statusCode =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  refreshCookiesFromResponse(reply);
  reply->deleteLater();

  if (upgradeStep == "getBuildPage") {
    // Bina adını bul
    QRegularExpression nameRegex(
        R"(<h1[^>]*class=\"titleInHeader\"[^>]*>([^<]+)</h1>)");
    QRegularExpressionMatch nameMatch = nameRegex.match(response);
    QString buildingName =
        nameMatch.hasMatch() ? nameMatch.captured(1).trimmed() : "Bina";

    // Upgrade linkini bul - yeni format: onclick içinde window.location.href
    // Format: onclick="this.disabled = true; window.location.href =
    // '/dorf2.php?id=34&amp;gid=19&amp;action=build&amp;checksum=74dc4a';
    // return false;"
    QRegularExpression upgradeRegex(
        R"(class=\"[^\"]*green[^\"]*build[^\"]*\"[^>]*onclick=\"[^\"]*window\.location\.href\s*=\s*'([^']+)')");
    QRegularExpressionMatch match = upgradeRegex.match(response);

    if (!match.hasMatch()) {
      // Fallback 1: Eski format - build.php?id=X&a=X&c=CHECKSUM
      QRegularExpression oldRegex(
          R"(build\.php\?id=(\d+)[^\"]*&amp;a=\d+[^\"]*&amp;c=([a-f0-9]+))");
      match = oldRegex.match(response);

      if (!match.hasMatch()) {
        // Fallback 2: href attribute
        QRegularExpression hrefRegex(
            R"(href=\"(/build\.php\?id=\d+[^\"]*a=\d+[^\"]*c=[a-f0-9]+)\")");
        match = hrefRegex.match(response);
      }
    }

    if (!match.hasMatch()) {
      emit upgradeFailed(
          villageId, slotId,
          "Yükseltme linki bulunamadı - yeterli kaynak yok olabilir");
      return;
    }

    QString upgradeUrl;
    QString capturedUrl = match.captured(1);

    // If captured URL starts with /, it's relative
    if (capturedUrl.startsWith("/")) {
      upgradeUrl = m_baseUrl + capturedUrl.replace("&amp;", "&");
    } else if (capturedUrl.contains("build.php") ||
               capturedUrl.contains("dorf2.php")) {
      // Full or partial URL captured
      upgradeUrl = m_baseUrl + "/" + capturedUrl.replace("&amp;", "&");
    } else {
      // Fallback: construct URL manually (shouldn't happen with new regex)
      upgradeUrl = m_baseUrl + "/build.php?id=" + QString::number(slotId);
    }

    // Köy ID'sini upgrade URL'sine ekle (farklı köy için upgrade yapabilmek
    // için)
    if (villageId > 0 && !upgradeUrl.contains("newdid")) {
      upgradeUrl += "&newdid=" + QString::number(villageId);
    }

    qDebug() << "[UPGRADE] villageId:" << villageId << "slotId:" << slotId
             << "buildingName:" << buildingName << "upgradeUrl:" << upgradeUrl;

    // Upgrade isteği gönder
    QNetworkRequest request;
    request.setUrl(QUrl(upgradeUrl));

    // Oturum boyunca sabit UA (gerçek tarayıcı gibi)
    request.setRawHeader("User-Agent", m_sessionUserAgent.toUtf8());

    QString refererUrl = m_baseUrl + "/build.php?id=" + QString::number(slotId);
    if (villageId > 0) {
      refererUrl += "&newdid=" + QString::number(villageId);
    }
    request.setRawHeader("Referer", refererUrl.toUtf8());

    QNetworkReply *upgradeReply = m_networkManager->get(request);
    upgradeReply->setProperty("isUpgradeRequest", true);
    upgradeReply->setProperty("upgradeStep", "doUpgrade");
    upgradeReply->setProperty("villageId", villageId);
    upgradeReply->setProperty("slotId", slotId);
    upgradeReply->setProperty("buildingName", buildingName);

    connect(upgradeReply, &QNetworkReply::finished, this,
            [this, upgradeReply]() { onUpgradeFinished(upgradeReply); });
    return;
  }

  if (upgradeStep == "doUpgrade") {
    QString buildingName = reply->property("buildingName").toString();

    // Başarı kontrolü - inşaat kuyruğuna eklenmiş mi?
    if (response.contains("buildingList") ||
        response.contains("constructionQueue") ||
        response.contains("buildDuration") ||
        response.contains("class=\"timer\"")) {
      emit upgradeStarted(villageId, slotId, buildingName);
    } else if (response.contains("notEnough") ||
               response.contains("enough resources")) {
      emit upgradeFailed(villageId, slotId, "Yeterli kaynak yok");
    } else {
      // Genelde başarılı olduğunda dorf1 veya build sayfasına yönlendirir
      emit upgradeStarted(villageId, slotId, buildingName);
    }
  }
}

// ============================================================================
// Troop Training Implementation
// ============================================================================

void TravianDataFetcher::trainTroops(int villageId, int slotId,
                                     const QString &troopId,
                                     const QString &troopName) {
  qDebug() << "[TROOP] trainTroops called - village:" << villageId
           << "slot:" << slotId << "troop:" << troopId << "name:" << troopName;

  // Step 1: Fetch the barracks/stable/workshop page
  QString buildUrl = m_baseUrl + "/build.php?id=" + QString::number(slotId);
  if (villageId > 0) {
    buildUrl += "&newdid=" + QString::number(villageId);
  }

  QNetworkRequest request;
  request.setUrl(QUrl(buildUrl));
  request.setRawHeader("User-Agent", m_sessionUserAgent.toUtf8());
  request.setRawHeader("Accept-Encoding", "identity");

  QNetworkReply *reply = m_networkManager->get(request);
  reply->setProperty("isTrainRequest", true);
  reply->setProperty("trainStep", "getPage");
  reply->setProperty("villageId", villageId);
  reply->setProperty("slotId", slotId);
  reply->setProperty("troopId", troopId);
  reply->setProperty("troopName", troopName);

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onTrainTroopFinished(reply); });
}

void TravianDataFetcher::onTrainTroopFinished(QNetworkReply *reply) {
  QString trainStep = reply->property("trainStep").toString();
  int villageId = reply->property("villageId").toInt();
  int slotId = reply->property("slotId").toInt();
  QString troopId = reply->property("troopId").toString();

  if (reply->error() != QNetworkReply::NoError) {
    QString error = reply->errorString();
    qWarning() << "[TROOP] Network error:" << error;
    emit troopTrainingResult(villageId, false, troopId, 0,
                             "Ağ hatası: " + error);
    reply->deleteLater();
    return;
  }

  QByteArray rawData = reply->readAll();
  rawData = decompressGzip(rawData);
  QString response = QString::fromUtf8(rawData);

  refreshCookiesFromResponse(reply);
  reply->deleteLater();

  if (trainStep == "getPage") {
    qDebug() << "[TROOP] Got building page, parsing for troop:" << troopId;

    // Save debug HTML
    QFile debugFile("/Users/kekinci/Desktop/test/config/debug_train_page.html");
    if (debugFile.open(QIODevice::WriteOnly)) {
      debugFile.write(response.toUtf8());
      debugFile.close();
    }

    // Extract troop number from troopId (e.g., "t1" -> "1", "t3" -> "3")
    // Also handle global IDs (e.g. "u11" -> "t1", "u21" -> "t1")
    QString inputName = troopId;
    if (troopId.startsWith("u")) {
      int gid = troopId.mid(1).toInt();
      int rid = (gid - 1) % 10 + 1;
      inputName = "t" + QString::number(rid);
      qDebug() << "[TROOP] Converted global ID" << troopId
               << "to relative input" << inputName;
    } else if (troopId.startsWith("t")) {
      // Already relative or old format
      inputName = troopId;
    }

    // Use troop name from config (passed via property), fallback to troopId
    QString troopName = reply->property("troopName").toString();
    if (troopName.isEmpty()) {
      troopName = troopId;
    }
    qDebug() << "[TROOP] Using troop name:" << troopName;

    // Find the max trainable count for this troop
    // HTML structure inside <div class="cta">:
    //   <input name="t1" value="0" />
    //   <span> / </span>
    //   <a href="#" onclick="...val(11)...">11</a>
    // The input and max link are very close together, within ~200 chars
    int maxCount = 0;

    // Pattern 1: Find name="t1" input, then the very next .val(N) within same
    // cta block Use [\s\S] instead of . to match across newlines, with negative
    // lookahead to stay in same section
    QRegularExpression maxRegex1(
        QString(
            R"~~(<input[^>]*name="%1"[^>]*/?>(?:(?!<input)[\s\S]){0,300}\.val\((\d+)\))~~")
            .arg(inputName));
    QRegularExpressionMatch maxMatch = maxRegex1.match(response);

    if (maxMatch.hasMatch()) {
      maxCount = maxMatch.captured(1).toInt();
      qDebug() << "[TROOP] Found max count via val() after input:" << maxCount;
    }

    if (maxCount <= 0) {
      // Pattern 2: Find name="t1" input, then the next <a>NUMBER</a> within 300
      // chars
      QRegularExpression maxRegex2(
          QString(
              R"~~(<input[^>]*name="%1"[^>]*/?>(?:(?!<input)[\s\S]){0,300}<a[^>]*>(\d+)</a>)~~")
              .arg(inputName));
      QRegularExpressionMatch maxMatch2 = maxRegex2.match(response);
      if (maxMatch2.hasMatch()) {
        maxCount = maxMatch2.captured(1).toInt();
        qDebug() << "[TROOP] Found max count via link text after input:"
                 << maxCount;
      }
    }

    if (maxCount <= 0) {
      qDebug() << "[TROOP] No troops available to train for" << troopName
               << "at village" << villageId << "- resources insufficient or building busy";
      // Don't emit error - this is expected when resources are low
      // Timer will try again on next interval
      return;
    }

    // Find the form action URL (method may be before or after action)
    QRegularExpression formRegex(R"~~(<form[^>]*action="([^"]+)"[^>]*>)~~");
    QRegularExpressionMatch formMatch = formRegex.match(response);

    QString formAction;
    if (formMatch.hasMatch()) {
      formAction = formMatch.captured(1).replace("&amp;", "&");
      if (formAction.startsWith("/")) {
        formAction = m_baseUrl + formAction;
      }
      // CRITICAL: Add newdid to form action to ensure POST is processed in correct village
      if (!formAction.contains("newdid=") && villageId > 0) {
        formAction += (formAction.contains("?") ? "&" : "?");
        formAction += "newdid=" + QString::number(villageId);
      }
      qDebug() << "[TROOP] Form action (with newdid):" << formAction;
    } else {
      // Fallback: use build.php with newdid
      formAction = m_baseUrl + "/build.php?id=" + QString::number(slotId);
      if (villageId > 0) {
        formAction += "&newdid=" + QString::number(villageId);
      }
      qDebug() << "[TROOP] Using fallback form action:" << formAction;
    }

    // Collect hidden inputs
    QRegularExpression hiddenRegex(
        R"~~(<input[^>]*type="hidden"[^>]*name="([^"]+)"[^>]*value="([^"]*)")~~");
    QRegularExpressionMatchIterator hiddenIt =
        hiddenRegex.globalMatch(response);

    QUrlQuery postData;
    while (hiddenIt.hasNext()) {
      QRegularExpressionMatch m = hiddenIt.next();
      postData.addQueryItem(m.captured(1), m.captured(2));
      qDebug() << "[TROOP] Hidden input:" << m.captured(1) << "="
               << m.captured(2);
    }

    // Also try reversed attribute order: value before name
    QRegularExpression hiddenRegex2(
        R"~~(<input[^>]*type="hidden"[^>]*value="([^"]*)"[^>]*name="([^"]+)")~~");
    QRegularExpressionMatchIterator hiddenIt2 =
        hiddenRegex2.globalMatch(response);
    while (hiddenIt2.hasNext()) {
      QRegularExpressionMatch m = hiddenIt2.next();
      QString name = m.captured(2);
      QString value = m.captured(1);
      if (!postData.hasQueryItem(name)) {
        postData.addQueryItem(name, value);
        qDebug() << "[TROOP] Hidden input (rev):" << name << "=" << value;
      }
    }

    // Set the troop count - HTML input name is "t1", "t2", etc. (same as
    // inputName)
    postData.addQueryItem(inputName, QString::number(maxCount));

    // Add submit button value (s1=ok) as required by the form
    postData.addQueryItem("s1", "ok");

    qDebug() << "[TROOP] Training" << maxCount << "of" << troopName << "("
             << troopId << "=" << maxCount << ")";
    qDebug() << "[TROOP] POST data:" << postData.toString(QUrl::FullyEncoded);

    // Submit the form via POST
    QNetworkRequest postRequest;
    postRequest.setUrl(QUrl(formAction));
    postRequest.setRawHeader("User-Agent", m_sessionUserAgent.toUtf8());
    postRequest.setRawHeader("Content-Type",
                             "application/x-www-form-urlencoded");
    postRequest.setRawHeader(
        "Referer",
        (m_baseUrl + "/build.php?id=" + QString::number(slotId)).toUtf8());

    QNetworkReply *postReply = m_networkManager->post(
        postRequest, postData.toString(QUrl::FullyEncoded).toUtf8());
    postReply->setProperty("isTrainRequest", true);
    postReply->setProperty("trainStep", "doTrain");
    postReply->setProperty("villageId", villageId);
    postReply->setProperty("slotId", slotId);
    postReply->setProperty("troopId", troopId);
    postReply->setProperty("troopName", troopName);
    postReply->setProperty("trainCount", maxCount);

    connect(postReply, &QNetworkReply::finished, this,
            [this, postReply]() { onTrainTroopFinished(postReply); });
    return;
  }

  if (trainStep == "doTrain") {
    QString troopName = reply->property("troopName").toString();
    int trainCount = reply->property("trainCount").toInt();

    qDebug() << "[TROOP] Training POST response received for" << troopName
             << "at village" << villageId;

    // Save response for debugging
    QFile responseFile(QString("/Users/kekinci/Desktop/test/config/debug_train_response_%1.html").arg(villageId));
    if (responseFile.open(QIODevice::WriteOnly)) {
      responseFile.write(response.toUtf8());
      responseFile.close();
    }

    // Check for success indicators
    if (response.contains("buildingList") ||
        response.contains("under_progress") || response.contains("timer") ||
        response.contains("dur_r")) {
      qInfo() << "[TROOP] Training started:" << trainCount << "x" << troopName
              << "at village" << villageId;
      emit troopTrainingResult(
          villageId, true, troopName, trainCount,
          QString("%1x %2 eğitim başlatıldı").arg(trainCount).arg(troopName));
    } else if (response.contains("notEnough") ||
               response.contains("enough resources")) {
      qWarning() << "[TROOP] Not enough resources for" << troopName
                 << "at village" << villageId;
      emit troopTrainingResult(villageId, false, troopName, 0,
                               "Yeterli kaynak yok");
    } else {
      // Usually successful - Travian redirects after training
      qInfo() << "[TROOP] Training likely started:" << trainCount << "x"
              << troopName << "at village" << villageId;
      emit troopTrainingResult(
          villageId, true, troopName, trainCount,
          QString("%1x %2 eğitim başlatıldı").arg(trainCount).arg(troopName));
    }
  }
}

// ============================================================================
// Request Queue Processing
// ============================================================================

void TravianDataFetcher::processNextRequest() {
  if (m_requestQueue.isEmpty()) {
    m_isProcessing = false;

    // If there are more villages to process
    if (!m_villages.isEmpty() && m_currentVillageIndex < m_villages.size()) {
      VillageInfo &village = m_villages[m_currentVillageIndex];

      fetchVillageData(village.id, village.name);
      m_currentVillageIndex++;

      m_totalRequests += m_requestQueue.size();
      processNextRequest();
      return;
    }

    // All done
    if (!m_villages.isEmpty()) {
    }

    emit allDataFetched(m_collectedData);
    return;
  }

  if (m_isProcessing) {
    return;
  }

  m_isProcessing = true;
  PendingRequest req = m_requestQueue.dequeue();
  m_currentPageName = req.pageName;

  int delay = getRandomDelay();
  QString displayName = req.villageName.isEmpty()
                            ? req.pageName
                            : req.villageName + "/" + req.pageName;

  QTimer::singleShot(delay, this, [this, req]() {
    QNetworkRequest request;
    request.setUrl(QUrl(req.url));

    // Oturum boyunca sabit UA (gerçek tarayıcı gibi)
    request.setRawHeader("User-Agent", m_sessionUserAgent.toUtf8());

    // Anti-bot: Add referer header (simulate browser navigation)
    if (!m_lastReferer.isEmpty()) {
      request.setRawHeader("Referer", m_lastReferer.toUtf8());
    }

    // Standard browser headers
    request.setRawHeader(
        "Accept",
        "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    request.setRawHeader("Accept-Language",
                         "tr-TR,tr;q=0.9,en-US;q=0.8,en;q=0.7");
    // Explicitly request no compression to avoid gzip decompression issues
    request.setRawHeader("Accept-Encoding", "identity");
    request.setRawHeader("DNT", "1");
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Upgrade-Insecure-Requests", "1");

    // Update last referer for next request
    const_cast<TravianDataFetcher *>(this)->m_lastReferer = req.url;

    QNetworkReply *reply = m_networkManager->get(request);

    // Store request info in reply for later retrieval
    reply->setProperty("pageConfig", QVariant::fromValue(req.pageConfig));
    reply->setProperty("pageName", req.pageName);
    reply->setProperty("villageId", req.villageId);
    reply->setProperty("villageName", req.villageName);
    reply->setProperty("isVillageListRequest", req.isVillageListRequest);
  });
}

// ============================================================================
// Response Handling
// ============================================================================

void TravianDataFetcher::onRequestFinished(QNetworkReply *reply) {
  // Login, upgrade, train, farm, session check - skip, handled by their own
  // slots
  if (reply->property("isLoginRequest").toBool() ||
      reply->property("isUpgradeRequest").toBool() ||
      reply->property("isTrainRequest").toBool() ||
      reply->property("isFarmListRequest").toBool() ||
      reply->property("isSessionCheck").toBool()) {
    return;
  }

  m_isProcessing = false;

  QString pageName = reply->property("pageName").toString();
  bool isVillageListRequest = reply->property("isVillageListRequest").toBool();

  // --- Fix 5: Network error retry with exponential backoff ---
  if (reply->error() != QNetworkReply::NoError) {
    if (shouldRetryNetworkError(reply->error()) &&
        m_currentRetryCount < MAX_REQUEST_RETRIES) {
      m_currentRetryCount++;
      int delayMs =
          2000 * (1 << (m_currentRetryCount - 1)); // 2s, 4s, 8s backoff
      qWarning() << "[FETCH] Network error:" << reply->errorString()
                 << "- retry" << m_currentRetryCount << "/"
                 << MAX_REQUEST_RETRIES << "in" << delayMs << "ms";

      // Capture request info for retry
      QUrl retryUrl = reply->url();
      QJsonObject pageConfig = reply->property("pageConfig").toJsonObject();
      int villageId = reply->property("villageId").toInt();
      QString villageName = reply->property("villageName").toString();

      reply->deleteLater();

      QTimer::singleShot(delayMs, this,
                         [this, pageName, retryUrl, pageConfig, villageId,
                          villageName, isVillageListRequest]() {
                           PendingRequest retryReq;
                           retryReq.pageName = pageName;
                           retryReq.pageConfig = pageConfig;
                           retryReq.villageId = villageId;
                           retryReq.villageName = villageName;
                           retryReq.isVillageListRequest =
                               isVillageListRequest;
                           retryReq.url = retryUrl.toString();
                           m_requestQueue.prepend(retryReq);
                           processNextRequest();
                         });
      return;
    }

    // Exhausted retries or non-retryable error
    m_currentRetryCount = 0;
    m_consecutiveNetworkErrors++;
    if (m_consecutiveNetworkErrors >= MAX_CONSECUTIVE_ERRORS_BEFORE_RESET) {
      resetNetworkManager();
    }
    emit fetchError(pageName, reply->errorString());
    reply->deleteLater();
    processNextRequest();
    return;
  }

  // Success - reset retry/error counters
  m_currentRetryCount = 0;
  m_consecutiveNetworkErrors = 0;

  // Refresh cookies from response headers
  refreshCookiesFromResponse(reply);

  QByteArray rawData = reply->readAll();
  rawData = decompressGzip(rawData);
  QString html = QString::fromUtf8(rawData);

  // Extract request info before deleting reply
  QJsonObject pageConfig = reply->property("pageConfig").toJsonObject();
  int villageId = reply->property("villageId").toInt();
  QString villageName = reply->property("villageName").toString();

  reply->deleteLater();

  m_completedRequests++;
  emit fetchProgress(m_completedRequests, m_totalRequests, pageName);

  if (isVillageListRequest) {
    handleVillageListResponse(html);
  } else {
    PendingRequest req;
    req.pageName = pageName;
    req.pageConfig = pageConfig;
    req.villageId = villageId;
    req.villageName = villageName;
    handlePageResponse(html, req);
  }

  processNextRequest();
}

void TravianDataFetcher::handleVillageListResponse(const QString &html) {
  // ✅ village list response'u kaydet
  // HTML logging disabled

  m_villages = VillageParser::parseVillageList(html);

  for (const VillageInfo &v : m_villages) {
  }

  emit villagesDiscovered(m_villages);

  if (m_villages.isEmpty()) {
    emit fetchError("_villageList",
                    "Session expired - no villages found (401)");
    return;
  }

  // Extract resources data for first village from this HTML
  QJsonObject pages = m_config["pages"].toObject();

  if (pages.contains("dorf1")) {
    QJsonObject page = pages["dorf1"].toObject();
    QVariantMap pageData = HtmlParser::parsePageData(html, page);
    storeVillageData(m_villages[0].id, m_villages[0].name, "dorf1", pageData);
  }

  // Queue dorf2.php request for first village
  if (pages.contains("dorf2")) {
    QJsonObject page = pages["dorf2"].toObject();

    PendingRequest req;
    req.pageName = "dorf2";
    req.url = m_baseUrl + "/dorf2.php";
    req.pageConfig = page;
    req.villageId = m_villages[0].id;
    req.villageName = m_villages[0].name;
    req.isVillageListRequest = false;

    m_requestQueue.enqueue(req);
    m_totalRequests++;
  }

  m_currentVillageIndex = 1; // First village already processed
}

void TravianDataFetcher::handlePageResponse(const QString &html,
                                            const PendingRequest &req) {
  QString displayName = req.villageName.isEmpty()
                            ? req.pageName
                            : req.villageName + "/" + req.pageName;

  // ✅ her HTML response'u program path'inde html_responses klasörüne kaydet
  // HTML logging disabled

  QVariantMap pageData = HtmlParser::parsePageData(html, req.pageConfig);
  QJsonObject embedded = HtmlParser::extractEmbeddedJson(html);
  if (!embedded.isEmpty()) {
    pageData["embeddedJson"] = embedded.toVariantMap();
  }

  // Extract village list with attack information (only from dorf1/dorf2 pages to avoid redundant parsing)
  if (req.pageName == "dorf1" || req.pageName == "dorf2") {
    QVariantList villageListWithAttacks = HtmlParser::extractVillageListWithAttacks(html);
    if (!villageListWithAttacks.isEmpty()) {
      m_collectedData["villageListWithAttacks"] = villageListWithAttacks;
    }
  }

  if (req.villageId > 0) {
    storeVillageData(req.villageId, req.villageName, req.pageName, pageData);
    emit villageDataUpdated(
        req.villageId, req.villageName,
        m_collectedData.value("village_" + QString::number(req.villageId))
            .toMap());

    // buildings sayfasından sonra askeri binaları kontrol et ve onlar için de
    // request ekle
    if (req.pageName == "dorf2") {
      enqueueMilitaryBuildingRequests(req.villageId, req.villageName, pageData);

      // After dorf2 is saved, check for attacks and create attack summary from existing data
      QVariantList villageListWithAttacks = m_collectedData["villageListWithAttacks"].toList();
      for (const QVariant &villageVar : villageListWithAttacks) {
        QVariantMap villageMap = villageVar.toMap();
        if (villageMap["id"].toInt() == req.villageId) {
          int attacksAmount = villageMap["incomingAttacksAmount"].toInt();
          if (attacksAmount > 0) {
            qDebug() << "[ATTACK] Village" << req.villageId << "has" << attacksAmount << "incoming attacks - creating attack summary from existing data";

            // Create attack entries from the symbols data we already have
            QVariantList attacks;
            QVariantMap symbols = villageMap["incomingAttacksSymbols"].toMap();

            int redCount = symbols["red"].toInt();
            int yellowCount = symbols["yellow"].toInt();
            int greenCount = symbols["green"].toInt();
            int grayCount = symbols["gray"].toInt();

            // Create placeholder attacks (we don't have exact timing from this data)
            // But we can show the user that attacks are coming
            for (int i = 0; i < redCount; i++) {
              QVariantMap attack;
              attack["type"] = "attack";
              attack["symbol"] = "red";
              attack["displayName"] = "Normal Saldırı";
              attack["remainingSeconds"] = 0; // Unknown
              attack["arrivalDateTime"] = "Bilinmiyor";
              attacks.append(attack);
            }
            for (int i = 0; i < yellowCount; i++) {
              QVariantMap attack;
              attack["type"] = "raid";
              attack["symbol"] = "yellow";
              attack["displayName"] = "Yağma";
              attack["remainingSeconds"] = 0;
              attack["arrivalDateTime"] = "Bilinmiyor";
              attacks.append(attack);
            }
            for (int i = 0; i < greenCount; i++) {
              QVariantMap attack;
              attack["type"] = "support";
              attack["symbol"] = "green";
              attack["displayName"] = "Destek";
              attack["remainingSeconds"] = 0;
              attack["arrivalDateTime"] = "Bilinmiyor";
              attacks.append(attack);
            }
            for (int i = 0; i < grayCount; i++) {
              QVariantMap attack;
              attack["type"] = "other";
              attack["symbol"] = "gray";
              attack["displayName"] = "Diğer";
              attack["remainingSeconds"] = 0;
              attack["arrivalDateTime"] = "Bilinmiyor";
              attacks.append(attack);
            }

            if (!attacks.isEmpty()) {
              qDebug() << "[ATTACK] ✅ Emitting" << attacks.size() << "attacks for village" << req.villageId;
              for (const QVariant &attack : attacks) {
                qDebug() << "[ATTACK]   -" << attack.toMap()["displayName"].toString()
                         << "(" << attack.toMap()["type"].toString() << ")";
              }
              emit incomingAttacksFetched(req.villageId, attacks);
            } else {
              qDebug() << "[ATTACK] ⚠️ No attacks created from symbols data (all counts were 0)";
            }

            // Fetch detailed timing from rally point HTML
            //             fetchIncomingAttacks(req.villageId);
          }
          break;
        }
      }
    }
  } else {
    m_collectedData[req.pageName] = pageData;
    emit dataUpdated(req.pageName, pageData);
  }

  logPageData(req.pageName, pageData);
}

void TravianDataFetcher::enqueueMilitaryBuildingRequests(
    int villageId, const QString &villageName,
    const QVariantMap &buildingsData) {
  // Askeri bina GID'leri: 19=Kışla, 20=Ahır, 21=Atölye
  QMap<int, QString> militaryBuildings;
  militaryBuildings[19] = "barracks";
  militaryBuildings[20] = "stable";
  militaryBuildings[21] = "workshop";

  QVariantList buildings = buildingsData["buildings"].toList();
  QJsonObject pages = m_config["pages"].toObject();

  for (const QVariant &building : buildings) {
    QVariantMap b = building.toMap();
    int gid = b["gid"].toInt();
    int slotId = b["slotId"].toInt();

    if (!militaryBuildings.contains(gid)) {
      continue;
    }

    QString pageName = militaryBuildings[gid];

    // Bu sayfa için config bul
    if (!pages.contains(pageName)) {
      continue;
    }

    QJsonObject page = pages[pageName].toObject();

    PendingRequest req;
    req.pageName = pageName;
    req.pageConfig = page;
    req.villageId = villageId;
    req.villageName = villageName;
    req.isVillageListRequest = false;

    // URL'i slot ID ile oluştur
    QString url = "/build.php?id=" + QString::number(slotId);
    if (villageId > 0) {
      url += "&newdid=" + QString::number(villageId);
    }
    req.url = m_baseUrl + url;

    m_requestQueue.enqueue(req);
    m_totalRequests++;
  }
}
// ============================================================================
// Farm List Implementation
// ============================================================================

void TravianDataFetcher::fetchFarmLists(int villageId) {
  qDebug() << "[FARM] fetchFarmLists called for villageId:" << villageId;

  // Find rally point (gid=16) slot ID from village data
  QString villageKey = "village_" + QString::number(villageId);
  QVariantMap villageData = m_collectedData.value(villageKey).toMap();
  QVariantMap dorf2 = villageData["dorf2"].toMap();
  QVariantList buildings = dorf2["buildings"].toList();

  int rallyPointSlotId = -1;
  for (const QVariant &building : buildings) {
    QVariantMap b = building.toMap();
    int gid = b["gid"].toInt();
    if (gid == 16) {
      rallyPointSlotId = b["slotId"].toInt();
      break;
    }
  }

  if (rallyPointSlotId == -1) {
    qDebug() << "[FARM] Rally point not found in village" << villageId;
    emit farmListsFetched(villageId, QVariantList());
    return;
  }

  // Fetch the farm list tab (tt=99)
  QString farmUrl = m_baseUrl +
                    "/build.php?id=" + QString::number(rallyPointSlotId) +
                    "&tt=99";
  if (villageId > 0) {
    farmUrl += "&newdid=" + QString::number(villageId);
  }

  qDebug() << "[FARM] Fetching farm list page:" << farmUrl;

  QNetworkRequest request;
  request.setUrl(QUrl(farmUrl));
  request.setRawHeader("User-Agent", m_sessionUserAgent.toUtf8());
  request.setRawHeader("Accept-Encoding", "identity");

  QNetworkReply *reply = m_networkManager->get(request);
  reply->setProperty("isFarmListRequest", true);
  reply->setProperty("farmStep", "fetchLists");
  reply->setProperty("villageId", villageId);
  reply->setProperty("rallyPointSlotId", rallyPointSlotId);

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onFarmListFinished(reply); });
}

void TravianDataFetcher::executeFarmList(int villageId, int listId) {
  qDebug() << "[FARM] executeFarmList called - village:" << villageId
           << "listId:" << listId;

  // Initialize retry info if needed
  QString retryKey = QString("%1_%2").arg(villageId).arg(listId);
  if (!m_farmRetries.contains(retryKey)) {
    FarmRetryInfo info;
    info.villageId = villageId;
    info.listId = listId;
    info.retryCount = 0;
    m_farmRetries[retryKey] = info;
  }

  // Step 1: Fetch farm list page to get active slot IDs
  QString fetchUrl =
      m_baseUrl + "/build.php?id=39&tt=99&newdid=" + QString::number(villageId);

  QNetworkRequest request;
  request.setUrl(QUrl(fetchUrl));
  request.setRawHeader("User-Agent", m_sessionUserAgent.toUtf8());
  request.setRawHeader("Accept-Encoding", "identity");

  QNetworkReply *reply = m_networkManager->get(request);
  reply->setProperty("isFarmListRequest", true);
  reply->setProperty("farmStep", "executeFetchSlots");
  reply->setProperty("villageId", villageId);
  reply->setProperty("listId", listId);

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onFarmListFinished(reply); });
}

void TravianDataFetcher::sendFarmListPost(int villageId, int listId,
                                          const QJsonArray &slotIds) {
  // Step 2: POST to /api/v1/farm-list/send with slot IDs
  QString apiUrl = m_baseUrl + "/api/v1/farm-list/send";

  QJsonObject listObj;
  listObj["id"] = listId;
  listObj["targets"] = slotIds;

  QJsonArray listsArray;
  listsArray.append(listObj);

  QJsonObject requestBody;
  requestBody["action"] = QString("farmList");
  requestBody["lists"] = listsArray;

  QByteArray jsonData =
      QJsonDocument(requestBody).toJson(QJsonDocument::Compact);

  qDebug() << "[FARM] POST" << apiUrl << "payload:" << jsonData;

  QNetworkRequest apiRequest;
  apiRequest.setUrl(QUrl(apiUrl));
  apiRequest.setRawHeader("User-Agent", m_sessionUserAgent.toUtf8());
  apiRequest.setRawHeader("Content-Type", "application/json");
  apiRequest.setRawHeader("Accept", "application/json");
  apiRequest.setRawHeader("Accept-Encoding", "identity");
  apiRequest.setRawHeader("X-Requested-With", "XMLHttpRequest");
  apiRequest.setRawHeader(
      "Referer", (m_baseUrl +
                  "/build.php?id=39&tt=99&newdid=" + QString::number(villageId))
                     .toUtf8());
  apiRequest.setRawHeader("Origin", m_baseUrl.toUtf8());

  QNetworkReply *reply = m_networkManager->post(apiRequest, jsonData);
  reply->setProperty("isFarmListRequest", true);
  reply->setProperty("farmStep", "executePost");
  reply->setProperty("villageId", villageId);
  reply->setProperty("listId", listId);

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onFarmListFinished(reply); });
}

void TravianDataFetcher::onFarmListFinished(QNetworkReply *reply) {
  QString farmStep = reply->property("farmStep").toString();
  int villageId = reply->property("villageId").toInt();

  qDebug() << "[FARM] onFarmListFinished - step:" << farmStep
           << "villageId:" << villageId;

  if (reply->error() != QNetworkReply::NoError) {
    QString error = reply->errorString();
    int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray errorBody = reply->readAll();
    qWarning() << "[FARM] Network error:" << error << "status:" << statusCode
               << "body:" << QString::fromUtf8(errorBody).left(500);

    // Retry logic for HTTP errors (400, 500, etc.)
    if (farmStep == "executePost" || farmStep == "executeFetchSlots") {
      int listId = reply->property("listId").toInt();
      QString retryKey = QString("%1_%2").arg(villageId).arg(listId);
      FarmRetryInfo &retryInfo = m_farmRetries[retryKey];

      // Retry on 400, 500, or network errors
      if ((statusCode >= 400 || statusCode == 0) && retryInfo.retryCount < MAX_FARM_RETRIES) {
        retryInfo.retryCount++;
        qWarning() << "[FARM] HTTP error" << statusCode << "for list" << listId
                   << "- retry" << retryInfo.retryCount << "/" << MAX_FARM_RETRIES
                   << "in" << FARM_RETRY_DELAY_MS << "ms";

        reply->deleteLater();
        QTimer::singleShot(FARM_RETRY_DELAY_MS, this, [this, villageId, listId]() {
          executeFarmList(villageId, listId);
        });
        return;
      }

      // Max retries reached - report error
      if (farmStep == "executePost") {
        qWarning() << "[FARM] HTTP error" << statusCode << "for list" << listId
                   << "after" << MAX_FARM_RETRIES << "retries - giving up";
        emit farmListExecuted(villageId, listId, false,
                            QString("HTTP %1: %2 (%3 deneme)")
                                .arg(statusCode)
                                .arg(QString::fromUtf8(errorBody).left(100))
                                .arg(MAX_FARM_RETRIES));
        retryInfo.retryCount = 0;
      }
    }
    reply->deleteLater();
    return;
  }

  // Extract all reply properties BEFORE deleteLater (Bonus fix)
  int listId = reply->property("listId").toInt();
  int httpStatusCode =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  QByteArray rawData = reply->readAll();
  rawData = decompressGzip(rawData);
  QString response = QString::fromUtf8(rawData);

  // Refresh cookies from response headers before deleting reply
  refreshCookiesFromResponse(reply);
  reply->deleteLater();

  // Save debug HTML
  QFile debugFile("/Users/kekinci/Desktop/test/config/debug_farm_page.html");
  if (debugFile.open(QIODevice::WriteOnly)) {
    debugFile.write(response.toUtf8());
    debugFile.close();
  }

  // Check for login/session expiration
  if (response.contains("class=\"login\"") ||
      response.contains("id=\"loginScene\"") ||
      response.contains("GİRİŞ</a>") ||
      (!response.contains("farmLists") && farmStep != "executePost")) {
    qWarning() << "[FARM] Session expired - login page detected"
               << "(response:" << response.size() << "bytes)";

    // Trigger auto-login
    emit sessionExpiredAutoLogin();

    // Retry with session refresh for farm execution
    if (farmStep == "executeFetchSlots") {
      QString retryKey = QString("%1_%2").arg(villageId).arg(listId);
      FarmRetryInfo &retryInfo = m_farmRetries[retryKey];

      if (retryInfo.retryCount < MAX_FARM_RETRIES) {
        retryInfo.retryCount++;
        int retryDelay = FARM_RETRY_DELAY_MS * 2; // 4 seconds for session issues
        qWarning() << "[FARM] Session expired for list" << listId
                   << "- retry" << retryInfo.retryCount << "/" << MAX_FARM_RETRIES
                   << "in" << retryDelay << "ms (longer delay for session issue)";

        QTimer::singleShot(retryDelay, this, [this, villageId, listId]() {
          executeFarmList(villageId, listId);
        });
        return;
      } else {
        qCritical() << "[FARM] Session expired for list" << listId
                    << "after" << MAX_FARM_RETRIES << "retries - CRITICAL: Cookies may need refresh!";
        emit farmListExecuted(villageId, listId, false,
                            "Oturum süresi doldu - cookie güncellemesi gerekli");
        retryInfo.retryCount = 0;
        return;
      }
    }

    // For fetchLists, just report error
    if (farmStep == "fetchLists") {
      emit farmListsFetched(villageId, QVariantList());
      return;
    }
  }

  if (farmStep == "fetchLists") {
    // Travian Legends uses React for farm lists - data is in viewData JSON
    // Parse: "farmLists":[{"id":1691,"name":"Offline","slotsAmount":5,...},...]
    QVariantList lists;

    // Extract the farmLists JSON array from the viewData
    QRegularExpression farmListsRegex(R"~~("farmLists"\s*:\s*\[)~~");
    QRegularExpressionMatch flMatch = farmListsRegex.match(response);

    if (flMatch.hasMatch()) {
      // Find the start of the array
      int arrayStart = flMatch.capturedEnd() - 1; // position of '['

      // Parse individual farm list objects from the JSON
      // Each list: {"id":1691,"name":"Offline","slotsAmount":5,...}
      QRegularExpression listItemRegex(
          R"~~(\{"id"\s*:\s*(\d+)\s*,\s*"name"\s*:\s*"([^"]*)")~~");
      // Also capture slotsAmount
      QRegularExpression slotsRegex(R"~~("slotsAmount"\s*:\s*(\d+))~~");
      QRegularExpression ownerRegex(
          R"~~("ownerVillage"\s*:\s*\{"id"\s*:\s*(\d+))~~");

      // Extract the farmLists portion of the JSON
      // Find matching bracket end
      int depth = 0;
      int pos = arrayStart;
      int arrayEnd = response.length();
      for (int i = pos; i < response.length(); i++) {
        QChar c = response[i];
        if (c == '[')
          depth++;
        else if (c == ']') {
          depth--;
          if (depth == 0) {
            arrayEnd = i + 1;
            break;
          }
        }
      }

      QString farmListsStr = response.mid(arrayStart, arrayEnd - arrayStart);
      qDebug() << "[FARM] farmLists JSON length:" << farmListsStr.length();

      // Parse each farm list entry
      QRegularExpressionMatchIterator it =
          listItemRegex.globalMatch(farmListsStr);
      while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        int listId = m.captured(1).toInt();
        QString listName = m.captured(2);

        QVariantMap listInfo;
        listInfo["id"] = listId;
        listInfo["name"] = listName;

        // Find slotsAmount after this list's id
        int searchStart = m.capturedEnd();
        // Look for slotsAmount and ownerVillage within the next 500 chars
        QString context = farmListsStr.mid(
            searchStart, qMin(500, farmListsStr.length() - searchStart));
        QRegularExpressionMatch slotsMatch = slotsRegex.match(context);
        if (slotsMatch.hasMatch()) {
          listInfo["slotsAmount"] = slotsMatch.captured(1).toInt();
        }

        // Find ownerVillage id
        QRegularExpressionMatch ownerMatch = ownerRegex.match(context);
        if (ownerMatch.hasMatch()) {
          listInfo["ownerVillageId"] = ownerMatch.captured(1).toInt();
        }

        lists.append(listInfo);
        qDebug() << "[FARM] Found farm list:" << listInfo;
      }
    } else {
      qWarning() << "[FARM] Could not find farmLists JSON in response";
    }

    qDebug() << "[FARM] Total farm lists found:" << lists.size();
    emit farmListsFetched(villageId, lists);
  }

  else if (farmStep == "executeFetchSlots") {
    // Step 1 response: Parse farm list page to get slot IDs for target list
    // listId already extracted before deleteLater

    qDebug() << "[FARM] Parsing farm list page for slot IDs - listId:" << listId
             << "response length:" << response.length();

    // Find slotsStates for the target farm list
    // Format in viewData:
    // "id":1691,...,"slotsStates":[{"id":56722,"isActive":true},...] We need to
    // find the slotsStates array for our specific list ID
    // Use regex for whitespace-tolerant matching: "id": 1691 or "id":1691
    QRegularExpression farmListsStartRegex(R"~~("farmLists"\s*:\s*\[)~~");
    QRegularExpressionMatch flStartMatch = farmListsStartRegex.match(response);
    int searchStartPos = flStartMatch.hasMatch() ? flStartMatch.capturedStart() : 0;

    QRegularExpression listIdRegex(
        QString(R"~~("id"\s*:\s*%1\b)~~").arg(listId));
    QRegularExpressionMatch listIdMatch = listIdRegex.match(response, searchStartPos);
    int listPos = listIdMatch.hasMatch() ? listIdMatch.capturedStart() : -1;

    qDebug() << "[FARM] List ID pattern found at position:" << listPos
             << "(searchStart:" << searchStartPos << ")";

    QJsonArray activeSlotIds;

    if (listPos >= 0) {
      // Find slotsStates after this list's position
      QString slotsStatesKey = "\"slotsStates\"";
      int slotsPos = response.indexOf(slotsStatesKey, listPos);

      qDebug() << "[FARM] slotsStates found at position:" << slotsPos;

      // Make sure we didn't overshoot to the next list
      int nextListPos = response.indexOf("\"farmLists\"", listPos + 10);
      if (nextListPos < 0)
        nextListPos = response.length();

      qDebug() << "[FARM] Next list position:" << nextListPos
               << "slotsPos valid:" << (slotsPos >= 0 && slotsPos < nextListPos);

      if (slotsPos >= 0 && slotsPos < nextListPos) {
        // Find the array start
        int arrayStart = response.indexOf('[', slotsPos);
        if (arrayStart >= 0) {
          // Find matching bracket end
          int depth = 0;
          int arrayEnd = arrayStart;
          for (int i = arrayStart; i < response.length(); i++) {
            QChar c = response[i];
            if (c == '[')
              depth++;
            else if (c == ']') {
              depth--;
              if (depth == 0) {
                arrayEnd = i + 1;
                break;
              }
            }
          }

          QString slotsJson = response.mid(arrayStart, arrayEnd - arrayStart);
          qDebug() << "[FARM] Extracted slotsStates JSON (first 200 chars):"
                   << slotsJson.left(200);

          QJsonDocument slotsDoc = QJsonDocument::fromJson(slotsJson.toUtf8());
          QJsonArray slotsArray = slotsDoc.array();

          qDebug() << "[FARM] Parsed slots array - total slots:" << slotsArray.size();

          for (const QJsonValue &slot : slotsArray) {
            QJsonObject slotObj = slot.toObject();
            int slotId = slotObj["id"].toInt();
            bool isActive = slotObj["isActive"].toBool();
            qDebug() << "[FARM] Slot" << slotId << "isActive:" << isActive;

            if (isActive) {
              activeSlotIds.append(slotId);
            }
          }
        } else {
          qWarning() << "[FARM] Could not find array start after slotsStates";
        }
      } else {
        qWarning() << "[FARM] slotsStates not found or out of range";
      }
    } else {
      qWarning() << "[FARM] List ID" << listId << "not found in response";
    }

    qDebug() << "[FARM] Found" << activeSlotIds.size()
             << "active slots for list" << listId;

    if (activeSlotIds.isEmpty()) {
      // Retry logic for empty slots
      QString retryKey = QString("%1_%2").arg(villageId).arg(listId);
      FarmRetryInfo &retryInfo = m_farmRetries[retryKey];

      if (retryInfo.retryCount < MAX_FARM_RETRIES) {
        retryInfo.retryCount++;
        qWarning() << "[FARM] No active slots found for list" << listId
                   << "- retry" << retryInfo.retryCount << "/" << MAX_FARM_RETRIES
                   << "in" << FARM_RETRY_DELAY_MS << "ms";

        QTimer::singleShot(FARM_RETRY_DELAY_MS, this, [this, villageId, listId]() {
          executeFarmList(villageId, listId);
        });
        return;
      } else {
        // Max retries reached
        qWarning() << "[FARM] No active slots found for list" << listId
                   << "after" << MAX_FARM_RETRIES << "retries - giving up";
        emit farmListExecuted(villageId, listId, false,
                            "Aktif slot bulunamadı (" + QString::number(MAX_FARM_RETRIES) + " deneme)");

        // Reset retry count for next execution
        retryInfo.retryCount = 0;
        return;
      }
    }

    // Success - reset retry count
    QString retryKey = QString("%1_%2").arg(villageId).arg(listId);
    if (m_farmRetries.contains(retryKey)) {
      m_farmRetries[retryKey].retryCount = 0;
    }

    // Proceed to Step 2: Send the farm list
    sendFarmListPost(villageId, listId, activeSlotIds);
  }

  else if (farmStep == "executePost") {
    // listId and httpStatusCode already extracted before deleteLater
    int statusCode = httpStatusCode;

    qDebug() << "[FARM] Farm list execution response for list:" << listId
             << "status:" << statusCode;
    qDebug() << "[FARM] Response:" << response.left(1000);

    // Parse response - Travian.api returns JSON
    QJsonDocument jsonDoc = QJsonDocument::fromJson(response.toUtf8());
    QJsonObject jsonObj = jsonDoc.object();

    // Reset retry count on any response (success or error)
    QString retryKey = QString("%1_%2").arg(villageId).arg(listId);
    if (m_farmRetries.contains(retryKey)) {
      m_farmRetries[retryKey].retryCount = 0;
    }

    // Check for errors
    if (jsonObj.contains("errors") && !jsonObj["errors"].isNull()) {
      QString errorMsg;
      if (jsonObj["errors"].isArray()) {
        QJsonArray errors = jsonObj["errors"].toArray();
        errorMsg = errors.first().toObject()["message"].toString();
      } else {
        errorMsg = jsonObj["errors"].toString();
      }
      qWarning() << "[FARM] Send error:" << errorMsg;
      emit farmListExecuted(villageId, listId, false, "Hata: " + errorMsg);
    }
    // Check for success
    else if (statusCode == 200) {
      qInfo() << "[FARM] Farm list" << listId << "started successfully!";
      emit farmListExecuted(villageId, listId, true,
                            "Yağma listesi başlatıldı");
    } else {
      emit farmListExecuted(villageId, listId, false,
                            "HTTP " + QString::number(statusCode) + ": " +
                                response.left(200));
    }
  }
}

// ============================================================================
// Incoming Attacks Implementation
// ============================================================================

void TravianDataFetcher::fetchIncomingAttacks(int villageId) {
  qDebug() << "[ATTACK] fetchIncomingAttacks called for villageId:" << villageId;

  // Find rally point (gid=16) slot ID from village data
  QString villageKey = "village_" + QString::number(villageId);
  QVariantMap villageData = m_collectedData.value(villageKey).toMap();
  QVariantMap dorf2 = villageData["dorf2"].toMap();
  QVariantList buildings = dorf2["buildings"].toList();

  int rallyPointSlotId = -1;
  for (const QVariant &building : buildings) {
    QVariantMap b = building.toMap();
    int gid = b["gid"].toInt();
    if (gid == 16) {
      rallyPointSlotId = b["slotId"].toInt();
      break;
    }
  }

  if (rallyPointSlotId == -1) {
    qDebug() << "[ATTACK] ❌ Rally point not found in village" << villageId;
    qDebug() << "[ATTACK] Available buildings in dorf2:";
    for (const QVariant &building : buildings) {
      QVariantMap b = building.toMap();
      qDebug() << "[ATTACK]   - gid:" << b["gid"].toInt() << "slotId:" << b["slotId"].toInt() << "name:" << b["name"].toString();
    }
    emit incomingAttacksFetched(villageId, QVariantList());
    return;
  }

  qDebug() << "[ATTACK] ✅ Found rally point at slotId:" << rallyPointSlotId;

  // Fetch the rally point incoming movements tab
  // IMPORTANT: Travian Legends requires both gid=16 and tt=1 parameters!
  QString attackUrl = m_baseUrl +
                    "/build.php?id=" + QString::number(rallyPointSlotId) +
                    "&gid=16&tt=1";
  if (villageId > 0) {
    attackUrl += "&newdid=" + QString::number(villageId);
  }

  qDebug() << "[ATTACK] Fetching rally point with gid+tt:" << attackUrl;

  QNetworkRequest request;
  request.setUrl(QUrl(attackUrl));
  request.setRawHeader("User-Agent", m_sessionUserAgent.toUtf8());
  request.setRawHeader("Accept-Encoding", "identity");

  QNetworkReply *reply = m_networkManager->get(request);
  reply->setProperty("isAttackRequest", true);
  reply->setProperty("villageId", villageId);
  reply->setProperty("rallyPointSlotId", rallyPointSlotId);

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onIncomingAttacksFinished(reply); });
}

void TravianDataFetcher::onIncomingAttacksFinished(QNetworkReply *reply) {
  int villageId = reply->property("villageId").toInt();

  if (reply->error() != QNetworkReply::NoError) {
    qWarning() << "[ATTACK] Network error:" << reply->errorString();
    emit incomingAttacksFetched(villageId, QVariantList());
    reply->deleteLater();
    return;
  }

  QByteArray rawData = reply->readAll();
  rawData = decompressGzip(rawData);
  QString response = QString::fromUtf8(rawData);

  qDebug() << "[ATTACK] Response size:" << response.length() << "bytes";
  qDebug() << "[ATTACK] First 200 chars:" << response.left(200);

  // Refresh cookies
  refreshCookiesFromResponse(reply);
  reply->deleteLater();

  // Save debug HTML
  QFile debugFile("/Users/kekinci/Desktop/test/config/debug_attacks_page.html");
  if (debugFile.open(QIODevice::WriteOnly)) {
    debugFile.write(response.toUtf8());
    debugFile.close();
    qDebug() << "[ATTACK] Saved debug HTML to:" << debugFile.fileName();
  }

  // Check for login/session expiration
  if (response.contains("class=\"login\"") ||
      response.contains("id=\"loginScene\"") ||
      response.contains("GİRİŞ</a>")) {
    qWarning() << "[ATTACK] Session expired - login page detected";

    // Trigger auto-login
    emit sessionExpiredAutoLogin();

    emit incomingAttacksFetched(villageId, QVariantList());
    return;
  }

  // Parse incoming movements using HTML parser (same approach as farm lists)
  QVariantList movements = HtmlParser::extractIncomingMovements(response);

  QVariantList attacks;

  if (!movements.isEmpty()) {
    qDebug() << "[ATTACK] Found" << movements.size() << "incoming movements";

    for (const QVariant& movementVar : movements) {
      QVariantMap mov = movementVar.toMap();

      QVariantMap attack;
      attack["id"] = mov["id"].toInt();

      // Determine attack type from movementType or attackType
      int movementType = mov["movementType"].toInt();
      int attackType = mov["attackType"].toInt();

      QString typeStr = "other";
      QString displayName = "Diğer";

      // Map Travian attack types
      if (attackType == 3 || movementType == 3) {
        typeStr = "attack";
        displayName = "Normal Saldırı";
      } else if (attackType == 4 || movementType == 4) {
        typeStr = "raid";
        displayName = "Yağma";
      } else if (attackType == 2 || movementType == 2) {
        typeStr = "reinforcement";
        displayName = "Destek";
      }

      attack["type"] = typeStr;
      attack["displayName"] = displayName;

      // Extract timing information
      if (mov.contains("arrivalTime")) {
        qint64 arrivalTimestamp = mov["arrivalTime"].toLongLong();
        QDateTime arrivalTime = QDateTime::fromSecsSinceEpoch(arrivalTimestamp);
        attack["arrivalDateTime"] = arrivalTime.toString("yyyy-MM-dd HH:mm:ss");

        qint64 now = QDateTime::currentSecsSinceEpoch();
        qint64 remaining = arrivalTimestamp - now;
        attack["remainingSeconds"] = static_cast<int>(remaining > 0 ? remaining : 0);

        qDebug() << "[ATTACK]   -" << displayName << "arrives in" << remaining << "seconds at" << attack["arrivalDateTime"].toString();
      } else if (mov.contains("remainingSeconds")) {
        int remaining = mov["remainingSeconds"].toInt();
        attack["remainingSeconds"] = remaining;

        QDateTime arrivalTime = QDateTime::currentDateTime().addSecs(remaining);
        attack["arrivalDateTime"] = arrivalTime.toString("yyyy-MM-dd HH:mm:ss");

        qDebug() << "[ATTACK]   -" << displayName << "arrives in" << remaining << "seconds";
      }

      // Extract source info
      if (mov.contains("fromVillageName")) {
        attack["fromVillageName"] = mov["fromVillageName"].toString();
      }
      if (mov.contains("fromPlayerName")) {
        attack["fromPlayerName"] = mov["fromPlayerName"].toString();
      }

      attacks.append(attack);
    }
  } else {
    qDebug() << "[ATTACK] No movement data found in rally point HTML";
  }

  qDebug() << "[ATTACK] Parsed" << attacks.size() << "incoming attacks for village" << villageId;
  emit incomingAttacksFetched(villageId, attacks);
}
