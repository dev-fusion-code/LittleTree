#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace littletree {

enum class Stage { Seedling, Sprout, Sapling, YoungTree, Mature };

struct DiaryEntry {
  std::int64_t at{};
  std::string text;
};

struct Plant {
  Stage stage{Stage::Seedling};
  double growth{0.0};       // 0..1, mature at 1.0
  double moisture{58.0};    // 0..100
  double health{100.0};     // 0..100
  std::int64_t plantedAt{};
  std::int64_t lastWatered{};
  std::int64_t lastUpdated{};
  bool dormant{false};
  bool withering{false};
  bool inDrought{false};
  std::vector<DiaryEntry> diary;
};

std::string stageName(Stage stage);
Stage stageForGrowth(double growth);
void addDiary(Plant& plant, std::int64_t now, std::string text);
Plant freshPlant(std::int64_t now);

}  // namespace littletree
