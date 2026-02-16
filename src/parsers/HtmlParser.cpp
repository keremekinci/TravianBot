#include "src/parsers/HtmlParser.h"
#include <QRegularExpression>
#include <QDebug>

QVariantMap HtmlParser::parsePageData(const QString &html, const QJsonObject &pageConfig)
{
    QVariantMap result;

    // Yeni format: "fields" bir object (her key bir field)
    QJsonObject fields = pageConfig["fields"].toObject();

    for (auto it = fields.begin(); it != fields.end(); ++it) {
        QString key = it.key();
        QJsonObject item = it.value().toObject();
        QString type = item["type"].toString("single");
        QString selector = item["selector"].toString();

        if (type == "single") {
            result[key] = parseSingleValue(html, selector);
        }
        else if (type == "list") {
            QStringList fieldNames;
            QJsonArray fieldsArray = item["fields"].toArray();
            for (const QJsonValue &f : fieldsArray) {
                fieldNames << f.toString();
            }
            result[key] = parseListValue(html, selector, fieldNames);
        }
        else if (type == "object") {
            QJsonArray children = item["children"].toArray();
            result[key] = parseObjectValue(html, children);
        }
    }

    return result;
}

QVariant HtmlParser::parseSingleValue(const QString &html, const QString &selector)
{
    QRegularExpression regex(selector);
    QRegularExpressionMatch match = regex.match(html);

    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    return QVariant();
}

QVariantList HtmlParser::parseListValue(const QString &html, const QString &selector, const QStringList &fields)
{
    QVariantList result;

    // DotMatchesEverythingOption ekle - \s\S yerine . kullanılabilsin
    QRegularExpression regex(selector, QRegularExpression::DotMatchesEverythingOption);

    if (!regex.isValid()) {
        return result;
    }

    QRegularExpressionMatchIterator it = regex.globalMatch(html);

    int matchCount = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QVariantMap item;

        for (int i = 0; i < fields.size() && i <= match.lastCapturedIndex(); ++i) {
            item[fields[i]] = match.captured(i + 1).trimmed();
        }

        if (!item.isEmpty()) {
            result.append(item);
            matchCount++;
            if (matchCount <= 3) {
            }
        }
    }

    if (matchCount == 0 && selector.contains("trainable")) {
    }

    return result;
}

QVariantMap HtmlParser::parseObjectValue(const QString &html, const QJsonArray &children)
{
    QVariantMap result;

    for (const QJsonValue &childVal : children) {
        QJsonObject child = childVal.toObject();
        QString key = child["key"].toString();
        QString selector = child["selector"].toString();

        QVariant value = parseSingleValue(html, selector);
        if (value.isValid()) {
            result[key] = value;
        }
    }

    return result;
}

QString HtmlParser::decodeTurkishUnicode(const QString &text)
{
    QString result = text;
    result.replace("\\u00fc", QString::fromUtf8("ü"));
    result.replace("\\u00f6", QString::fromUtf8("ö"));
    result.replace("\\u00e7", QString::fromUtf8("ç"));
    result.replace("\\u011f", QString::fromUtf8("ğ"));
    result.replace("\\u0131", QString::fromUtf8("ı"));
    result.replace("\\u015f", QString::fromUtf8("ş"));
    result.replace("\\u00dc", QString::fromUtf8("Ü"));
    result.replace("\\u00d6", QString::fromUtf8("Ö"));
    result.replace("\\u00c7", QString::fromUtf8("Ç"));
    result.replace("\\u011e", QString::fromUtf8("Ğ"));
    result.replace("\\u0130", QString::fromUtf8("İ"));
    result.replace("\\u015e", QString::fromUtf8("Ş"));
    return result;
}

#include <QJsonDocument>
#include <QJsonObject>

QJsonObject HtmlParser::extractEmbeddedJson(const QString& html)
{
    // Travian sayfalarında genelde büyük JSON script içinde oluyor.
    // Biz burada kaba ama işe yarayan şekilde "Travian.Game.Preferences.initialize(" gibi yerlerden yakalamaya çalışıyoruz.

    // 1) Örnek yakalama:
    // Travian.Game.Preferences.initialize({...});
    QRegularExpression rx(R"(Travian\.Game\.Preferences\.initialize\((\{.*?\})\);)",
                          QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatch m = rx.match(html);
    if (m.hasMatch()) {
        QString jsonStr = m.captured(1);

        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
        if (!doc.isNull() && doc.isObject()) {
            return doc.object();
        }
    }

    // 2) fallback boş dön
    return QJsonObject();
}

QVariantList HtmlParser::extractVillageListWithAttacks(const QString& html)
{
    QVariantList result;

    // Find villageList array directly - more reliable than parsing entire viewData
    QRegularExpression rx(R"("villageList":\s*(\[[^\]]+\]))",
                          QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatch m = rx.match(html);
    if (!m.hasMatch()) {
        return result;
    }

    QString jsonStr = m.captured(1);

    // Parse the JSON array directly
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
    if (doc.isNull() || !doc.isArray()) {
        qWarning() << "[PARSER] Failed to parse villageList JSON:" << err.errorString();
        return result;
    }

    QJsonArray villageList = doc.array();

    // Convert to QVariantList
    for (const QJsonValue &val : villageList) {
        if (!val.isObject()) {
            continue;
        }

        QJsonObject village = val.toObject();
        QVariantMap villageMap;

        villageMap["id"] = village["id"].toInt();
        villageMap["name"] = village["name"].toString();
        villageMap["x"] = village["x"].toInt();
        villageMap["y"] = village["y"].toInt();
        villageMap["distance"] = village["distance"].toDouble();
        villageMap["incomingAttacksAmount"] = village["incomingAttacksAmount"].toInt();

        // Parse attack symbols
        if (village.contains("incomingAttacksSymbols")) {
            QJsonObject symbols = village["incomingAttacksSymbols"].toObject();
            QVariantMap symbolsMap;
            symbolsMap["gray"] = symbols["gray"].toInt();
            symbolsMap["green"] = symbols["green"].toInt();
            symbolsMap["red"] = symbols["red"].toInt();
            symbolsMap["yellow"] = symbols["yellow"].toInt();
            villageMap["incomingAttacksSymbols"] = symbolsMap;
        }

        result.append(villageMap);
    }

    return result;
}

QVariantList HtmlParser::extractIncomingMovements(const QString& html)
{
    QVariantList result;

    // Travian stores movement data in viewData JSON, similar to farm lists
    // Look for patterns like: "movements":[...], "incomingTroops":[...], "troops":[...]

    // Try multiple possible field names
    QStringList possibleFields = {"movements", "incomingTroops", "troops", "incomingAttacks"};

    for (const QString& fieldName : possibleFields) {
        // Build regex to find this specific array
        QString pattern = QString(R"("%1"\s*:\s*\[)").arg(fieldName);
        QRegularExpression rx(pattern);
        QRegularExpressionMatch m = rx.match(html);

        if (!m.hasMatch()) {
            continue;
        }

        // Found the field - now extract the full array
        int startPos = m.capturedEnd(0) - 1; // Back to '['
        int braceCount = 0;
        int endPos = startPos;
        bool inString = false;
        bool escapeNext = false;

        for (int i = startPos; i < html.length(); ++i) {
            QChar c = html[i];

            if (escapeNext) {
                escapeNext = false;
                continue;
            }

            if (c == '\\') {
                escapeNext = true;
                continue;
            }

            if (c == '"' && !escapeNext) {
                inString = !inString;
                continue;
            }

            if (inString) {
                continue;
            }

            if (c == '[' || c == '{') {
                braceCount++;
            } else if (c == ']' || c == '}') {
                braceCount--;
                if (braceCount == 0) {
                    endPos = i + 1;
                    break;
                }
            }
        }

        if (endPos > startPos) {
            QString jsonStr = html.mid(startPos, endPos - startPos);

            QJsonParseError err{};
            QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);

            if (!doc.isNull() && doc.isArray()) {
                QJsonArray movements = doc.array();
                qDebug() << "[PARSER] Found" << movements.size() << "movements in field:" << fieldName;

                // Parse each movement
                for (const QJsonValue& val : movements) {
                    if (!val.isObject()) {
                        continue;
                    }

                    QJsonObject mov = val.toObject();
                    QVariantMap movement;

                    // Extract common fields
                    movement["id"] = mov["id"].toInt();
                    movement["type"] = mov["type"].toInt(); // 3=attack, 4=raid, etc
                    movement["movementType"] = mov["movementType"].toInt();
                    movement["arrivalTime"] = mov["arrivalTime"].toInt(); // Unix timestamp
                    movement["remainingSeconds"] = mov["remainingSeconds"].toInt();
                    movement["attackType"] = mov["attackType"].toInt();

                    // Source/destination info
                    if (mov.contains("from")) {
                        QJsonObject from = mov["from"].toObject();
                        movement["fromVillageId"] = from["villageId"].toInt();
                        movement["fromVillageName"] = from["villageName"].toString();
                        movement["fromPlayerName"] = from["playerName"].toString();
                    }

                    if (mov.contains("to")) {
                        QJsonObject to = mov["to"].toObject();
                        movement["toVillageId"] = to["villageId"].toInt();
                        movement["toVillageName"] = to["villageName"].toString();
                    }

                    // Troop info if available
                    if (mov.contains("troops")) {
                        QJsonObject troops = mov["troops"].toObject();
                        movement["troops"] = troops.toVariantMap();
                    }

                    // Resources being carried
                    if (mov.contains("resources")) {
                        QJsonObject resources = mov["resources"].toObject();
                        movement["resources"] = resources.toVariantMap();
                    }

                    result.append(movement);
                }

                return result; // Found and parsed successfully
            } else {
                qWarning() << "[PARSER] Failed to parse" << fieldName << "JSON:" << err.errorString();
            }
        }
    }

    qDebug() << "[PARSER] No movement data found in HTML - tried fields:" << possibleFields;
    return result;
}
