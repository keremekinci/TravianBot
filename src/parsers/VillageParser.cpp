#include "src/parsers/VillageParser.h"
#include "src/parsers/HtmlParser.h"
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

QList<VillageInfo> VillageParser::parseVillageList(const QString &html)
{
    // Try primary method first
    QList<VillageInfo> villages = parseFromVillageListJson(html);

    // Fallback to alternative method if needed
    if (villages.isEmpty()) {
        villages = parseFromIdNamePairs(html);
    }

    return villages;
}

QList<VillageInfo> VillageParser::parseFromVillageListJson(const QString &html)
{
    QList<VillageInfo> villages;

    // Look for: "villageList":[{"id":45172,"name":"Village Name",...
    QRegularExpression regex("\"villageList\":\\[(\\{[^\\]]+\\})\\]");
    QRegularExpressionMatch match = regex.match(html);

    if (!match.hasMatch()) {
        return villages;
    }

    QString villagesJson = "[" + match.captured(1) + "]";
    villagesJson = HtmlParser::decodeTurkishUnicode(villagesJson);

    QJsonDocument doc = QJsonDocument::fromJson(villagesJson.toUtf8());
    if (doc.isNull() || !doc.isArray()) {
        return villages;
    }

    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        VillageInfo village;
        village.id = obj["id"].toInt();
        village.name = obj["name"].toString();
        villages.append(village);
    }

    return villages;
}

QList<VillageInfo> VillageParser::parseFromIdNamePairs(const QString &html)
{
    QList<VillageInfo> villages;
    QSet<int> addedIds;

    QRegularExpression regex("\"id\":(\\d+),\"name\":\"([^\"]+)\"");
    QRegularExpressionMatchIterator it = regex.globalMatch(html);

    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        int id = m.captured(1).toInt();

        if (addedIds.contains(id)) {
            continue;
        }

        VillageInfo village;
        village.id = id;
        village.name = HtmlParser::decodeTurkishUnicode(m.captured(2));
        villages.append(village);
        addedIds.insert(id);
    }

    return villages;
}
