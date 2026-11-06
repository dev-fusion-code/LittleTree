#pragma once
#include "core/Simulation.hpp"
#include "graphics/Layout.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <string>

namespace littletree {

class Renderer {
 public:
  Renderer(Display* display, Window window, Visual* visual, int depth);
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void draw(const Simulation& sim, double seconds, bool status, bool diary, int diaryIndex, bool resetPrompt, double waterElapsed);

 private:
  void rect(int x, int y, int width, int height, unsigned long rgb, unsigned short alpha = 65535, int op = PictOpOver);
  void drawString(int x, int y, const std::string& text, unsigned long rgb, bool bold = false);

  Display* d_{};
  Window w_{};
  Visual* v_{};
  int depth_{};
  XRenderPictFormat* format_{};
  Picture picture_{};
  GC gc_{};
  XFontStruct* titleFont_{};
  XFontStruct* bodyFont_{};
};

}  // namespace littletree
