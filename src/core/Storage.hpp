#pragma once
#include "core/Plant.hpp"
#include "core/Weather.hpp"
#include <filesystem>
#include <optional>
#include <utility>

namespace littletree {

class Storage {
 public:
  static std::filesystem::path defaultPath();
  static bool save(const std::filesystem::path& path, const Plant& plant, Weather weather);
  static std::optional<std::pair<Plant, Weather>> load(const std::filesystem::path& path);
};

}  // namespace littletree
