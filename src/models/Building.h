#ifndef BUILDING_H
#define BUILDING_H

#include <QString>
#include <QVariantMap>

/**
 * @brief Bir köydeki tek bir binayı temsil eder
 */
class Building {
public:
  int slotId = 0;
  int gid = 0;
  QString name;
  int level = 0;
  bool isUpgrading = false;
  int upgradeTimeRemaining = 0; // saniye

  Building() = default;
  Building(int slot, int buildingGid, const QString &buildingName,
           int buildingLevel)
      : slotId(slot), gid(buildingGid), name(buildingName),
        level(buildingLevel) {}

  // Serileştirme
  QVariantMap toVariant() const;
  static Building fromVariant(const QVariantMap &data);

  // Yardımcı metodlar
  bool isEmpty() const { return gid == 0; }
  QString displayName() const;
};

#endif // BUILDING_H
