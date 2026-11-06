# LittleTree

LittleTree is a desktop companion: a small tree that lives in a transparent, borderless window. It develops from a seedling into a mature tree over about seven days of care, responding to watering, neglect, and simulated weather.

## Features

- **Growth Stages:** The tree evolves across 25 progressive stages from a planted seed into a full blossoming tree, rendered at 8× integer pixel scale.
- **Care Simulation:** Soil moisture depletes over time and replenishes with watering. The tree responds to overwatering, drought, and winter dormancy, taking about a week of steady care to reach full maturity.
- **Daily Weather:** Changes each day between sunny, cloudy, rain, storm, and snow based on the system date, requiring no internet connection.
- **Visual Feedback:** Foliage and bark colors shift dynamically across healthy green, faded olive, wilted yellow-brown, and brittle dry brown depending on plant health. Soil tones darken when waterlogged and lighten to pale dusty loam during drought. Upper canopy rows sway gently with time while the soil base remains stationary.
- **Growth Diary:** Persistent event log capped at 80 entries. Records initial planting, watering actions, overwatering alerts, drought onset and recovery, withering onset and recovery, snow dormancy transitions, stage advancements, maturity, and user resets. Entries are browsable directly within the application.
- **Companion Window:** Transparent 32-bit ARGB X11 window with drag-to-move support, `_NET_WM_STATE_ABOVE` always-on-top hint, and `WM_DELETE_WINDOW` shutdown handling.
- **Save State:** Progress is saved locally in plain text to `~/.local/share/littletree/state-v1.txt`. Completely offline with no network requests or telemetry.

## Requirements and Dependencies

Target environment: Ubuntu Linux ARM64 (AArch64) running an X11 or XWayland graphical session.

Build dependencies:
- C++20 compiler (GCC 11+ recommended)
- CMake 3.20 or newer
- pkg-config
- libX11 development headers (`libx11-dev`)
- libXrender development headers (`libxrender-dev`)

Install dependencies on Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libx11-dev libxrender-dev
```

## Building

Configure and build with CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

This produces two binaries in the `build/` directory:
- `LittleTree` (the companion application)
- `little_tree_tests` (the core simulation and persistence test runner)

## Running

Launch the application from an active graphical desktop session:

```bash
./build/LittleTree
```

## Controls

| Input | Action |
|---|---|
| **Left Click** | Water the tree (applies if mouse movement is within 4 pixels). |
| **Left Drag** | Reposition the companion window (movement beyond 4 pixels; does not water). |
| **Right Click** or **I** | Toggle the status overlay (displays stage, age in days, health, soil moisture, and weather). |
| **D** | Toggle the Growth Diary overlay. |
| **Up / Down Arrow** | Scroll through past diary entries while the diary is open (newest first). |
| **R** | Open the plant reset confirmation dialog. |
| **Y** | Confirm reset (only while reset dialog is active). |
| **N** | Cancel reset (only while reset dialog is active). |
| **Escape** | Cancel reset dialog if open; close diary or status overlay if open; exit application if no overlay is active. |

## Running Tests

Execute the automated test suite through CTest:

```bash
ctest --test-dir build --output-on-failure
```

Or run the test binary directly:

```bash
./build/little_tree_tests
```

The test suite covers weather determinism, stage boundaries, maturity clamping, watering and overwatering penalties, withering and recovery thresholds, dormancy preservation, diary ring-buffer capping, persistence roundtrips, invalid save rejection, and multi-day stepped offline progression.

## Save Data Location

Application state is saved on normal exit, on reset, and automatically every 30 seconds to:

- `$XDG_DATA_HOME/littletree/state-v1.txt`
- Default fallback: `~/.local/share/littletree/state-v1.txt`

The file is written in a versioned, human-readable format containing stage, growth, moisture, health, timestamps, weather, and diary entries. If the file is missing, unreadable, or contains out-of-range values, LittleTree safely initializes a fresh seedling.

## Compatibility and Notes

- **Window Management and Wayland:** LittleTree requests borderless and always-on-top presentation via `_NET_WM_WINDOW_TYPE_DOCK` and `_NET_WM_STATE_ABOVE`. Under Wayland compositors (such as GNOME Mutter), these X11 hints are handled by XWayland.
- **Display Server:** An active X11 display or XWayland server is required (`DISPLAY` must be set). Pure headless sessions or native Wayland without XWayland are not supported.
