#include "core/Simulation.hpp"
#include <algorithm>
#include <cmath>

namespace littletree {

namespace {
double clampPercent(double x) {
  return std::clamp(x, 0.0, 100.0);
}
}  // namespace

void Simulation::step(std::int64_t toTime, Weather weather) {
  if (toTime <= plant_.lastUpdated) return;

  double seconds = static_cast<double>(toTime - plant_.lastUpdated);
  double hours = seconds / 3600.0;
  weather_ = weather;

  // Rain moisture contribution per hour
  double rain = (weather == Weather::Rain) ? 2.5
              : (weather == Weather::Storm) ? 4.5
              : (weather == Weather::Snow) ? 0.3
              : 0.0;

  // Evaporation / absorption
  plant_.moisture = clampPercent(plant_.moisture - hours * 0.72 + hours * rain);

  // Snow dormancy
  bool wasDormant = plant_.dormant;
  plant_.dormant = (weather == Weather::Snow);
  if (!wasDormant && plant_.dormant) {
    addDiary(plant_, toTime, "Winter snow has induced dormancy.");
  } else if (wasDormant && !plant_.dormant) {
    addDiary(plant_, toTime, "The tree woke from winter dormancy.");
  }

  // Health progression
  // Normal healthy soil: 32% - 76%
  bool soilGood = (plant_.moisture >= 32.0 && plant_.moisture <= 76.0);
  double healthRate = 0.0;

  if (plant_.dormant) {
    // In dormancy, plant rests. No growth, but no unfair health loss if soil is reasonable.
    if (plant_.moisture < 15.0 || plant_.moisture > 92.0) {
      healthRate = -0.10;
    } else {
      healthRate = 0.0;
    }
  } else if (soilGood) {
    healthRate = 0.16; // steady recovery/wellness
  } else if (plant_.moisture < 20.0 || plant_.moisture > 90.0) {
    healthRate = -0.52; // severe stress
  } else {
    healthRate = -0.16; // mild stress
  }

  plant_.health = clampPercent(plant_.health + hours * healthRate);

  // Drought tracking and diary events
  if (plant_.moisture < 20.0) {
    if (!plant_.inDrought) {
      plant_.inDrought = true;
      addDiary(plant_, toTime, "Soil is parched. The plant needs water.");
    }
  } else if (plant_.moisture >= 32.0 && plant_.inDrought) {
    plant_.inDrought = false;
    addDiary(plant_, toTime, "Soil moisture restored to healthy levels.");
  }

  // Withering hysteresis:
  // Enters withering when health < 22
  if (plant_.health < 22.0 && !plant_.withering) {
    plant_.withering = true;
    addDiary(plant_, toTime, "The tree is withering from severe neglect.");
  } else if (plant_.withering && plant_.health >= 45.0 && healthRate > 0) {
    plant_.withering = false;
    addDiary(plant_, toTime, "The tree has recovered from withering.");
  }

  // Growth calculation
  double weatherFactor = (weather == Weather::Sunny) ? 1.10
                       : (weather == Weather::Cloudy) ? 0.82
                       : (weather == Weather::Rain) ? 0.93
                       : (weather == Weather::Storm) ? 0.62
                       : 0.0;

  if (soilGood && !plant_.dormant && plant_.health > 28.0 && plant_.growth < 1.0) {
    double growthInc = (hours / (24.0 * 7.0)) * weatherFactor * (plant_.health / 100.0);
    plant_.growth = std::min(1.0, plant_.growth + growthInc);
  }

  // Stage transitions
  Stage before = plant_.stage;
  plant_.stage = stageForGrowth(plant_.growth);
  if (before != plant_.stage) {
    if (plant_.stage == Stage::Mature) {
      addDiary(plant_, toTime, "The tree has reached full maturity!");
    } else {
      addDiary(plant_, toTime, "Grew into: " + stageName(plant_.stage) + ".");
    }
  }

  plant_.lastUpdated = toTime;
}

void Simulation::update(std::int64_t now, Weather weather) {
  if (now <= plant_.lastUpdated) {
    weather_ = weather;
    return;
  }

  // Guard against extreme future timestamps (cap at 365 days)
  std::int64_t totalSeconds = std::min<std::int64_t>(now - plant_.lastUpdated, 86400LL * 365);
  std::int64_t targetTime = plant_.lastUpdated + totalSeconds;

  // Step resolution: 30 minutes (1800s) for fine-grained simulation
  // Cap at 500 steps to guarantee instant computation even after months of downtime
  constexpr std::int64_t MaxSteps = 500;
  constexpr std::int64_t BaseStep = 1800; // 30 min
  std::int64_t stepSize = std::max<std::int64_t>(BaseStep, (totalSeconds + MaxSteps - 1) / MaxSteps);

  while (plant_.lastUpdated < targetTime) {
    std::int64_t nextTime = std::min(targetTime, plant_.lastUpdated + stepSize);
    Weather curWeather = weatherForDay(plant_.lastUpdated);
    step(nextTime, curWeather);
  }

  weather_ = weatherForDay(now);
}

void Simulation::water(std::int64_t now) {
  update(now, weatherForDay(now));
  bool wasOverwatered = plant_.moisture > 74.0;
  plant_.moisture = clampPercent(plant_.moisture + 26.0);
  plant_.lastWatered = now;

  if (plant_.moisture > 90.0) {
    addDiary(plant_, now, "Soil is waterlogged from overwatering.");
  } else if (wasOverwatered) {
    addDiary(plant_, now, "Watered while soil was already wet.");
  } else {
    addDiary(plant_, now, "The tree was watered.");
  }
}

void Simulation::reset(std::int64_t now) {
  plant_ = freshPlant(now);
  weather_ = weatherForDay(now);
  addDiary(plant_, now, "Life restarted by choice.");
}

}  // namespace littletree
