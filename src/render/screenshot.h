#pragma once

namespace aimc {

// Reads the current back buffer and writes a vertically flipped PNG.
bool saveFramebufferPng(const char* path, int width, int height);

}  // namespace aimc
