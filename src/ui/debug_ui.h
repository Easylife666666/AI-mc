#pragma once

#include <SDL.h>
#include <string>

#include "game/settings.h"

namespace aimc {

struct Camera;
class World;
struct DrawStats;
struct RayHit;
class Atlas;

struct UiActions {
    bool saveWorld = false;
    bool loadWorld = false;
    bool saveSettings = false;
    bool toggleFullscreen = false;
    bool newWorld = false;
};

class DebugUI {
public:
    DebugUI() = default;
    ~DebugUI();

    DebugUI(const DebugUI&) = delete;
    DebugUI& operator=(const DebugUI&) = delete;

    bool init(SDL_Window* window, SDL_GLContext context);
    void shutdown();
    void processEvent(const SDL_Event& event);

    void beginFrame();
    UiActions draw(const Camera& camera, const World& world, const DrawStats& stats, float dt,
                   Settings& settings, float& timeOfDay, const std::string& status);
    void drawHud(const RayHit& hit, const int* hotbarBlocks, int hotbarCount, int selected,
                 const Atlas& atlas, bool flying);
    void endFrame();

    void toggle() { m_visible = !m_visible; }
    bool visible() const { return m_visible; }
    bool wantsMouse() const;

private:
    bool m_initialized = false;
    bool m_visible = true;
    float m_fps = 0.0f;
};

}  // namespace aimc
