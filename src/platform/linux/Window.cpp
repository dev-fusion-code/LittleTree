#include "platform/linux/Window.hpp"
#include <X11/Xatom.h>
#include <stdexcept>

namespace littletree {

LinuxWindow::LinuxWindow(int width, int height) : width_(width), height_(height) {
  display_ = XOpenDisplay(nullptr);
  if (!display_) {
    throw std::runtime_error("Unable to open an X display. Run LittleTree in an X11 or XWayland session.");
  }

  int screen = DefaultScreen(display_);
  XVisualInfo tpl{};
  tpl.screen = screen;
  tpl.depth = 32;
  int n = 0;
  XVisualInfo* info = XGetVisualInfo(display_, VisualScreenMask | VisualDepthMask, &tpl, &n);
  for (int i = 0; info && i < n; i++) {
    auto fmt = XRenderFindVisualFormat(display_, info[i].visual);
    if (fmt && fmt->type == PictTypeDirect && fmt->direct.alphaMask != 0) {
      visual_ = info[i].visual;
      depth_ = info[i].depth;
      break;
    }
  }
  if (info) XFree(info);

  if (!visual_) {
    XCloseDisplay(display_);
    display_ = nullptr;
    throw std::runtime_error("No 32-bit ARGB XRender visual is available.");
  }

  cmap_ = XCreateColormap(display_, RootWindow(display_, screen), visual_, AllocNone);
  XSetWindowAttributes a{};
  a.colormap = cmap_;
  a.border_pixel = 0;
  a.background_pixel = 0;
  a.override_redirect = False;
  a.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask | StructureNotifyMask;

  window_ = XCreateWindow(display_, RootWindow(display_, screen), 80, 80, width_, height_, 0, depth_,
                          InputOutput, visual_, CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &a);

  // Set window type DOCK to hint borderless companion floating
  Atom type = XInternAtom(display_, "_NET_WM_WINDOW_TYPE", False);
  Atom dock = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_DOCK", False);
  XChangeProperty(display_, window_, type, XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&dock), 1);

  // Set always on top
  Atom state = XInternAtom(display_, "_NET_WM_STATE", False);
  Atom above = XInternAtom(display_, "_NET_WM_STATE_ABOVE", False);
  XChangeProperty(display_, window_, state, XA_ATOM, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&above), 1);

  // Set WM_DELETE_WINDOW protocol
  wmDeleteMessage_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display_, window_, &wmDeleteMessage_, 1);

  XStoreName(display_, window_, "LittleTree");
}

LinuxWindow::~LinuxWindow() {
  if (display_) {
    if (window_) XDestroyWindow(display_, window_);
    if (cmap_) XFreeColormap(display_, cmap_);
    XCloseDisplay(display_);
  }
}

bool LinuxWindow::nextEvent(XEvent& e) {
  return XPending(display_) && XNextEvent(display_, &e) == 0;
}

void LinuxWindow::show() {
  XMapRaised(display_, window_);
  XFlush(display_);
}

}  // namespace littletree
