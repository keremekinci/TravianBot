#include "Travianrequestmanager.h"
#include <QRegularExpression>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslError>

TravianRequestManager::TravianRequestManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_delayTimer(new QTimer(this))
    , m_isLoginRequest(false)
{
    // Network manager için finished signal'ını bağla
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &TravianRequestManager::onRequestFinished);

    // SSL hatalarını ignore et (sadece development için)
    connect(m_networkManager, &QNetworkAccessManager::sslErrors,
            [](QNetworkReply *reply, const QList<QSslError> &errors) {
                for (const QSslError &error : errors) {
                }
                reply->ignoreSslErrors();
            });

    // Timer single shot olarak ayarla
    m_delayTimer->setSingleShot(true);
    connect(m_delayTimer, &QTimer::timeout,
            this, &TravianRequestManager::onDelayFinished);
}

TravianRequestManager::~TravianRequestManager()
{
}

int TravianRequestManager::getRandomDelay() const
{
    // 100-300ms arası random bir değer üret
    return QRandomGenerator::global()->bounded(100, 301);
}

void TravianRequestManager::makeRequest(const QString &url)
{
    m_pendingUrl = url;

    // Random delay başlat
    int delayMs = getRandomDelay();

    m_delayTimer->start(delayMs);
}

void TravianRequestManager::onDelayFinished()
{
    executeRequest();
}

void TravianRequestManager::executeRequest()
{
    QNetworkRequest request;
    request.setUrl(QUrl(m_pendingUrl));

    // Gerçek bir tarayıcı gibi görünmek için headers ekle
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                         "AppleWebKit/537.36 (KHTML, like Gecko) "
                         "Chrome/120.0.0.0 Safari/537.36");

    request.setRawHeader("Accept",
                         "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");

    request.setRawHeader("Accept-Language", "tr-TR,tr;q=0.9,en-US;q=0.8,en;q=0.7");
    // Accept-Encoding'i kaldırıyoruz - Qt otomatik handle etsin
    request.setRawHeader("Connection", "keep-alive");
    request.setRawHeader("Cache-Control", "max-age=0");

    // Otomatik decompression için
    request.setAttribute(QNetworkRequest::AutoDeleteReplyOnFinishAttribute, false);

    // GET request gönder
    m_networkManager->get(request);
}

void TravianRequestManager::onRequestFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray rawData = reply->readAll();
        QString response;

        // Gzip sıkıştırması varsa decompress et
        QByteArray decompressed;
        if (rawData.size() > 2 &&
            (unsigned char)rawData[0] == 0x1f &&
            (unsigned char)rawData[1] == 0x8b) {
            // Gzip formatı tespit edildi

            decompressed = qUncompress(rawData);

            if (decompressed.isEmpty()) {
                response = QString::fromUtf8(rawData);
            } else {
                response = QString::fromUtf8(decompressed);
            }
        } else {
            // Sıkıştırma yok, direkt kullan
            response = QString::fromUtf8(rawData);
        }


        // Debug: HTML'i dosyaya kaydet
        QFile debugFile("travian_response.html");
        if (debugFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&debugFile);
            out << response;
            debugFile.close();
        }

        // Eğer login request ise
        if (m_isLoginRequest) {
            checkLoginResponse(response);
            m_isLoginRequest = false;
        }

        emit requestCompleted(response);

        // Eğer dorf1.php ise köy ismini parse et
        if (m_pendingUrl.contains("dorf1.php")) {
            QString villageName = parseVillageName(response);
            if (!villageName.isEmpty()) {
                emit villageNameReceived(villageName);
            }
        }
    } else {
        QString errorString = reply->errorString();
        emit requestFailed(errorString);

        if (m_isLoginRequest) {
            emit loginFailed("Network error: " + errorString);
            m_isLoginRequest = false;
        }
    }

    reply->deleteLater();
}

QString TravianRequestManager::parseVillageName(const QString &html) const
{
    // Debug: HTML'in bir kısmını yazdır

    // Travian'da köy ismi genellikle şu formatta olur:
    // <div class="villageName">Köy İsmi</div>
    // veya
    // <span class="villageName">Köy İsmi</span>

    // 1. Deneme: villageName input value (yeni Travian formatı)
    QRegularExpression regex("villageName\"[^>]*value=\"([^\"]+)\"");
    QRegularExpressionMatch match = regex.match(html);

    if (match.hasMatch()) {
        QString villageName = match.captured(1).trimmed();
        return villageName;
    }

    // 2. Deneme: villageName class içeriği
    QRegularExpression regex2("<[^>]*class=\"villageName\"[^>]*>([^<]+)</[^>]*>");
    match = regex2.match(html);
    if (match.hasMatch()) {
        QString villageName = match.captured(1).trimmed();
        return villageName;
    }

    // 3. Deneme: dorf1.php'de header veya title alanı
    QRegularExpression regex3("<h1[^>]*>([^<]+)</h1>");
    match = regex3.match(html);
    if (match.hasMatch()) {
        QString villageName = match.captured(1).trimmed();
        return villageName;
    }

    // 4. Deneme: title attribute'unda
    QRegularExpression titleRegex("title=\"([^\"]+)\"");
    QRegularExpressionMatchIterator it = titleRegex.globalMatch(html);

    int count = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch titleMatch = it.next();
        QString title = titleMatch.captured(1);
        count++;

        if (count < 10 && title.length() > 3 && title.length() < 50) {
            // İlk birkaç title'ı logla
        }
    }

    return QString();
}

void TravianRequestManager::fetchVillageName()
{
    QString url = "https://ts30.x3.international.travian.com/dorf1.php";
    m_isLoginRequest = false;
    makeRequest(url);
}

void TravianRequestManager::login(const QString &email, const QString &password)
{

    m_isLoginRequest = true;

    // Travian login URL'i - yeni endpoint (2024'te değişti)
    // Önce lobby'de login yapıp, sonra game server'a geçiş yapmalıyız
    QString loginUrl = "https://lobby.legends.travian.com/api/v1/auth/login";

    // JSON POST data hazırla
    QJsonObject loginData;
    loginData["email"] = email;
    loginData["password"] = password;

    QJsonDocument doc(loginData);
    QByteArray postData = doc.toJson();

    QNetworkRequest request;
    request.setUrl(QUrl(loginUrl));

    // SSL hatalarını yoksay (sadece test için)
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);

    // Headers
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                         "AppleWebKit/537.36 (KHTML, like Gecko) "
                         "Chrome/120.0.0.0 Safari/537.36");

    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Accept-Language", "tr-TR,tr;q=0.9,en-US;q=0.8,en;q=0.7");
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("Origin", "https://lobby.legends.travian.com");
    request.setRawHeader("Referer", "https://lobby.legends.travian.com/");
    request.setRawHeader("Connection", "keep-alive");

    // Random delay sonrası POST request gönder
    int delayMs = getRandomDelay();

    QTimer::singleShot(delayMs, this, [this, request, postData]() mutable {
        m_networkManager->post(request, postData);
    });
}

void TravianRequestManager::setCookie(const QString &name, const QString &value, const QString &domain)
{
    QNetworkCookie cookie;
    cookie.setName(name.toUtf8());
    cookie.setValue(value.toUtf8());
    cookie.setDomain(domain);
    cookie.setPath("/");

    QList<QNetworkCookie> cookies;
    cookies.append(cookie);

    m_networkManager->cookieJar()->setCookiesFromUrl(
        cookies,
        QUrl("https://ts30.x3.international.travian.com"));

}

void TravianRequestManager::clearCookies()
{
    if (m_networkManager->cookieJar()) {
        QList<QNetworkCookie> cookies = m_networkManager->cookieJar()->cookiesForUrl(
            QUrl("https://ts30.x3.international.travian.com"));

        for (const QNetworkCookie &cookie : cookies) {
        }
    }

    m_networkManager->setCookieJar(new QNetworkCookieJar(m_networkManager));
}

bool TravianRequestManager::saveCookiesToFile(const QString &filename)
{
    QList<QNetworkCookie> cookies = m_networkManager->cookieJar()->cookiesForUrl(
        QUrl("https://ts30.x3.international.travian.com"));

    if (cookies.isEmpty()) {
        return false;
    }

    QJsonArray cookieArray;
    for (const QNetworkCookie &cookie : cookies) {
        QJsonObject cookieObj;
        cookieObj["name"] = QString::fromUtf8(cookie.name());
        cookieObj["value"] = QString::fromUtf8(cookie.value());
        cookieObj["domain"] = cookie.domain();
        cookieObj["path"] = cookie.path();
        cookieArray.append(cookieObj);

    }

    QJsonObject root;
    root["cookies"] = cookieArray;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(QJsonDocument(root).toJson());
    file.close();

    return true;
}

bool TravianRequestManager::loadCookiesFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        return false;
    }

    QJsonArray cookieArray = doc.object()["cookies"].toArray();

    for (const QJsonValue &val : cookieArray) {
        QJsonObject obj = val.toObject();
        setCookie(
            obj["name"].toString(),
            obj["value"].toString(),
            obj["domain"].toString()
            );
    }

    return true;
}

void TravianRequestManager::checkLoginResponse(const QString &html)
{
    // API JSON response olabilir, kontrol et
    QJsonDocument doc = QJsonDocument::fromJson(html.toUtf8());

    if (!doc.isNull() && doc.isObject()) {
        // JSON response
        QJsonObject obj = doc.object();


        // Başarılı login kontrolü
        if (obj.contains("token") || obj.contains("msid") || obj.contains("success")) {

            // Token varsa cookie olarak kaydet
            if (obj.contains("token")) {
                QString token = obj["token"].toString();
                setCookie("token", token, ".travian.com");
            }

            if (obj.contains("msid")) {
                QString msid = obj["msid"].toString();
                setCookie("msid", msid, ".travian.com");
            }

            // Cookie'leri kaydet
            saveCookiesToFile();

            emit loginSuccess();
            return;
        } else if (obj.contains("error") || obj.contains("errors")) {
            QString errorMsg = obj["error"].toString();
            if (errorMsg.isEmpty() && obj.contains("errors")) {
                errorMsg = "Login hatası";
            }

            emit loginFailed(errorMsg);
            return;
        }
    }

    // HTML response kontrolü
    if (html.contains("dorf1.php") ||
        html.contains("logout") ||
        html.contains("Logout") ||
        html.contains("class=\"village\"")) {


        // Cookie'leri kaydet
        saveCookiesToFile();

        emit loginSuccess();
    } else if (html.contains("error") ||
               html.contains("wrong") ||
               html.contains("incorrect") ||
               (html.contains("Login") && html.contains("Register"))) {

        emit loginFailed("Hatalı kullanıcı adı veya şifre");
    } else {
        emit loginFailed("Login sonucu belirsiz");
    }
}
