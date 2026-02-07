#ifndef VILLAGE_H
#define VILLAGE_H

#include "src/managers/BuildQueueManager.h"
#include "src/models/Building.h"
#include <QList>
#include <QString>
#include <QVariantMap>

/**
 * @brief Tüm verileriyle birlikte tek bir köyü temsil eder
 */
class Village {
public:
  int id = 0;
  QString name;

  // Kaynaklar
  struct Resources {
    int lumber = 0;
    int clay = 0;
    int iron = 0;
    int crop = 0;
    int lumberCapacity = 0;
    int clayCapacity = 0;
    int ironCapacity = 0;
    int cropCapacity = 0;

    QVariantMap toVariant() const;
    static Resources fromVariant(const QVariantMap &data);
  } resources;

  // Üretim
  struct Production {
    int lumber = 0;
    int clay = 0;
    int iron = 0;
    int crop = 0;

    QVariantMap toVariant() const;
    static Production fromVariant(const QVariantMap &data);
  } production;

  // Binalar
  QList<Building> buildings;

  // Askerler (şimdilik variant olarak tut, sonra tiplenebilir)
  QVariantMap troops;

  // İnşaat bilgisi
  struct ConstructionInfo {
    QString buildingName;
    int level = 0;
    int remainingTime = 0; // saniye

    QVariantMap toVariant() const;
    static ConstructionInfo fromVariant(const QVariantMap &data);
  } construction;

  // İnşaat kuyruğu (görev listesi)
  QList<BuildQueueManager::BuildTask> buildQueue;

  // Yardımcı metodlar
  Building *getBuildingBySlot(int slotId);
  Building *getBuildingByGid(int gid);
  bool hasActiveConstruction() const { return construction.remainingTime > 0; }

  // Serileştirme
  QVariantMap toVariant() const;
  static Village fromVariant(const QVariantMap &data);
};

#endif // VILLAGE_H
