#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "src/models/Village.h"
#include <QList>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

/**
 * @brief Oyuncunun hesabını tüm köyleriyle birlikte temsil eder
 */
class Account : public QObject {
  Q_OBJECT
  Q_PROPERTY(int currentVillageId READ currentVillageId WRITE
                 setCurrentVillageId NOTIFY currentVillageIdChanged)
  Q_PROPERTY(QVariantList villages READ villagesVariant NOTIFY villagesChanged)
  Q_PROPERTY(QVariantMap currentVillage READ currentVillageVariant NOTIFY
                 currentVillageChanged)
  Q_PROPERTY(int tribe READ tribe WRITE setTribe NOTIFY tribeChanged)

public:
  explicit Account(QObject *parent = nullptr);

  // Köyler
  QList<Village> villages;

  // Mevcut köy
  int currentVillageId() const { return m_currentVillageId; }
  void setCurrentVillageId(int villageId);

  // Kabile
  int tribe() const { return m_tribe; }
  void setTribe(int tribeId);

  // Köy erişimi
  Village *getCurrentVillage();
  Village *getVillage(int villageId);
  void addOrUpdateVillage(const Village &village);
  void removeVillage(int villageId);

  // QML için erişimciler
  QVariantList villagesVariant() const;
  QVariantMap currentVillageVariant() const;

  // Serileştirme
  QVariantMap toVariant() const;
  static Account *fromVariant(const QVariantMap &data,
                              QObject *parent = nullptr);

signals:
  void villagesChanged();
  void currentVillageChanged();
  void currentVillageIdChanged();
  void tribeChanged();

private:
  int m_currentVillageId = -1;
  int m_tribe = 0;
};

#endif // ACCOUNT_H
