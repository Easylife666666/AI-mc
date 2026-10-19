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
    case 12: {  // stone bricks: running bond, darker mortar
        const int row = y / 8;
        const int shift = (row % 2) * 4;
        const glm::vec3 stone = mixv({0.44f, 0.44f, 0.46f}, {0.64f, 0.64f, 0.66f},
                                     valueNoise(u * 6.0f, v * 6.0f, 151));
        const bool mortar = (y % 8 == 7) || ((x + shift) % 8 == 7);
        return mortar ? stone * 0.62f : stone;
    }
    case 13: {  // bricks: running bond, pale mortar
        const int row = y / 4;
        const int shift = (row % 2) * 4;
        const glm::vec3 clay = mixv({0.52f, 0.24f, 0.18f}, {0.74f, 0.38f, 0.29f},
                                    valueNoise(u * 7.0f, v * 7.0f, 161));
        const bool mortar = (y % 4 == 3) || ((y % 4 != 3) && ((x + shift) % 8 == 7));
        return mortar ? glm::vec3(0.63f, 0.61f, 0.57f) : clay;
    }
    case 14: {  // snow block: almost white with a faint blue cast
        glm::vec3 c = mixv({0.86f, 0.90f, 0.96f}, {0.98f, 0.99f, 1.0f},
                           valueNoise(u * 6.0f, v * 6.0f, 171));
        if (hash01(x, y, 172) < 0.06f) {
            c *= 0.94f;
        }
        return c;
    }
    case 16: {  // flint and steel inventory icon (alpha comes from tileAlpha)
        // y grows upwards, so the flint sits low-left and the striker runs up-right.
        const bool flint = x >= 3 && x <= 8 && y >= 2 && y <= 6 &&
                           !(x == 3 && y == 2) && !(x == 8 && y == 2) &&
                           !(x == 3 && y == 6) && !(x == 8 && y == 6);
        const bool bar = x >= 8 && x <= 13 && (y - x) >= -2 && (y - x) <= 1;
        const bool hook = x >= 6 && x <= 8 && y >= 4 && y <= 6;
        const bool spark = (x == 12 && y == 12) || (x == 13 && y == 12) || (x == 12 && y == 13);
        if (spark) {
            return glm::vec3(1.0f, 0.76f, 0.20f);
        }
        if (flint) {
            return mixv({0.16f, 0.14f, 0.17f}, {0.38f, 0.34f, 0.37f},
                        valueNoise(u * 9.0f, v * 9.0f, 201));
        }
        if (bar || hook) {
            const float n = valueNoise(u * 10.0f, v * 10.0f, 202);
            return mixv({0.58f, 0.60f, 0.64f}, {0.92f, 0.94f, 0.98f}, n);
        }
        return glm::vec3(0.0f);
    }
    case 17: {  // obsidian: near-black with sparse purple glints
        glm::vec3 c = mixv({0.05f, 0.04f, 0.08f}, {0.15f, 0.11f, 0.21f},
                           valueNoise(u * 6.0f, v * 6.0f, 181));
        if (hash01(x, y, 182) < 0.03f) {
            c = mixv(c, {0.26f, 0.16f, 0.38f}, 0.7f);
        }
        return c;
    }
    case 19: {  // netherrack: dark red, crumbly
        glm::vec3 c = mixv({0.26f, 0.07f, 0.07f}, {0.55f, 0.20f, 0.18f},
                           valueNoise(u * 7.0f, v * 7.0f, 211));
        if (hash01(x, y, 212) < 0.14f) {
            c *= 0.76f;
        }
        if (hash01(x, y, 213) < 0.05f) {
            c = mixv(c, {0.62f, 0.28f, 0.24f}, 0.6f);
        }
        return c;
    }
    case 20: {  // soul sand: brown with dark blotches
        glm::vec3 c = mixv({0.34f, 0.24f, 0.19f}, {0.55f, 0.41f, 0.33f},
                           valueNoise(u * 6.0f, v * 6.0f, 221));
        if (valueNoise(u * 4.0f + 3.1f, v * 4.0f + 1.7f, 222) > 0.62f) {
            c *= 0.70f;
        }
        return c;
    }
    case 21: {  // glowstone
        glm::vec3 c = mixv({0.55f, 0.32f, 0.08f}, {1.0f, 0.80f, 0.32f},
                           valueNoise(u * 7.0f, v * 7.0f, 231));
        if (hash01(x, y, 232) < 0.18f) {
            c = mixv(c, {1.0f, 0.95f, 0.62f}, 0.8f);
        }
        if (hash01(x, y, 233) < 0.08f) {
            c *= 0.72f;
        }
        return c;
    }
    case 22: {  // bedrock
        const float n = valueNoise(u * 8.0f, v * 8.0f, 241);
        if (n < 0.35f) {
            return glm::vec3(0.08f, 0.08f, 0.09f);
        }
        if (n > 0.72f) {
            return glm::vec3(0.40f, 0.40f, 0.42f);
        }
        return mixv({0.16f, 0.16f, 0.17f}, {0.30f, 0.30f, 0.32f}, n);
    }
    case 23: {  // lava
        const float swirl = std::sin(static_cast<float>(x) * 0.7f +
                                     static_cast<float>(y) * 0.9f +
                                     valueNoise(u * 4.0f, v * 4.0f, 251) * 5.0f);
        glm::vec3 c = mixv({0.75f, 0.18f, 0.02f}, {1.0f, 0.62f, 0.05f},
                           0.5f + 0.5f * swirl);
        if (valueNoise(u * 6.0f, v * 6.0f, 252) < 0.25f) {
            c = mixv(c, {0.35f, 0.08f, 0.02f}, 0.55f);
        }
        return c;
    }
    case 24: {  // nether bricks
        const int row = y / 4;
        const int shift = (row % 2) * 4;
        const glm::vec3 brick = mixv({0.24f, 0.07f, 0.08f}, {0.38f, 0.13f, 0.13f},
                                     valueNoise(u * 7.0f, v * 7.0f, 261));
        const bool mortar = (y % 4 == 3) || ((x + shift) % 8 == 7);
        return mortar ? glm::vec3(0.12f, 0.10f, 0.10f) : brick;
    }
    case 18: {  // nether portal swirl
        const float swirl = std::sin((static_cast<float>(x) * 0.9f +
                                      static_cast<float>(y) * 0.55f) * 0.9f +
                                     valueNoise(u * 3.0f, v * 3.0f, 191) * 5.0f);
        glm::vec3 c = mixv({0.20f, 0.02f, 0.40f}, {0.66f, 0.22f, 1.0f},
                           0.5f + 0.5f * swirl);
        if (valueNoise(u * 4.0f, v * 4.0f, 192) < 0.26f) {
            c *= 0.62f;
        }
        return c;
    }
    default:
        return glm::vec3(1.0f, 0.0f, 1.0f);  // debug magenta
    }
}

float tileAlpha(int tile, int x, int y) {
    if (tile != 16) {
        return 1.0f;
    }
    const bool flint = x >= 3 && x <= 8 && y >= 2 && y <= 6 &&
                       !(x == 3 && y == 2) && !(x == 8 && y == 2) &&
                       !(x == 3 && y == 6) && !(x == 8 && y == 6);
    const bool bar = x >= 8 && x <= 13 && (y - x) >= -2 && (y - x) <= 1;
    const bool hook = x >= 6 && x <= 8 && y >= 4 && y <= 6;
    const bool spark = (x == 12 && y == 12) || (x == 13 && y == 12) || (x == 12 && y == 13);
    return (flint || bar || hook || spark) ? 1.0f : 0.0f;
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
                pixels[o + 3] =
                    static_cast<std::uint8_t>(tileAlpha(tile, cx, cy) * 255.0f + 0.5f);
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
