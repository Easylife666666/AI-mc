#include "ui/debug_ui.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <spdlog/spdlog.h>

#include "render/camera.h"
#include "world/world.h"

namespace aimc {

DebugUI::~DebugUI() { shutdown(); }

bool DebugUI::init(SDL_Window* window, SDL_GLContext context) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // do not litter the working directory
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 5.0f;
    ImGui::GetStyle().FrameRounding = 3.0f;

    if (!ImGui_ImplSDL2_InitForOpenGL(window, context)) {
        spdlog::error("ImGui SDL2 backend init failed");
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        spdlog::error("ImGui OpenGL3 backend init failed");
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    m_initialized = true;
    return true;
}

void DebugUI::shutdown() {
    if (!m_initialized) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void DebugUI::processEvent(const SDL_Event& event) {
    if (m_initialized) {
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
}

void DebugUI::beginFrame() {
    if (!m_initialized) {
        return;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void DebugUI::draw(const Camera& camera, const World& world, const DrawStats& stats, float dt) {
    if (!m_initialized) {
        return;
    }

    const float instant = dt > 0.0f ? std::min(1.0f / dt, 999.0f) : 0.0f;
    m_fps = m_fps <= 0.0f ? instant : m_fps * 0.90f + instant * 0.10f;

    if (!m_visible) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(330.0f, 0.0f), ImGuiCond_Once);
    if (ImGui::Begin("AI-mc", &m_visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("FPS  %.1f   (%.2f ms)", m_fps, dt * 1000.0f);
        ImGui::Separator();

        ImGui::Text("camera  %.1f  %.1f  %.1f", camera.pos.x, camera.pos.y, camera.pos.z);
        ImGui::Text("yaw %.1f   pitch %.1f", camera.yawDeg, camera.pitchDeg);
        ImGui::Separator();

        ImGui::Text("chunks      %zu", world.chunkCount());
        ImGui::Text("draw calls  %d", stats.drawCalls);
        ImGui::Text("triangles   %lld", stats.triangles);
        ImGui::Text("seed %d   radius %d", world.seed(), world.radius());
        ImGui::Separator();

        ImGui::TextDisabled("WASD move  Space/Ctrl up-down");
        ImGui::TextDisabled("Shift boost  F1 panel  Esc quit");
    }
    ImGui::End();
}

void DebugUI::endFrame() {
    if (!m_initialized) {
        return;
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace aimc
