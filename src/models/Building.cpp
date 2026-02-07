#include "src/models/Building.h"

QVariantMap Building::toVariant() const {
  QVariantMap map;
  map["slotId"] = slotId;
  map["gid"] = gid;
  map["name"] = name;
  map["level"] = level;
  map["isUpgrading"] = isUpgrading;
  map["upgradeTimeRemaining"] = upgradeTimeRemaining;
  return map;
}

Building Building::fromVariant(const QVariantMap &data) {
  Building b;
  b.slotId = data.value("slotId", 0).toInt();
  b.gid = data.value("gid", 0).toInt();
  b.name = data.value("name", "").toString();
  b.level = data.value("level", 0).toInt();
  b.isUpgrading = data.value("isUpgrading", false).toBool();
  b.upgradeTimeRemaining = data.value("upgradeTimeRemaining", 0).toInt();
  return b;
}

QString Building::displayName() const {
  if (!name.isEmpty()) {
    return name;
  }

  // Fallback to GID-based names
  switch (gid) {
  case 1:
    return "Oduncu";
  case 2:
    return "Tuğla Ocağı";
  case 3:
    return "Demir Madeni";
  case 4:
    return "Tarla";
  case 10:
    return "Hammadde Deposu";
  case 11:
    return "Tahıl Deposu";
  case 13:
    return "Zırh Dökümhanesi";
  case 15:
    return "Merkez Binası";
  case 16:
    return "Askeri Üs";
  case 17:
    return "Pazar";
  case 18:
    return "Elçilik Binası";
  case 19:
    return "Kışla";
  case 20:
    return "Ahır";
  case 21:
    return "Atölye";
  case 22:
    return "Akademi";
  case 23:
    return "Sığınak";
  case 25:
    return "Köşk";
  case 32:
    return "Toprak Siper";
  default:
    return QString("Bina #%1").arg(gid);
  }
}
