#ifndef TRAVIANREQUESTMANAGER_H
#define TRAVIANREQUESTMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QTimer>
#include <QRandomGenerator>
#include <QString>
#include <QUrl>

class TravianRequestManager : public QObject
{
    Q_OBJECT

public:
    explicit TravianRequestManager(QObject *parent = nullptr);
    ~TravianRequestManager();

    // Ana request fonksiyonu - random delay ile
    void makeRequest(const QString &url);

    // Köy ismini çekme
    void fetchVillageName();

    // Login fonksiyonu
    void login(const QString &email, const QString &password);

    // Cookie ekle
    void setCookie(const QString &name, const QString &value, const QString &domain = ".travian.com");

    // Cookie'leri temizle
    void clearCookies();

    // Cookie'leri dosyadan yükle
    bool loadCookiesFromFile(const QString &filename = "travian_cookies.dat");

    // Cookie'leri dosyaya kaydet
    bool saveCookiesToFile(const QString &filename = "travian_cookies.dat");

signals:
    void villageNameReceived(const QString &villageName);
    void requestCompleted(const QString &response);
    void requestFailed(const QString &error);
    void loginSuccess();
    void loginFailed(const QString &reason);

private slots:
    void onDelayFinished();
    void onRequestFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    QTimer *m_delayTimer;
    QString m_pendingUrl;
    bool m_isLoginRequest;

    // Random delay generator (100-300ms arası)
    int getRandomDelay() const;

    // Gerçek request'i gönder
    void executeRequest();

    // HTML'den köy ismini parse et
    QString parseVillageName(const QString &html) const;

    // Login response'u kontrol et
    void checkLoginResponse(const QString &html);
};

#endif // TRAVIANREQUESTMANAGER_H
