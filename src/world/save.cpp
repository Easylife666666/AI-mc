#include "world/save.h"

#include <cstdint>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>
#include <zlib.h>

namespace aimc {
namespace {

constexpr char kMagic[4] = {'A', 'I', 'M', 'C'};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kMaxChunks = 1u << 20;
constexpr std::uint32_t kMaxEdits = 1u << 16;

template <typename T>
bool writePod(gzFile file, const T& value) {
    return gzwrite(file, &value, static_cast<unsigned int>(sizeof(T))) ==
           static_cast<int>(sizeof(T));
}

template <typename T>
bool readPod(gzFile file, T& value) {
    return gzread(file, &value, static_cast<unsigned int>(sizeof(T))) ==
           static_cast<int>(sizeof(T));
}

}  // namespace

bool saveWorld(const std::string& path, const SaveData& data) {
    gzFile file = gzopen(path.c_str(), "wb6");
    if (file == nullptr) {
        spdlog::error("could not open save file for writing: {}", path);
        return false;
    }

    bool ok = true;
    ok = ok && gzwrite(file, kMagic, 4) == 4;
    ok = ok && writePod(file, kVersion);
    ok = ok && writePod(file, data.seed);
    ok = ok && writePod(file, data.playerPos.x);
    ok = ok && writePod(file, data.playerPos.y);
    ok = ok && writePod(file, data.playerPos.z);
    ok = ok && writePod(file, data.yaw);
    ok = ok && writePod(file, data.pitch);
    ok = ok && writePod(file, data.selected);
    ok = ok && writePod(file, data.timeOfDay);

    const auto chunkCount = static_cast<std::uint32_t>(data.edits.size());
    ok = ok && writePod(file, chunkCount);
    for (const auto& [key, edits] : data.edits) {
        const auto cx = static_cast<std::int32_t>(key >> 32);
        const auto cz = static_cast<std::int32_t>(static_cast<std::uint32_t>(key));
        const auto count = static_cast<std::uint32_t>(edits.size());
        ok = ok && writePod(file, cx);
        ok = ok && writePod(file, cz);
        ok = ok && writePod(file, count);
        for (const auto& [index, block] : edits) {
            const auto idx = static_cast<std::uint16_t>(index);
            ok = ok && writePod(file, idx);
            ok = ok && writePod(file, block);
        }
        if (!ok) {
            break;
        }
    }

    const int closeResult = gzclose(file);
    if (!ok || closeResult != Z_OK) {
        spdlog::error("failed to write save file: {}", path);
        return false;
    }
    spdlog::info("saved {} chunks of edits to {}", chunkCount, path);
    return true;
}

bool loadWorld(const std::string& path, SaveData& data) {
    gzFile file = gzopen(path.c_str(), "rb");
    if (file == nullptr) {
        spdlog::warn("could not open save file: {}", path);
        return false;
    }

    bool ok = true;
    char magic[4] = {};
    ok = ok && gzread(file, magic, 4) == 4;
    if (!ok || magic[0] != kMagic[0] || magic[1] != kMagic[1] || magic[2] != kMagic[2] ||
        magic[3] != kMagic[3]) {
        spdlog::error("not an AI-mc save file: {}", path);
        gzclose(file);
        return false;
    }

    std::uint32_t version = 0;
    ok = ok && readPod(file, version);
    if (!ok || version != kVersion) {
        spdlog::error("unsupported save version {}: {}", version, path);
        gzclose(file);
        return false;
    }

    SaveData loaded;
    ok = ok && readPod(file, loaded.seed);
    ok = ok && readPod(file, loaded.playerPos.x);
    ok = ok && readPod(file, loaded.playerPos.y);
    ok = ok && readPod(file, loaded.playerPos.z);
    ok = ok && readPod(file, loaded.yaw);
    ok = ok && readPod(file, loaded.pitch);
    ok = ok && readPod(file, loaded.selected);
    ok = ok && readPod(file, loaded.timeOfDay);

    std::uint32_t chunkCount = 0;
    ok = ok && readPod(file, chunkCount);
    if (!ok || chunkCount > kMaxChunks) {
        spdlog::error("corrupt save file (chunk count {}): {}", chunkCount, path);
        gzclose(file);
        return false;
    }

    for (std::uint32_t i = 0; i < chunkCount && ok; ++i) {
        std::int32_t cx = 0;
        std::int32_t cz = 0;
        std::uint32_t count = 0;
        ok = ok && readPod(file, cx);
        ok = ok && readPod(file, cz);
        ok = ok && readPod(file, count);
        if (!ok || count > kMaxEdits) {
            ok = false;
            break;
        }
        std::map<std::uint32_t, std::uint8_t>& edits =
            loaded.edits[(static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32) |
                         static_cast<std::uint32_t>(cz)];
        for (std::uint32_t e = 0; e < count; ++e) {
            std::uint16_t index = 0;
            std::uint8_t block = 0;
            ok = ok && readPod(file, index);
            ok = ok && readPod(file, block);
            edits[index] = block;
        }
    }

    gzclose(file);
    if (!ok) {
        spdlog::error("corrupt save file: {}", path);
        return false;
    }

    data = std::move(loaded);
    spdlog::info("loaded {} chunk edits from {}", data.edits.size(), path);
    return true;
}

}  // namespace aimc
