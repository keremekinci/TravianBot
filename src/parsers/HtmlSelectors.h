#ifndef HTMLSELECTORS_H
#define HTMLSELECTORS_H

#include <QMap>
#include <QRegularExpression>
#include <QString>

/**
 * @brief Travian sayfalarını parse etmek için HTML selector pattern'leri
 *
 * Bu sınıf Travian HTML'inden veri çıkarmak için kullanılan tüm regex
 * pattern'lerini içerir. Eski scrape_config.json dosyasının yerine compile-time
 * constant'lar kullanır.
 */
class HtmlSelectors {
public:
  // ========== DORF1 (Kaynak Alanları) ==========

  // Köy bilgisi
  static constexpr const char *TRIBE =
      R"DELIM(resourceFieldContainer"[^>]*class="[^"]*tribe(\d+))DELIM";
  static constexpr const char *VILLAGE_NAME =
      R"DELIM(villageName"[^>]*value="([^"]+)")DELIM";

  // Kaynaklar (mevcut miktarlar)
  static constexpr const char *LUMBER =
      R"DELIM(id="l1"[^>]*>&#x202d;([\d.]+)&#x202c;)DELIM";
  static constexpr const char *CLAY =
      R"DELIM(id="l2"[^>]*>&#x202d;([\d.]+)&#x202c;)DELIM";
  static constexpr const char *IRON =
      R"DELIM(id="l3"[^>]*>&#x202d;([\d.]+)&#x202c;)DELIM";
  static constexpr const char *CROP =
      R"DELIM(id="l4"[^>]*>&#x202d;([\d.]+)&#x202c;)DELIM";

  // Depo kapasiteleri
  static constexpr const char *WAREHOUSE_CAPACITY =
      R"DELIM(class="warehouse"[^>]*>[\s\S]*?class="value">&#x202d;([\d.]+)&#x202c;)DELIM";
  static constexpr const char *GRANARY_CAPACITY =
      R"DELIM(class="granary"[^>]*>[\s\S]*?class="value">&#x202d;([\d.]+)&#x202c;)DELIM";

  // Üretim oranları (saat başına)
  static constexpr const char *PRODUCTION_LUMBER =
      R"DELIM(resource1[^>]*title="[^|]*\|\|[^:]*retim: (\d+))DELIM";
  static constexpr const char *PRODUCTION_CLAY =
      R"DELIM(resource2[^>]*title="[^|]*\|\|[^:]*retim: (\d+))DELIM";
  static constexpr const char *PRODUCTION_IRON =
      R"DELIM(resource3[^>]*title="[^|]*\|\|[^:]*retim: (\d+))DELIM";
  static constexpr const char *PRODUCTION_CROP =
      R"DELIM(resource4[^>]*title="[^|]*\|\|[^:]*retim[^:]*: (-?\d+))DELIM";

  // Kaynak alanları (odun/kil/demir/tahıl alanları)
  static constexpr const char *RESOURCE_FIELDS =
      R"DELIM(resourceField\s+gid(\d+)\s+buildingSlot(\d+)[^"]*level(\d+)"[^>]*data-aid="(\d+)"\s+data-gid="\d+"[^>]*title="([^&<]+))DELIM";

  // İnşaat kuyruğu
  static constexpr const char *CONSTRUCTION_QUEUE =
      R"DELIM(<div class="name">\s*([^<]+?)\s*<span class="lvl">Seviye\s*(\d+)</span>[\s\S]*?<span\s+class="timer"[^>]*>(\d+:\d+:\d+)</span>)DELIM";

  // Köydeki askerler
  static constexpr const char *TROOPS =
      R"DELIM(unit (u\w+)[^>]*alt="([^"]+)"[\s\S]*?<td class="num">(\d+)</td>[\s\S]*?<td class="un">([^<]+)</td>)DELIM";

  // ========== DORF2 (Köy Merkezi) ==========

  // Köy merkezindeki binalar
  static constexpr const char *BUILDINGS =
      R"DELIM(buildingSlot\s+a(\d+)\s+g(\d+)[^"]*"[^>]*data-aid="(\d+)"\s+data-gid="(\d+)"[^>]*data-name="([^"]*)"[^>]*>[^<]*<a[^>]*data-level="(\d+)")DELIM";

  // Bina seviyeleri (görsel etiketler)
  static constexpr const char *BUILDING_LEVELS =
      R"DELIM(data-level="(\d+)"[^>]*><div class="labelLayer">(\d+)</div>)DELIM";

  // ========== ASKERİ BİNALAR ==========

  // Eğitilebilir askerler (kışla/ahır/atölye)
  static constexpr const char *TRAINABLE_TROOPS =
      R"DELIM(innerTroopWrapper\s+troopt(\d+)[^"]*"[^>]*data-troop(?:id|ID)="(t\d+)"[\s\S]*?alt="([^"]+)")DELIM";

  // Kilitli askerler (henüz araştırılmamış)
  static constexpr const char *LOCKED_TROOPS =
      R"DELIM(action troop troopt(\d+) empty)DELIM";

  // ========== KÖY LİSTESİ ==========

  // Oyuncunun tüm köyleri
  static constexpr const char *VILLAGE_LIST =
      R"DELIM(villageList":\s*\[([^\]]+)\])DELIM";
  static constexpr const char *VILLAGE_LIST_ITEM =
      R"DELIM(\{[^}]*"villageId":(\d+)[^}]*"name":"([^"]+)"[^}]*\})DELIM";

  // ========== PARA BİRİMLERİ ==========

  static constexpr const char *GOLD =
      R"DELIM(ajaxReplaceableGoldAmount">\s*&#x202d;([\d.]+)&#x202c;)DELIM";
  static constexpr const char *SILVER =
      R"DELIM(ajaxReplaceableSilverAmount"[^>]*>\s*&#x202d;([\d.]+)&#x202c;)DELIM";

  // ========== YARDIMCI METODLAR ==========

  /**
   * @brief Pattern'den QRegularExpression oluştur
   * @param pattern Regex pattern string'i
   * @return Yapılandırılmış QRegularExpression nesnesi
   */
  static QRegularExpression createRegex(const char *pattern) {
    return QRegularExpression(QString::fromUtf8(pattern));
  }

  /**
   * @brief Tüm regex pattern'lerini map olarak döndür (debug için)
   * @return Pattern isimlerinden pattern'lere map
   */
  static QMap<QString, QString> getAllPatterns();
};

#endif // HTMLSELECTORS_H
