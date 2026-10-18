#include <set>

#include <gtest/gtest.h>
#include <glm/glm.hpp>

#include "core/math.h"
#include "render/atlas.h"
#include "render/chunk_mesh.h"
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

TEST(Mesh, SingleBlockEmitsSixFaces) {
    Atlas atlas;
    Chunk chunk(glm::ivec2(0, 0));
    World world;  // no chunks generated: every neighbour is air
    chunk.set(8, 8, 8, static_cast<std::uint8_t>(BLOCK_STONE));

    MeshData mesh;
    buildChunkMesh(chunk, world, atlas, mesh);

    EXPECT_EQ(mesh.vertices.size(), 24u);
    EXPECT_EQ(mesh.indices.size(), 36u);
}

TEST(Mesh, SharedFaceIsCulled) {
    Atlas atlas;
    Chunk chunk(glm::ivec2(0, 0));
    World world;
    chunk.set(8, 8, 8, static_cast<std::uint8_t>(BLOCK_STONE));
    chunk.set(9, 8, 8, static_cast<std::uint8_t>(BLOCK_STONE));

    MeshData mesh;
    buildChunkMesh(chunk, world, atlas, mesh);

    EXPECT_EQ(mesh.vertices.size(), 40u);  // 10 faces
    EXPECT_EQ(mesh.indices.size(), 60u);
}

TEST(Mesh, OpaqueNeighbourCullsFace) {
    Atlas atlas;
    Chunk chunk(glm::ivec2(0, 0));
    World world;
    chunk.set(8, 8, 8, static_cast<std::uint8_t>(BLOCK_STONE));
    chunk.set(8, 9, 8, static_cast<std::uint8_t>(BLOCK_DIRT));

    MeshData mesh;
    buildChunkMesh(chunk, world, atlas, mesh);

    EXPECT_EQ(mesh.vertices.size(), 40u);  // the top face of stone is hidden
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

TEST(World, ChunksAndBlockLookup) {
    World world;
    world.init(99, 1);
    EXPECT_EQ(world.chunkCount(), 9u);
    EXPECT_EQ(world.getBlock(0, 0, 0), BLOCK_STONE);
    EXPECT_NE(world.getBlock(-1, 0, -1), BLOCK_AIR);
    EXPECT_EQ(world.getBlock(0, Chunk::SY, 0), BLOCK_AIR);
    EXPECT_EQ(world.getBlock(10000, 0, 10000), BLOCK_AIR);
}

}  // namespace
