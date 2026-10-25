#include "world/world.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

#include <GL/glew.h>
#include <spdlog/spdlog.h>

#include "core/math.h"
#include "render/atlas.h"
#include "render/frustum.h"
#include "world/chunk.h"
#include "world/terrain.h"

namespace aimc {
namespace {

// One terrain generator per worker thread: FastNoiseLite is cheap to build and
// this keeps generation fully thread-local.
Terrain& threadTerrain(int seed) {
    static thread_local std::unique_ptr<Terrain> terrain;
    static thread_local int cachedSeed = -1;
    if (!terrain || cachedSeed != seed) {
        terrain = std::make_unique<Terrain>(seed);
        cachedSeed = seed;
    }
    return *terrain;
}

NetherTerrain& threadNether(int seed) {
    static thread_local std::unique_ptr<NetherTerrain> terrain;
    static thread_local int cachedSeed = -1;
    if (!terrain || cachedSeed != seed) {
        terrain = std::make_unique<NetherTerrain>(seed);
        cachedSeed = seed;
    }
    return *terrain;
}

struct PendingGuard {
    std::atomic<int>& counter;
    ~PendingGuard() { counter.fetch_sub(1); }
};

void uploadMesh(GpuMesh& gpu, const MeshData& mesh) {
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        gpu.indexCount = 0;
        return;
    }

    if (gpu.vao == 0) {
        glGenVertexArrays(1, &gpu.vao);
        glGenBuffers(1, &gpu.vbo);
        glGenBuffers(1, &gpu.ibo);
    }

    glBindVertexArray(gpu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                 mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, light)));

    gpu.indexCount = static_cast<int>(mesh.indices.size());
}

void drawGpuMesh(const GpuMesh& gpu) {
    if (gpu.indexCount <= 0) {
        return;
    }
    glBindVertexArray(gpu.vao);
    glDrawElements(GL_TRIANGLES, gpu.indexCount, GL_UNSIGNED_INT, nullptr);
}

}  // namespace

World::World() = default;
World::~World() { shutdown(); }

std::uint64_t World::keyFor(int cx, int cz) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32) |
           static_cast<std::uint32_t>(cz);
}

void World::freeGpu(GpuMesh& mesh) {
    if (mesh.vao != 0) {
        glDeleteVertexArrays(1, &mesh.vao);
        mesh.vao = 0;
    }
    if (mesh.vbo != 0) {
        glDeleteBuffers(1, &mesh.vbo);
        mesh.vbo = 0;
    }
    if (mesh.ibo != 0) {
        glDeleteBuffers(1, &mesh.ibo);
        mesh.ibo = 0;
    }
    mesh.indexCount = 0;
}

void World::init(int seed) {
    m_seed = seed;
    spdlog::info("world init: seed={}", seed);
}

void World::ensureChunk(int cx, int cz) {
    std::shared_ptr<Chunk> chunk;
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        const std::uint64_t key = keyFor(cx, cz);
        if (m_chunks.find(key) != m_chunks.end()) {
            return;
        }
        chunk = std::make_shared<Chunk>(glm::ivec2(cx, cz));
        m_chunks.emplace(key, chunk);
    }

    chunk->state.store(ChunkState::Generating, std::memory_order_release);
    m_pending.fetch_add(1);
    m_group.run([this, chunk] { runGenerate(std::move(chunk)); });
}

void World::runGenerate(std::shared_ptr<Chunk> chunk) {
    PendingGuard guard(m_pending);
    {
        auto lock = chunk->writeLock();
        if (m_dimension == Dimension::Nether) {
            threadNether(m_seed).generate(*chunk);
        } else {
            threadTerrain(m_seed).generate(*chunk);
        }
        applyEditsLocked(*chunk);
    }
    chunk->state.store(ChunkState::Generated, std::memory_order_release);
}

void World::applyEditsLocked(Chunk& chunk) {
    std::lock_guard<std::mutex> lock(m_editsMutex);
    const std::uint64_t key = keyFor(chunk.coord().x, chunk.coord().y);
    const auto it = m_edits.find(key);
    if (it == m_edits.end()) {
        return;
    }
    auto& blocks = chunk.rawBlocks();
    for (const auto& [index, id] : it->second) {
        if (index < blocks.size()) {
            blocks[index] = id;
        }
    }
}

void World::tryScheduleMesh(int cx, int cz) {
    if (m_atlas == nullptr) {
        return;
    }

    std::shared_ptr<Chunk> chunk;
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        const auto it = m_chunks.find(keyFor(cx, cz));
        if (it == m_chunks.end()) {
            return;
        }
        chunk = it->second;

        if (chunk->state.load(std::memory_order_acquire) != ChunkState::Generated) {
            return;
        }

        // Only mesh when all four face neighbours have block data; otherwise the
        // border faces would be wrong and would need a remesh later.
        static constexpr int kNeighbors[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& offset : kNeighbors) {
            const auto nit = m_chunks.find(keyFor(cx + offset[0], cz + offset[1]));
            if (nit == m_chunks.end() ||
                nit->second->state.load(std::memory_order_acquire) < ChunkState::Generated) {
                return;
            }
        }
    }

    ChunkState expected = ChunkState::Generated;
    if (!chunk->state.compare_exchange_strong(expected, ChunkState::Meshing)) {
        return;
    }

    m_pending.fetch_add(1);
    m_group.run([this, chunk] { runMesh(std::move(chunk)); });
}

void World::runMesh(std::shared_ptr<Chunk> chunk) {
    PendingGuard guard(m_pending);
    const std::uint32_t version = chunk->version.load(std::memory_order_acquire);

    MeshResult result;
    result.chunk = chunk;
    result.version = version;
    buildChunkMesh(*chunk, *this, *m_atlas, result.mesh);

    m_meshQueue.push(std::move(result));
    chunk->state.store(ChunkState::Meshed, std::memory_order_release);
}

void World::drainUploads(int budget) {
    MeshResult result;
    while (budget-- > 0 && m_meshQueue.try_pop(result)) {
        bool alive = false;
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            const auto it =
                m_chunks.find(keyFor(result.chunk->coord().x, result.chunk->coord().y));
            alive = it != m_chunks.end() && it->second.get() == result.chunk.get();
        }
        if (!alive) {
            continue;
        }

        if (result.version != result.chunk->version.load(std::memory_order_acquire)) {
            // A newer edit arrived while this mesh was building; remesh.
            result.chunk->state.store(ChunkState::Generated, std::memory_order_release);
            continue;
        }

        uploadMesh(result.chunk->opaqueMesh, result.mesh.opaque);
        uploadMesh(result.chunk->waterMesh, result.mesh.water);
        uploadMesh(result.chunk->lavaMesh, result.mesh.lava);
        uploadMesh(result.chunk->portalMesh, result.mesh.portal);
        result.chunk->state.store(ChunkState::Ready, std::memory_order_release);
        if (!result.chunk->fluidWoken) {
            result.chunk->fluidWoken = true;
            m_fluid.wakeChunk(*this, *result.chunk);
        }
    }
}

void World::unloadOutside(int centerX, int centerZ, int radius) {
    std::vector<std::shared_ptr<Chunk>> removed;
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        for (auto it = m_chunks.begin(); it != m_chunks.end();) {
            const auto& chunk = it->second;
            const int dx = std::abs(chunk->coord().x - centerX);
            const int dz = std::abs(chunk->coord().y - centerZ);
            if (dx > radius || dz > radius) {
                removed.push_back(std::move(it->second));
                it = m_chunks.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& chunk : removed) {
        freeGpu(chunk->opaqueMesh);
        freeGpu(chunk->waterMesh);
        freeGpu(chunk->lavaMesh);
        freeGpu(chunk->portalMesh);
        chunk->state.store(ChunkState::Empty, std::memory_order_release);
    }
}

void World::update(const glm::vec3& cameraPos, int viewDistance, bool upload) {
    m_viewDistance = std::clamp(viewDistance, 2, 16);
    const int centerX = floorDiv(static_cast<int>(std::floor(cameraPos.x)), Chunk::SX);
    const int centerZ = floorDiv(static_cast<int>(std::floor(cameraPos.z)), Chunk::SZ);

    const int generateRadius = m_viewDistance + 1;
    for (int dz = -generateRadius; dz <= generateRadius; ++dz) {
        for (int dx = -generateRadius; dx <= generateRadius; ++dx) {
            ensureChunk(centerX + dx, centerZ + dz);
        }
    }

    for (int dz = -m_viewDistance; dz <= m_viewDistance; ++dz) {
        for (int dx = -m_viewDistance; dx <= m_viewDistance; ++dx) {
            tryScheduleMesh(centerX + dx, centerZ + dz);
        }
    }

    unloadOutside(centerX, centerZ, m_viewDistance + 2);

    if (upload) {
        drainUploads(8);
    }
}

bool World::allReady(int centerX, int centerZ, int radius) const {
    std::lock_guard<std::mutex> lock(m_mapMutex);
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const auto it = m_chunks.find(keyFor(centerX + dx, centerZ + dz));
            if (it == m_chunks.end() ||
                it->second->state.load(std::memory_order_acquire) != ChunkState::Ready) {
                return false;
            }
        }
    }
    return true;
}

void World::prewarm(const glm::vec3& cameraPos, int viewDistance, int timeoutMs) {
    const auto start = std::chrono::steady_clock::now();
    const int centerX = floorDiv(static_cast<int>(std::floor(cameraPos.x)), Chunk::SX);
    const int centerZ = floorDiv(static_cast<int>(std::floor(cameraPos.z)), Chunk::SZ);

    while (true) {
        update(cameraPos, viewDistance, true);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
        if (m_pending.load() == 0 && m_meshQueue.empty() &&
            allReady(centerX, centerZ, viewDistance)) {
            break;
        }
        if (elapsed > timeoutMs) {
            spdlog::warn("prewarm timed out after {} ms ({} chunks pending)", timeoutMs,
                         m_pending.load());
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    m_group.wait();
    drainUploads(1 << 20);
    spdlog::info("world ready: {} chunks", chunkCount());
}

DrawStats World::drawOpaque(const Frustum& frustum) const {
    DrawStats stats;
    stats.totalChunks = static_cast<int>(chunkCount());

    std::lock_guard<std::mutex> lock(m_mapMutex);
    for (const auto& [key, chunk] : m_chunks) {
        if (chunk->opaqueMesh.indexCount <= 0 && chunk->waterMesh.indexCount <= 0) {
            continue;
        }
        const glm::vec3 origin = glm::vec3(chunk->origin());
        const glm::vec3 min = origin;
        const glm::vec3 max = origin + glm::vec3(Chunk::SX, Chunk::SY, Chunk::SZ);
        if (!frustum.intersectsAabb(min, max)) {
            continue;
        }
        ++stats.visibleChunks;
        if (chunk->opaqueMesh.indexCount > 0) {
            drawGpuMesh(chunk->opaqueMesh);
            ++stats.drawCalls;
            stats.triangles += chunk->opaqueMesh.indexCount / 3;
        }
    }
    glBindVertexArray(0);
    return stats;
}

DrawStats World::drawWater(const Frustum& frustum) const {
    DrawStats stats;
    std::lock_guard<std::mutex> lock(m_mapMutex);
    for (const auto& [key, chunk] : m_chunks) {
        if (chunk->waterMesh.indexCount <= 0) {
            continue;
        }
        const glm::vec3 origin = glm::vec3(chunk->origin());
        const glm::vec3 min = origin;
        const glm::vec3 max = origin + glm::vec3(Chunk::SX, Chunk::SY, Chunk::SZ);
        if (!frustum.intersectsAabb(min, max)) {
            continue;
        }
        drawGpuMesh(chunk->waterMesh);
        ++stats.drawCalls;
        stats.triangles += chunk->waterMesh.indexCount / 3;
    }
    glBindVertexArray(0);
    return stats;
}

DrawStats World::drawLava(const Frustum& frustum) const {
    DrawStats stats;
    std::lock_guard<std::mutex> lock(m_mapMutex);
    for (const auto& [key, chunk] : m_chunks) {
        if (chunk->lavaMesh.indexCount <= 0) {
            continue;
        }
        const glm::vec3 origin = glm::vec3(chunk->origin());
        const glm::vec3 min = origin;
        const glm::vec3 max = origin + glm::vec3(Chunk::SX, Chunk::SY, Chunk::SZ);
        if (!frustum.intersectsAabb(min, max)) {
            continue;
        }
        drawGpuMesh(chunk->lavaMesh);
        ++stats.drawCalls;
        stats.triangles += chunk->lavaMesh.indexCount / 3;
    }
    glBindVertexArray(0);
    return stats;
}

DrawStats World::drawPortal(const Frustum& frustum) const {
    DrawStats stats;
    std::lock_guard<std::mutex> lock(m_mapMutex);
    for (const auto& [key, chunk] : m_chunks) {
        if (chunk->portalMesh.indexCount <= 0) {
            continue;
        }
        const glm::vec3 origin = glm::vec3(chunk->origin());
        const glm::vec3 min = origin;
        const glm::vec3 max = origin + glm::vec3(Chunk::SX, Chunk::SY, Chunk::SZ);
        if (!frustum.intersectsAabb(min, max)) {
            continue;
        }
        drawGpuMesh(chunk->portalMesh);
        ++stats.drawCalls;
        stats.triangles += chunk->portalMesh.indexCount / 3;
    }
    glBindVertexArray(0);
    return stats;
}

BlockId World::getBlock(int worldX, int worldY, int worldZ) const {
    if (worldY < 0 || worldY >= Chunk::SY) {
        return BLOCK_AIR;
    }
    const int cx = floorDiv(worldX, Chunk::SX);
    const int cz = floorDiv(worldZ, Chunk::SZ);

    std::shared_ptr<Chunk> chunk;
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        const auto it = m_chunks.find(keyFor(cx, cz));
        if (it == m_chunks.end()) {
            return BLOCK_AIR;
        }
        chunk = it->second;
    }
    if (chunk->state.load(std::memory_order_acquire) < ChunkState::Generated) {
        return BLOCK_AIR;
    }

    auto lock = chunk->readLock();
    return static_cast<BlockId>(chunk->get(floorMod(worldX, Chunk::SX), worldY,
                                          floorMod(worldZ, Chunk::SZ)));
}

void World::markDirty(Chunk& chunk, bool bumpVersion) {
    if (bumpVersion) {
        chunk.version.fetch_add(1, std::memory_order_relaxed);
    }
    const ChunkState state = chunk.state.load(std::memory_order_acquire);
    if (state == ChunkState::Ready || state == ChunkState::Meshed) {
        chunk.state.store(ChunkState::Generated, std::memory_order_release);
    }
}

void World::markDirtyAt(int cx, int cz) {
    std::shared_ptr<Chunk> chunk;
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        const auto it = m_chunks.find(keyFor(cx, cz));
        if (it == m_chunks.end()) {
            return;
        }
        chunk = it->second;
    }
    markDirty(*chunk, true);
}

bool World::setBlock(int worldX, int worldY, int worldZ, BlockId id) {
    if (worldY < 0 || worldY >= Chunk::SY) {
        return false;
    }
    if (m_dimension == Dimension::Nether && isWater(id)) {
        return false;  // water evaporates in the nether
    }
    const int cx = floorDiv(worldX, Chunk::SX);
    const int cz = floorDiv(worldZ, Chunk::SZ);
    const int lx = floorMod(worldX, Chunk::SX);
    const int lz = floorMod(worldZ, Chunk::SZ);

    std::shared_ptr<Chunk> chunk;
    {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        const auto it = m_chunks.find(keyFor(cx, cz));
        if (it == m_chunks.end()) {
            return false;
        }
        chunk = it->second;
    }
    if (chunk->state.load(std::memory_order_acquire) < ChunkState::Generated) {
        return false;
    }

    {
        auto lock = chunk->writeLock();
        if (chunk->get(lx, worldY, lz) == static_cast<std::uint8_t>(id)) {
            return false;
        }
        chunk->set(lx, worldY, lz, static_cast<std::uint8_t>(id));
        chunk->version.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> edits(m_editsMutex);
            m_edits[keyFor(cx, cz)][static_cast<std::uint32_t>(
                Chunk::index(lx, worldY, lz))] = static_cast<std::uint8_t>(id);
        }
    }

    markDirty(*chunk, false);
    if (lx == 0) {
        markDirtyAt(cx - 1, cz);
    }
    if (lx == Chunk::SX - 1) {
        markDirtyAt(cx + 1, cz);
    }
    if (lz == 0) {
        markDirtyAt(cx, cz - 1);
    }
    if (lz == Chunk::SZ - 1) {
        markDirtyAt(cx, cz + 1);
    }
    m_fluid.scheduleAround(*this, worldX, worldY, worldZ);
    if (id == BLOCK_FIRE) {
        scheduleFire(worldX, worldY, worldZ);
    }
    return true;
}

void World::tickFluids() { m_fluid.tick(*this); }

void World::scheduleFire(int worldX, int worldY, int worldZ) {
    constexpr std::uint64_t kBurnTicks = 200;  // 10 seconds at 20 Hz
    const glm::ivec3 pos{worldX, worldY, worldZ};
    for (FireEntry& entry : m_fires) {
        if (entry.pos == pos) {
            entry.due = m_fireTick + kBurnTicks;
            return;
        }
    }
    m_fires.push_back(FireEntry{pos, m_fireTick + kBurnTicks});
}

void World::tickFire() {
    ++m_fireTick;
    static const glm::ivec3 kNeighbours[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                              {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    for (auto it = m_fires.begin(); it != m_fires.end();) {
        const glm::ivec3 pos = it->pos;
        if (getBlock(pos.x, pos.y, pos.z) != BLOCK_FIRE) {
            it = m_fires.erase(it);
            continue;
        }
        const bool supported = isSolid(getBlock(pos.x, pos.y - 1, pos.z));
        bool wet = false;
        for (const glm::ivec3& dir : kNeighbours) {
            if (isWater(getBlock(pos.x + dir.x, pos.y + dir.y, pos.z + dir.z))) {
                wet = true;
                break;
            }
        }
        if (!supported || wet || m_fireTick >= it->due) {
            (void)setBlock(pos.x, pos.y, pos.z, BLOCK_AIR);
            it = m_fires.erase(it);
        } else {
            ++it;
        }
    }
}

int World::surfaceY(int worldX, int worldZ) const {
    for (int y = Chunk::SY - 1; y >= 0; --y) {
        if (isSolid(getBlock(worldX, y, worldZ))) {
            return y;
        }
    }
    return -1;
}

std::size_t World::chunkCount() const {
    std::lock_guard<std::mutex> lock(m_mapMutex);
    return m_chunks.size();
}

bool World::chunkReady(int cx, int cz) const {
    std::lock_guard<std::mutex> lock(m_mapMutex);
    const auto it = m_chunks.find(keyFor(cx, cz));
    return it != m_chunks.end() &&
           it->second->state.load(std::memory_order_acquire) == ChunkState::Ready;
}

std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> World::snapshotEdits() const {
    std::lock_guard<std::mutex> lock(m_editsMutex);
    return m_edits;
}

void World::restoreEdits(std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> edits,
                         int seed) {
    clearChunks();
    m_seed = seed;
    std::lock_guard<std::mutex> lock(m_editsMutex);
    m_edits = std::move(edits);
}

void World::reset(int seed) {
    clearChunks();
    m_seed = seed;
    m_dimension = Dimension::Overworld;
    std::lock_guard<std::mutex> lock(m_editsMutex);
    m_edits.clear();
    for (auto& edits : m_storedEdits) {
        edits.clear();
    }
}

void World::setDimension(Dimension dim) {
    if (dim == m_dimension) {
        return;
    }
    clearChunks();
    {
        std::lock_guard<std::mutex> lock(m_editsMutex);
        const std::size_t from = static_cast<std::size_t>(m_dimension);
        const std::size_t to = static_cast<std::size_t>(dim);
        m_storedEdits[from] = std::move(m_edits);
        m_edits = std::move(m_storedEdits[to]);
        m_storedEdits[to].clear();
    }
    m_dimension = dim;
    spdlog::info("switched to dimension {}", dimensionName(dim));
}

std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> World::editsFor(
    Dimension dim) const {
    std::lock_guard<std::mutex> lock(m_editsMutex);
    if (dim == m_dimension) {
        return m_edits;
    }
    return m_storedEdits[static_cast<std::size_t>(dim)];
}

void World::setDimensionEdits(
    Dimension dim, std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> edits) {
    std::lock_guard<std::mutex> lock(m_editsMutex);
    if (dim == m_dimension) {
        m_edits = std::move(edits);
    } else {
        m_storedEdits[static_cast<std::size_t>(dim)] = std::move(edits);
    }
}

void World::clearChunks() {
    m_group.wait();
    m_pending.store(0);
    m_fluid.clear();
    m_fires.clear();

    MeshResult discarded;
    while (m_meshQueue.try_pop(discarded)) {
    }

    std::lock_guard<std::mutex> lock(m_mapMutex);
    for (auto& [key, chunk] : m_chunks) {
        freeGpu(chunk->opaqueMesh);
        freeGpu(chunk->waterMesh);
        freeGpu(chunk->lavaMesh);
        freeGpu(chunk->portalMesh);
    }
    m_chunks.clear();
}

void World::shutdown() { clearChunks(); }

}  // namespace aimc
