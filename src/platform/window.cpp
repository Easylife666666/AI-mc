#include "platform/window.h"

#include <GL/glew.h>
#include <spdlog/spdlog.h>

namespace aimc {

Window::~Window() { shutdown(); }

bool Window::init(const WindowConfig& cfg) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (cfg.hidden) {
        flags |= SDL_WINDOW_HIDDEN;
    }

    m_window = SDL_CreateWindow("AI-mc", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                cfg.width, cfg.height, flags);
    if (m_window == nullptr) {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    m_context = SDL_GL_CreateContext(m_window);
    if (m_context == nullptr) {
        spdlog::error("SDL_GL_CreateContext failed: {}", SDL_GetError());
        return false;
    }
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        spdlog::error("SDL_GL_MakeCurrent failed: {}", SDL_GetError());
        return false;
    }

    glewExperimental = GL_TRUE;
    const GLenum err = glewInit();
    if (err != GLEW_OK) {
        spdlog::error("glewInit failed: {}", reinterpret_cast<const char*>(glewGetErrorString(err)));
        return false;
    }
    // glewInit under a core profile can leave a spurious INVALID_ENUM behind.
    while (glGetError() != GL_NO_ERROR) {
    }

    SDL_GL_SetSwapInterval(cfg.vsync ? 1 : 0);

    m_width = cfg.width;
    m_height = cfg.height;
    spdlog::info("GL {} | {} | GLSL {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                 reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
                 reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
    return true;
}

void Window::shutdown() {
    if (m_context != nullptr) {
        SDL_GL_DeleteContext(m_context);
        m_context = nullptr;
    }
    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

void Window::swapBuffers() {
    if (m_window != nullptr) {
        SDL_GL_SwapWindow(m_window);
    }
}

bool Window::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        m_shouldClose = true;
        return true;
    }
    if (event.type == SDL_WINDOWEVENT &&
        (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         event.window.event == SDL_WINDOWEVENT_RESIZED)) {
        m_width = event.window.data1;
        m_height = event.window.data2;
        return true;
    }
    return false;
}

void Window::setRelativeMouse(bool enabled) {
    SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE);
}

void Window::setVsync(bool enabled) { SDL_GL_SetSwapInterval(enabled ? 1 : 0); }

void Window::setFullscreen(bool enabled) {
    if (m_window == nullptr) {
        return;
    }
    SDL_SetWindowFullscreen(m_window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    m_fullscreen = enabled;
}

}  // namespace aimc
