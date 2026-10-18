#include "render/atlas.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <GL/glew.h>
#include <spdlog/spdlog.h>

namespace aimc {
namespace {

float hash01(int x, int y, int seed) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u +
                      static_cast<std::uint32_t>(y) * 668265263u +
                      static_cast<std::uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

float valueNoise(float x, float y, int seed) {
    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));
    const float xf = x - static_cast<float>(xi);
    const float yf = y - static_cast<float>(yi);
    const float u = xf * xf * (3.0f - 2.0f * xf);
    const float v = yf * yf * (3.0f - 2.0f * yf);
    const float a = hash01(xi, yi, seed);
    const float b = hash01(xi + 1, yi, seed);
    const float c = hash01(xi, yi + 1, seed);
    const float d = hash01(xi + 1, yi + 1, seed);
    const float ab = a + (b - a) * u;
    const float cd = c + (d - c) * u;
    return ab + (cd - ab) * v;
}

glm::vec3 mixv(glm::vec3 a, glm::vec3 b, float t) { return a + (b - a) * t; }

// y == 0 is the bottom row of the tile (texture space), y grows upwards.
glm::vec3 tileColor(int tile, int x, int y) {
    constexpr int T = Atlas::TileSize;
    const float u = static_cast<float>(x) / static_cast<float>(T);
    const float v = static_cast<float>(y) / static_cast<float>(T);

    switch (tile) {
    case 0: {  // grass top
        const float n = valueNoise(u * 5.0f, v * 5.0f, 11);
        glm::vec3 c = mixv({0.24f, 0.48f, 0.15f}, {0.38f, 0.65f, 0.24f}, n);
        if (hash01(x, y, 21) < 0.07f) {
            c *= 0.80f;
        }
        return c;
    }
    case 1: {  // grass side: dirt with a wavy green top band
        const float n = valueNoise(u * 6.0f, v * 6.0f, 31);
        const glm::vec3 dirt = mixv({0.38f, 0.27f, 0.18f}, {0.52f, 0.39f, 0.26f}, n);
        const int edge = 12 + static_cast<int>(hash01(x, 0, 41) * 2.0f);
        if (y >= edge) {
            const float g = valueNoise(u * 7.0f, v * 7.0f, 51);
            return mixv({0.26f, 0.50f, 0.17f}, {0.40f, 0.68f, 0.25f}, g);
        }
        return dirt;
    }
    case 2:  // dirt
        return mixv({0.38f, 0.27f, 0.18f}, {0.55f, 0.41f, 0.28f},
                    valueNoise(u * 6.0f, v * 6.0f, 61));
    case 3: {  // stone
        glm::vec3 c = mixv({0.42f, 0.42f, 0.44f}, {0.60f, 0.60f, 0.62f},
                           valueNoise(u * 5.0f, v * 5.0f, 71));
        if (valueNoise(u * 9.0f, v * 9.0f, 72) < 0.18f) {
            c *= 0.85f;
        }
        return c;
    }
    case 4:  // sand
        return mixv({0.76f, 0.70f, 0.47f}, {0.90f, 0.85f, 0.60f},
                    valueNoise(u * 7.0f, v * 7.0f, 81));
    case 5: {  // log side
        const float stripe = 0.85f + 0.15f * std::sin(static_cast<float>(x) * 1.4f);
        glm::vec3 c = mixv({0.30f, 0.21f, 0.12f}, {0.47f, 0.34f, 0.19f}, stripe);
        if (hash01(x / 3, y, 91) < 0.20f) {
            c *= 0.90f;
        }
        return c;
    }
    case 6: {  // log top rings
        const float dx = static_cast<float>(x) + 0.5f - 8.0f;
        const float dy = static_cast<float>(y) + 0.5f - 8.0f;
        const float r = std::sqrt(dx * dx + dy * dy);
        const float ring = 0.5f + 0.5f * std::sin(r * 1.8f);
        return mixv({0.42f, 0.30f, 0.16f}, {0.62f, 0.47f, 0.26f}, ring);
    }
    case 7: {  // leaves
        glm::vec3 c = mixv({0.13f, 0.30f, 0.10f}, {0.25f, 0.48f, 0.18f},
                           valueNoise(u * 8.0f, v * 8.0f, 101));
        if (hash01(x, y, 102) < 0.12f) {
            c *= 0.70f;
        }
        return c;
    }
    case 8: {  // planks
        glm::vec3 c = mixv({0.52f, 0.37f, 0.22f}, {0.68f, 0.51f, 0.32f},
                           valueNoise(u * 3.0f, v * 6.0f, 111));
        if (y % 4 == 0) {
            c *= 0.75f;
        }
        if ((x == 7 || x == 15) && (y % 4 != 0)) {
            c *= 0.85f;
        }
        return c;
    }
    case 9: {  // cobblestone
        const float n = valueNoise(u * 4.0f, v * 4.0f, 121);
        if (n < 0.42f) {
            return glm::vec3(0.30f, 0.30f, 0.31f);
        }
        return mixv({0.45f, 0.45f, 0.47f}, {0.68f, 0.68f, 0.70f}, n);
    }
    case 10: {  // water
        const float n = valueNoise(u * 5.0f, v * 5.0f, 131);
        const float wave =
            0.5f + 0.5f * std::sin((static_cast<float>(x) + static_cast<float>(y) * 2.0f) * 0.6f);
        return mixv({0.08f, 0.28f, 0.55f}, {0.15f, 0.42f, 0.72f}, n * 0.6f + wave * 0.4f);
    }
    case 11:  // gravel
        return mixv({0.36f, 0.33f, 0.31f}, {0.60f, 0.57f, 0.54f},
                    valueNoise(u * 8.0f, v * 8.0f, 141));
    default:
        return glm::vec3(1.0f, 0.0f, 1.0f);  // debug magenta
    }
}

}  // namespace

Atlas::~Atlas() {
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

bool Atlas::build() {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(Size) * Size * 4, 0);

    for (int tile = 0; tile < Grid * Grid; ++tile) {
        const int tx = tile % Grid;
        const int ty = tile / Grid;
        for (int py = 0; py < Pitch; ++py) {
            for (int px = 0; px < Pitch; ++px) {
                // Replicate the edge texels into the padding ring.
                const int cx = std::clamp(px - Padding, 0, TileSize - 1);
                const int cy = std::clamp(py - Padding, 0, TileSize - 1);
                const glm::vec3 c =
                    glm::clamp(tileColor(tile, cx, cy), glm::vec3(0.0f), glm::vec3(1.0f));

                const int x = tx * Pitch + px;
                const int y = ty * Pitch + py;
                const std::size_t o = (static_cast<std::size_t>(y) * Size + x) * 4;
                pixels[o + 0] = static_cast<std::uint8_t>(c.x * 255.0f + 0.5f);
                pixels[o + 1] = static_cast<std::uint8_t>(c.y * 255.0f + 0.5f);
                pixels[o + 2] = static_cast<std::uint8_t>(c.z * 255.0f + 0.5f);
                pixels[o + 3] = 255;
            }
        }
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Size, Size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // One mip level: the 2 px padding ring still guards against bleed at 40x40.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
    glBindTexture(GL_TEXTURE_2D, 0);

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        spdlog::error("atlas upload failed, GL error 0x{:04x}", static_cast<unsigned int>(err));
        return false;
    }
    spdlog::info("atlas built: {}x{} ({} tiles, padding {})", Size, Size, Grid * Grid, Padding);
    return true;
}

glm::vec4 Atlas::tileUV(int tile) const {
    const int clamped = std::clamp(tile, 0, Grid * Grid - 1);
    const int tx = clamped % Grid;
    const int ty = clamped / Grid;
    const float inv = 1.0f / static_cast<float>(Size);
    // Half-texel inset: the content occupies texels [p*Pitch+Padding, +TileSize).
    const float u0 = (static_cast<float>(tx * Pitch + Padding) + 0.5f) * inv;
    const float u1 = (static_cast<float>(tx * Pitch + Padding + TileSize) - 0.5f) * inv;
    const float v0 = (static_cast<float>(ty * Pitch + Padding) + 0.5f) * inv;
    const float v1 = (static_cast<float>(ty * Pitch + Padding + TileSize) - 0.5f) * inv;
    return glm::vec4(u0, v0, u1, v1);
}

}  // namespace aimc
