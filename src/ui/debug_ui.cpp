#include "ui/debug_ui.h"

#include <algorithm>
#include <cstdint>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <spdlog/spdlog.h>

#include "game/raycast.h"
#include "render/atlas.h"
#include "render/camera.h"
#include "world/block.h"
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

UiActions DebugUI::draw(const Camera& camera, const World& world, const DrawStats& stats,
                          float dt, Settings& settings, float& timeOfDay,
                          const std::string& status) {
    UiActions actions;
    if (!m_initialized) {
        return actions;
    }

    const float instant = dt > 0.0f ? std::min(1.0f / dt, 999.0f) : 0.0f;
    m_fps = m_fps <= 0.0f ? instant : m_fps * 0.90f + instant * 0.10f;

    if (!m_visible) {
        return actions;
    }

    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(350.0f, 0.0f), ImGuiCond_Once);
    if (ImGui::Begin("AI-mc", &m_visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("FPS  %.1f   (%.2f ms)", m_fps, dt * 1000.0f);
        ImGui::Text("camera  %.1f  %.1f  %.1f", camera.pos.x, camera.pos.y, camera.pos.z);
        ImGui::Text("chunks  %d / %d visible", stats.visibleChunks, stats.totalChunks);
        ImGui::Text("draw calls %d   triangles %lld", stats.drawCalls, stats.triangles);
        ImGui::Text("seed %d   time %.3f", world.seed(), timeOfDay);
        ImGui::Separator();

        ImGui::SliderInt("view distance", &settings.viewDistance, 3, 10);
        ImGui::Checkbox("adaptive view distance", &settings.adaptive);
        ImGui::SliderFloat("fov", &settings.fov, 50.0f, 100.0f, "%.0f");
        ImGui::SliderFloat("sensitivity", &settings.sensitivity, 0.02f, 0.30f, "%.3f");
        ImGui::SliderFloat("volume", &settings.volume, 0.0f, 1.0f, "%.2f");
        ImGui::Checkbox("hud", &settings.hud);
        ImGui::Checkbox("vsync", &settings.vsync);
        ImGui::SliderFloat("time of day", &timeOfDay, 0.0f, 0.999f, "%.3f");
        ImGui::SliderFloat("day length (s)", &settings.dayLength, 0.0f, 1800.0f, "%.0f");
        ImGui::Separator();

        if (ImGui::Button("Save world (F5)")) {
            actions.saveWorld = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load world (F9)")) {
            actions.loadWorld = true;
        }
        if (ImGui::Button("Save settings")) {
            actions.saveSettings = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Fullscreen")) {
            actions.toggleFullscreen = true;
        }
        if (ImGui::Button("New world")) {
            actions.newWorld = true;
        }

        ImGui::Separator();
        ImGui::TextWrapped("%s", status.c_str());
        ImGui::TextDisabled("WASD move  double-tap W sprint");
        ImGui::TextDisabled("Space jump  F fly  Shift boosts flight");
        ImGui::TextDisabled("LMB break  RMB place  1-9 select");
        ImGui::TextDisabled("F1 close panel and capture mouse  Esc quit");
    }
    ImGui::End();
    return actions;
}

void DebugUI::drawHud(const RayHit& hit, const int* hotbarBlocks, int hotbarCount, int selected,
                      const Atlas& atlas, bool flying) {
    if (!m_initialized) {
        return;
    }
    (void)hit;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;

    // Crosshair: dark outline + white core.
    const ImVec2 center(display.x * 0.5f, display.y * 0.5f);
    draw->AddLine(ImVec2(center.x - 9.0f, center.y), ImVec2(center.x + 9.0f, center.y),
                  IM_COL32(0, 0, 0, 190), 3.0f);
    draw->AddLine(ImVec2(center.x, center.y - 9.0f), ImVec2(center.x, center.y + 9.0f),
                  IM_COL32(0, 0, 0, 190), 3.0f);
    draw->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x + 8.0f, center.y),
                  IM_COL32(255, 255, 255, 235), 1.5f);
    draw->AddLine(ImVec2(center.x, center.y - 8.0f), ImVec2(center.x, center.y + 8.0f),
                  IM_COL32(255, 255, 255, 235), 1.5f);

    if (hotbarCount <= 0) {
        return;
    }

    const float slot = 50.0f;
    const float gap = 4.0f;
    const float total = static_cast<float>(hotbarCount) * (slot + gap) - gap;
    const float x0 = (display.x - total) * 0.5f;
    const float y0 = display.y - slot - 18.0f;
    const ImTextureID texture = static_cast<ImTextureID>(static_cast<std::intptr_t>(atlas.texture()));

    for (int i = 0; i < hotbarCount; ++i) {
        const ImVec2 p0(x0 + static_cast<float>(i) * (slot + gap), y0);
        const ImVec2 p1(p0.x + slot, p0.y + slot);
        const bool chosen = i == selected;
        draw->AddRectFilled(p0, p1, chosen ? IM_COL32(45, 45, 45, 225) : IM_COL32(20, 20, 20, 170),
                            5.0f);
        draw->AddRect(p0, p1, chosen ? IM_COL32(255, 255, 255, 255) : IM_COL32(90, 90, 90, 210),
                      5.0f, 0, chosen ? 2.0f : 1.0f);

        const BlockId block = static_cast<BlockId>(hotbarBlocks[i]);
        const glm::vec4 uv = atlas.tileUV(blockDef(block).tileSide);
        // OpenGL v=0 is the bottom of the atlas; ImGui's (0,0) is the top.
        draw->AddImage(texture, ImVec2(p0.x + 6.0f, p0.y + 6.0f), ImVec2(p1.x - 6.0f, p1.y - 6.0f),
                       ImVec2(uv.x, uv.w), ImVec2(uv.z, uv.y));

        char label[2] = {static_cast<char>('1' + i), '\0'};
        draw->AddText(ImVec2(p0.x + 4.0f, p0.y + 2.0f), IM_COL32(255, 255, 255, 190), label);
    }

    if (selected >= 0 && selected < hotbarCount) {
        const char* name = blockDef(static_cast<BlockId>(hotbarBlocks[selected])).name;
        const ImVec2 size = ImGui::CalcTextSize(name);
        draw->AddText(ImVec2((display.x - size.x) * 0.5f, y0 - 22.0f),
                      IM_COL32(255, 255, 255, 235), name);
    }

    if (flying) {
        const char* text = "FLYING";
        const ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2((display.x - size.x) * 0.5f, y0 - 44.0f),
                      IM_COL32(120, 220, 255, 220), text);
    }
}

bool DebugUI::wantsMouse() const {
    return m_initialized && ImGui::GetIO().WantCaptureMouse;
}

void DebugUI::endFrame() {    if (!m_initialized) {
        return;
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace aimc
