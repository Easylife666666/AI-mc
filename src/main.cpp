#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include <SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "audio/audio.h"
#include "core/log.h"
#include "game/creative.h"
#include "game/player.h"
#include "game/raycast.h"
#include "game/settings.h"
#include "platform/window.h"
#include "render/atlas.h"
#include "render/camera.h"
#include "render/frustum.h"
#include "render/screenshot.h"
#include "render/selection.h"
#include "render/shader.h"
#include "render/sky.h"
#include "ui/creative_screen.h"
#include "ui/debug_ui.h"
#include "world/block.h"
#include "world/chunk.h"
#include "world/portal.h"
#include "world/save.h"
#include "world/world.h"

namespace {

const char* kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in float aLight;

uniform mat4 uViewProj;

out vec2 vUV;
out float vLight;
out vec3 vWorld;

void main() {
    vUV = aUV;
    vLight = aLight;
    vWorld = aPos;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)GLSL";

const char* kFragmentShader = R"GLSL(
#version 330 core
in vec2 vUV;
in float vLight;
in vec3 vWorld;

uniform sampler2D uAtlas;
uniform vec3 uCamPos;
uniform vec3 uFogColor;
uniform vec2 uFog;
uniform float uLight;
uniform float uAlpha;

out vec4 FragColor;

void main() {
    vec3 albedo = texture(uAtlas, vUV).rgb;
    vec3 color = albedo * vLight * uLight;
    float dist = distance(vWorld, uCamPos);
    float fog = clamp((dist - uFog.x) / max(uFog.y - uFog.x, 0.001), 0.0, 1.0);
    color = mix(color, uFogColor, fog);
    FragColor = vec4(color, uAlpha);
}
)GLSL";

struct Options {
    int width = 1280;
    int height = 720;
    bool hidden = false;
    bool vsync = true;
    bool vsyncGiven = false;
    bool hud = true;
    bool hudGiven = false;
    bool audioEnabled = true;
    bool audioGiven = false;
    bool verbose = false;
    float timeOfDay = 0.42f;
    float dayLength = 600.0f;
    bool dayLengthGiven = false;
    int seed = 1337;
    bool seedGiven = false;
    int viewDistance = 7;
    bool viewGiven = false;
    int frames = 4;
    float fogStart = -1.0f;
    float fogEnd = -1.0f;
    float benchmark = 0.0f;
    float quitAfter = 0.0f;
    bool panel = false;
    bool inventory = false;
    std::string tab;
    std::string shot;
    std::string savePath = "save.aimc";
    std::string loadPath;
    bool newWorld = false;
    bool autosave = true;
    bool hasCamera = false;
    aimc::Camera camera;
    bool showHelp = false;
};

void printUsage(const char* argv0) {
    std::printf(
        "AI-mc -- voxel sandbox (M5)\n"
        "\n"
        "usage: %s [options]\n"
        "  --width N          window width  (default 1280)\n"
        "  --height N         window height (default 720)\n"
        "  --view N           view distance in chunks (default 7)\n"
        "  --radius N         alias for --view\n"
        "  --seed N           world seed for a new world (default 1337)\n"
        "  --new              ignore save.aimc and start a new world\n"
        "  --load PATH        load a save file\n"
        "  --save PATH        save file used by F5 / autosave (default save.aimc)\n"
        "  --no-autosave      do not save on exit\n"
        "  --cam x,y,z,yaw,pitch\n"
        "                     fixed camera pose (screenshot mode)\n"
        "  --shot PATH        render, save a PNG, then exit\n"
        "  --frames N         frames to render before the screenshot (default 4)\n"
        "  --fog start,end    fog range in blocks (default: derived from --view)\n"
        "  --time T           time of day 0..1 (0.25 sunrise, 0.5 noon, 0.75 sunset)\n"
        "  --day-length N     seconds per in-game day, 0 freezes time (default 600)\n"
        "  --benchmark N      run N seconds, print average fps, then exit\n"
        "  --quit-after N     quit cleanly after N seconds (used for tests)\n"
        "  --panel            show the debug panel in screenshot mode\n"
        "  --inventory        open the creative inventory (screenshot/demo mode)\n"
        "  --tab NAME         creative tab: building, natural, nether, tools or search\n"
        "  --hidden           create a hidden window\n"
        "  --no-vsync         disable vsync\n"
        "  --no-hud           hide crosshair and hotbar\n"
        "  --no-audio         disable sound output\n"
        "  --verbose          debug logging\n"
        "  --help             this text\n"
        "\n"
        "controls: WASD move, Space jump, double-tap W to sprint, F fly,\n"
        "          Shift boosts flight, Ctrl down (fly), E creative inventory,\n"
        "          RMB place, flint & steel lights obsidian frames,\n"
        "          LMB break, 1-9/wheel select, F1 panel, F5 save,\n"
        "          F9 load, M mute, Esc quit\n",
        argv0);
}

Options parseArgs(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto nextValue = [&]() -> std::string {
            if (i + 1 < argc) {
                return argv[++i];
            }
            return {};
        };

        if (arg == "--width") {
            opt.width = std::stoi(nextValue());
        } else if (arg == "--height") {
            opt.height = std::stoi(nextValue());
        } else if (arg == "--view" || arg == "--radius") {
            opt.viewDistance = std::stoi(nextValue());
            opt.viewGiven = true;
        } else if (arg == "--seed") {
            opt.seed = std::stoi(nextValue());
            opt.seedGiven = true;
        } else if (arg == "--new") {
            opt.newWorld = true;
        } else if (arg == "--load") {
            opt.loadPath = nextValue();
        } else if (arg == "--save") {
            opt.savePath = nextValue();
        } else if (arg == "--no-autosave") {
            opt.autosave = false;
        } else if (arg == "--frames") {
            opt.frames = std::stoi(nextValue());
        } else if (arg == "--shot") {
            opt.shot = nextValue();
        } else if (arg == "--fog") {
            float start = 0.0f;
            float end = 0.0f;
            if (std::sscanf(nextValue().c_str(), "%f,%f", &start, &end) == 2 && end > start) {
                opt.fogStart = start;
                opt.fogEnd = end;
            } else {
                spdlog::warn("--fog expects start,end with end > start");
            }
        } else if (arg == "--time") {
            opt.timeOfDay = std::stof(nextValue());
        } else if (arg == "--day-length") {
            opt.dayLength = std::stof(nextValue());
            opt.dayLengthGiven = true;
        } else if (arg == "--benchmark") {
            opt.benchmark = std::stof(nextValue());
        } else if (arg == "--quit-after") {
            opt.quitAfter = std::stof(nextValue());
        } else if (arg == "--panel") {
            opt.panel = true;
        } else if (arg == "--inventory") {
            opt.inventory = true;
        } else if (arg == "--tab") {
            opt.tab = nextValue();
            opt.inventory = true;
        } else if (arg == "--hidden") {
            opt.hidden = true;
        } else if (arg == "--no-vsync") {
            opt.vsync = false;
            opt.vsyncGiven = true;
        } else if (arg == "--no-hud") {
            opt.hud = false;
            opt.hudGiven = true;
        } else if (arg == "--no-audio") {
            opt.audioEnabled = false;
            opt.audioGiven = true;
        } else if (arg == "--verbose") {
            opt.verbose = true;
        } else if (arg == "--cam") {
            float x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f, pitch = 0.0f;
            if (std::sscanf(nextValue().c_str(), "%f,%f,%f,%f,%f", &x, &y, &z, &yaw, &pitch) ==
                5) {
                opt.camera.pos = glm::vec3(x, y, z);
                opt.camera.yawDeg = yaw;
                opt.camera.pitchDeg = pitch;
                opt.hasCamera = true;
            } else {
                spdlog::warn("--cam expects x,y,z,yaw,pitch");
            }
        } else if (arg == "--help" || arg == "-h") {
            opt.showHelp = true;
        } else {
            spdlog::warn("unknown argument: {}", arg);
        }
    }
    opt.width = std::clamp(opt.width, 160, 3840);
    opt.height = std::clamp(opt.height, 120, 2160);
    opt.viewDistance = std::clamp(opt.viewDistance, 2, 12);
    opt.frames = std::clamp(opt.frames, 1, 600);
    opt.timeOfDay = std::clamp(opt.timeOfDay, 0.0f, 0.9999f);
    opt.dayLength = std::clamp(opt.dayLength, 0.0f, 86400.0f);
    return opt;
}

}  // namespace

int main(int argc, char** argv) {
    aimc::initLogging();

    const Options opt = parseArgs(argc, argv);
    if (opt.verbose) {
        spdlog::set_level(spdlog::level::debug);
    }
    if (opt.showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    aimc::Settings settings;
    aimc::loadSettings("settings.json", settings);
    if (opt.viewGiven) {
        settings.viewDistance = opt.viewDistance;
    }
    if (opt.vsyncGiven) {
        settings.vsync = opt.vsync;
    }
    if (opt.audioGiven) {
        settings.audio = opt.audioEnabled;
    }
    if (opt.hudGiven) {
        settings.hud = opt.hud;
    }
    if (opt.dayLengthGiven) {
        settings.dayLength = opt.dayLength;
    }
    settings.viewDistance = std::clamp(settings.viewDistance, 3, 10);

    aimc::Window window;
    aimc::WindowConfig windowConfig;
    windowConfig.width = opt.width;
    windowConfig.height = opt.height;
    windowConfig.hidden = opt.hidden;
    windowConfig.vsync = settings.vsync;
    if (!window.init(windowConfig)) {
        return 1;
    }

    aimc::Atlas atlas;
    if (!atlas.build()) {
        return 1;
    }

    const std::string savePath = !opt.loadPath.empty() ? opt.loadPath : opt.savePath;
    const bool autoLoad = !opt.newWorld && !opt.seedGiven && opt.benchmark <= 0.0f &&
                          opt.shot.empty();
    const bool wantsLoad = !opt.loadPath.empty() || autoLoad;

    aimc::SaveData saveData;
    bool loaded = false;
    if (wantsLoad) {
        if (std::filesystem::exists(savePath)) {
            loaded = aimc::loadWorld(savePath, saveData);
        } else if (!opt.loadPath.empty()) {
            spdlog::warn("save file not found: {}", savePath);
        }
    }

    aimc::World world;
    world.init(loaded ? saveData.seed : opt.seed);
    world.setAtlas(&atlas);

    aimc::Shader shader;
    if (!shader.compile(kVertexShader, kFragmentShader)) {
        return 1;
    }

    aimc::Sky sky;
    if (!sky.init()) {
        return 1;
    }

    aimc::SelectionBox selection;
    if (!selection.init()) {
        return 1;
    }

    aimc::DebugUI ui;
    if (!ui.init(window.handle(), window.context())) {
        return 1;
    }
    if (!opt.shot.empty() && !opt.panel) {
        ui.toggle();  // clean vista screenshots unless --panel is given
    }

    aimc::Audio audio;
    if (settings.audio && !audio.init()) {
        spdlog::warn("continuing without audio");
    }

    aimc::Camera camera;
    aimc::Player player;
    aimc::CreativeScreen creative;
    std::array<aimc::ItemRef, aimc::kHotbarSlots> hotbar = aimc::defaultHotbar();
    aimc::Dimension dimension = aimc::Dimension::Overworld;
    std::array<glm::vec3, aimc::kDimensionCount> dimensionPositions{};
    std::array<float, aimc::kDimensionCount> dimensionYaw{};
    std::array<float, aimc::kDimensionCount> dimensionPitch{};
    for (int i = 0; i < aimc::kDimensionCount; ++i) {
        dimensionPositions[static_cast<std::size_t>(i)] = glm::vec3(0.5f, 70.0f, 0.5f);
        dimensionYaw[static_cast<std::size_t>(i)] = -90.0f;
        dimensionPitch[static_cast<std::size_t>(i)] = -18.0f;
    }
    int selected = 0;
    float timeOfDay = opt.timeOfDay;

    if (loaded) {
        world.reset(saveData.seed);
        world.setDimensionEdits(aimc::Dimension::Overworld, saveData.dimensions[0].edits);
        world.setDimensionEdits(aimc::Dimension::Nether, saveData.dimensions[1].edits);
        dimension = saveData.current;
        world.setDimension(dimension);
        for (int i = 0; i < aimc::kDimensionCount; ++i) {
            dimensionPositions[static_cast<std::size_t>(i)] =
                saveData.dimensions[static_cast<std::size_t>(i)].playerPos;
            dimensionYaw[static_cast<std::size_t>(i)] =
                saveData.dimensions[static_cast<std::size_t>(i)].yaw;
            dimensionPitch[static_cast<std::size_t>(i)] =
                saveData.dimensions[static_cast<std::size_t>(i)].pitch;
        }
        player.setPosition(dimensionPositions[static_cast<std::size_t>(dimension)]);
        camera.pos = player.eyePosition();
        camera.yawDeg = dimensionYaw[static_cast<std::size_t>(dimension)];
        camera.pitchDeg = dimensionPitch[static_cast<std::size_t>(dimension)];
        selected = std::clamp(saveData.selected, 0, aimc::kHotbarSlots - 1);
        hotbar = saveData.hotbar;
        timeOfDay = saveData.timeOfDay;
    } else {
        camera.pos = glm::vec3(0.5f, 70.0f, 0.5f);
        camera.yawDeg = -90.0f;
        camera.pitchDeg = -18.0f;
        dimensionPositions[1] = glm::vec3(0.5f, 40.0f, 0.5f);
    }

    const auto prewarmStart = std::chrono::steady_clock::now();
    world.prewarm(camera.pos, settings.viewDistance, 30000);
    spdlog::info("prewarm finished in {} ms",
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - prewarmStart)
                     .count());

    if (!loaded) {
        int surface = world.surfaceY(0, 0);
        if (surface < 0) {
            surface = 40;
        }
        while (surface < aimc::Chunk::SY - 1 &&
               world.getBlock(0, surface + 1, 0) != aimc::BLOCK_AIR) {
            ++surface;
        }
        player.setPosition(glm::vec3(0.5f, static_cast<float>(surface) + 1.001f, 0.5f));
        camera.pos = player.eyePosition();
        dimensionPositions[0] = player.position();
        dimensionYaw[0] = camera.yawDeg;
        dimensionPitch[0] = camera.pitchDeg;
    }

    if (opt.hasCamera) {
        camera = opt.camera;
        player.setPosition(camera.pos - glm::vec3(0.0f, aimc::Player::EyeHeight, 0.0f));
    }

    const bool interactive = opt.shot.empty();
    if (opt.inventory) {
        creative.setOpen(true);
        if (ui.visible()) {
            ui.toggle();
        }
    }
    if (opt.tab == "natural") {
        creative.setTab(aimc::CreativeTab::Natural);
    } else if (opt.tab == "search") {
        creative.setTab(aimc::CreativeTab::Search);
    } else if (opt.tab == "nether") {
        creative.setTab(aimc::CreativeTab::Nether);
    } else if (opt.tab == "tools") {
        creative.setTab(aimc::CreativeTab::Tools);
    } else if (opt.tab == "building") {
        creative.setTab(aimc::CreativeTab::Building);
    } else if (!opt.tab.empty()) {
        spdlog::warn("unknown --tab '{}', expected building, natural, nether, tools or search",
                     opt.tab);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    std::string status =
        loaded ? "loaded " + savePath : "new world, seed " + std::to_string(world.seed());
    bool mouseCaptured = false;
    int mouseSkipFrames = 0;
    bool sprinting = false;
    bool sprintActive = false;
    Uint64 lastWTapMs = 0;
    float smoothedFov = settings.fov;
    bool muted = false;
    bool appliedVsync = settings.vsync;
    bool running = true;
    const auto wallStart = std::chrono::steady_clock::now();
    int frame = 0;
    double benchElapsed = 0.0;
    int benchFrames = 0;
    double physicsAccumulator = 0.0;
    float smoothedMs = 16.7f;
    float adaptTimer = 0.0f;
    auto last = std::chrono::steady_clock::now();

    const auto saveGame = [&](const std::string& path) -> bool {
        aimc::SaveData data;
        data.seed = world.seed();
        data.timeOfDay = timeOfDay;
        data.selected = selected;
        data.hotbar = hotbar;
        data.current = dimension;
        const int active = static_cast<int>(dimension);
        for (int i = 0; i < aimc::kDimensionCount; ++i) {
            const std::size_t index = static_cast<std::size_t>(i);
            aimc::DimensionSave& dim = data.dimensions[index];
            if (i == active) {
                dim.playerPos = player.position();
                dim.yaw = camera.yawDeg;
                dim.pitch = camera.pitchDeg;
            } else {
                dim.playerPos = dimensionPositions[index];
                dim.yaw = dimensionYaw[index];
                dim.pitch = dimensionPitch[index];
            }
            dim.edits = world.editsFor(static_cast<aimc::Dimension>(i));
        }
        return aimc::saveWorld(path, data);
    };

    const auto loadGame = [&](const std::string& path) -> bool {
        aimc::SaveData data;
        if (!aimc::loadWorld(path, data)) {
            return false;
        }
        world.reset(data.seed);
        world.setDimensionEdits(aimc::Dimension::Overworld, data.dimensions[0].edits);
        world.setDimensionEdits(aimc::Dimension::Nether, data.dimensions[1].edits);
        dimension = data.current;
        world.setDimension(dimension);
        for (int i = 0; i < aimc::kDimensionCount; ++i) {
            const std::size_t index = static_cast<std::size_t>(i);
            dimensionPositions[index] = data.dimensions[index].playerPos;
            dimensionYaw[index] = data.dimensions[index].yaw;
            dimensionPitch[index] = data.dimensions[index].pitch;
        }
        const std::size_t active = static_cast<std::size_t>(dimension);
        player.setPosition(dimensionPositions[active]);
        camera.pos = player.eyePosition();
        camera.yawDeg = dimensionYaw[active];
        camera.pitchDeg = dimensionPitch[active];
        selected = std::clamp(data.selected, 0, aimc::kHotbarSlots - 1);
        hotbar = data.hotbar;
        timeOfDay = data.timeOfDay;
        world.prewarm(camera.pos, settings.viewDistance, 30000);
        return true;
    };

    const auto newWorld = [&]() {
        world.reset(opt.seed);
        dimension = aimc::Dimension::Overworld;
        world.setDimension(dimension);
        camera.pos = glm::vec3(0.5f, 70.0f, 0.5f);
        world.prewarm(camera.pos, settings.viewDistance, 30000);
        int surface = world.surfaceY(0, 0);
        if (surface < 0) {
            surface = 40;
        }
        player.setPosition(glm::vec3(0.5f, static_cast<float>(surface) + 1.001f, 0.5f));
        camera.pos = player.eyePosition();
        camera.yawDeg = -90.0f;
        camera.pitchDeg = -18.0f;
        for (int i = 0; i < aimc::kDimensionCount; ++i) {
            dimensionYaw[static_cast<std::size_t>(i)] = -90.0f;
            dimensionPitch[static_cast<std::size_t>(i)] = -18.0f;
        }
        dimensionPositions[0] = player.position();
        dimensionPositions[1] = glm::vec3(0.5f, 40.0f, 0.5f);
        hotbar = aimc::defaultHotbar();
        selected = 0;
        timeOfDay = 0.42f;
    };

    float portalTimer = 0.0f;
    float portalCooldown = 0.0f;

    const auto travelToDimension = [&]() {
        const aimc::Dimension target = dimension == aimc::Dimension::Overworld
                                           ? aimc::Dimension::Nether
                                           : aimc::Dimension::Overworld;
        const std::size_t sourceIndex = static_cast<std::size_t>(dimension);
        dimensionPositions[sourceIndex] = player.position();
        dimensionYaw[sourceIndex] = camera.yawDeg;
        dimensionPitch[sourceIndex] = camera.pitchDeg;

        const glm::vec3 p = player.position();
        const glm::ivec3 source(static_cast<int>(std::floor(p.x)),
                                static_cast<int>(std::floor(p.y)),
                                static_cast<int>(std::floor(p.z)));
        const glm::ivec3 targetCoords =
            aimc::scaleBetweenDimensions(source, dimension, target);

        world.setDimension(target);
        dimension = target;
        world.prewarm(glm::vec3(static_cast<float>(targetCoords.x) + 0.5f, 40.0f,
                                static_cast<float>(targetCoords.z) + 0.5f),
                      settings.viewDistance, 30000);

        glm::ivec3 dest{0, 0, 0};
        glm::vec3 spawn;
        if (aimc::findPortalNear(world, targetCoords, 16, dest)) {
            spawn = glm::vec3(static_cast<float>(dest.x) + 0.5f,
                              static_cast<float>(dest.y) + 0.01f,
                              static_cast<float>(dest.z) + 0.5f);
        } else if (!aimc::buildPortalAt(world, targetCoords, spawn)) {
            spawn = glm::vec3(static_cast<float>(targetCoords.x) + 0.5f, 48.0f,
                              static_cast<float>(targetCoords.z) + 0.5f);
        }
        player.setPosition(spawn);
        camera.pos = player.eyePosition();
        const std::size_t targetIndex = static_cast<std::size_t>(target);
        dimensionPositions[targetIndex] = player.position();
        dimensionYaw[targetIndex] = camera.yawDeg;
        dimensionPitch[targetIndex] = camera.pitchDeg;
        portalTimer = 0.0f;
        portalCooldown = 2.5f;
        status = std::string("entered the ") + aimc::dimensionName(target);
        audio.play(aimc::Audio::Sound::Ignite, 0.6f);
    };

    while (running) {
        bool clickBreak = false;
        bool clickPlace = false;
        bool clickPick = false;
        bool requestSave = false;
        bool requestLoad = false;

        const auto now = std::chrono::steady_clock::now();
        const float rawDt = std::chrono::duration<float>(now - last).count();
        last = now;
        const float dt = std::clamp(rawDt, 0.0f, 0.1f);

        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            ui.processEvent(event);
            window.handleEvent(event);

            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                const SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE) {
                    if (creative.open()) {
                        creative.close();
                    } else if (ui.visible()) {
                        ui.toggle();
                    } else {
                        running = false;
                    }
                } else if (key == SDLK_F1) {
                    if (!creative.open()) {
                        ui.toggle();
                    }
                } else if (key == SDLK_e && interactive) {
                    if (creative.open()) {
                        if (!creative.capturesText()) {
                            creative.close();
                        }
                    } else if (!ui.visible()) {
                        creative.setOpen(true);
                    }
                } else if (key == SDLK_F5) {
                    requestSave = true;
                } else if (key == SDLK_F9) {
                    requestLoad = true;
                } else if (key == SDLK_f && interactive && !creative.open()) {
                    player.toggleFlying();
                } else if (key == SDLK_m && !creative.open()) {
                    muted = !muted;
                } else if (key == SDLK_w) {
                    const Uint64 nowMs = SDL_GetTicks64();
                    if (nowMs - lastWTapMs <= 300) {
                        sprinting = true;
                    }
                    lastWTapMs = nowMs;
                } else if (key >= SDLK_1 && key <= SDLK_9 && !creative.open()) {
                    selected = key - SDLK_1;
                }
            }
            if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_w) {
                sprinting = false;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                sprinting = false;
            }
            if (event.type == SDL_MOUSEWHEEL && interactive && !ui.visible() &&
                !creative.open()) {
                selected = (selected - event.wheel.y + static_cast<int>(hotbar.size())) %
                           static_cast<int>(hotbar.size());
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && interactive && !ui.visible() &&
                !creative.open()) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    clickBreak = true;
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    clickPlace = true;
                } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    clickPick = true;
                }
            }
        }

        const bool wantCapture = interactive && !ui.visible() && !creative.open();
        if (wantCapture != mouseCaptured) {
            window.setRelativeMouse(wantCapture);
            mouseCaptured = wantCapture;
            if (wantCapture) {
                // Drop the motion accumulated while a GUI was open. Enabling
                // relative mode also warps the pointer to the window centre and
                // SDL reports that warp as a big delta on the next frame.
                SDL_GetRelativeMouseState(nullptr, nullptr);
                mouseSkipFrames = 2;
            } else {
                mouseSkipFrames = 0;
            }
        }

        // Drain the relative state every frame so nothing piles up while a GUI
        // is open; the camera only sees motion while the mouse is captured.
        int mouseDX = 0;
        int mouseDY = 0;
        SDL_GetRelativeMouseState(&mouseDX, &mouseDY);
        if (!wantCapture || mouseSkipFrames > 0) {
            mouseDX = 0;
            mouseDY = 0;
        }
        if (mouseSkipFrames > 0) {
            --mouseSkipFrames;
        }
        if (!wantCapture) {
            sprinting = false;
        }

        sprintActive = false;

        if (interactive && !ui.visible() && !creative.open()) {
            aimc::PlayerInput input;
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            input.forward = keys[SDL_SCANCODE_W] != 0;
            input.back = keys[SDL_SCANCODE_S] != 0;
            input.left = keys[SDL_SCANCODE_A] != 0;
            input.right = keys[SDL_SCANCODE_D] != 0;
            input.jump = keys[SDL_SCANCODE_SPACE] != 0;
            input.up = keys[SDL_SCANCODE_SPACE] != 0;
            input.down = (keys[SDL_SCANCODE_LCTRL] != 0 || keys[SDL_SCANCODE_C] != 0);
            input.sprint = sprinting && input.forward;
            input.boost = keys[SDL_SCANCODE_LSHIFT] != 0;
            sprintActive = input.sprint;

            camera.yawDeg += static_cast<float>(mouseDX) * settings.sensitivity;
            camera.pitchDeg -= static_cast<float>(mouseDY) * settings.sensitivity;
            camera.pitchDeg = std::clamp(camera.pitchDeg, -89.0f, 89.0f);

            if (settings.dayLength > 0.0f) {
                timeOfDay = std::fmod(timeOfDay + dt / settings.dayLength, 1.0f);
            }

            constexpr double kFixedStep = 1.0 / 120.0;
            physicsAccumulator += static_cast<double>(dt);
            int steps = 0;
            while (physicsAccumulator >= kFixedStep && steps < 8) {
                player.update(world, camera.yawDeg, input, static_cast<float>(kFixedStep));
                physicsAccumulator -= kFixedStep;
                ++steps;
                if (player.consumeStep()) {
                    audio.play(aimc::Audio::Sound::Step, 0.45f);
                }
                if (player.consumeJump()) {
                    audio.play(aimc::Audio::Sound::Jump, 0.5f);
                }
                if (player.consumeLand()) {
                    audio.play(aimc::Audio::Sound::Land, 0.7f);
                }
            }
            camera.pos = player.eyePosition();

            const glm::vec3 feet = player.position();
            bool inPortal = false;
            for (const float dy : {0.1f, 0.9f, 1.7f}) {
                const aimc::BlockId block =
                    world.getBlock(static_cast<int>(std::floor(feet.x)),
                                   static_cast<int>(std::floor(feet.y + dy)),
                                   static_cast<int>(std::floor(feet.z)));
                if (aimc::isPortalBlock(block)) {
                    inPortal = true;
                    break;
                }
            }
            if (portalCooldown > 0.0f) {
                portalCooldown = std::max(0.0f, portalCooldown - dt);
            }
            if (inPortal && portalCooldown <= 0.0f) {
                portalTimer += dt;
                if (portalTimer >= 1.0f) {
                    travelToDimension();
                }
            } else if (!inPortal) {
                portalTimer = 0.0f;
            }

            if (player.position().y < -30.0f) {
                if (dimension == aimc::Dimension::Overworld) {
                    int surface = world.surfaceY(0, 0);
                    if (surface < 0) {
                        surface = 40;
                    }
                    player.setPosition(
                        glm::vec3(0.5f, static_cast<float>(surface) + 1.001f, 0.5f));
                } else {
                    player.setPosition(glm::vec3(0.5f, 40.0f, 0.5f));
                }
                camera.pos = player.eyePosition();
            }
        } else {
            physicsAccumulator = 0.0;
        }

        const float targetFov = settings.fov + (sprintActive ? 7.0f : 0.0f);
        smoothedFov += (targetFov - smoothedFov) * std::min(1.0f, 12.0f * dt);
        camera.fovDeg = smoothedFov;

        smoothedMs = smoothedMs * 0.95f + rawDt * 1000.0f * 0.05f;
        if (settings.adaptive && interactive && opt.benchmark <= 0.0f) {
            adaptTimer += dt;
            if (adaptTimer >= 2.0f) {
                adaptTimer = 0.0f;
                if (smoothedMs > 22.0f && settings.viewDistance > 3) {
                    --settings.viewDistance;
                    status = "adaptive: view distance " + std::to_string(settings.viewDistance);
                } else if (smoothedMs < 13.0f && settings.viewDistance < 10) {
                    ++settings.viewDistance;
                    status = "adaptive: view distance " + std::to_string(settings.viewDistance);
                }
            }
        }

        const float effectiveVolume = muted ? 0.0f : settings.volume;
        if (std::abs(audio.volume() - effectiveVolume) > 0.001f) {
            audio.setVolume(effectiveVolume);
        }
        if (appliedVsync != settings.vsync) {
            window.setVsync(settings.vsync);
            appliedVsync = settings.vsync;
        }

        world.update(camera.pos, settings.viewDistance, true);

        const auto blockGetter = [&world](int x, int y, int z) -> aimc::BlockId {
            const aimc::BlockId block = world.getBlock(x, y, z);
            const bool passThrough = block == aimc::BLOCK_WATER ||
                                     block == aimc::BLOCK_LAVA ||
                                     block == aimc::BLOCK_PORTAL_X ||
                                     block == aimc::BLOCK_PORTAL_Z;
            return passThrough ? aimc::BLOCK_AIR : block;
        };
        const aimc::RayHit hit =
            aimc::raycastVoxel(blockGetter, camera.pos, camera.forward(), 6.0f);

        if (interactive && !ui.visible() && !creative.open()) {
            if (clickBreak && hit.hit) {
                if (world.setBlock(hit.block.x, hit.block.y, hit.block.z, aimc::BLOCK_AIR)) {
                    audio.play(aimc::Audio::Sound::Break, 0.8f);
                }
            }
            const aimc::ItemRef held = hotbar[selected];
            if (clickPlace && hit.hit && held.isItem()) {
                if (held.item == aimc::ItemId::FlintAndSteel) {
                    if (aimc::lightPortal(world, hit.block + hit.normal)) {
                        audio.play(aimc::Audio::Sound::Ignite, 0.7f);
                        status = "portal lit";
                    } else {
                        status = "flint and steel: no obsidian frame";
                    }
                }
            } else if (clickPlace && hit.hit && !held.empty()) {
                const glm::ivec3 target = hit.block + hit.normal;
                const aimc::BlockId existing = world.getBlock(target.x, target.y, target.z);
                if (existing == aimc::BLOCK_AIR || existing == aimc::BLOCK_WATER ||
                    existing == aimc::BLOCK_LAVA) {
                    const glm::vec3 pmin = player.position() - glm::vec3(0.3f, 0.0f, 0.3f);
                    const glm::vec3 pmax = player.position() + glm::vec3(0.3f, 1.8f, 0.3f);
                    const bool overlaps =
                        static_cast<float>(target.x) < pmax.x &&
                        static_cast<float>(target.x + 1) > pmin.x &&
                        static_cast<float>(target.y) < pmax.y &&
                        static_cast<float>(target.y + 1) > pmin.y &&
                        static_cast<float>(target.z) < pmax.z &&
                        static_cast<float>(target.z + 1) > pmin.z;
                    if (!overlaps) {
                        if (world.setBlock(target.x, target.y, target.z, held.block)) {
                            audio.play(aimc::Audio::Sound::Place, 0.7f);
                        }
                    }
                }
            }
            if (clickPick && hit.hit) {
                const aimc::ItemRef picked = aimc::ItemRef::ofBlock(
                    world.getBlock(hit.block.x, hit.block.y, hit.block.z));
                const auto it = std::find(hotbar.begin(), hotbar.end(), picked);
                if (it != hotbar.end()) {
                    selected = static_cast<int>(it - hotbar.begin());
                } else {
                    hotbar[selected] = picked;
                }
            }
        }

        if (requestSave) {
            status = saveGame(opt.savePath) ? "saved " + opt.savePath : "save failed";
        }
        if (requestLoad) {
            status = loadGame(savePath) ? "loaded " + savePath : "load failed";
        }

        ui.beginFrame();

        const int width = window.width();
        const int height = window.height();
        glViewport(0, 0, width, height);

        const aimc::SkyState skyState = dimension == aimc::Dimension::Nether
                                            ? aimc::computeNetherSky(timeOfDay)
                                            : aimc::computeSky(timeOfDay);
        glm::vec3 fogColor = skyState.fogColor;
        glm::vec2 fogRange;
        if (opt.fogStart >= 0.0f && opt.fogEnd > opt.fogStart) {
            fogRange = glm::vec2(opt.fogStart, opt.fogEnd);
        } else {
            const float end = static_cast<float>(settings.viewDistance) * aimc::Chunk::SX * 0.95f;
            fogRange = glm::vec2(end * 0.55f, end);
        }
        float lightScale = skyState.light;

        const aimc::BlockId eyeBlock =
            world.getBlock(static_cast<int>(std::floor(camera.pos.x)),
                           static_cast<int>(std::floor(camera.pos.y)),
                           static_cast<int>(std::floor(camera.pos.z)));
        if (interactive && eyeBlock == aimc::BLOCK_WATER) {
            fogRange = glm::vec2(0.5f, 18.0f);
            fogColor = glm::vec3(0.06f, 0.22f, 0.34f) * (0.4f + 0.6f * skyState.dayFactor);
            lightScale *= 0.75f;
        } else if (interactive && eyeBlock == aimc::BLOCK_LAVA) {
            fogRange = glm::vec2(0.2f, 6.0f);
            fogColor = glm::vec3(1.0f, 0.42f, 0.06f);
            lightScale = 1.0f;
        }

        glClearColor(skyState.horizon.x, skyState.horizon.y, skyState.horizon.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 viewProj = camera.viewProj(window.aspect());
        const aimc::Frustum frustum = aimc::Frustum::fromViewProj(viewProj);

        if (dimension == aimc::Dimension::Overworld) {
            sky.draw(camera, glm::inverse(viewProj), skyState);
        }

        shader.use();
        shader.setMat4("uViewProj", viewProj);
        shader.setVec3("uCamPos", camera.pos);
        shader.setVec3("uFogColor", fogColor);
        shader.setVec2("uFog", fogRange);
        shader.setFloat("uLight", lightScale);
        shader.setFloat("uAlpha", 1.0f);
        shader.setInt("uAtlas", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas.texture());

        aimc::DrawStats stats = world.drawOpaque(frustum);

        // Lava is an opaque liquid but emits its own light.
        shader.setFloat("uLight", 1.0f);
        glDisable(GL_CULL_FACE);
        const aimc::DrawStats lavaStats = world.drawLava(frustum);
        glEnable(GL_CULL_FACE);
        shader.setFloat("uLight", lightScale);
        stats.drawCalls += lavaStats.drawCalls;
        stats.triangles += lavaStats.triangles;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        shader.setFloat("uAlpha", 0.72f);
        const aimc::DrawStats waterStats = world.drawWater(frustum);
        // Portals are emissive: draw them with a fixed light so they glow at night.
        shader.setFloat("uAlpha", 0.80f);
        shader.setFloat("uLight", 1.0f);
        const aimc::DrawStats portalStats = world.drawPortal(frustum);
        shader.setFloat("uLight", lightScale);
        shader.setFloat("uAlpha", 1.0f);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        stats.drawCalls += waterStats.drawCalls + portalStats.drawCalls;
        stats.triangles += waterStats.triangles + portalStats.triangles;

        if (settings.hud && hit.hit) {
            selection.draw(viewProj, hit.block);
        }

        const float uiDt = opt.shot.empty() ? dt : (1.0f / 60.0f);
        const aimc::UiActions actions =
            ui.draw(camera, world, stats, uiDt, settings, timeOfDay, status);
        const bool creativeOpen = creative.open();
        if (settings.hud && !creativeOpen) {
            ui.drawHud(hit, hotbar.data(), static_cast<int>(hotbar.size()), selected, atlas,
                       player.flying());
        }
        creative.draw(atlas, hotbar, selected, interactive);
        ui.endFrame();

        if (actions.saveWorld) {
            status = saveGame(opt.savePath) ? "saved " + opt.savePath : "save failed";
        }
        if (actions.loadWorld) {
            status = loadGame(savePath) ? "loaded " + savePath : "load failed";
        }
        if (actions.saveSettings) {
            status = aimc::saveSettings("settings.json", settings) ? "settings saved"
                                                                   : "settings save failed";
        }
        if (actions.toggleFullscreen) {
            window.setFullscreen(!window.fullscreen());
        }
        if (actions.newWorld) {
            newWorld();
            status = "new world, seed " + std::to_string(world.seed());
        }

        if (opt.quitAfter > 0.0f &&
            std::chrono::duration<float>(std::chrono::steady_clock::now() - wallStart).count() >=
                opt.quitAfter) {
            running = false;
        }

        ++frame;
        benchElapsed += static_cast<double>(rawDt);
        ++benchFrames;
        if (opt.benchmark > 0.0f && benchElapsed >= opt.benchmark) {
            const double fps = static_cast<double>(benchFrames) / benchElapsed;
            spdlog::info("benchmark: {} frames in {:.2f}s = {:.1f} fps ({:.2f} ms average)",
                         benchFrames, benchElapsed, fps, 1000.0 / fps);
            running = false;
        }
        if (!opt.shot.empty() && frame >= opt.frames) {
            glFinish();
            if (aimc::saveFramebufferPng(opt.shot.c_str(), width, height)) {
                spdlog::info("screenshot written: {}", opt.shot);
            }
            running = false;
        }

        window.swapBuffers();
    }

    if (interactive && opt.autosave && opt.benchmark <= 0.0f) {
        if (saveGame(opt.savePath)) {
            spdlog::info("autosaved to {}", opt.savePath);
        }
    }
    if (interactive && opt.benchmark <= 0.0f) {
        aimc::saveSettings("settings.json", settings);
    }

    ui.shutdown();
    audio.shutdown();
    selection.shutdown();
    world.shutdown();
    sky.shutdown();
    shader.destroy();
    if (mouseCaptured) {
        window.setRelativeMouse(false);
    }
    return 0;
}
