#include "world/save.h"

#include <cstdint>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>
#include <zlib.h>

namespace aimc {
namespace {

constexpr char kMagic[4] = {'A', 'I', 'M', 'C'};
constexpr std::uint32_t kVersion = 4;  // 4 adds per-dimension state
constexpr std::uint32_t kMinVersion = 1;
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

bool writeHotbar(gzFile file, const std::array<ItemRef, kHotbarSlots>& hotbar) {
    bool ok = true;
    for (const ItemRef& slot : hotbar) {
        const auto block = static_cast<std::uint8_t>(slot.block);
        const auto item = static_cast<std::uint8_t>(slot.item);
        ok = ok && writePod(file, block);
        ok = ok && writePod(file, item);
    }
    return ok;
}

bool readHotbar(gzFile file, std::array<ItemRef, kHotbarSlots>& hotbar, bool withItems) {
    bool ok = true;
    for (ItemRef& slot : hotbar) {
        std::uint8_t block = 0;
        ok = ok && readPod(file, block);
        slot = block < static_cast<std::uint8_t>(BLOCK_COUNT)
                   ? ItemRef::ofBlock(static_cast<BlockId>(block))
                   : ItemRef{};
        if (withItems) {
            std::uint8_t item = 0;
            ok = ok && readPod(file, item);
            if (item < static_cast<std::uint8_t>(ItemId::Count) && item != 0) {
                slot.item = static_cast<ItemId>(item);
                slot.block = BLOCK_AIR;
            }
        }
    }
    return ok;
}

bool writeEdits(gzFile file,
                const std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>>& edits) {
    bool ok = true;
    const auto chunkCount = static_cast<std::uint32_t>(edits.size());
    ok = ok && writePod(file, chunkCount);
    for (const auto& [key, chunkEdits] : edits) {
        const auto cx = static_cast<std::int32_t>(key >> 32);
        const auto cz = static_cast<std::int32_t>(static_cast<std::uint32_t>(key));
        const auto count = static_cast<std::uint32_t>(chunkEdits.size());
        ok = ok && writePod(file, cx);
        ok = ok && writePod(file, cz);
        ok = ok && writePod(file, count);
        for (const auto& [index, block] : chunkEdits) {
            const auto idx = static_cast<std::uint16_t>(index);
            ok = ok && writePod(file, idx);
            ok = ok && writePod(file, block);
        }
        if (!ok) {
            break;
        }
    }
    return ok;
}

bool readEdits(gzFile file,
               std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>>& edits) {
    std::uint32_t chunkCount = 0;
    bool ok = readPod(file, chunkCount);
    if (!ok || chunkCount > kMaxChunks) {
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
            return false;
        }
        std::map<std::uint32_t, std::uint8_t>& chunkEdits =
            edits[(static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32) |
                  static_cast<std::uint32_t>(cz)];
        for (std::uint32_t e = 0; e < count; ++e) {
            std::uint16_t index = 0;
            std::uint8_t block = 0;
            ok = ok && readPod(file, index);
            ok = ok && readPod(file, block);
            chunkEdits[index] = block;
        }
    }
    return ok;
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
    ok = ok && writePod(file, data.timeOfDay);
    ok = ok && writePod(file, data.selected);
    ok = ok && writeHotbar(file, data.hotbar);

    const auto current = static_cast<std::uint8_t>(data.current);
    ok = ok && writePod(file, current);

    for (const DimensionSave& dim : data.dimensions) {
        ok = ok && writePod(file, dim.playerPos.x);
        ok = ok && writePod(file, dim.playerPos.y);
        ok = ok && writePod(file, dim.playerPos.z);
        ok = ok && writePod(file, dim.yaw);
        ok = ok && writePod(file, dim.pitch);
        ok = ok && writeEdits(file, dim.edits);
        if (!ok) {
            break;
        }
    }

    const int closeResult = gzclose(file);
    if (!ok || closeResult != Z_OK) {
        spdlog::error("failed to write save file: {}", path);
        return false;
    }
    spdlog::info("saved {} dimension states to {}", data.dimensions.size(), path);
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
    if (!ok || version < kMinVersion || version > kVersion) {
        spdlog::error("unsupported save version {}: {}", version, path);
        gzclose(file);
        return false;
    }

    SaveData loaded;
    ok = ok && readPod(file, loaded.seed);

    if (version >= 4) {
        ok = ok && readPod(file, loaded.timeOfDay);
        ok = ok && readPod(file, loaded.selected);
        ok = ok && readHotbar(file, loaded.hotbar, true);
        std::uint8_t current = 0;
        ok = ok && readPod(file, current);
        loaded.current = current < static_cast<std::uint8_t>(Dimension::Count)
                             ? static_cast<Dimension>(current)
                             : Dimension::Overworld;
        for (DimensionSave& dim : loaded.dimensions) {
            ok = ok && readPod(file, dim.playerPos.x);
            ok = ok && readPod(file, dim.playerPos.y);
            ok = ok && readPod(file, dim.playerPos.z);
            ok = ok && readPod(file, dim.yaw);
            ok = ok && readPod(file, dim.pitch);
            ok = ok && readEdits(file, dim.edits);
            if (!ok) {
                break;
            }
        }
    } else {
        // v1..v3 layout: one dimension, with the old field order.
        DimensionSave& overworld = loaded.dimensions[0];
        ok = ok && readPod(file, overworld.playerPos.x);
        ok = ok && readPod(file, overworld.playerPos.y);
        ok = ok && readPod(file, overworld.playerPos.z);
        ok = ok && readPod(file, overworld.yaw);
        ok = ok && readPod(file, overworld.pitch);
        ok = ok && readPod(file, loaded.selected);
        ok = ok && readPod(file, loaded.timeOfDay);
        if (version >= 3) {
            ok = ok && readHotbar(file, loaded.hotbar, true);
        } else if (version >= 2) {
            ok = ok && readHotbar(file, loaded.hotbar, false);
        }
        ok = ok && readEdits(file, overworld.edits);
        loaded.current = Dimension::Overworld;
    }

    gzclose(file);
    if (!ok) {
        spdlog::error("corrupt save file: {}", path);
        return false;
    }

    data = std::move(loaded);
    spdlog::info("loaded save: {} overworld chunks, {} nether chunks",
                 data.dimensions[0].edits.size(), data.dimensions[1].edits.size());
    return true;
}

}  // namespace aimc
