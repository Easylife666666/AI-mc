#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

#include <SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "core/log.h"
#include "game/player.h"
#include "game/raycast.h"
#include "platform/window.h"
#include "render/atlas.h"
#include "render/camera.h"
#include "render/frustum.h"
#include "render/screenshot.h"
#include "render/selection.h"
#include "render/shader.h"
#include "ui/debug_ui.h"
#include "world/block.h"
#include "world/chunk.h"
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
    bool hud = true;
    bool verbose = false;
    int seed = 1337;
    int viewDistance = 7;
    int frames = 4;
    float fogStart = -1.0f;
    float fogEnd = -1.0f;
    float benchmark = 0.0f;
    std::string shot;
    bool hasCamera = false;
    aimc::Camera camera;
    bool showHelp = false;
};

void printUsage(const char* argv0) {
    std::printf(
        "AI-mc -- voxel sandbox (M3)\n"
        "\n"
        "usage: %s [options]\n"
        "  --width N          window width  (default 1280)\n"
        "  --height N         window height (default 720)\n"
        "  --view N           view distance in chunks (default 7)\n"
        "  --radius N         alias for --view\n"
        "  --seed N           world seed    (default 1337)\n"
        "  --cam x,y,z,yaw,pitch\n"
        "                     fixed camera pose (screenshot mode)\n"
        "  --shot PATH        render, save a PNG, then exit\n"
        "  --frames N         frames to render before the screenshot (default 4)\n"
        "  --fog start,end    fog range in blocks (default: derived from --view)\n"
        "  --benchmark N      run N seconds, print average fps, then exit\n"
        "  --hidden           create a hidden window (screenshot mode)\n"
        "  --no-vsync         disable vsync\n"
        "  --no-hud           hide crosshair and hotbar\n"
        "  --verbose          debug logging (player state, chunks)\n"
        "  --help             this text\n"
        "\n"
        "controls:\n"
        "  WASD move   Space jump/fly up   Shift sprint/fly fast   F toggle fly\n"
        "  Ctrl/C down (fly)   LMB break   RMB place   1-9 / wheel select block\n"
        "  F1 panel    Esc quit\n",
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
        } else if (arg == "--seed") {
            opt.seed = std::stoi(nextValue());
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
        } else if (arg == "--benchmark") {
            opt.benchmark = std::stof(nextValue());
        } else if (arg == "--hidden") {
            opt.hidden = true;
        } else if (arg == "--no-vsync") {
            opt.vsync = false;
        } else if (arg == "--no-hud") {
            opt.hud = false;
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

    aimc::Window window;
    aimc::WindowConfig windowConfig;
    windowConfig.width = opt.width;
    windowConfig.height = opt.height;
    windowConfig.hidden = opt.hidden;
    windowConfig.vsync = opt.vsync;
    if (!window.init(windowConfig)) {
        return 1;
    }

    aimc::Atlas atlas;
    if (!atlas.build()) {
        return 1;
    }

    aimc::World world;
    world.init(opt.seed);
    world.setAtlas(&atlas);

    aimc::Shader shader;
    if (!shader.compile(kVertexShader, kFragmentShader)) {
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

    aimc::Camera camera = opt.camera;
    if (!opt.hasCamera) {
        camera.pos = glm::vec3(0.5f, 70.0f, 0.5f);
        camera.yawDeg = -90.0f;
        camera.pitchDeg = -18.0f;
    }

    const auto prewarmStart = std::chrono::steady_clock::now();
    world.prewarm(camera.pos, opt.viewDistance, 30000);
    const auto prewarmMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - prewarmStart)
                               .count();
    spdlog::info("prewarm finished in {} ms", prewarmMs);

    aimc::Player player;
    if (opt.hasCamera) {
        player.setPosition(camera.pos - glm::vec3(0.0f, aimc::Player::EyeHeight, 0.0f));
    } else {
        int surface = world.surfaceY(0, 0);
        if (surface < 0) {
            surface = 40;
        }
        while (surface < aimc::Chunk::SY - 1 &&
               world.getBlock(0, surface + 1, 0) != aimc::BLOCK_AIR) {
            ++surface;
        }
        const glm::vec3 feet(0.5f, static_cast<float>(surface) + 1.001f, 0.5f);
        player.setPosition(feet);
        camera.pos = player.eyePosition();
    }

    const bool interactive = opt.shot.empty();
    if (interactive) {
        window.setRelativeMouse(true);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    const glm::vec3 skyColor(0.53f, 0.72f, 0.95f);
    float fogEnd = opt.fogEnd;
    float fogStart = opt.fogStart;
    if (fogEnd <= fogStart) {
        fogEnd = static_cast<float>(opt.viewDistance) * aimc::Chunk::SX * 0.95f;
        fogStart = fogEnd * 0.45f;
    }
    const glm::vec2 fogRange(fogStart, fogEnd);

    std::array<int, 9> hotbar = {
        aimc::BLOCK_GRASS, aimc::BLOCK_DIRT,  aimc::BLOCK_STONE,
        aimc::BLOCK_SAND,  aimc::BLOCK_LOG,   aimc::BLOCK_LEAVES,
        aimc::BLOCK_PLANKS, aimc::BLOCK_COBBLE, aimc::BLOCK_WATER,
    };
    int selected = 0;

    bool running = true;
    int frame = 0;
    double benchElapsed = 0.0;
    int benchFrames = 0;
    double physicsAccumulator = 0.0;
    auto last = std::chrono::steady_clock::now();

    while (running) {
        bool clickBreak = false;
        bool clickPlace = false;
        bool clickPick = false;

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
                    running = false;
                } else if (key == SDLK_F1) {
                    ui.toggle();
                } else if (key == SDLK_f && interactive) {
                    player.toggleFlying();
                } else if (key >= SDLK_1 && key <= SDLK_9) {
                    selected = key - SDLK_1;
                }
            }
            if (event.type == SDL_MOUSEWHEEL && interactive) {
                selected = (selected - event.wheel.y + static_cast<int>(hotbar.size())) %
                           static_cast<int>(hotbar.size());
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && interactive) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    clickBreak = true;
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    clickPlace = true;
                } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    clickPick = true;
                }
            }
        }

        if (interactive) {
            aimc::PlayerInput input;
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            input.forward = keys[SDL_SCANCODE_W] != 0;
            input.back = keys[SDL_SCANCODE_S] != 0;
            input.left = keys[SDL_SCANCODE_A] != 0;
            input.right = keys[SDL_SCANCODE_D] != 0;
            input.jump = keys[SDL_SCANCODE_SPACE] != 0;
            input.up = keys[SDL_SCANCODE_SPACE] != 0;
            input.down = (keys[SDL_SCANCODE_LCTRL] != 0 || keys[SDL_SCANCODE_C] != 0);
            input.sprint = keys[SDL_SCANCODE_LSHIFT] != 0;

            int mouseX = 0;
            int mouseY = 0;
            SDL_GetRelativeMouseState(&mouseX, &mouseY);
            constexpr float kSensitivity = 0.10f;
            camera.yawDeg += static_cast<float>(mouseX) * kSensitivity;
            camera.pitchDeg -= static_cast<float>(mouseY) * kSensitivity;
            camera.pitchDeg = std::clamp(camera.pitchDeg, -89.0f, 89.0f);

            constexpr double kFixedStep = 1.0 / 120.0;
            physicsAccumulator += static_cast<double>(dt);
            int steps = 0;
            while (physicsAccumulator >= kFixedStep && steps < 8) {
                player.update(world, camera.yawDeg, input, static_cast<float>(kFixedStep));
                physicsAccumulator -= kFixedStep;
                ++steps;
            }
            camera.pos = player.eyePosition();

            if (player.position().y < -30.0f) {
                int surface = world.surfaceY(0, 0);
                if (surface < 0) {
                    surface = 40;
                }
                player.setPosition(glm::vec3(0.5f, static_cast<float>(surface) + 1.001f, 0.5f));
                camera.pos = player.eyePosition();
            }
        }

        world.update(camera.pos, opt.viewDistance, true);

        const auto blockGetter = [&world](int x, int y, int z) -> aimc::BlockId {
            const aimc::BlockId block = world.getBlock(x, y, z);
            return block == aimc::BLOCK_WATER ? aimc::BLOCK_AIR : block;
        };
        const aimc::RayHit hit =
            aimc::raycastVoxel(blockGetter, camera.pos, camera.forward(), 6.0f);

        if (interactive && !ui.wantsMouse()) {
            if (clickBreak && hit.hit) {
                world.setBlock(hit.block.x, hit.block.y, hit.block.z, aimc::BLOCK_AIR);
            }
            if (clickPlace && hit.hit) {
                const glm::ivec3 target = hit.block + hit.normal;
                const aimc::BlockId existing =
                    world.getBlock(target.x, target.y, target.z);
                if (existing == aimc::BLOCK_AIR || existing == aimc::BLOCK_WATER) {
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
                        world.setBlock(target.x, target.y, target.z,
                                       static_cast<aimc::BlockId>(hotbar[selected]));
                    }
                }
            }
            if (clickPick && hit.hit) {
                const int picked =
                    static_cast<int>(world.getBlock(hit.block.x, hit.block.y, hit.block.z));
                const auto it = std::find(hotbar.begin(), hotbar.end(), picked);
                if (it != hotbar.end()) {
                    selected = static_cast<int>(it - hotbar.begin());
                } else {
                    hotbar[selected] = picked;
                }
            }
        }

        if (interactive && frame % 60 == 0) {
            const glm::vec3 p = player.position();
            spdlog::debug("player {:.2f},{:.2f},{:.2f} vy={:.2f} ground={} fly={} water={}", p.x,
                          p.y, p.z, player.velocity().y, player.onGround(), player.flying(),
                          player.inWater());
        }

        ui.beginFrame();

        const int width = window.width();
        const int height = window.height();
        glViewport(0, 0, width, height);
        glClearColor(skyColor.x, skyColor.y, skyColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 viewProj = camera.viewProj(window.aspect());
        const aimc::Frustum frustum = aimc::Frustum::fromViewProj(viewProj);

        shader.use();
        shader.setMat4("uViewProj", viewProj);
        shader.setVec3("uCamPos", camera.pos);
        shader.setVec3("uFogColor", skyColor);
        shader.setVec2("uFog", fogRange);
        shader.setFloat("uLight", 1.0f);
        shader.setFloat("uAlpha", 1.0f);
        shader.setInt("uAtlas", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas.texture());

        aimc::DrawStats stats = world.drawOpaque(frustum);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        shader.setFloat("uAlpha", 0.72f);
        const aimc::DrawStats waterStats = world.drawWater(frustum);
        shader.setFloat("uAlpha", 1.0f);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        stats.drawCalls += waterStats.drawCalls;
        stats.triangles += waterStats.triangles;

        if (opt.hud && hit.hit) {
            selection.draw(viewProj, hit.block);
        }

        const float uiDt = opt.shot.empty() ? dt : (1.0f / 60.0f);
        ui.draw(camera, world, stats, uiDt);
        if (opt.hud) {
            ui.drawHud(hit, hotbar.data(), static_cast<int>(hotbar.size()), selected, atlas,
                       player.flying());
        }
        ui.endFrame();

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

    ui.shutdown();
    selection.shutdown();
    world.shutdown();
    shader.destroy();
    if (interactive) {
        window.setRelativeMouse(false);
    }
    return 0;
}
