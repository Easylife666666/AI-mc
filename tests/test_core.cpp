#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <set>
#include <thread>

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <zlib.h>

#include <cstdio>

#include "core/math.h"
#include "game/creative.h"
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
    data.hotbar[0] = BLOCK_SNOW;
    data.hotbar[4] = BLOCK_BRICKS;
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
    EXPECT_EQ(loaded.hotbar, data.hotbar);
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

TEST(Player, CannotJumpOutInOpenWater) {
    Atlas atlas;
    World world;
    world.init(13);
    world.setAtlas(&atlas);
    world.update(glm::vec3(0.5f, 64.0f, 0.5f), 2, false);

    for (int i = 0; i < 400 && world.getBlock(0, 0, 0) == BLOCK_AIR; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(world.getBlock(0, 0, 0), BLOCK_STONE);

    // Open water: two layers deep, no shore anywhere near the player.
    for (int z = -10; z <= 10; ++z) {
        for (int x = -10; x <= 10; ++x) {
            for (int y = 46; y <= 52; ++y) {
                world.setBlock(x, y, z, BLOCK_AIR);
            }
            world.setBlock(x, 45, z, BLOCK_STONE);
            world.setBlock(x, 46, z, BLOCK_WATER);
            world.setBlock(x, 47, z, BLOCK_WATER);
        }
    }

    Player player;
    player.setPosition(glm::vec3(0.5f, 46.2f, 0.5f));

    PlayerInput input;
    input.forward = true;
    input.jump = true;

    float maxY = 0.0f;
    for (int step = 0; step < 300; ++step) {
        player.update(world, 0.0f, input, 1.0f / 120.0f);
        maxY = std::max(maxY, player.position().y);
    }
    EXPECT_LT(maxY, 48.5f) << "open-water jump reached y=" << maxY;
}

TEST(World, EmptyWorldQueriesAreSafe) {
    World world;
    world.init(99);
    EXPECT_EQ(world.chunkCount(), 0u);
    EXPECT_EQ(world.getBlock(0, 0, 0), BLOCK_AIR);
    EXPECT_EQ(world.getBlock(10000, 0, 10000), BLOCK_AIR);
    EXPECT_FALSE(world.chunkReady(0, 0));
}

TEST(Creative, TabsContainOnlyPlaceableBlocks) {
    const std::vector<BlockId>& all = creativeAllBlocks();
    EXPECT_FALSE(all.empty());
    for (const BlockId block : all) {
        EXPECT_NE(block, BLOCK_AIR);
        EXPECT_LT(block, BLOCK_COUNT);
        EXPECT_TRUE(blockMatchesQuery(block, ""));
    }

    const std::vector<BlockId>& building = creativeTabItems(CreativeTab::Building);
    EXPECT_NE(std::find(building.begin(), building.end(), BLOCK_STONE), building.end());
    EXPECT_NE(std::find(building.begin(), building.end(), BLOCK_STONE_BRICKS),
              building.end());

    const std::vector<BlockId>& natural = creativeTabItems(CreativeTab::Natural);
    EXPECT_NE(std::find(natural.begin(), natural.end(), BLOCK_GRASS), natural.end());
    EXPECT_NE(std::find(natural.begin(), natural.end(), BLOCK_WATER), natural.end());
}

TEST(Creative, SearchMatchesNamesCaseInsensitively) {
    const std::vector<BlockId> hits = creativeItems(CreativeTab::Search, "STONE");
    EXPECT_NE(std::find(hits.begin(), hits.end(), BLOCK_STONE), hits.end());
    EXPECT_NE(std::find(hits.begin(), hits.end(), BLOCK_STONE_BRICKS), hits.end());
    EXPECT_EQ(std::find(hits.begin(), hits.end(), BLOCK_WATER), hits.end());

    EXPECT_EQ(creativeItems(CreativeTab::Search, "").size(), creativeAllBlocks().size());
    EXPECT_TRUE(creativeItems(CreativeTab::Search, "zzz").empty());
}

TEST(Creative, ScrollRowsClampToContent) {
    EXPECT_EQ(creativeRowCount(0, 9), 0);
    EXPECT_EQ(creativeRowCount(9, 9), 1);
    EXPECT_EQ(creativeRowCount(10, 9), 2);
    EXPECT_EQ(creativeMaxScrollRow(10, 9, 5), 0);
    EXPECT_EQ(creativeMaxScrollRow(100, 9, 5), 7);  // 12 rows - 5 visible
    EXPECT_EQ(creativeMaxScrollRow(0, 9, 5), 0);
}

TEST(Creative, HotbarFillsFirstEmptySlot) {
    std::array<BlockId, kHotbarSlots> bar = defaultHotbar();
    EXPECT_EQ(bar.size(), static_cast<std::size_t>(kHotbarSlots));
    EXPECT_EQ(firstEmptyHotbarSlot(bar), -1);
    EXPECT_EQ(putInHotbar(bar, BLOCK_SNOW), -1);

    bar[3] = BLOCK_AIR;
    EXPECT_EQ(firstEmptyHotbarSlot(bar), 3);
    EXPECT_EQ(putInHotbar(bar, BLOCK_STONE_BRICKS), 3);
    EXPECT_EQ(bar[3], BLOCK_STONE_BRICKS);

    bar[0] = BLOCK_AIR;
    EXPECT_EQ(putInHotbar(bar, BLOCK_BRICKS), 0);
    EXPECT_EQ(bar[0], BLOCK_BRICKS);
}

TEST(Save, LoadsVersion1WithoutHotbar) {
    const char* path = "aimc_test_save_v1.aimc";
    gzFile file = gzopen(path, "wb6");
    ASSERT_NE(file, nullptr);

    const char magic[4] = {'A', 'I', 'M', 'C'};
    const std::uint32_t version = 1;
    const std::int32_t seed = 777;
    const float pose[5] = {1.0f, 2.0f, 3.0f, 45.0f, -10.0f};
    const std::int32_t selected = 2;
    const float time = 0.5f;
    const std::uint32_t chunks = 0;
    ASSERT_EQ(gzwrite(file, magic, 4), 4);
    ASSERT_EQ(gzwrite(file, &version, 4), 4);
    ASSERT_EQ(gzwrite(file, &seed, 4), 4);
    ASSERT_EQ(gzwrite(file, pose, static_cast<unsigned int>(sizeof(pose))),
              static_cast<int>(sizeof(pose)));
    ASSERT_EQ(gzwrite(file, &selected, 4), 4);
    ASSERT_EQ(gzwrite(file, &time, 4), 4);
    ASSERT_EQ(gzwrite(file, &chunks, 4), 4);
    ASSERT_EQ(gzclose(file), Z_OK);

    SaveData loaded;
    ASSERT_TRUE(loadWorld(path, loaded));
    std::remove(path);

    EXPECT_EQ(loaded.seed, 777);
    EXPECT_EQ(loaded.selected, 2);
    EXPECT_FLOAT_EQ(loaded.timeOfDay, 0.5f);
    EXPECT_EQ(loaded.hotbar, defaultHotbar());
    EXPECT_TRUE(loaded.edits.empty());
}

TEST(Save, RejectsNewerVersion) {
    const char* path = "aimc_test_save_v9.aimc";
    gzFile file = gzopen(path, "wb6");
    ASSERT_NE(file, nullptr);
    const char magic[4] = {'A', 'I', 'M', 'C'};
    const std::uint32_t version = 9;
    ASSERT_EQ(gzwrite(file, magic, 4), 4);
    ASSERT_EQ(gzwrite(file, &version, 4), 4);
    ASSERT_EQ(gzclose(file), Z_OK);

    SaveData loaded;
    EXPECT_FALSE(loadWorld(path, loaded));
    std::remove(path);
}

TEST(Creative, CursorStacksFollowVanillaRules) {
    ItemStack cursor;
    takeItem(cursor, BLOCK_STONE);
    EXPECT_EQ(cursor.block, BLOCK_STONE);
    EXPECT_EQ(cursor.count, 1);

    takeItem(cursor, BLOCK_STONE);
    EXPECT_EQ(cursor.count, 2);
    for (int i = 0; i < 200; ++i) {
        takeItem(cursor, BLOCK_STONE);
    }
    EXPECT_EQ(cursor.count, kMaxStack);

    takeItem(cursor, BLOCK_DIRT);
    EXPECT_EQ(cursor.block, BLOCK_DIRT);
    EXPECT_EQ(cursor.count, 1);

    takeOneItem(cursor, BLOCK_DIRT);
    EXPECT_TRUE(cursor.empty());
    takeOneItem(cursor, BLOCK_SAND);
    EXPECT_EQ(cursor.block, BLOCK_SAND);
    EXPECT_EQ(cursor.count, 1);
}

TEST(Creative, HotbarClicksPlaceAndPickUp) {
    std::array<BlockId, kHotbarSlots> bar = defaultHotbar();
    ItemStack cursor;
    takeItem(cursor, BLOCK_SNOW);

    clickHotbarSlot(bar, cursor, 2);  // occupied: swap the held stack with the slot
    EXPECT_EQ(bar[2], BLOCK_SNOW);
    EXPECT_EQ(cursor.block, BLOCK_STONE);
    EXPECT_EQ(cursor.count, kMaxStack);

    clickHotbarSlot(bar, cursor, 4);
    EXPECT_EQ(bar[4], BLOCK_STONE);
    EXPECT_EQ(cursor.block, BLOCK_LOG);

    cursor.clear();
    clickHotbarSlot(bar, cursor, 0);  // empty cursor: pick the slot up
    EXPECT_EQ(cursor.block, BLOCK_GRASS);
    EXPECT_EQ(bar[0], BLOCK_AIR);
    EXPECT_EQ(cursor.count, kMaxStack);

    clickHotbarSlot(bar, cursor, 0);  // put it back
    EXPECT_TRUE(cursor.empty());
    EXPECT_EQ(bar[0], BLOCK_GRASS);
}

}  // namespace
