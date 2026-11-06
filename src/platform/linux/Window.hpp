#pragma once
#include "graphics/Layout.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

namespace littletree {

class LinuxWindow {
 public:
  static constexpr int DefaultWidth = Layout::WindowWidth;
  static constexpr int DefaultHeight = Layout::WindowHeight;

  explicit LinuxWindow(int width = DefaultWidth, int height = DefaultHeight);
  ~LinuxWindow();
  LinuxWindow(const LinuxWindow&) = delete;
  LinuxWindow& operator=(const LinuxWindow&) = delete;

  bool valid() const { return display_ != nullptr; }
  Display* display() const { return display_; }
  Window window() const { return window_; }
  Visual* visual() const { return visual_; }
  int depth() const { return depth_; }
  int width() const { return width_; }
  int height() const { return height_; }
  Atom wmDeleteMessage() const { return wmDeleteMessage_; }

  bool nextEvent(XEvent& e);
  void show();

 private:
  Display* display_{};
  Window window_{};
  Visual* visual_{};
  int depth_{};
  int width_{DefaultWidth};
  int height_{DefaultHeight};
  Colormap cmap_{};
  Atom wmDeleteMessage_{};
};

}  // namespace littletree
