#include <set>

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/math.h"
#include "game/raycast.h"
#include "render/atlas.h"
#include "render/chunk_mesh.h"
#include "render/frustum.h"
#include "world/block.h"
#include "world/chunk.h"
#include "world/terrain.h"
#include "world/world.h"

namespace {

using namespace aimc;

TEST(Math, FloorDivModHandlesNegatives) {
    EXPECT_EQ(floorDiv(0, 16), 0);
    EXPECT_EQ(floorDiv(15, 16), 0);
    EXPECT_EQ(floorDiv(16, 16), 1);
    EXPECT_EQ(floorDiv(-1, 16), -1);
    EXPECT_EQ(floorDiv(-16, 16), -1);
    EXPECT_EQ(floorDiv(-17, 16), -2);

    EXPECT_EQ(floorMod(-1, 16), 15);
    EXPECT_EQ(floorMod(-16, 16), 0);
    EXPECT_EQ(floorMod(-17, 16), 15);
    EXPECT_EQ(floorMod(17, 16), 1);
}

TEST(Chunk, IndexIsUniqueAndInRange) {
    std::set<int> seen;
    for (int y = 0; y < Chunk::SY; ++y) {
        for (int z = 0; z < Chunk::SZ; ++z) {
            for (int x = 0; x < Chunk::SX; ++x) {
                const int i = Chunk::index(x, y, z);
                EXPECT_GE(i, 0);
                EXPECT_LT(i, Chunk::VOLUME);
                EXPECT_TRUE(seen.insert(i).second) << "duplicate index at " << x << "," << y
                                                   << "," << z;
            }
        }
    }
    EXPECT_EQ(seen.size(), static_cast<std::size_t>(Chunk::VOLUME));
}

TEST(Chunk, GetSetRoundtrip) {
    Chunk chunk(glm::ivec2(0, 0));
    chunk.set(1, 2, 3, static_cast<std::uint8_t>(BLOCK_STONE));
    EXPECT_EQ(chunk.get(1, 2, 3), static_cast<std::uint8_t>(BLOCK_STONE));
    EXPECT_EQ(chunk.get(0, 0, 0), static_cast<std::uint8_t>(BLOCK_AIR));
    EXPECT_EQ(chunk.origin().x, 0);
    EXPECT_EQ(chunk.origin().z, 0);
}

TEST(Terrain, HeightStaysInBounds) {
    Terrain terrain(7);
    for (int x = -64; x <= 64; x += 8) {
        for (int z = -64; z <= 64; z += 8) {
            const int h = terrain.heightAt(x, z);
            EXPECT_GE(h, 2);
            EXPECT_LE(h, Chunk::SY - 6);
        }
    }
}

TEST(Terrain, SameSeedSameWorld) {
    Terrain a(42);
    Terrain b(42);
    Terrain c(43);

    Chunk ca(glm::ivec2(0, 0));
    Chunk cb(glm::ivec2(0, 0));
    Chunk cc(glm::ivec2(0, 0));
    a.generate(ca);
    b.generate(cb);
    c.generate(cc);

    int same = 0;
    int different = 0;
    for (int y = 0; y < Chunk::SY; y += 3) {
        for (int z = 0; z < Chunk::SZ; z += 3) {
            for (int x = 0; x < Chunk::SX; x += 3) {
                same += ca.get(x, y, z) == cb.get(x, y, z) ? 1 : 0;
                different += ca.get(x, y, z) != cc.get(x, y, z) ? 1 : 0;
            }
        }
    }
    EXPECT_EQ(same, 22 * 6 * 6);  // y: 0..63 step 3, x/z: 0..15 step 3
    EXPECT_GT(different, 0);
}

TEST(Terrain, TreesAppearAboveSeaLevel) {
    Terrain terrain(1234);
    int leaves = 0;
    int logs = 0;
    for (int cz = -1; cz <= 1; ++cz) {
        for (int cx = -1; cx <= 1; ++cx) {
            Chunk chunk(glm::ivec2(cx, cz));
            terrain.generate(chunk);
            for (int y = 0; y < Chunk::SY; ++y) {
                for (int z = 0; z < Chunk::SZ; ++z) {
                    for (int x = 0; x < Chunk::SX; ++x) {
                        const auto block = chunk.get(x, y, z);
                        leaves += block == static_cast<std::uint8_t>(BLOCK_LEAVES) ? 1 : 0;
                        logs += block == static_cast<std::uint8_t>(BLOCK_LOG) ? 1 : 0;
                    }
                }
            }
        }
    }
    EXPECT_GT(leaves, 0);
    EXPECT_GT(logs, 0);
}

TEST(Mesh, SingleBlockEmitsSixFaces) {
    Atlas atlas;
    Chunk chunk(glm::ivec2(0, 0));
    World world;  // no chunks generated: every neighbour is air
    chunk.set(8, 8, 8, static_cast<std::uint8_t>(BLOCK_STONE));

    ChunkMeshData mesh;
    buildChunkMesh(chunk, world, atlas, mesh);

    EXPECT_EQ(mesh.opaque.vertices.size(), 24u);
    EXPECT_EQ(mesh.opaque.indices.size(), 36u);
    EXPECT_TRUE(mesh.water.vertices.empty());
}

TEST(Mesh, SharedFaceIsCulled) {
    Atlas atlas;
    Chunk chunk(glm::ivec2(0, 0));
    World world;
    chunk.set(8, 8, 8, static_cast<std::uint8_t>(BLOCK_STONE));
    chunk.set(9, 8, 8, static_cast<std::uint8_t>(BLOCK_STONE));

    ChunkMeshData mesh;
    buildChunkMesh(chunk, world, atlas, mesh);

    EXPECT_EQ(mesh.opaque.vertices.size(), 40u);  // 10 faces
    EXPECT_EQ(mesh.opaque.indices.size(), 60u);
}

TEST(Mesh, OpaqueNeighbourCullsFace) {
    Atlas atlas;
    Chunk chunk(glm::ivec2(0, 0));
    World world;
    chunk.set(8, 8, 8, static_cast<std::uint8_t>(BLOCK_STONE));
    chunk.set(8, 9, 8, static_cast<std::uint8_t>(BLOCK_DIRT));

    ChunkMeshData mesh;
    buildChunkMesh(chunk, world, atlas, mesh);

    EXPECT_EQ(mesh.opaque.vertices.size(), 40u);  // the top face of stone is hidden
}

TEST(Mesh, WaterGoesIntoItsOwnBuffer) {
    Atlas atlas;
    Chunk chunk(glm::ivec2(0, 0));
    World world;
    chunk.set(4, 4, 4, static_cast<std::uint8_t>(BLOCK_WATER));

    ChunkMeshData mesh;
    buildChunkMesh(chunk, world, atlas, mesh);

    EXPECT_EQ(mesh.opaque.vertices.size(), 0u);
    EXPECT_EQ(mesh.water.vertices.size(), 24u);
    EXPECT_EQ(mesh.water.indices.size(), 36u);
}

TEST(Atlas, UVsStayInsideUnitRange) {
    Atlas atlas;
    for (int tile = 0; tile < Atlas::Grid * Atlas::Grid; ++tile) {
        const glm::vec4 uv = atlas.tileUV(tile);
        EXPECT_GE(uv.x, 0.0f);
        EXPECT_GE(uv.y, 0.0f);
        EXPECT_LE(uv.z, 1.0f);
        EXPECT_LE(uv.w, 1.0f);
        EXPECT_LT(uv.x, uv.z);
        EXPECT_LT(uv.y, uv.w);
    }
}

TEST(Frustum, CullsBoxesBehindTheCamera) {
    const glm::mat4 viewProj =
        glm::perspective(glm::radians(70.0f), 16.0f / 9.0f, 0.1f, 200.0f) *
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f));
    const Frustum frustum = Frustum::fromViewProj(viewProj);

    EXPECT_TRUE(frustum.intersectsAabb(glm::vec3(-1.0f, -1.0f, -20.0f),
                                       glm::vec3(1.0f, 1.0f, -18.0f)));
    EXPECT_FALSE(frustum.intersectsAabb(glm::vec3(-1.0f, -1.0f, 18.0f),
                                        glm::vec3(1.0f, 1.0f, 20.0f)));
    EXPECT_FALSE(frustum.intersectsAabb(glm::vec3(100.0f, -1.0f, -20.0f),
                                        glm::vec3(102.0f, 1.0f, -18.0f)));
}

TEST(Raycast, HitsExpectedBlockAndNormal) {
    const auto get = [](int x, int y, int z) -> BlockId {
        return (x == 0 && y == 0 && z == -3) ? BLOCK_STONE : BLOCK_AIR;
    };
    const RayHit hit = raycastVoxel(get, glm::vec3(0.5f), glm::vec3(0.0f, 0.0f, -1.0f), 10.0f);
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.block, glm::ivec3(0, 0, -3));
    EXPECT_EQ(hit.normal, glm::ivec3(0, 0, 1));
    EXPECT_NEAR(hit.distance, 2.5f, 1.0e-4f);
}

TEST(Raycast, RespectsMaxDistance) {
    const auto get = [](int x, int y, int z) -> BlockId {
        return (x == 0 && y == 0 && z == -5) ? BLOCK_STONE : BLOCK_AIR;
    };
    const RayHit hit = raycastVoxel(get, glm::vec3(0.5f), glm::vec3(0.0f, 0.0f, -1.0f), 2.0f);
    EXPECT_FALSE(hit.hit);
}

TEST(Raycast, VerticalHitNormal) {
    const auto get = [](int x, int y, int z) -> BlockId {
        return (x == 0 && y == -2 && z == 0) ? BLOCK_DIRT : BLOCK_AIR;
    };
    const RayHit hit = raycastVoxel(get, glm::vec3(0.5f, 0.5f, 0.5f),
                                    glm::vec3(0.0f, -1.0f, 0.0f), 10.0f);
    EXPECT_TRUE(hit.hit);
    EXPECT_EQ(hit.block, glm::ivec3(0, -2, 0));
    EXPECT_EQ(hit.normal, glm::ivec3(0, 1, 0));
}

TEST(World, EmptyWorldQueriesAreSafe) {
    World world;
    world.init(99);
    EXPECT_EQ(world.chunkCount(), 0u);
    EXPECT_EQ(world.getBlock(0, 0, 0), BLOCK_AIR);
    EXPECT_EQ(world.getBlock(10000, 0, 10000), BLOCK_AIR);
    EXPECT_FALSE(world.chunkReady(0, 0));
}

}  // namespace
