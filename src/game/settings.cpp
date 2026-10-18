#include "game/settings.h"

#include <algorithm>
#include <fstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace aimc {

bool loadSettings(const std::string& path, Settings& settings) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    try {
        nlohmann::json json;
        in >> json;
        settings.viewDistance = json.value("viewDistance", settings.viewDistance);
        settings.fov = json.value("fov", settings.fov);
        settings.sensitivity = json.value("sensitivity", settings.sensitivity);
        settings.volume = json.value("volume", settings.volume);
        settings.dayLength = json.value("dayLength", settings.dayLength);
        settings.vsync = json.value("vsync", settings.vsync);
        settings.adaptive = json.value("adaptive", settings.adaptive);
        settings.audio = json.value("audio", settings.audio);
        settings.hud = json.value("hud", settings.hud);
    } catch (const std::exception& error) {
        spdlog::warn("could not parse {}: {}", path, error.what());
        return false;
    }

    settings.viewDistance = std::clamp(settings.viewDistance, 3, 10);
    settings.fov = std::clamp(settings.fov, 50.0f, 100.0f);
    settings.sensitivity = std::clamp(settings.sensitivity, 0.02f, 0.30f);
    settings.volume = std::clamp(settings.volume, 0.0f, 1.0f);
    settings.dayLength = std::clamp(settings.dayLength, 0.0f, 3600.0f);
    spdlog::info("settings loaded from {}", path);
    return true;
}

bool saveSettings(const std::string& path, const Settings& settings) {
    nlohmann::json json;
    json["viewDistance"] = settings.viewDistance;
    json["fov"] = settings.fov;
    json["sensitivity"] = settings.sensitivity;
    json["volume"] = settings.volume;
    json["dayLength"] = settings.dayLength;
    json["vsync"] = settings.vsync;
    json["adaptive"] = settings.adaptive;
    json["audio"] = settings.audio;
    json["hud"] = settings.hud;

    std::ofstream out(path);
    if (!out) {
        spdlog::error("could not write settings to {}", path);
        return false;
    }
    out << json.dump(2) << '\n';
    return static_cast<bool>(out);
}

}  // namespace aimc
