#pragma once

#include <string>

namespace aimc {

struct Settings {
    int viewDistance = 7;
    float fov = 70.0f;
    float sensitivity = 0.10f;
    float volume = 0.7f;
    float dayLength = 600.0f;
    bool vsync = true;
    bool adaptive = true;
    bool audio = true;
    bool hud = true;
};

bool loadSettings(const std::string& path, Settings& settings);
bool saveSettings(const std::string& path, const Settings& settings);

}  // namespace aimc
