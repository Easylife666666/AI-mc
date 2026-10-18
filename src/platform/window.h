#pragma once

#include <SDL.h>

namespace aimc {

struct WindowConfig {
    int width = 1280;
    int height = 720;
    bool hidden = false;
    bool vsync = true;
};

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool init(const WindowConfig& cfg);
    void shutdown();
    void swapBuffers();

    // Returns true if the event was consumed by the window.
    bool handleEvent(const SDL_Event& event);
    bool shouldClose() const { return m_shouldClose; }

    SDL_Window* handle() const { return m_window; }
    SDL_GLContext context() const { return m_context; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    float aspect() const {
        return m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    }

    void setRelativeMouse(bool enabled);

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_shouldClose = false;
};

}  // namespace aimc
