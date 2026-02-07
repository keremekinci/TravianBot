#include "HtmlSelectors.h"

QMap<QString, QString> HtmlSelectors::getAllPatterns() {
  QMap<QString, QString> patterns;

  // Village info
  patterns["TRIBE"] = TRIBE;
  patterns["VILLAGE_NAME"] = VILLAGE_NAME;

  // Resources
  patterns["LUMBER"] = LUMBER;
  patterns["CLAY"] = CLAY;
  patterns["IRON"] = IRON;
  patterns["CROP"] = CROP;

  // Capacities
  patterns["WAREHOUSE_CAPACITY"] = WAREHOUSE_CAPACITY;
  patterns["GRANARY_CAPACITY"] = GRANARY_CAPACITY;

  // Production
  patterns["PRODUCTION_LUMBER"] = PRODUCTION_LUMBER;
  patterns["PRODUCTION_CLAY"] = PRODUCTION_CLAY;
  patterns["PRODUCTION_IRON"] = PRODUCTION_IRON;
  patterns["PRODUCTION_CROP"] = PRODUCTION_CROP;

  // Fields and buildings
  patterns["RESOURCE_FIELDS"] = RESOURCE_FIELDS;
  patterns["BUILDINGS"] = BUILDINGS;
  patterns["BUILDING_LEVELS"] = BUILDING_LEVELS;

  // Construction and troops
  patterns["CONSTRUCTION_QUEUE"] = CONSTRUCTION_QUEUE;
  patterns["TROOPS"] = TROOPS;
  patterns["TRAINABLE_TROOPS"] = TRAINABLE_TROOPS;
  patterns["LOCKED_TROOPS"] = LOCKED_TROOPS;

  // Village list
  patterns["VILLAGE_LIST"] = VILLAGE_LIST;
  patterns["VILLAGE_LIST_ITEM"] = VILLAGE_LIST_ITEM;

  // Currency
  patterns["GOLD"] = GOLD;
  patterns["SILVER"] = SILVER;

  return patterns;
}
