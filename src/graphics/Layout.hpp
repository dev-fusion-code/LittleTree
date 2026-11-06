#pragma once

namespace littletree {

struct Layout {
  static constexpr int WindowWidth = 800;
  static constexpr int WindowHeight = 700;
  static constexpr int SpriteScale = 8;
  static constexpr int SpriteWidth = 64;
  static constexpr int SpriteHeight = 48;
  static constexpr int RenderedSpriteWidth = SpriteWidth * SpriteScale;   // 512
  static constexpr int RenderedSpriteHeight = SpriteHeight * SpriteScale; // 384

  // Centered horizontally: (800 - 512) / 2 = 144 (over 200px margin prevents clipping)
  static constexpr int TreeOffsetX = (WindowWidth - RenderedSpriteWidth) / 2;
  // Positioned below overlay with clean spacing: 210 + 384 = 594 (106px bottom margin)
  static constexpr int TreeOffsetY = 210;

  // Spacious overlay box positioned above tree canopy
  static constexpr int OverlayX = 40;
  static constexpr int OverlayY = 20;
  static constexpr int OverlayWidth = 720;
  static constexpr int OverlayHeight = 160;
};

}  // namespace littletree
