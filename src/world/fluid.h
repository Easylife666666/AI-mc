#pragma once

#include <cstdint>
#include <queue>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "world/block.h"

namespace aimc {

class World;
class Chunk;

// Main-thread fluid scheduler. Water and lava are full blocks whose level is
// encoded in the block id; sources are BLOCK_WATER / BLOCK_LAVA and never dry.
class FluidSim {
public:
    void clear();
    void tick(World& world);

    // Queues the given block and its six direct neighbours when they are fluids.
    void scheduleAround(World& world, int x, int y, int z);

    // Re-queues flowing blocks and sources with an open side in a freshly
    // loaded chunk, plus fluids just outside its border.
    void wakeChunk(World& world, const Chunk& chunk);

    std::size_t pending() const { return m_pending.size(); }

private:
    struct Entry {
        std::uint64_t due = 0;
        glm::ivec3 pos{0};
    };
    struct EntryGreater {
        bool operator()(const Entry& a, const Entry& b) const;
    };
    struct PosHash {
        std::size_t operator()(const glm::ivec3& p) const noexcept;
    };

    static int delayFor(const World& world, BlockId block);
    void scheduleIfFluid(World& world, const glm::ivec3& pos);
    void schedule(const glm::ivec3& pos, int delayTicks);
    void process(World& world, const glm::ivec3& pos);

    std::priority_queue<Entry, std::vector<Entry>, EntryGreater> m_queue;
    std::unordered_map<glm::ivec3, std::uint64_t, PosHash> m_pending;
    std::uint64_t m_tick = 0;
};

}  // namespace aimc
