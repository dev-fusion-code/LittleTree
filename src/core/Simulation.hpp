#pragma once
#include "core/Plant.hpp"
#include "core/Weather.hpp"
#include <utility>

namespace littletree {

class Simulation {
 public:
  explicit Simulation(Plant plant) : plant_(std::move(plant)) {}

  Plant& plant() { return plant_; }
  const Plant& plant() const { return plant_; }

  void update(std::int64_t now, Weather weather);
  void water(std::int64_t now);
  void reset(std::int64_t now);

  Weather weather() const { return weather_; }

 private:
  void step(std::int64_t toTime, Weather weather);

  Plant plant_;
  Weather weather_{Weather::Sunny};
};

}  // namespace littletree
