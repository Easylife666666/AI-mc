#include "render/screenshot.h"

#include <cstddef>
#include <cstring>
#include <vector>

#include <GL/glew.h>
#include <spdlog/spdlog.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

namespace aimc {

bool saveFramebufferPng(const char* path, int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    std::vector<std::uint8_t> pixels(rowBytes * static_cast<std::size_t>(height));

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    std::vector<std::uint8_t> flipped(pixels.size());
    for (int y = 0; y < height; ++y) {
        std::memcpy(&flipped[static_cast<std::size_t>(y) * rowBytes],
                    &pixels[static_cast<std::size_t>(height - 1 - y) * rowBytes], rowBytes);
    }

    const int ok = stbi_write_png(path, width, height, 4, flipped.data(),
                                  static_cast<int>(rowBytes));
    if (ok == 0) {
        spdlog::error("failed to write screenshot: {}", path);
        return false;
    }
    return true;
}

}  // namespace aimc
