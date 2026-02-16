#ifndef HTMLPARSER_H
#define HTMLPARSER_H

#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QJsonObject>
#include <QJsonArray>



/**
 * @brief HTML parsing utilities for extracting data from Travian pages
 */
class HtmlParser
{
public:
    /**
     * @brief Parse page data based on JSON configuration
     * @param html Raw HTML content
     * @param pageConfig JSON configuration for the page
     * @return Extracted data as QVariantMap
     */
    static QVariantMap parsePageData(const QString &html, const QJsonObject &pageConfig);

    /**
     * @brief Extract a single value using regex selector
     * @param html Raw HTML content
     * @param selector Regex pattern with capture group
     * @return Captured value or invalid QVariant
     */
    static QVariant parseSingleValue(const QString &html, const QString &selector);

    /**
     * @brief Extract a list of items using regex selector
     * @param html Raw HTML content
     * @param selector Regex pattern with multiple capture groups
     * @param fields Field names for each capture group
     * @return List of QVariantMap items
     */
    static QVariantList parseListValue(const QString &html, const QString &selector, const QStringList &fields);

    /**
     * @brief Extract an object with multiple child values
     * @param html Raw HTML content
     * @param children JSON array of child selectors
     * @return QVariantMap with extracted values
     */
    static QVariantMap parseObjectValue(const QString &html, const QJsonArray &children);

    /**
     * @brief Decode common Turkish Unicode escape sequences
     * @param text Text with Unicode escapes
     * @return Decoded text
     */
    static QString decodeTurkishUnicode(const QString &text);

    static QJsonObject extractEmbeddedJson(const QString& html);

    /**
     * @brief Extract village list with attack information from embedded JSON
     * @param html Raw HTML content
     * @return List of villages with attack data
     */
    static QVariantList extractVillageListWithAttacks(const QString& html);

    /**
     * @brief Extract incoming troop movements from rally point HTML
     * @param html Raw HTML content from rally point page (tt=1)
     * @return List of incoming movements with timing data
     */
    static QVariantList extractIncomingMovements(const QString& html);
};

#endif // HTMLPARSER_H
