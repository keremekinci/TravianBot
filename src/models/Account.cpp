#include "src/models/Account.h"
#include <QDebug>

Account::Account(QObject *parent) : QObject(parent) {}

void Account::setCurrentVillageId(int villageId) {
  if (m_currentVillageId == villageId) {
    return;
  }

  m_currentVillageId = villageId;
  emit currentVillageIdChanged();
  emit currentVillageChanged();

}

void Account::setTribe(int tribeId) {
  if (m_tribe == tribeId) {
    return;
  }

  m_tribe = tribeId;
  emit tribeChanged();
}

Village *Account::getCurrentVillage() { return getVillage(m_currentVillageId); }

Village *Account::getVillage(int villageId) {
  for (Village &v : villages) {
    if (v.id == villageId) {
      return &v;
    }
  }
  return nullptr;
}

void Account::addOrUpdateVillage(const Village &village) {
  // Check if village exists
  for (int i = 0; i < villages.size(); ++i) {
    if (villages[i].id == village.id) {
      // Update existing
      villages[i] = village;
      emit villagesChanged();

      // If this is current village, emit change
      if (village.id == m_currentVillageId) {
        emit currentVillageChanged();
      }
      return;
    }
  }

  // Add new village
  villages.append(village);
  emit villagesChanged();

  // Set as current if first village
  if (villages.size() == 1) {
    setCurrentVillageId(village.id);
  }
}

void Account::removeVillage(int villageId) {
  for (int i = 0; i < villages.size(); ++i) {
    if (villages[i].id == villageId) {
      villages.removeAt(i);
      emit villagesChanged();

      // If removed current village, switch to first available
      if (villageId == m_currentVillageId && !villages.isEmpty()) {
        setCurrentVillageId(villages.first().id);
      }
      return;
    }
  }
}

QVariantList Account::villagesVariant() const {
  QVariantList list;
  for (const Village &v : villages) {
    list.append(v.toVariant());
  }
  return list;
}

QVariantMap Account::currentVillageVariant() const {
  for (const Village &v : villages) {
    if (v.id == m_currentVillageId) {
      return v.toVariant();
    }
  }
  return QVariantMap();
}

QVariantMap Account::toVariant() const {
  QVariantMap map;
  map["currentVillageId"] = m_currentVillageId;
  map["tribe"] = m_tribe;
  map["villages"] = villagesVariant();
  return map;
}

Account *Account::fromVariant(const QVariantMap &data, QObject *parent) {
  Account *account = new Account(parent);
  account->m_currentVillageId = data.value("currentVillageId", -1).toInt();
  account->m_tribe = data.value("tribe", 0).toInt();

  QVariantList villagesList = data.value("villages").toList();
  for (const QVariant &vVar : villagesList) {
    account->villages.append(Village::fromVariant(vVar.toMap()));
  }

  return account;
}
