# Travian Bot

Modern QML arayüzüne sahip Qt/C++ tabanlı Travian otomasyon botu.

## Özellikler

- 🔐 **Otomatik Giriş** - Oturum çerezlerini kaydederek kalıcı giriş
- 🏘️ **Çoklu Köy Desteği** - Birden fazla köyü yönet
- 🏗️ **İnşaat Kuyruğu** - Öncelik tabanlı otomatik bina yükseltme
- ⚔️ **Asker Eğitimi** - Otomatik asker eğitim kuyruğu
- 🌾 **Çiftlik Listeleri** - Otomatik yağma saldırıları
- 📊 **Kaynak Takibi** - Gerçek zamanlı kaynak ve üretim izleme
- 🔄 **Akıllı Yenileme** - İnşaat sürelerine göre yenileme aralığı ayarlama
- 🔔 **Telegram Bildirimleri** - Gelen saldırılarda telefona anlık bildirim
- 🎨 **Modern Arayüz** - Sekmeli temiz QML arayüzü

## Gereksinimler

- Qt 6.10.2 veya üstü
- CMake 3.16+
- C++17 derleyici
- macOS (macOS üzerinde test edildi)

## Kurulum

### 1. Depoyu Klonla

```bash
git clone https://github.com/keremekinci/TravianBot.git
cd TravianBot
```

### 2. Ayarları Yapılandır

```bash
# Örnek ayar dosyasını kopyala
cp config/settings.ini.example config/settings.ini

# Bilgilerini gir
nano config/settings.ini
```

**settings.ini formatı:**
```ini
[Server]
baseUrl=https://ts30.x3.europe.travian.com

[Credentials]
username=kullanici_adin
password=sifren
```

### 3. Projeyi Derle

```bash
mkdir build
cd build
cmake ..
make
```

### 4. Çalıştır

```bash
./TravianBot
```

## Proje Yapısı

```
.
├── CMakeLists.txt
├── main.cpp
├── config/
│   ├── settings.ini.example    # Örnek ayar dosyası (settings.ini olarak kopyala)
│   ├── settings.ini            # Senin bilgilerin (git'te yok)
│   ├── farm_config.json        # Çiftlik listesi ayarları
│   └── troop_config.json       # Asker eğitim ayarları
└── src/
    ├── ui/
    │   ├── Main.qml            # QML Arayüz
    │   └── TravianUiBridge.*   # Qt/QML köprüsü
    ├── network/
    │   ├── TravianDataFetcher.*    # HTTP istekleri
    │   └── Travianrequestmanager.* # İstek yönetimi
    ├── managers/
    │   ├── BuildQueueManager.*     # İnşaat kuyruğu
    │   ├── TroopQueueManager.*     # Asker eğitim kuyruğu
    │   └── FarmListManager.*       # Çiftlik listesi otomasyonu
    ├── parsers/
    │   ├── HtmlParser.*        # HTML ayrıştırma
    │   ├── HtmlSelectors.*     # CSS seçiciler
    │   └── VillageParser.*     # Köy verisi ayrıştırma
    └── models/
        ├── Account.*           # Hesap modeli
        ├── Village.*           # Köy modeli
        └── Building.*          # Bina modeli
```

## Kullanım

1. **Giriş**: Uygulama `settings.ini` bilgilerinle otomatik giriş yapar
2. **Köy Seç**: Açılır menüden köyler arasında geçiş yap
3. **Tarlalar Sekmesi**: Kaynak tarlalarını görüntüle ve yükselt
4. **Binalar Sekmesi**: Köy binalarını görüntüle ve yükselt
5. **İnşaat Kuyruğu**: Otomatik inşaat kuyruğunu yönet
6. **Askerler Sekmesi**: Asker eğitimini yapılandır
7. **Çiftlik Listeleri**: Otomatik yağma saldırılarını ayarla
8. **Telegram Bildirimleri**: 
   - `settings.ini` dosyasına `[Telegram]` bölümü ekle
   - `chatId=SENIN_CHAT_IDN` satırını ekle (Chat ID'ni öğrenmek için botla konuş)
   - Uygulama saldırı tespit ettiğinde otomatik bildirim gönderir
   - **Not:** Chat ID girilmesi zorunludur.

## Güvenlik Notları

⚠️ **Önemli**: 
- `settings.ini` dosyasını gerçek bilgilerinle asla commit etme
- `settings.ini` ve çerez dosyaları `.gitignore` ile git'ten hariç tutulmuştur
- Kendi sorumluluğunda kullan - otomatik oyun, oyun Kullanım Şartlarını ihlal edebilir

## Lisans

MIT Lisansı

## Sorumluluk Reddi

Bu bot yalnızca eğitim amaçlıdır. Kendi sorumluluğunda kullan. Otomatik oyun, oyunun Kullanım Şartlarını ihlal edebilir.
