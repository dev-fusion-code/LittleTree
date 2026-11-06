#include "core/Storage.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace littletree {

std::filesystem::path Storage::defaultPath() {
  const char* xdg = std::getenv("XDG_DATA_HOME");
  std::filesystem::path base;
  if (xdg && *xdg) {
    base = std::filesystem::path(xdg);
  } else {
    const char* home = std::getenv("HOME");
    base = std::filesystem::path(home ? home : ".") / ".local" / "share";
  }
  return base / "littletree" / "state-v1.txt";
}

bool Storage::save(const std::filesystem::path& path, const Plant& p, Weather w) {
  try {
    std::filesystem::create_directories(path.parent_path());
    auto temp = path;
    temp += ".tmp";
    std::ofstream f(temp, std::ios::trunc);
    if (!f) return false;

    f << "LittleTree 1\n"
      << static_cast<int>(p.stage) << ' '
      << p.growth << ' '
      << p.moisture << ' '
      << p.health << ' '
      << p.plantedAt << ' '
      << p.lastWatered << ' '
      << p.lastUpdated << ' '
      << static_cast<int>(w) << "\n"
      << p.diary.size() << "\n";

    for (const auto& e : p.diary) {
      f << e.at << ' ' << e.text << "\n";
    }

    f.close();
    if (!f) return false;

    std::filesystem::rename(temp, path);
    return true;
  } catch (...) {
    return false;
  }
}

std::optional<std::pair<Plant, Weather>> Storage::load(const std::filesystem::path& path) {
  try {
    std::ifstream f(path);
    if (!f) return std::nullopt;

    std::string magic;
    int ver = 0;
    int stage = 0;
    int w = 0;
    Plant p;

    if (!(f >> magic >> ver) || magic != "LittleTree" || ver != 1) return std::nullopt;
    if (!(f >> stage >> p.growth >> p.moisture >> p.health >> p.plantedAt >> p.lastWatered >> p.lastUpdated >> w)) {
      return std::nullopt;
    }

    if (stage < 0 || stage > 4 || w < 0 || w > 4) return std::nullopt;
    if (p.growth < 0.0 || p.growth > 1.0) return std::nullopt;
    if (p.moisture < 0.0 || p.moisture > 100.0) return std::nullopt;
    if (p.health < 0.0 || p.health > 100.0) return std::nullopt;
    if (p.plantedAt < 0 || p.lastUpdated < p.plantedAt) return std::nullopt;

    p.stage = static_cast<Stage>(stage);
    p.withering = (p.health < 22.0);
    p.inDrought = (p.moisture < 20.0);

    std::size_t count = 0;
    if (!(f >> count) || count > 80) return std::nullopt;

    std::string dummy;
    std::getline(f, dummy); // consume remainder of the count line

    for (std::size_t i = 0; i < count; ++i) {
      DiaryEntry e;
      if (!(f >> e.at)) return std::nullopt;
      std::getline(f, e.text);
      if (!e.text.empty() && e.text.front() == ' ') {
        e.text.erase(0, 1);
      }
      if (e.text.size() > 200) return std::nullopt;
      p.diary.push_back(std::move(e));
    }

    return std::pair{p, static_cast<Weather>(w)};
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace littletree
