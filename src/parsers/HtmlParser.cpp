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
