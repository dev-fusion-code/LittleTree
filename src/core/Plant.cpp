#include "core/Plant.hpp"
#include <algorithm>

namespace littletree {

std::string stageName(Stage s) {
  static const char* const names[] = {
    "Seedling",
    "Young sprout",
    "Growing sapling",
    "Young tree",
    "Mature little tree"
  };
  auto idx = static_cast<std::size_t>(s);
  if (idx < 5) return names[idx];
  return "Unknown";
}

Stage stageForGrowth(double g) {
  if (g < 0.14) return Stage::Seedling;
  if (g < 0.32) return Stage::Sprout;
  if (g < 0.58) return Stage::Sapling;
  if (g < 0.82) return Stage::YoungTree;
  return Stage::Mature;
}

void addDiary(Plant& p, std::int64_t now, std::string text) {
  p.diary.push_back({now, std::move(text)});
  if (p.diary.size() > 80) {
    p.diary.erase(p.diary.begin(), p.diary.begin() + static_cast<std::ptrdiff_t>(p.diary.size() - 80));
  }
}

Plant freshPlant(std::int64_t now) {
  Plant p;
  p.plantedAt = now;
  p.lastWatered = now;
  p.lastUpdated = now;
  addDiary(p, now, "A new seedling was planted.");
  return p;
}

}  // namespace littletree
