#include "core/Simulation.hpp"
#include "core/Storage.hpp"
#include "graphics/Renderer.hpp"
#include "platform/linux/Window.hpp"
#include <X11/keysym.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace littletree;

static std::int64_t nowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

int main() {
  try {
    auto path = Storage::defaultPath();
    auto now = nowSeconds();
    auto loaded = Storage::load(path);

    Simulation sim(loaded ? loaded->first : freshPlant(now));
    Weather savedWeather = loaded ? loaded->second : weatherForDay(now);
    sim.update(now, savedWeather);

    LinuxWindow shell;
    shell.show();

    Renderer renderer(shell.display(), shell.window(), shell.visual(), shell.depth());

    bool running = true;
    bool status = false;
    bool diary = false;
    int diaryIndex = 0;
    bool confirm = false;
    bool watered = false;

    bool button1Down = false;
    bool isDragging = false;
    int pressX = 0;
    int pressY = 0;
    int lastRootX = 0;
    int lastRootY = 0;

    auto nowSteady = std::chrono::steady_clock::now();
    auto statusUntil = nowSteady;
    auto waterStartTime = nowSteady - std::chrono::seconds(10);
    auto lastSave = nowSteady;

    while (running) {
      XEvent e;
      while (shell.nextEvent(e)) {
        if (e.type == ClientMessage) {
          if (static_cast<Atom>(e.xclient.data.l[0]) == shell.wmDeleteMessage()) {
            running = false;
          }
        } else if (e.type == ButtonPress) {
          if (e.xbutton.button == Button1) {
            button1Down = true;
            isDragging = false;
            pressX = e.xbutton.x_root;
            pressY = e.xbutton.y_root;
            lastRootX = pressX;
            lastRootY = pressY;
          } else if (e.xbutton.button == Button3) {
            status = !status;
            if (status) {
              statusUntil = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            }
          }
        } else if (e.type == ButtonRelease) {
          if (e.xbutton.button == Button1 && button1Down) {
            if (!isDragging) {
              // Genuine click without drag: water the plant
              sim.water(nowSeconds());
              watered = true;
              waterStartTime = std::chrono::steady_clock::now();
            }
            button1Down = false;
            isDragging = false;
          }
        } else if (e.type == MotionNotify && button1Down) {
          int totalDx = std::abs(e.xmotion.x_root - pressX);
          int totalDy = std::abs(e.xmotion.y_root - pressY);
          if (!isDragging && (totalDx > 5 || totalDy > 5)) {
            isDragging = true;
          }
          if (isDragging) {
            int stepX = e.xmotion.x_root - lastRootX;
            int stepY = e.xmotion.y_root - lastRootY;
            if (stepX != 0 || stepY != 0) {
              XWindowAttributes attr;
              XGetWindowAttributes(shell.display(), shell.window(), &attr);
              XMoveWindow(shell.display(), shell.window(), attr.x + stepX, attr.y + stepY);
              lastRootX = e.xmotion.x_root;
              lastRootY = e.xmotion.y_root;
            }
          }
        } else if (e.type == KeyPress) {
          KeySym ks = XLookupKeysym(&e.xkey, 0);
          char c = 0;
          XLookupString(&e.xkey, &c, 1, nullptr, nullptr);

          if (ks == XK_Escape) {
            if (confirm) {
              confirm = false;
            } else if (diary) {
              diary = false;
            } else if (status) {
              status = false;
            } else {
              running = false;
            }
          } else if (confirm) {
            if (ks == XK_y || ks == XK_Y || c == 'y' || c == 'Y') {
              sim.reset(nowSeconds());
              confirm = false;
              diary = false;
              diaryIndex = 0;
            } else if (ks == XK_n || ks == XK_N || c == 'n' || c == 'N') {
              confirm = false;
            }
          } else {
            if (ks == XK_r || ks == XK_R || c == 'r' || c == 'R') {
              confirm = true;
            } else if (ks == XK_d || ks == XK_D || c == 'd' || c == 'D') {
              diary = !diary;
              if (diary) {
                diaryIndex = 0;
              }
            } else if (ks == XK_i || ks == XK_I || c == 'i' || c == 'I') {
              status = !status;
              if (status) {
                statusUntil = std::chrono::steady_clock::now() + std::chrono::seconds(8);
              }
            } else if (ks == XK_Up) {
              if (diary && diaryIndex > 0) {
                diaryIndex--;
              }
            } else if (ks == XK_Down) {
              if (diary && diaryIndex + 1 < static_cast<int>(sim.plant().diary.size())) {
                diaryIndex++;
              }
            }
          }
        }
      }

      now = nowSeconds();
      sim.update(now, weatherForDay(now));

      auto steady = std::chrono::steady_clock::now();
      if (status && steady > statusUntil) {
        status = false;
      }
      double waterElapsed = -1.0;
      if (watered) {
        double elapsed = std::chrono::duration<double>(steady - waterStartTime).count();
        if (elapsed <= 2.2) {
          waterElapsed = elapsed;
        } else {
          watered = false;
        }
      }

      double elapsedSec = std::chrono::duration<double>(steady.time_since_epoch()).count();
      renderer.draw(sim, elapsedSec, status, diary, diaryIndex, confirm, waterElapsed);

      if (steady - lastSave > std::chrono::seconds(30)) {
        Storage::save(path, sim.plant(), sim.weather());
        lastSave = steady;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    Storage::save(path, sim.plant(), sim.weather());
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "LittleTree: " << e.what() << '\n';
    return 1;
  }
}
