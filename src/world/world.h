#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <glm/glm.hpp>
#include <tbb/concurrent_queue.h>
#include <tbb/task_group.h>

#include "render/chunk_mesh.h"
#include "world/block.h"
#include "world/dimension.h"
#include "world/chunk.h"
#include "world/fluid.h"

namespace aimc {

class Chunk;
class Terrain;
class Atlas;
class Frustum;

struct DrawStats {
    int drawCalls = 0;
    long long triangles = 0;
    int visibleChunks = 0;
    int totalChunks = 0;
};

// Streaming voxel world: chunks are generated and meshed on TBB workers, while
// the main thread only uploads finished meshes (GL calls are single-threaded).
class World {
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void init(int seed);
    void setAtlas(const Atlas* atlas) { m_atlas = atlas; }

    // Schedules generation/meshing around the camera, unloads far chunks and
    // (when upload is true) uploads a few finished meshes.
    void update(const glm::vec3& cameraPos, int viewDistance, bool upload);
    void prewarm(const glm::vec3& cameraPos, int viewDistance, int timeoutMs = 15000);

    DrawStats drawOpaque(const Frustum& frustum) const;
    DrawStats drawWater(const Frustum& frustum) const;
    DrawStats drawLava(const Frustum& frustum) const;
    DrawStats drawPortal(const Frustum& frustum) const;

    BlockId getBlock(int worldX, int worldY, int worldZ) const;
    bool setBlock(int worldX, int worldY, int worldZ, BlockId id);
    int surfaceY(int worldX, int worldZ) const;

    // Runs one 20 Hz fluid simulation step (main thread).
    void tickFluids();

    int seed() const { return m_seed; }
    Dimension dimension() const { return m_dimension; }
    void setDimension(Dimension dim);
    std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> editsFor(
        Dimension dim) const;
    void setDimensionEdits(Dimension dim,
                           std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> edits);
    std::size_t chunkCount() const;
    bool chunkReady(int cx, int cz) const;

    std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> snapshotEdits() const;
    void restoreEdits(std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> edits,
                      int seed);
    void reset(int seed);
    void shutdown();

private:
    struct MeshResult {
        std::shared_ptr<Chunk> chunk;
        ChunkMeshData mesh;
        std::uint32_t version = 0;
    };

    static std::uint64_t keyFor(int cx, int cz);
    static void freeGpu(GpuMesh& mesh);

    void ensureChunk(int cx, int cz);
    void tryScheduleMesh(int cx, int cz);
    void unloadOutside(int centerX, int centerZ, int radius);
    void drainUploads(int budget);
    bool allReady(int centerX, int centerZ, int radius) const;

    void runGenerate(std::shared_ptr<Chunk> chunk);
    void runMesh(std::shared_ptr<Chunk> chunk);
    void clearChunks();
    void applyEditsLocked(Chunk& chunk);
    void markDirty(Chunk& chunk, bool bumpVersion);
    void markDirtyAt(int cx, int cz);

    mutable std::mutex m_mapMutex;
    std::unordered_map<std::uint64_t, std::shared_ptr<Chunk>> m_chunks;

    mutable std::mutex m_editsMutex;
    std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> m_edits;
    std::array<std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>>,
               kDimensionCount>
        m_storedEdits;
    Dimension m_dimension = Dimension::Overworld;

    tbb::task_group m_group;
    tbb::concurrent_queue<MeshResult> m_meshQueue;
    std::atomic<int> m_pending{0};

    FluidSim m_fluid;

    const Atlas* m_atlas = nullptr;
    int m_seed = 0;
    int m_viewDistance = 0;
};

}  // namespace aimc
