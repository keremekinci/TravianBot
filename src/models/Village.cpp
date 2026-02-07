#include "src/models/Village.h"

// Resources serialization
QVariantMap Village::Resources::toVariant() const {
  QVariantMap map;
  map["lumber"] = lumber;
  map["clay"] = clay;
  map["iron"] = iron;
  map["crop"] = crop;
  map["lumberCapacity"] = lumberCapacity;
  map["clayCapacity"] = clayCapacity;
  map["ironCapacity"] = ironCapacity;
  map["cropCapacity"] = cropCapacity;
  return map;
}

Village::Resources Village::Resources::fromVariant(const QVariantMap &data) {
  Resources r;
  r.lumber = data.value("lumber", 0).toInt();
  r.clay = data.value("clay", 0).toInt();
  r.iron = data.value("iron", 0).toInt();
  r.crop = data.value("crop", 0).toInt();
  r.lumberCapacity = data.value("lumberCapacity", 0).toInt();
  r.clayCapacity = data.value("clayCapacity", 0).toInt();
  r.ironCapacity = data.value("ironCapacity", 0).toInt();
  r.cropCapacity = data.value("cropCapacity", 0).toInt();
  return r;
}

// Production serialization
QVariantMap Village::Production::toVariant() const {
  QVariantMap map;
  map["lumber"] = lumber;
  map["clay"] = clay;
  map["iron"] = iron;
  map["crop"] = crop;
  return map;
}

Village::Production Village::Production::fromVariant(const QVariantMap &data) {
  Production p;
  p.lumber = data.value("lumber", 0).toInt();
  p.clay = data.value("clay", 0).toInt();
  p.iron = data.value("iron", 0).toInt();
  p.crop = data.value("crop", 0).toInt();
  return p;
}

// ConstructionInfo serialization
QVariantMap Village::ConstructionInfo::toVariant() const {
  QVariantMap map;
  map["buildingName"] = buildingName;
  map["level"] = level;
  map["remainingTime"] = remainingTime;
  return map;
}

Village::ConstructionInfo
Village::ConstructionInfo::fromVariant(const QVariantMap &data) {
  ConstructionInfo c;
  c.buildingName = data.value("buildingName", "").toString();
  c.level = data.value("level", 0).toInt();
  c.remainingTime = data.value("remainingTime", 0).toInt();
  return c;
}

// Village serialization
QVariantMap Village::toVariant() const {
  QVariantMap map;
  map["id"] = id;
  map["name"] = name;
  map["resources"] = resources.toVariant();
  map["production"] = production.toVariant();
  map["construction"] = construction.toVariant();
  map["troops"] = troops;

  // Buildings
  QVariantList buildingsList;
  for (const Building &b : buildings) {
    buildingsList.append(b.toVariant());
  }
  map["buildings"] = buildingsList;

  // Build queue
  QVariantList queueList;
  for (const BuildQueueManager::BuildTask &task : buildQueue) {
    QVariantMap taskMap;
    taskMap["villageId"] = task.villageId;
    taskMap["slotId"] = task.slotId;
    taskMap["targetLevel"] = task.targetLevel;
    taskMap["buildingName"] = task.buildingName;
    taskMap["priority"] = task.priority;
    taskMap["currentLevel"] = task.currentLevel;
    queueList.append(taskMap);
  }
  map["buildQueue"] = queueList;

  return map;
}

Village Village::fromVariant(const QVariantMap &data) {
  Village v;
  v.id = data.value("id", 0).toInt();
  v.name = data.value("name", "").toString();
  v.resources = Resources::fromVariant(data.value("resources").toMap());
  v.production = Production::fromVariant(data.value("production").toMap());
  v.construction =
      ConstructionInfo::fromVariant(data.value("construction").toMap());
  v.troops = data.value("troops").toMap();

  // Buildings
  QVariantList buildingsList = data.value("buildings").toList();
  for (const QVariant &bVar : buildingsList) {
    v.buildings.append(Building::fromVariant(bVar.toMap()));
  }

  // Build queue
  QVariantList queueList = data.value("buildQueue").toList();
  for (const QVariant &taskVar : queueList) {
    QVariantMap taskMap = taskVar.toMap();
    BuildQueueManager::BuildTask task;
    task.villageId = taskMap.value("villageId", 0).toInt();
    task.slotId = taskMap.value("slotId", 0).toInt();
    task.targetLevel = taskMap.value("targetLevel", 0).toInt();
    task.buildingName = taskMap.value("buildingName", "").toString();
    task.priority = taskMap.value("priority", 0).toInt();
    task.currentLevel = taskMap.value("currentLevel", 0).toInt();
    v.buildQueue.append(task);
  }

  return v;
}

// Helper methods
Building *Village::getBuildingBySlot(int slotId) {
  for (Building &b : buildings) {
    if (b.slotId == slotId) {
      return &b;
    }
  }
  return nullptr;
}

Building *Village::getBuildingByGid(int gid) {
  for (Building &b : buildings) {
    if (b.gid == gid) {
      return &b;
    }
  }
  return nullptr;
}
