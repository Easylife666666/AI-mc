#pragma once

#include <SDL.h>

namespace aimc {

struct Camera;
class World;
struct DrawStats;

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
    void draw(const Camera& camera, const World& world, const DrawStats& stats, float dt);
    void endFrame();

    void toggle() { m_visible = !m_visible; }
    bool visible() const { return m_visible; }

private:
    bool m_initialized = false;
    bool m_visible = true;
    float m_fps = 0.0f;
};

}  // namespace aimc
