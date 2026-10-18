#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

#include <SDL.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "core/log.h"
#include "platform/window.h"
#include "render/atlas.h"
#include "render/camera.h"
#include "render/screenshot.h"
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
uniform vec3 uSkyColor;
uniform vec2 uFog;

out vec4 FragColor;

void main() {
    vec3 albedo = texture(uAtlas, vUV).rgb;
    vec3 color = albedo * vLight;
    float dist = distance(vWorld, uCamPos);
    float fog = clamp((dist - uFog.x) / max(uFog.y - uFog.x, 0.001), 0.0, 1.0);
    color = mix(color, uSkyColor, fog);
    FragColor = vec4(color, 1.0);
}
)GLSL";

struct Options {
    int width = 1280;
    int height = 720;
    bool hidden = false;
    bool vsync = true;
    int seed = 1337;
    int radius = 5;
    int frames = 4;
    float fogStart = 45.0f;
    float fogEnd = 105.0f;
    float benchmark = 0.0f;
    std::string shot;
    bool hasCamera = false;
    aimc::Camera camera;
    bool showHelp = false;
};

void printUsage(const char* argv0) {
    std::printf(
        "AI-mc -- voxel sandbox (M0/M1)\n"
        "\n"
        "usage: %s [options]\n"
        "  --width N          window width  (default 1280)\n"
        "  --height N         window height (default 720)\n"
        "  --radius N         chunk radius  (default 5)\n"
        "  --seed N           world seed    (default 1337)\n"
        "  --cam x,y,z,yaw,pitch\n"
        "                     fixed camera pose\n"
        "  --shot PATH        render, save a PNG, then exit\n"
        "  --frames N         frames to render before the screenshot (default 4)\n"
        "  --fog start,end    fog range in blocks (default 45,105)\n"
        "  --benchmark N      run N seconds, print average fps, then exit\n"
        "  --hidden           create a hidden window (screenshot mode)\n"
        "  --no-vsync         disable vsync\n"
        "  --help             this text\n"
        "\n"
        "controls: WASD move, Space/Ctrl up/down, Shift boost, F1 panel, Esc quit\n",
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
        } else if (arg == "--radius") {
            opt.radius = std::stoi(nextValue());
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
    opt.radius = std::clamp(opt.radius, 0, 16);
    opt.frames = std::clamp(opt.frames, 1, 600);
    return opt;
}

}  // namespace

int main(int argc, char** argv) {
    aimc::initLogging();

    const Options opt = parseArgs(argc, argv);
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

    aimc::World world;
    world.init(opt.seed, opt.radius);

    aimc::Atlas atlas;
    if (!atlas.build()) {
        return 1;
    }

    aimc::Shader shader;
    if (!shader.compile(kVertexShader, kFragmentShader)) {
        return 1;
    }

    aimc::DebugUI ui;
    if (!ui.init(window.handle(), window.context())) {
        return 1;
    }

    world.buildAllMeshes(atlas);

    aimc::Camera camera;
    if (opt.hasCamera) {
        camera = opt.camera;
    } else {
        int surface = 0;
        for (int y = aimc::Chunk::SY - 1; y >= 0; --y) {
            if (world.getBlock(0, y, 0) != aimc::BLOCK_AIR) {
                surface = y;
                break;
            }
        }
        camera.pos = glm::vec3(0.0f, static_cast<float>(surface) + 16.0f, 0.0f);
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
    const glm::vec2 fogRange(opt.fogStart, opt.fogEnd);

    bool running = true;
    int frame = 0;
    double benchElapsed = 0.0;
    int benchFrames = 0;
    auto last = std::chrono::steady_clock::now();

    while (running) {
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
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                if (event.key.keysym.sym == SDLK_F1) {
                    ui.toggle();
                }
            }
        }

        if (interactive) {
            aimc::CameraInput input;
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            input.forward = keys[SDL_SCANCODE_W] != 0;
            input.back = keys[SDL_SCANCODE_S] != 0;
            input.left = keys[SDL_SCANCODE_A] != 0;
            input.right = keys[SDL_SCANCODE_D] != 0;
            input.up = keys[SDL_SCANCODE_SPACE] != 0;
            input.down = (keys[SDL_SCANCODE_LCTRL] != 0 || keys[SDL_SCANCODE_C] != 0);
            input.boost = keys[SDL_SCANCODE_LSHIFT] != 0;

            int mouseX = 0;
            int mouseY = 0;
            SDL_GetRelativeMouseState(&mouseX, &mouseY);
            input.mouseDx = static_cast<float>(mouseX);
            input.mouseDy = static_cast<float>(mouseY);
            input.mouseLook = true;

            aimc::updateFreeCamera(camera, input, dt);
        }

        ui.beginFrame();

        const int width = window.width();
        const int height = window.height();
        glViewport(0, 0, width, height);
        glClearColor(skyColor.x, skyColor.y, skyColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        shader.setMat4("uViewProj", camera.viewProj(window.aspect()));
        shader.setVec3("uCamPos", camera.pos);
        shader.setVec3("uSkyColor", skyColor);
        shader.setVec2("uFog", fogRange);
        shader.setInt("uAtlas", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas.texture());

        const aimc::DrawStats stats = world.draw();

        // Screenshot mode renders a handful of frames without vsync; feed the
        // overlay a fixed 60 Hz step so the still shows a sensible frame time.
        const float uiDt = opt.shot.empty() ? dt : (1.0f / 60.0f);
        ui.draw(camera, world, stats, uiDt);
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
    world.destroyGpu();
    shader.destroy();
    if (interactive) {
        window.setRelativeMouse(false);
    }
    return 0;
}
