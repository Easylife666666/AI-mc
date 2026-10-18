#include <chrono>
#include <set>
#include <thread>

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>

#include "core/math.h"
#include "game/player.h"
#include "game/raycast.h"
#include "render/atlas.h"
#include "render/chunk_mesh.h"
#include "render/frustum.h"
#include "world/block.h"
#include "world/chunk.h"
#include "world/terrain.h"
#include "world/save.h"
#include "world/world.h"
#include "game/settings.h"

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

TEST(Save, RoundtripPreservesData) {
    SaveData data;
    data.seed = 4242;
    data.playerPos = glm::vec3(12.5f, 30.25f, -4.75f);
    data.yaw = 45.0f;
    data.pitch = -12.0f;
    data.selected = 6;
    data.timeOfDay = 0.625f;
    data.edits[0x0000000100000002ull][123] = static_cast<std::uint8_t>(BLOCK_STONE);

    ASSERT_TRUE(saveWorld("aimc_test_save.aimc", data));

    SaveData loaded;
    ASSERT_TRUE(loadWorld("aimc_test_save.aimc", loaded));
    std::remove("aimc_test_save.aimc");

    EXPECT_EQ(loaded.seed, 4242);
    EXPECT_FLOAT_EQ(loaded.playerPos.x, 12.5f);
    EXPECT_FLOAT_EQ(loaded.playerPos.y, 30.25f);
    EXPECT_FLOAT_EQ(loaded.playerPos.z, -4.75f);
    EXPECT_FLOAT_EQ(loaded.yaw, 45.0f);
    EXPECT_FLOAT_EQ(loaded.pitch, -12.0f);
    EXPECT_EQ(loaded.selected, 6);
    EXPECT_FLOAT_EQ(loaded.timeOfDay, 0.625f);
    ASSERT_EQ(loaded.edits.size(), 1u);
    ASSERT_EQ(loaded.edits.begin()->second.size(), 1u);
    EXPECT_EQ(loaded.edits.begin()->second.at(123), static_cast<std::uint8_t>(BLOCK_STONE));
}

TEST(Settings, RoundtripClamps) {
    Settings settings;
    settings.viewDistance = 9;
    settings.fov = 85.0f;
    settings.volume = 0.25f;
    settings.dayLength = 120.0f;
    settings.adaptive = false;
    ASSERT_TRUE(saveSettings("aimc_test_settings.json", settings));

    Settings loaded;
    ASSERT_TRUE(loadSettings("aimc_test_settings.json", loaded));
    std::remove("aimc_test_settings.json");

    EXPECT_EQ(loaded.viewDistance, 9);
    EXPECT_FLOAT_EQ(loaded.fov, 85.0f);
    EXPECT_FLOAT_EQ(loaded.volume, 0.25f);
    EXPECT_FLOAT_EQ(loaded.dayLength, 120.0f);
    EXPECT_FALSE(loaded.adaptive);
}

TEST(World, StreamingGenerationAndBlockEdits) {
    Atlas atlas;
    World world;
    world.init(123);
    world.setAtlas(&atlas);
    world.update(glm::vec3(0.5f, 30.0f, 0.5f), 1, false);

    for (int i = 0; i < 400 && world.getBlock(0, 0, 0) == BLOCK_AIR; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(world.getBlock(0, 0, 0), BLOCK_STONE);
    EXPECT_GE(world.chunkCount(), 9u);

    const int surface = world.surfaceY(0, 0);
    ASSERT_GE(surface, 0);
    const BlockId before = world.getBlock(0, surface, 0);
    ASSERT_NE(before, BLOCK_AIR);

    ASSERT_TRUE(world.setBlock(0, surface, 0, BLOCK_AIR));
    EXPECT_EQ(world.getBlock(0, surface, 0), BLOCK_AIR);
    EXPECT_FALSE(world.snapshotEdits().empty());
    EXPECT_FALSE(world.setBlock(10000, 0, 10000, BLOCK_STONE));
}

TEST(Player, CanClimbOutOfWater) {
    Atlas atlas;
    World world;
    world.init(7);
    world.setAtlas(&atlas);
    world.update(glm::vec3(0.5f, 64.0f, 0.5f), 2, false);

    for (int i = 0; i < 400 && world.getBlock(0, 0, 0) == BLOCK_AIR; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(world.getBlock(0, 0, 0), BLOCK_STONE);

    // Controlled pool at y=46..47 (surface at 48); the shore top is one block
    // above the water at y=49, which is exactly the case players got stuck on.
    for (int z = -2; z <= 2; ++z) {
        for (int x = -4; x <= 4; ++x) {
            for (int y = 46; y <= 52; ++y) {
                world.setBlock(x, y, z, BLOCK_AIR);
            }
            world.setBlock(x, 45, z, BLOCK_STONE);
        }
    }
    for (int z = -2; z <= 2; ++z) {
        for (int x = -4; x <= 0; ++x) {
            for (int y = 46; y <= 47; ++y) {
                world.setBlock(x, y, z, BLOCK_WATER);
            }
        }
        for (int x = 1; x <= 4; ++x) {
            for (int y = 46; y <= 48; ++y) {
                world.setBlock(x, y, z, BLOCK_STONE);
            }
        }
    }

    Player player;
    player.setPosition(glm::vec3(0.5f, 46.2f, 0.5f));

    PlayerInput input;
    input.forward = true;
    input.jump = true;

    bool climbed = false;
    for (int step = 0; step < 1200; ++step) {
        player.update(world, 0.0f, input, 1.0f / 120.0f);
        if (player.position().y >= 48.9f && player.onGround()) {
            climbed = true;
            break;
        }
    }
    EXPECT_TRUE(climbed) << "player stuck at x=" << player.position().x
                         << " y=" << player.position().y << " vy=" << player.velocity().y;
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
