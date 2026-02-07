#ifndef VILLAGEPARSER_H
#define VILLAGEPARSER_H

#include <QString>
#include <QList>
#include <QVariantMap>

/**
 * @brief Information about a Travian village
 */
struct VillageInfo {
    int id = 0;
    QString name;
    QVariantMap data;
};

/**
 * @brief Parser for extracting village information from Travian HTML
 */
class VillageParser
{
public:
    /**
     * @brief Parse village list from HTML page
     * @param html Raw HTML content containing village data
     * @return List of discovered villages
     */
    static QList<VillageInfo> parseVillageList(const QString &html);

private:
    /**
     * @brief Try parsing villageList JSON format
     */
    static QList<VillageInfo> parseFromVillageListJson(const QString &html);

    /**
     * @brief Fallback: parse from individual id/name pairs
     */
    static QList<VillageInfo> parseFromIdNamePairs(const QString &html);
};

#endif // VILLAGEPARSER_H
