#include "graphics/Renderer.hpp"
#include "graphics/Sprite.hpp"
#include <algorithm>
#include <cmath>

namespace littletree {

Renderer::Renderer(Display* d, Window w, Visual* v, int depth)
    : d_(d), w_(w), v_(v), depth_(depth),
      format_(XRenderFindVisualFormat(d, v)) {
  picture_ = XRenderCreatePicture(d_, w_, format_, 0, nullptr);
  gc_ = XCreateGC(d_, w_, 0, nullptr);

  // Load larger fonts for 560x460 layout (~14-18px)
  static const char* const titleCandidates[] = {
    "-*-*-bold-r-*-*-18-*-*-*-*-*-*-*",
    "-misc-fixed-bold-r-normal--18-*-*-*-*-*-*-*",
    "10x20",
    "-*-*-bold-r-*-*-16-*-*-*-*-*-*-*",
    "9x15bold",
    "9x15",
    "fixed"
  };
  for (const char* name : titleCandidates) {
    titleFont_ = XLoadQueryFont(d_, name);
    if (titleFont_) break;
  }

  static const char* const bodyCandidates[] = {
    "-*-*-medium-r-*-*-16-*-*-*-*-*-*-*",
    "-misc-fixed-medium-r-normal--16-*-*-*-*-*-*-*",
    "9x15",
    "-*-*-*-r-*-*-14-*-*-*-*-*-*-*",
    "8x16",
    "fixed"
  };
  for (const char* name : bodyCandidates) {
    bodyFont_ = XLoadQueryFont(d_, name);
    if (bodyFont_) break;
  }
}

Renderer::~Renderer() {
  if (titleFont_) XFreeFont(d_, titleFont_);
  if (bodyFont_) XFreeFont(d_, bodyFont_);
  if (gc_) XFreeGC(d_, gc_);
  if (picture_) XRenderFreePicture(d_, picture_);
}

void Renderer::rect(int x, int y, int width, int height, unsigned long rgb, unsigned short alpha, int op) {
  unsigned short r = static_cast<unsigned short>(((rgb >> 16) & 255) * 257);
  unsigned short g = static_cast<unsigned short>(((rgb >> 8) & 255) * 257);
  unsigned short b = static_cast<unsigned short>((rgb & 255) * 257);

  if (alpha != 65535) {
    r = static_cast<unsigned short>((static_cast<unsigned long>(r) * alpha) / 65535);
    g = static_cast<unsigned short>((static_cast<unsigned long>(g) * alpha) / 65535);
    b = static_cast<unsigned short>((static_cast<unsigned long>(b) * alpha) / 65535);
  }

  XRenderColor c{r, g, b, alpha};
  XRenderFillRectangle(d_, op, picture_, &c, x, y, width, height);
}

void Renderer::drawString(int x, int y, const std::string& text, unsigned long rgb, bool bold) {
  if (text.empty() || !gc_) return;

  XFontStruct* font = bold ? (titleFont_ ? titleFont_ : bodyFont_) : (bodyFont_ ? bodyFont_ : titleFont_);
  if (font) {
    XSetFont(d_, gc_, font->fid);
  }

  // Ensure alpha bits are full opacity 0xFF000000 on 32-bit ARGB visual
  XSetForeground(d_, gc_, 0xFF000000 | (rgb & 0x00FFFFFF));
  XDrawString(d_, w_, gc_, x, y, text.data(), static_cast<int>(text.size()));
}

void Renderer::draw(const Simulation& sim, double seconds, bool status, bool diary, int diaryIndex, bool resetPrompt, double waterElapsed) {
  // Clear full window canvas
  XRenderColor clear{0, 0, 0, 0};
  XRenderFillRectangle(d_, PictOpSrc, picture_, &clear, 0, 0, Layout::WindowWidth, Layout::WindowHeight);

  const auto& plant = sim.plant();
  const auto& s = spriteFor(plant.stage, plant.growth);

  // Draw tree sprite at 8x scale with organic canopy sway and stationary soil base
  for (int y = 0; y < s.height; ++y) {
    int rowSway = 0;
    // Rows 0..39 are canopy and upper trunk; rows 40..47 are rooted soil mound
    if (y < 40) {
      double flex = static_cast<double>(40 - y) / 40.0;
      // Integer sprite-pixel sway (max 1 sprite pixel at crown)
      rowSway = static_cast<int>(std::round(std::sin(seconds * 1.5 + y * 0.05) * 1.0 * flex));
    }

    for (int x = 0; x < s.width; ++x) {
      char c = s.pixels[y * s.width + x];
      if (c != '.') {
        unsigned long color = colorFor(c, plant.health, plant.moisture);
        rect(Layout::TreeOffsetX + (x + rowSway) * Layout::SpriteScale,
             Layout::TreeOffsetY + y * Layout::SpriteScale,
             Layout::SpriteScale, Layout::SpriteScale,
             color, 65535, PictOpOver);
      }
    }
  }

  // Weather particles spanning window
  if (sim.weather() == Weather::Rain || sim.weather() == Weather::Storm || sim.weather() == Weather::Snow) {
    bool isSnow = (sim.weather() == Weather::Snow);
    int count = (sim.weather() == Weather::Storm) ? 44 : 30;
    for (int i = 0; i < count; ++i) {
      int speedX = isSnow ? 18 : (sim.weather() == Weather::Storm ? 110 : 35);
      int speedY = isSnow ? 45 : (sim.weather() == Weather::Storm ? 140 : 95);
      int px = (i * 61 + static_cast<int>(seconds * speedX)) % (Layout::WindowWidth - 8);
      int py = (i * 79 + static_cast<int>(seconds * speedY)) % (Layout::WindowHeight - 8);
      int pw = isSnow ? 6 : (sim.weather() == Weather::Storm ? 3 : 2);
      int ph = isSnow ? 6 : (sim.weather() == Weather::Storm ? 18 : 12);
      unsigned long pColor = isSnow ? 0xeaf4fc : 0x82b2d2;
      rect(px, py, pw, ph, pColor, 46000, PictOpOver);
    }
  }

  // Dynamic watering effect: falling shower streaks, canopy impacts, bounce splashes, and soil glistening
  if (waterElapsed >= 0.0 && waterElapsed <= 2.2) {
    // 36 water droplets in a gentle shower
    struct DropConfig {
      int startX;
      int startY;
      double launchTime;
      int speed;
      int targetY;
    };

    // Precomputed deterministic distribution covering the wide canopy and soil mound
    static const DropConfig drops[36] = {
      {230, 115, 0.00, 390, 550}, {290, 130, 0.05, 420, 290}, {350, 110, 0.08, 400, 255},
      {400, 125, 0.02, 430, 245}, {450, 115, 0.12, 410, 260}, {510, 135, 0.06, 395, 300},
      {570, 120, 0.15, 415, 552}, {250, 105, 0.18, 405, 558}, {310, 140, 0.22, 425, 295},
      {370, 115, 0.14, 435, 250}, {420, 130, 0.25, 395, 250}, {470, 108, 0.20, 410, 275},
      {530, 125, 0.28, 420, 560}, {260, 135, 0.32, 400, 554}, {330, 112, 0.35, 430, 290},
      {370, 128, 0.38, 415, 255}, {410, 110, 0.30, 440, 248}, {460, 138, 0.42, 390, 278},
      {520, 122, 0.36, 425, 310}, {560, 114, 0.45, 405, 562}, {280, 130, 0.50, 410, 556},
      {340, 108, 0.48, 435, 285}, {390, 136, 0.52, 400, 248}, {430, 116, 0.55, 420, 265},
      {480, 132, 0.58, 430, 295}, {540, 110, 0.62, 395, 564}, {240, 124, 0.65, 415, 558},
      {300, 140, 0.70, 425, 292}, {350, 112, 0.68, 440, 260}, {400, 134, 0.72, 405, 246},
      {440, 118, 0.75, 415, 275}, {500, 130, 0.78, 430, 305}, {560, 116, 0.82, 400, 560},
      {360, 128, 0.85, 420, 258}, {410, 112, 0.88, 435, 252}, {460, 135, 0.92, 410, 288}
    };

    for (const auto& d : drops) {
      if (waterElapsed < d.launchTime) {
        continue;
      }
      double dt = waterElapsed - d.launchTime;
      double flightTime = static_cast<double>(d.targetY - d.startY) / d.speed;

      if (dt < flightTime) {
        // Droplet is descending
        int curY = static_cast<int>(d.startY + d.speed * dt);
        // Falling streak: bright core with softer trailing streak
        rect(d.startX, curY, 2, 8, 0x85cbff, 54000, PictOpOver);
        rect(d.startX, curY + 4, 2, 4, 0xdcf4ff, 62000, PictOpOver);
      } else {
        // Droplet reached target (canopy foliage or soil): trigger impact splash
        double splashDt = dt - flightTime;
        if (splashDt < 0.28) {
          double splashProgress = splashDt / 0.28;
          auto alpha = static_cast<unsigned short>(56000 * (1.0 - splashProgress));

          // Two tiny droplets bounce outwards in parabolic arcs
          int p1x = static_cast<int>(d.startX - 18.0 * splashDt);
          int p1y = static_cast<int>(d.targetY - 38.0 * splashDt + 95.0 * splashDt * splashDt);
          int p2x = static_cast<int>(d.startX + 22.0 * splashDt);
          int p2y = static_cast<int>(d.targetY - 46.0 * splashDt + 105.0 * splashDt * splashDt);

          rect(p1x, p1y, 2, 2, 0xddf5ff, alpha, PictOpOver);
          rect(p2x, p2y, 2, 2, 0x9de2ff, alpha, PictOpOver);

          // Small impact ripple at strike point
          if (splashDt < 0.12) {
            rect(d.startX - 3, d.targetY, 7, 2, 0x82d4f8, static_cast<unsigned short>(48000 * (1.0 - splashDt / 0.12)), PictOpOver);
          }
        }
      }
    }

    // Dewdrop twinkles on the upper canopy foliage (t = 0.3s to 2.0s)
    if (waterElapsed >= 0.3 && waterElapsed <= 2.0) {
      static const struct { int x; int y; double phase; } dewdrops[] = {
        {330, 260, 0.0}, {400, 245, 1.2}, {460, 255, 2.4},
        {300, 290, 3.1}, {380, 275, 4.3}, {490, 295, 0.8},
        {350, 315, 2.0}, {450, 305, 5.0}
      };
      for (const auto& dw : dewdrops) {
        double cycle = std::sin((waterElapsed - 0.3) * 7.0 + dw.phase);
        if (cycle > 0.3) {
          auto alpha = static_cast<unsigned short>(25000 + 35000 * ((cycle - 0.3) / 0.7));
          rect(dw.x, dw.y, 3, 3, 0xffffff, alpha, PictOpOver);
          rect(dw.x + 1, dw.y - 1, 1, 5, 0xaff0ff, alpha / 2, PictOpOver);
          rect(dw.x - 1, dw.y + 1, 5, 1, 0xaff0ff, alpha / 2, PictOpOver);
        }
      }
    }

    // Glistening moisture shimmer on the freshly watered soil bed (t = 0.4s to 2.2s)
    if (waterElapsed >= 0.4) {
      double soilFade = (waterElapsed > 1.8) ? (2.2 - waterElapsed) / 0.4 : 1.0;
      static const struct { int x; int y; double phase; } glints[] = {
        {250, 542, 0.2}, {290, 558, 1.5}, {340, 548, 2.7}, {390, 564, 0.9},
        {430, 546, 3.4}, {470, 562, 2.1}, {510, 550, 4.5}, {550, 566, 1.1},
        {270, 570, 3.8}, {320, 564, 0.5}, {370, 574, 2.3}, {410, 566, 4.1},
        {450, 572, 1.8}, {490, 566, 3.0}, {530, 574, 5.2}, {560, 560, 2.6}
      };
      for (const auto& g : glints) {
        double pulse = std::sin((waterElapsed - 0.4) * 6.5 + g.phase);
        if (pulse > 0.2) {
          auto alpha = static_cast<unsigned short>((28000 + 34000 * ((pulse - 0.2) / 0.8)) * soilFade);
          rect(g.x, g.y, 3, 2, 0xb8eeff, alpha, PictOpOver);
          rect(g.x + 1, g.y, 1, 2, 0xffffff, alpha, PictOpOver);
        }
      }
    }
  }

  // Interface overlays (positioned above the tree canopy without overlap)
  if (resetPrompt) {
    // Reset confirmation takes precedence over everything
    rect(Layout::OverlayX, Layout::OverlayY, Layout::OverlayWidth, Layout::OverlayHeight, 0x2a1212, 62000, PictOpOver);
    rect(Layout::OverlayX, Layout::OverlayY, Layout::OverlayWidth, 1, 0x5a2020, 50000, PictOpOver);
    rect(Layout::OverlayX, Layout::OverlayY + Layout::OverlayHeight - 1, Layout::OverlayWidth, 1, 0x5a2020, 50000, PictOpOver);
    rect(Layout::OverlayX, Layout::OverlayY, 1, Layout::OverlayHeight, 0x5a2020, 50000, PictOpOver);
    rect(Layout::OverlayX + Layout::OverlayWidth - 1, Layout::OverlayY, 1, Layout::OverlayHeight, 0x5a2020, 50000, PictOpOver);

    drawString(64, 52, "Reset Plant Progress?", 0xffdf9e, true);

    rect(64, 68, Layout::OverlayWidth - 48, 60, 0x361616, 48000, PictOpOver);
    rect(64, 68, Layout::OverlayWidth - 48, 1, 0x642424, 50000, PictOpOver);
    drawString(80, 104, "All growth, stage progress, and diary history will be cleared.", 0xffcccc);

    drawString(64, 154, "Press Y to confirm reset   •   Press N or Esc to cancel", 0xffffff, true);
  } else if (diary) {
    rect(Layout::OverlayX, Layout::OverlayY, Layout::OverlayWidth, Layout::OverlayHeight, 0x111e16, 60000, PictOpOver);
    rect(Layout::OverlayX, Layout::OverlayY, Layout::OverlayWidth, 1, 0x22382a, 50000, PictOpOver);
    rect(Layout::OverlayX, Layout::OverlayY + Layout::OverlayHeight - 1, Layout::OverlayWidth, 1, 0x22382a, 50000, PictOpOver);
    rect(Layout::OverlayX, Layout::OverlayY, 1, Layout::OverlayHeight, 0x22382a, 50000, PictOpOver);
    rect(Layout::OverlayX + Layout::OverlayWidth - 1, Layout::OverlayY, 1, Layout::OverlayHeight, 0x22382a, 50000, PictOpOver);

    const auto& diaryList = plant.diary;
    if (diaryList.empty()) {
      drawString(64, 52, "Growth Diary", 0xc6efce, true);
      rect(64, 68, Layout::OverlayWidth - 48, 60, 0x16261c, 48000, PictOpOver);
      drawString(80, 104, "No diary entries recorded yet.", 0xdddddd);
      drawString(64, 154, "D: close diary   •   R: reset plant   •   Esc: close", 0x88aa90);
    } else {
      int total = static_cast<int>(diaryList.size());
      int safeIdx = std::clamp(diaryIndex, 0, total - 1);
      // Newest first
      const auto& entry = diaryList[static_cast<std::size_t>(total - 1 - safeIdx)];

      std::int64_t diffSec = std::max<std::int64_t>(0, plant.lastUpdated - entry.at);
      std::string timeStr;
      if (diffSec < 60) {
        timeStr = "just now";
      } else if (diffSec < 3600) {
        timeStr = std::to_string(diffSec / 60) + "m ago";
      } else if (diffSec < 86400) {
        timeStr = std::to_string(diffSec / 3600) + "h ago";
      } else {
        timeStr = std::to_string(diffSec / 86400) + "d ago";
      }

      std::string header = "Growth Diary  [" + std::to_string(safeIdx + 1) + " / " + std::to_string(total) + "]";
      drawString(64, 52, header, 0xc6efce, true);
      drawString(600, 52, "•  " + timeStr, 0x98b89e);

      rect(64, 68, Layout::OverlayWidth - 48, 60, 0x16261c, 48000, PictOpOver);
      rect(64, 68, Layout::OverlayWidth - 48, 1, 0x274431, 50000, PictOpOver);
      drawString(80, 104, entry.text, 0xffffff);

      drawString(64, 154, "▲ / ▼: browse entries   •   D: close diary   •   R: reset plant   •   Esc: close", 0x88aa90);
    }
  } else if (status) {
    rect(Layout::OverlayX, Layout::OverlayY, Layout::OverlayWidth, Layout::OverlayHeight, 0x121d15, 58000, PictOpOver);
    rect(Layout::OverlayX, Layout::OverlayY, Layout::OverlayWidth, 1, 0x22382a, 50000, PictOpOver);
    rect(Layout::OverlayX, Layout::OverlayY + Layout::OverlayHeight - 1, Layout::OverlayWidth, 1, 0x22382a, 50000, PictOpOver);
    rect(Layout::OverlayX, Layout::OverlayY, 1, Layout::OverlayHeight, 0x22382a, 50000, PictOpOver);
    rect(Layout::OverlayX + Layout::OverlayWidth - 1, Layout::OverlayY, 1, Layout::OverlayHeight, 0x22382a, 50000, PictOpOver);

    auto ageDays = (plant.lastUpdated - plant.plantedAt) / 86400;
    std::string line1 = stageName(plant.stage) + "  •  Day " + std::to_string(ageDays + 1);
    drawString(64, 52, line1, 0xc6efce, true);

    std::string condition = plant.dormant ? "Dormant"
                          : plant.withering ? "Withering"
                          : (plant.health > 80.0) ? "Thriving"
                          : "Growing";
    unsigned long condColor = plant.dormant ? 0xbed4e8
                            : plant.withering ? 0xffb0b0
                            : (plant.health > 80.0) ? 0x98e8a8
                            : 0xd8e8b0;
    drawString(600, 52, "[ " + condition + " ]", condColor, true);

    rect(64, 66, Layout::OverlayWidth - 48, 1, 0x24392b, 40000, PictOpOver);

    // Stat line with clear separation
    drawString(64, 88, "Health:  " + std::to_string(static_cast<int>(plant.health)) + "%", 0xffffff);
    drawString(280, 88, "Soil:  " + std::to_string(static_cast<int>(plant.moisture)) + "%", 0xffffff);
    drawString(520, 88, "Weather:  " + weatherName(sim.weather()), 0xffffff);

    // Dynamic guidance text based on plant condition
    std::string guide;
    if (plant.dormant) {
      guide = "Winter snow has paused growth. The tree is resting safely.";
    } else if (plant.withering) {
      guide = "Critical neglect! Keep soil balanced to nurse the tree back to health.";
    } else if (plant.inDrought) {
      guide = "Soil is parched! Click to water the tree.";
    } else if (plant.moisture > 76.0) {
      guide = "Soil is saturated. Avoid overwatering to prevent root rot.";
    } else {
      guide = "Soil moisture and weather conditions are well balanced for growth.";
    }
    drawString(64, 118, guide, 0xa4c4ab);

    rect(64, 136, Layout::OverlayWidth - 48, 1, 0x24392b, 40000, PictOpOver);
    drawString(64, 154, "Click: water   •   Right-Click / I: status   •   D: diary   •   R: reset", 0x88aa90);
  }

  XFlush(d_);
}

}  // namespace littletree
