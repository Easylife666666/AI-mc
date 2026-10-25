#include "world/fluid.h"

#include <algorithm>
#include <cstdint>

#include "world/chunk.h"
#include "world/world.h"

namespace aimc {
namespace {

constexpr int kUpdatesPerTick = 512;
constexpr std::uint64_t kQueueLimit = 100000;

bool isPassable(BlockId id) { return id == BLOCK_AIR; }

bool sameKind(BlockId id, FluidKind kind) { return fluidKind(id) == kind; }

int maxLevel(const World& world, FluidKind kind) {
    if (kind == FluidKind::Lava) {
        return world.dimension() == Dimension::Nether ? 7 : 3;
    }
    return 7;
}

bool supported(const World& world, const glm::ivec3& pos, FluidKind kind, int level) {
    if (pos.y + 1 < Chunk::SY && sameKind(world.getBlock(pos.x, pos.y + 1, pos.z), kind)) {
        return true;
    }
    static const glm::ivec3 dirs[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const glm::ivec3& dir : dirs) {
        const BlockId neighbour =
            world.getBlock(pos.x + dir.x, pos.y, pos.z + dir.z);
        if (sameKind(neighbour, kind) && fluidLevel(neighbour) < level) {
            return true;
        }
    }
    return false;
}

void spread(World& world, const glm::ivec3& pos, FluidKind kind, int level, int limit) {
    if (level >= limit) {
        return;
    }
    const int next = level + 1;
    const BlockId flow = fluidBlock(kind, next);
    static const glm::ivec3 dirs[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const glm::ivec3& dir : dirs) {
        const glm::ivec3 target = pos + dir;
        if (target.y < 0 || target.y >= Chunk::SY) {
            continue;
        }
        const BlockId block = world.getBlock(target.x, target.y, target.z);
        if (block == BLOCK_AIR) {
            world.setBlock(target.x, target.y, target.z, flow);
        } else if (sameKind(block, kind) && fluidLevel(block) > next &&
                   fluidLevel(block) <= 7) {
            world.setBlock(target.x, target.y, target.z, flow);
        }
    }
}

const glm::ivec3 kNeighbours6[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                     {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};

bool hasNeighbourKind(const World& world, const glm::ivec3& pos, FluidKind kind) {
    for (const glm::ivec3& dir : kNeighbours6) {
        if (fluidKind(world.getBlock(pos.x + dir.x, pos.y + dir.y, pos.z + dir.z)) == kind) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool FluidSim::EntryGreater::operator()(const Entry& a, const Entry& b) const {
    if (a.due != b.due) {
        return a.due > b.due;
    }
    if (a.pos.x != b.pos.x) {
        return a.pos.x > b.pos.x;
    }
    if (a.pos.y != b.pos.y) {
        return a.pos.y > b.pos.y;
    }
    return a.pos.z > b.pos.z;
}

std::size_t FluidSim::PosHash::operator()(const glm::ivec3& p) const noexcept {
    std::uint64_t h =
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(p.x)) * 0x9E3779B97F4A7C15ull;
    h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(p.z)) +
         0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(p.y)) +
         0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return static_cast<std::size_t>(h);
}

void FluidSim::clear() {
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    m_pending.clear();
    m_tick = 0;
}

int FluidSim::delayFor(const World& world, BlockId block) {
    if (isWater(block)) {
        return 5;
    }
    if (isLava(block)) {
        return world.dimension() == Dimension::Nether ? 10 : 30;
    }
    return 0;
}

void FluidSim::schedule(const glm::ivec3& pos, int delayTicks) {
    if (delayTicks <= 0) {
        delayTicks = 1;
    }
    if (m_pending.size() >= kQueueLimit) {
        return;
    }
    const std::uint64_t due = m_tick + static_cast<std::uint64_t>(delayTicks);
    const auto it = m_pending.find(pos);
    if (it != m_pending.end() && it->second <= due) {
        return;
    }
    m_pending[pos] = due;
    m_queue.push(Entry{due, pos});
}

void FluidSim::scheduleIfFluid(World& world, const glm::ivec3& pos) {
    if (pos.y < 0 || pos.y >= Chunk::SY) {
        return;
    }
    const BlockId block = world.getBlock(pos.x, pos.y, pos.z);
    if (!isLiquid(block)) {
        return;
    }
    schedule(pos, delayFor(world, block));
}

void FluidSim::scheduleAround(World& world, int x, int y, int z) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if ((dx != 0) + (dy != 0) + (dz != 0) > 1) {
                    continue;
                }
                scheduleIfFluid(world, glm::ivec3(x + dx, y + dy, z + dz));
            }
        }
    }
}

void FluidSim::tick(World& world) {
    ++m_tick;
    int budget = kUpdatesPerTick;
    while (budget > 0 && !m_queue.empty()) {
        const Entry& top = m_queue.top();
        if (top.due > m_tick) {
            break;
        }
        const Entry entry = top;
        m_queue.pop();
        const auto it = m_pending.find(entry.pos);
        if (it == m_pending.end() || it->second != entry.due) {
            continue;  // stale entry superseded by a newer schedule
        }
        m_pending.erase(it);
        --budget;
        process(world, entry.pos);
    }
}

void FluidSim::process(World& world, const glm::ivec3& pos) {
    const BlockId block = world.getBlock(pos.x, pos.y, pos.z);
    const FluidKind kind = fluidKind(block);
    if (kind == FluidKind::None) {
        return;
    }

    // Water cannot exist in the nether.
    if (world.dimension() == Dimension::Nether && kind == FluidKind::Water) {
        world.setBlock(pos.x, pos.y, pos.z, BLOCK_AIR);
        return;
    }

    // Lava touching water solidifies; the water is not consumed.
    const int level = fluidLevel(block);
    if (kind == FluidKind::Lava && hasNeighbourKind(world, pos, FluidKind::Water)) {
        world.setBlock(pos.x, pos.y, pos.z,
                       level == 0 ? BLOCK_OBSIDIAN : BLOCK_COBBLE);
        return;
    }
    if (kind == FluidKind::Water) {
        for (const glm::ivec3& dir : kNeighbours6) {
            const glm::ivec3 n = pos + dir;
            const BlockId neighbour = world.getBlock(n.x, n.y, n.z);
            if (fluidKind(neighbour) == FluidKind::Lava) {
                world.setBlock(n.x, n.y, n.z,
                               fluidLevel(neighbour) == 0 ? BLOCK_OBSIDIAN : BLOCK_COBBLE);
            }
        }
    }
    const int limit = maxLevel(world, kind);
    const BlockId below = pos.y > 0 ? world.getBlock(pos.x, pos.y - 1, pos.z) : BLOCK_BEDROCK;
    const bool canFall = pos.y > 0 && isPassable(below);

    if (level == 0) {  // source: permanent, but it can feed a column or spread
        if (canFall) {
            world.setBlock(pos.x, pos.y - 1, pos.z, fluidBlock(kind, 8));
            return;
        }
        if (sameKind(below, kind) && fluidLevel(below) == 8) {
            return;  // the column below is still falling; do not fan out here
        }
        spread(world, pos, kind, 0, limit);
        return;
    }

    if (level == 8) {  // falling column
        // A falling block must be fed from above; otherwise it dries out.
        if (pos.y + 1 >= Chunk::SY || !sameKind(world.getBlock(pos.x, pos.y + 1, pos.z), kind)) {
            world.setBlock(pos.x, pos.y, pos.z, BLOCK_AIR);
            return;
        }
        if (canFall) {
            world.setBlock(pos.x, pos.y - 1, pos.z, fluidBlock(kind, 8));
            return;
        }
        if (sameKind(below, kind)) {
            return;  // the column feeds into a pool; no side spread at this height
        }
        // Landed: continue as a level-1 flow from here.
        if (world.setBlock(pos.x, pos.y, pos.z, fluidBlock(kind, 1))) {
            spread(world, pos, kind, 1, limit);
        }
        return;
    }

    // Levels 1..7: a flow must be fed by fluid above or a closer neighbour.
    if (!supported(world, pos, kind, level)) {
        world.setBlock(pos.x, pos.y, pos.z, BLOCK_AIR);
        return;
    }
    if (canFall) {
        world.setBlock(pos.x, pos.y - 1, pos.z, fluidBlock(kind, 8));
        return;
    }
    spread(world, pos, kind, level, limit);
}

void FluidSim::wakeChunk(World& world, const Chunk& chunk) {
    const glm::ivec3 origin = chunk.origin();
    const auto blockAt = [&](int lx, int ly, int lz) -> BlockId {
        if (lx >= 0 && lx < Chunk::SX && ly >= 0 && ly < Chunk::SY && lz >= 0 &&
            lz < Chunk::SZ) {
            return static_cast<BlockId>(chunk.get(lx, ly, lz));
        }
        return world.getBlock(origin.x + lx, origin.y + ly, origin.z + lz);
    };

    for (int y = 0; y < Chunk::SY; ++y) {
        for (int z = 0; z < Chunk::SZ; ++z) {
            for (int x = 0; x < Chunk::SX; ++x) {
                const BlockId block = static_cast<BlockId>(chunk.get(x, y, z));
                if (!isLiquid(block)) {
                    continue;
                }
                if (world.dimension() == Dimension::Nether && isWater(block)) {
                    (void)world.setBlock(origin.x + x, y, origin.z + z, BLOCK_AIR);
                    continue;
                }
                const glm::ivec3 pos(origin.x + x, y, origin.z + z);
                if (isFluidFlow(block)) {
                    scheduleIfFluid(world, pos);
                    continue;
                }
                // Sources only need waking when they have somewhere to go.
                bool open = y > 0 && isPassable(blockAt(x, y - 1, z));
                open = open || isPassable(blockAt(x - 1, y, z)) ||
                       isPassable(blockAt(x + 1, y, z)) ||
                       isPassable(blockAt(x, y, z - 1)) ||
                       isPassable(blockAt(x, y, z + 1));
                if (open) {
                    scheduleIfFluid(world, pos);
                }
            }
        }
    }

    // Resume flows that stopped at this border while the neighbour was unloaded.
    for (int y = 0; y < Chunk::SY; ++y) {
        for (int i = 0; i < Chunk::SX; ++i) {
            scheduleIfFluid(world, glm::ivec3(origin.x - 1, y, origin.z + i));
            scheduleIfFluid(world, glm::ivec3(origin.x + Chunk::SX, y, origin.z + i));
            scheduleIfFluid(world, glm::ivec3(origin.x + i, y, origin.z - 1));
            scheduleIfFluid(world, glm::ivec3(origin.x + i, y, origin.z + Chunk::SZ));
        }
    }
}

}  // namespace aimc
