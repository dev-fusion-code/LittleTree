#pragma once
#include "core/Plant.hpp"
#include <string_view>

namespace littletree {

struct Sprite {
  int width;
  int height;
  std::string_view pixels;
};

const Sprite& spriteFor(Stage stage);
const Sprite& spriteFor(Stage stage, double growth);
unsigned long colorFor(char pixel, double health = 100.0, double moisture = 50.0);

}  // namespace littletree
