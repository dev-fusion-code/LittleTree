#include "core/Plant.hpp"
#include "core/Simulation.hpp"
#include "core/Storage.hpp"
#include "core/Weather.hpp"
#include "graphics/Layout.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace littletree;

static int totalTests = 0;
static int failedTests = 0;

static void check(bool condition, const std::string& name) {
  ++totalTests;
  if (!condition) {
    ++failedTests;
    std::cerr << "FAIL: " << name << '\n';
  } else {
    std::cout << "PASS: " << name << '\n';
  }
}

int main() {
  std::cout << "=== LittleTree Core Tests ===\n";

  // 1. Weather determinism and daily transitions
  {
    Weather w1 = weatherForDay(0);
    Weather w2 = weatherForDay(86399);
    check(w1 == w2, "Weather remains constant throughout the same Unix day");

    Weather wNext = weatherForDay(86400);
    check(w1 != wNext, "Weather changes deterministically on day boundary");

    // Negative / epoch edge case
    check(weatherForDay(-100) == Weather::Sunny, "Negative timestamps handled safely without crash");

    // Cycle check: verify all weather types can occur across days
    bool sawSunny = false, sawCloudy = false, sawRain = false, sawStorm = false, sawSnow = false;
    for (std::int64_t d = 0; d < 50; ++d) {
      Weather w = weatherForDay(d * 86400);
      if (w == Weather::Sunny) sawSunny = true;
      if (w == Weather::Cloudy) sawCloudy = true;
      if (w == Weather::Rain) sawRain = true;
      if (w == Weather::Storm) sawStorm = true;
      if (w == Weather::Snow) sawSnow = true;
    }
    check(sawSunny && sawCloudy && sawRain && sawStorm && sawSnow,
          "All 5 weather states generated across days");
  }

  // 2. Growth stage boundaries
  {
    check(stageForGrowth(0.0) == Stage::Seedling, "Stage at 0.0 is Seedling");
    check(stageForGrowth(0.13) == Stage::Seedling, "Stage at 0.13 is Seedling");
    check(stageForGrowth(0.14) == Stage::Sprout, "Stage at 0.14 is Sprout");
    check(stageForGrowth(0.31) == Stage::Sprout, "Stage at 0.31 is Sprout");
    check(stageForGrowth(0.32) == Stage::Sapling, "Stage at 0.32 is Sapling");
    check(stageForGrowth(0.57) == Stage::Sapling, "Stage at 0.57 is Sapling");
    check(stageForGrowth(0.58) == Stage::YoungTree, "Stage at 0.58 is YoungTree");
    check(stageForGrowth(0.81) == Stage::YoungTree, "Stage at 0.81 is YoungTree");
    check(stageForGrowth(0.82) == Stage::Mature, "Stage at 0.82 is Mature");
    check(stageForGrowth(1.0) == Stage::Mature, "Stage at 1.0 is Mature");
  }

  // 3. Initial state and fresh plant
  {
    Plant p = freshPlant(1000);
    check(p.stage == Stage::Seedling, "Fresh plant is Seedling");
    check(p.health == 100.0, "Fresh plant has 100% health");
    check(p.moisture == 58.0, "Fresh plant has 58% moisture");
    check(p.growth == 0.0, "Fresh plant has 0.0 growth");
    check(p.plantedAt == 1000 && p.lastUpdated == 1000, "Timestamps initialized correctly");
    check(!p.diary.empty() && p.diary.front().text == "A new seedling was planted.",
          "Initial planting diary entry logged");
  }

  // 4. Moisture depletion, watering and overwatering
  {
    Simulation s(freshPlant(0));
    s.update(3600 * 10, Weather::Sunny);
    check(s.plant().moisture < 58.0, "Moisture depletes over 10 hours in sunny weather");

    s.water(3600 * 10);
    check(s.plant().moisture > 70.0, "Watering increases moisture (+26)");
    check(s.plant().lastWatered == 3600 * 10, "lastWatered updated on water action");

    // Overwater to extreme level (>90%)
    s.water(3600 * 10 + 1);
    s.water(3600 * 10 + 2);
    check(s.plant().moisture == 100.0, "Moisture clamped at 100%");

    double preHealth = s.plant().health;
    s.update(3600 * 20, Weather::Sunny);
    check(s.plant().health < preHealth, "Overwatering (>90%) degrades plant health");

    bool sawWaterloggedEntry = false;
    for (const auto& e : s.plant().diary) {
      if (e.text.find("waterlogged") != std::string::npos) {
        sawWaterloggedEntry = true;
        break;
      }
    }
    check(sawWaterloggedEntry, "Overwatering logs waterlogged diary entry");
  }

  // 5. Drought, withering threshold and recovery hysteresis
  {
    Simulation s(freshPlant(0));
    s.plant().moisture = 5.0; // severe drought
    s.update(3600 * 10, Weather::Sunny);
    check(s.plant().inDrought, "Drought status flagged when moisture < 20%");

    // Drive health below 22% to trigger withering
    s.plant().health = 20.0;
    s.update(3600 * 11, Weather::Sunny);
    check(s.plant().withering, "Withering activates when health < 22%");

    bool sawWitheringEntry = false;
    for (const auto& e : s.plant().diary) {
      if (e.text.find("withering") != std::string::npos) {
        sawWitheringEntry = true;
        break;
      }
    }
    check(sawWitheringEntry, "Withering diary entry logged on threshold breach");

    // Test recovery hysteresis:
    // When health rises to 30%, withering must remain true (until 45%)
    s.plant().moisture = 55.0; // optimal moisture
    s.plant().health = 30.0;
    s.update(3600 * 12, Weather::Sunny);
    check(s.plant().withering, "Withering latched and persists while health < 45%");

    // Drive health above 45% under good care -> recovery occurs!
    s.plant().health = 46.0;
    s.update(3600 * 13, Weather::Sunny);
    check(!s.plant().withering, "Withering cleared when health >= 45%");

    bool sawRecoveryEntry = false;
    for (const auto& e : s.plant().diary) {
      if (e.text.find("recovered from withering") != std::string::npos) {
        sawRecoveryEntry = true;
        break;
      }
    }
    check(sawRecoveryEntry, "Recovery diary entry successfully logged");
  }

  // 6. Snow dormancy does not unfairly deplete health
  {
    Simulation s(freshPlant(0));
    s.plant().moisture = 55.0; // optimal soil
    double initialHealth = s.plant().health;
    s.update(3600 * 24, Weather::Snow);

    check(s.plant().dormant, "Snow activates dormant state");
    check(s.plant().growth == 0.0, "Dormancy pauses growth");
    check(s.plant().health == initialHealth, "Snow dormancy preserves health when soil is good");
  }

  // 7. Full growth progression, maturity clamping and diary
  {
    Simulation s(freshPlant(0));
    // Simulate ideal care over 10 days
    for (int day = 1; day <= 10; ++day) {
      s.plant().moisture = 55.0;
      s.plant().health = 100.0;
      s.update(3600 * 24 * day, Weather::Sunny);
    }

    check(s.plant().growth == 1.0, "Growth clamped at exactly 1.0");
    check(s.plant().stage == Stage::Mature, "Plant reached Mature stage");

    bool sawMaturityEntry = false;
    for (const auto& e : s.plant().diary) {
      if (e.text.find("reached full maturity") != std::string::npos) {
        sawMaturityEntry = true;
        break;
      }
    }
    check(sawMaturityEntry, "Maturity diary event recorded");
  }

  // 8. Diary 80-entry limit and ordering
  {
    Plant p = freshPlant(0);
    for (int i = 1; i <= 100; ++i) {
      addDiary(p, i, "Event " + std::to_string(i));
    }
    check(p.diary.size() == 80, "Diary enforces maximum 80 entries");
    check(p.diary.front().text == "Event 21", "Oldest entries evicted (front is Event 21)");
    check(p.diary.back().text == "Event 100", "Newest entries preserved (back is Event 100)");
  }

  // 9. Storage roundtrip, atomic persistence, and validation
  {
    auto tempPath = std::filesystem::temp_directory_path() / "littletree_unit_test.txt";

    Plant p = freshPlant(100);
    p.growth = 0.65;
    p.stage = Stage::YoungTree;
    p.moisture = 42.5;
    p.health = 88.0;
    p.lastUpdated = 2000;
    addDiary(p, 1500, "Young tree milestone.");

    check(Storage::save(tempPath, p, Weather::Rain), "Storage::save succeeds");

    auto loadedOpt = Storage::load(tempPath);
    check(loadedOpt.has_value(), "Storage::load reads saved file");
    if (loadedOpt) {
      const auto& [loadedPlant, loadedWeather] = *loadedOpt;
      check(loadedPlant.stage == Stage::YoungTree, "Loaded stage matches");
      check(std::abs(loadedPlant.growth - 0.65) < 1e-4, "Loaded growth matches");
      check(std::abs(loadedPlant.moisture - 42.5) < 1e-4, "Loaded moisture matches");
      check(std::abs(loadedPlant.health - 88.0) < 1e-4, "Loaded health matches");
      check(loadedWeather == Weather::Rain, "Loaded weather matches");
      check(loadedPlant.diary.size() == p.diary.size(), "Loaded diary entry count matches");
      check(loadedPlant.diary.back().text == "Young tree milestone.", "Loaded diary entry text matches");
    }

    // Malformed storage validations
    {
      std::ofstream f(tempPath, std::ios::trunc);
      f << "LittleTree 2\n0 0 50 100 0 0 0 0\n0\n"; // Bad version (2)
    }
    check(!Storage::load(tempPath).has_value(), "Rejects invalid file version");

    {
      std::ofstream f(tempPath, std::ios::trunc);
      f << "LittleTree 1\n9 0 50 100 0 0 0 0\n0\n"; // Bad stage (9)
    }
    check(!Storage::load(tempPath).has_value(), "Rejects out-of-range stage");

    {
      std::ofstream f(tempPath, std::ios::trunc);
      f << "LittleTree 1\n0 1.5 50 100 0 0 0 0\n0\n"; // Bad growth (1.5)
    }
    check(!Storage::load(tempPath).has_value(), "Rejects out-of-range growth");

    {
      std::ofstream f(tempPath, std::ios::trunc);
      f << "LittleTree 1\n0 0.5 50 100 1000 500 200 0\n0\n"; // lastUpdated < plantedAt
    }
    check(!Storage::load(tempPath).has_value(), "Rejects inconsistent timestamps");

    {
      std::ofstream f(tempPath, std::ios::trunc);
      f << "LittleTree 1\n0 0.5 50 100 0 0 0 0\n999\n"; // count > 80
    }
    check(!Storage::load(tempPath).has_value(), "Rejects diary count > 80");

    std::filesystem::remove(tempPath);
  }

  // 10. Stepped offline progression handles downtime without lump-sum distortion
  {
    Simulation s(freshPlant(0));
    s.plant().moisture = 50.0;
    // Advance 3 days (259200 seconds) in one call
    s.update(86400 * 3, Weather::Sunny);
    check(s.plant().lastUpdated == 86400 * 3, "Downtime timestamp accurately advanced");
    check(s.plant().growth > 0.0, "Plant grew during multi-day absence");
  }

  // 11. Reset functionality
  {
    Simulation s(freshPlant(0));
    s.plant().growth = 0.9;
    s.plant().stage = Stage::Mature;
    s.reset(5000);

    check(s.plant().stage == Stage::Seedling, "Reset returns stage to Seedling");
    check(s.plant().growth == 0.0, "Reset clears growth to 0.0");
    check(s.plant().health == 100.0, "Reset restores full health");
    check(s.plant().plantedAt == 5000, "Reset updates plantedAt timestamp");
    check(s.plant().diary.back().text == "Life restarted by choice.", "Reset logs choice diary entry");
  }

  // 12. Layout, dimensions and scaling verification
  {
    check(Layout::WindowWidth == 800, "Window width is 800 pixels");
    check(Layout::WindowHeight == 700, "Window height is 700 pixels");
    check(Layout::SpriteScale == 8, "Sprite scale is integer 8x");
    check(Layout::SpriteWidth == 64, "Sprite pixel width is 64");
    check(Layout::SpriteHeight == 48, "Sprite pixel height is 48");
    check(Layout::RenderedSpriteWidth == 512, "Rendered tree width is 512 pixels");
    check(Layout::RenderedSpriteHeight == 384, "Rendered tree height is 384 pixels");

    // Horizontal and vertical containment
    check(Layout::TreeOffsetX >= 0 && Layout::TreeOffsetX + Layout::RenderedSpriteWidth <= Layout::WindowWidth,
          "Tree is contained horizontally within window");
    check(Layout::TreeOffsetY >= 0 && Layout::TreeOffsetY + Layout::RenderedSpriteHeight <= Layout::WindowHeight,
          "Tree is contained vertically within window");

    // Overlay containment and non-overlap with canopy
    check(Layout::OverlayX >= 0 && Layout::OverlayX + Layout::OverlayWidth <= Layout::WindowWidth,
          "Overlay fits within window horizontally");
    check(Layout::OverlayY >= 0 && Layout::OverlayY + Layout::OverlayHeight <= Layout::WindowHeight,
          "Overlay fits within window vertically");
    check(Layout::OverlayY + Layout::OverlayHeight <= Layout::TreeOffsetY,
          "Overlay does not overlap tree canopy");
  }

  std::cout << "\n=================================\n";
  std::cout << "Tests run: " << totalTests << " | Failed: " << failedTests << '\n';
  std::cout << "=================================\n";

  return failedTests ? 1 : 0;
}
