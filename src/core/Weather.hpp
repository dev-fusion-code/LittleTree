#pragma once
#include <cstdint>
#include <string>

namespace littletree {

enum class Weather { Sunny, Cloudy, Rain, Storm, Snow };

std::string weatherName(Weather weather);
Weather weatherForDay(std::int64_t unixSeconds);

}  // namespace littletree
