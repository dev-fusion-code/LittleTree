#include "core/Weather.hpp"

namespace littletree {

std::string weatherName(Weather w) {
  static const char* const names[] = {"Sunny", "Cloudy", "Rain", "Storm", "Snow"};
  auto idx = static_cast<std::size_t>(w);
  if (idx < 5) return names[idx];
  return "Sunny";
}

Weather weatherForDay(std::int64_t t) {
  if (t < 0) t = 0;
  auto d = static_cast<unsigned long long>(t / 86400);
  auto v = d * 1103515245ULL + 12345ULL;
  switch ((v >> 16) % 5) {
    case 0: return Weather::Sunny;
    case 1: return Weather::Cloudy;
    case 2: return Weather::Rain;
    case 3: return Weather::Storm;
    default: return Weather::Snow;
  }
}

}  // namespace littletree
