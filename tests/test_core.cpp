#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <tuple>

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <zlib.h>

#include <cstdio>

#include "core/math.h"
#include "game/creative.h"
#include "game/item.h"
#include "game/player.h"
#include "game/raycast.h"
#include "render/atlas.h"
#include "render/chunk_mesh.h"
#include "render/frustum.h"
#include "world/block.h"
#include "world/chunk.h"
#include "world/terrain.h"
#include "world/save.h"
#include "world/portal.h"
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
    data.selected = 6;
    data.timeOfDay = 0.625f;
    data.hotbar[0] = ItemRef::ofBlock(BLOCK_SNOW);
    data.hotbar[4] = ItemRef::ofItem(ItemId::FlintAndSteel);
    data.current = Dimension::Nether;

    DimensionSave& overworld = data.dimensions[0];
    overworld.playerPos = glm::vec3(12.5f, 30.25f, -4.75f);
    overworld.yaw = 45.0f;
    overworld.pitch = -12.0f;
    overworld.edits[0x0000000100000002ull][123] =
        static_cast<std::uint8_t>(BLOCK_STONE);

    DimensionSave& nether = data.dimensions[1];
    nether.playerPos = glm::vec3(-8.0f, 40.0f, 8.0f);
    nether.yaw = 10.0f;
    nether.pitch = -5.0f;
    nether.edits[0x0000000000000001ull][7] =
        static_cast<std::uint8_t>(BLOCK_NETHERRACK);

    ASSERT_TRUE(saveWorld("aimc_test_save.aimc", data));

    SaveData loaded;
    ASSERT_TRUE(loadWorld("aimc_test_save.aimc", loaded));
    std::remove("aimc_test_save.aimc");

    EXPECT_EQ(loaded.seed, 4242);
    EXPECT_EQ(loaded.selected, 6);
    EXPECT_FLOAT_EQ(loaded.timeOfDay, 0.625f);
    EXPECT_EQ(loaded.hotbar, data.hotbar);
    EXPECT_EQ(loaded.current, Dimension::Nether);

    EXPECT_FLOAT_EQ(loaded.dimensions[0].playerPos.x, 12.5f);
    EXPECT_FLOAT_EQ(loaded.dimensions[0].playerPos.y, 30.25f);
    EXPECT_FLOAT_EQ(loaded.dimensions[0].playerPos.z, -4.75f);
    EXPECT_FLOAT_EQ(loaded.dimensions[0].yaw, 45.0f);
    EXPECT_FLOAT_EQ(loaded.dimensions[0].pitch, -12.0f);

    EXPECT_FLOAT_EQ(loaded.dimensions[1].playerPos.x, -8.0f);
    EXPECT_FLOAT_EQ(loaded.dimensions[1].playerPos.y, 40.0f);
    EXPECT_FLOAT_EQ(loaded.dimensions[1].yaw, 10.0f);
    EXPECT_FLOAT_EQ(loaded.dimensions[1].pitch, -5.0f);

    ASSERT_EQ(loaded.dimensions[0].edits.size(), 1u);
    ASSERT_EQ(loaded.dimensions[0].edits.begin()->second.size(), 1u);
    EXPECT_EQ(loaded.dimensions[0].edits.begin()->second.at(123),
              static_cast<std::uint8_t>(BLOCK_STONE));
    ASSERT_EQ(loaded.dimensions[1].edits.size(), 1u);
    ASSERT_EQ(loaded.dimensions[1].edits.begin()->second.size(), 1u);
    EXPECT_EQ(loaded.dimensions[1].edits.begin()->second.at(7),
              static_cast<std::uint8_t>(BLOCK_NETHERRACK));
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

TEST(Creative, TabsContainRealItems) {
    const std::vector<ItemRef>& all = creativeAllItems();
    EXPECT_FALSE(all.empty());
    for (const ItemRef& item : all) {
        EXPECT_FALSE(item.empty());
        EXPECT_TRUE(itemMatchesQuery(item, ""));
    }

    const std::vector<ItemRef>& building = creativeTabItems(CreativeTab::Building);
    EXPECT_NE(std::find(building.begin(), building.end(), ItemRef::ofBlock(BLOCK_STONE)),
              building.end());
    EXPECT_NE(std::find(building.begin(), building.end(),
                        ItemRef::ofBlock(BLOCK_STONE_BRICKS)),
              building.end());

    const std::vector<ItemRef>& natural = creativeTabItems(CreativeTab::Natural);
    EXPECT_NE(std::find(natural.begin(), natural.end(), ItemRef::ofBlock(BLOCK_GRASS)),
              natural.end());
    EXPECT_NE(std::find(natural.begin(), natural.end(), ItemRef::ofBlock(BLOCK_WATER)),
              natural.end());

    const std::vector<ItemRef>& tools = creativeTabItems(CreativeTab::Tools);
    EXPECT_NE(std::find(tools.begin(), tools.end(), ItemRef::ofItem(ItemId::FlintAndSteel)),
              tools.end());
}

TEST(Creative, SearchMatchesNamesCaseInsensitively) {
    const std::vector<ItemRef> hits = creativeItems(CreativeTab::Search, "STONE");
    EXPECT_NE(std::find(hits.begin(), hits.end(), ItemRef::ofBlock(BLOCK_STONE)), hits.end());
    EXPECT_NE(std::find(hits.begin(), hits.end(), ItemRef::ofBlock(BLOCK_STONE_BRICKS)),
              hits.end());
    EXPECT_EQ(std::find(hits.begin(), hits.end(), ItemRef::ofBlock(BLOCK_WATER)), hits.end());

    EXPECT_EQ(creativeItems(CreativeTab::Search, "").size(), creativeAllItems().size());
    EXPECT_TRUE(creativeItems(CreativeTab::Search, "zzz").empty());
}

TEST(Creative, SearchFindsItems) {
    const std::vector<ItemRef> hits = creativeItems(CreativeTab::Search, "flint");
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_TRUE(hits[0].isItem());
    EXPECT_EQ(hits[0].item, ItemId::FlintAndSteel);
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
    std::array<ItemRef, kHotbarSlots> bar = defaultHotbar();
    EXPECT_EQ(bar.size(), static_cast<std::size_t>(kHotbarSlots));
    EXPECT_EQ(firstEmptyHotbarSlot(bar), -1);
    EXPECT_EQ(putInHotbar(bar, ItemRef::ofBlock(BLOCK_SNOW)), -1);

    bar[3] = {};
    EXPECT_EQ(firstEmptyHotbarSlot(bar), 3);
    EXPECT_EQ(putInHotbar(bar, ItemRef::ofBlock(BLOCK_STONE_BRICKS)), 3);
    EXPECT_EQ(bar[3], ItemRef::ofBlock(BLOCK_STONE_BRICKS));

    bar[0] = {};
    EXPECT_EQ(putInHotbar(bar, ItemRef::ofItem(ItemId::FlintAndSteel)), 0);
    EXPECT_EQ(bar[0], ItemRef::ofItem(ItemId::FlintAndSteel));
}

TEST(Item, RefsDistinguishBlocksAndItems) {
    const ItemRef flint = ItemRef::ofItem(ItemId::FlintAndSteel);
    EXPECT_TRUE(flint.isItem());
    EXPECT_FALSE(flint.empty());
    EXPECT_EQ(std::string(flint.name()), "Flint and Steel");
    EXPECT_EQ(flint.iconTile(), 16);
    EXPECT_EQ(maxStackSize(flint), 1);

    const ItemRef stone = ItemRef::ofBlock(BLOCK_STONE);
    EXPECT_FALSE(stone.isItem());
    EXPECT_EQ(std::string(stone.name()), "Stone");
    EXPECT_EQ(maxStackSize(stone), kMaxBlockStack);

    EXPECT_TRUE(ItemRef{}.empty());
    EXPECT_EQ(maxStackSize(ItemRef::ofBlock(BLOCK_STONE)), 64);
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
    EXPECT_EQ(loaded.current, Dimension::Overworld);
    EXPECT_FLOAT_EQ(loaded.dimensions[0].playerPos.x, 1.0f);
    EXPECT_FLOAT_EQ(loaded.dimensions[0].playerPos.y, 2.0f);
    EXPECT_FLOAT_EQ(loaded.dimensions[0].yaw, 45.0f);
    EXPECT_TRUE(loaded.dimensions[0].edits.empty());
    EXPECT_TRUE(loaded.dimensions[1].edits.empty());
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
    takeItem(cursor, ItemRef::ofBlock(BLOCK_STONE));
    EXPECT_EQ(cursor.ref.block, BLOCK_STONE);
    EXPECT_EQ(cursor.count, 1);

    takeItem(cursor, ItemRef::ofBlock(BLOCK_STONE));
    EXPECT_EQ(cursor.count, 2);
    for (int i = 0; i < 200; ++i) {
        takeItem(cursor, ItemRef::ofBlock(BLOCK_STONE));
    }
    EXPECT_EQ(cursor.count, kMaxBlockStack);

    takeItem(cursor, ItemRef::ofBlock(BLOCK_DIRT));
    EXPECT_EQ(cursor.ref.block, BLOCK_DIRT);
    EXPECT_EQ(cursor.count, 1);

    takeOneItem(cursor, ItemRef::ofBlock(BLOCK_DIRT));
    EXPECT_TRUE(cursor.empty());
    takeOneItem(cursor, ItemRef::ofBlock(BLOCK_SAND));
    EXPECT_EQ(cursor.ref.block, BLOCK_SAND);
    EXPECT_EQ(cursor.count, 1);
}

TEST(Creative, CursorRespectsItemStackLimits) {
    ItemStack cursor;
    const ItemRef flint = ItemRef::ofItem(ItemId::FlintAndSteel);
    takeItem(cursor, flint);
    EXPECT_EQ(cursor.count, 1);
    for (int i = 0; i < 10; ++i) {
        takeItem(cursor, flint);
    }
    EXPECT_EQ(cursor.count, 1);  // tools do not stack

    const std::array<ItemRef, kHotbarSlots> bar = defaultHotbar();
    ItemStack toolCursor;
    toolCursor.ref = flint;
    toolCursor.count = static_cast<std::uint8_t>(maxStackSize(flint));
    EXPECT_EQ(toolCursor.count, 1);
    EXPECT_EQ(bar[0].block, BLOCK_GRASS);
}

TEST(Creative, HotbarClicksPlaceAndPickUp) {
    std::array<ItemRef, kHotbarSlots> bar = defaultHotbar();
    ItemStack cursor;
    takeItem(cursor, ItemRef::ofBlock(BLOCK_SNOW));

    clickHotbarSlot(bar, cursor, 2);  // occupied: swap the held stack with the slot
    EXPECT_EQ(bar[2], ItemRef::ofBlock(BLOCK_SNOW));
    EXPECT_EQ(cursor.ref.block, BLOCK_STONE);
    EXPECT_EQ(cursor.count, kMaxBlockStack);

    clickHotbarSlot(bar, cursor, 4);
    EXPECT_EQ(bar[4], ItemRef::ofBlock(BLOCK_STONE));
    EXPECT_EQ(cursor.ref.block, BLOCK_LOG);

    cursor.clear();
    clickHotbarSlot(bar, cursor, 0);  // empty cursor: pick the slot up
    EXPECT_EQ(cursor.ref.block, BLOCK_GRASS);
    EXPECT_TRUE(bar[0].empty());
    EXPECT_EQ(cursor.count, kMaxBlockStack);

    clickHotbarSlot(bar, cursor, 0);  // put it back
    EXPECT_TRUE(cursor.empty());
    EXPECT_EQ(bar[0].block, BLOCK_GRASS);
}

TEST(Creative, HotbarAcceptsItems) {
    std::array<ItemRef, kHotbarSlots> bar = defaultHotbar();
    bar[5] = {};  // empty slot: the item is dropped in and the cursor clears
    ItemStack cursor;
    takeItem(cursor, ItemRef::ofItem(ItemId::FlintAndSteel));
    clickHotbarSlot(bar, cursor, 5);
    EXPECT_TRUE(bar[5].isItem());
    EXPECT_EQ(bar[5].item, ItemId::FlintAndSteel);
    EXPECT_TRUE(cursor.empty());

    // Swapping into an occupied slot puts the old block back on the cursor.
    takeItem(cursor, ItemRef::ofItem(ItemId::FlintAndSteel));
    clickHotbarSlot(bar, cursor, 0);
    EXPECT_EQ(bar[0], ItemRef::ofItem(ItemId::FlintAndSteel));
    EXPECT_EQ(cursor.ref, ItemRef::ofBlock(BLOCK_GRASS));
}

TEST(Block, AirTextureIsReserved) {
    // Air must not point at a real tile: the HUD and item icons skip air, and a
    // missed check should show the unused magenta tile, not another block.
    const int airTile = blockDef(BLOCK_AIR).tileSide;
    EXPECT_EQ(blockDef(BLOCK_AIR).tileTop, airTile);
    EXPECT_EQ(blockDef(BLOCK_AIR).tileBottom, airTile);
    for (int i = 1; i < static_cast<int>(BLOCK_COUNT); ++i) {
        const BlockDef& def = blockDef(static_cast<BlockId>(i));
        EXPECT_NE(def.tileTop, airTile) << def.name;
        EXPECT_NE(def.tileBottom, airTile) << def.name;
        EXPECT_NE(def.tileSide, airTile) << def.name;
    }
}

TEST(Portal, LightsMinimalFrame) {
    std::map<std::tuple<int, int, int>, BlockId> blocks;
    const auto get = [&blocks](int x, int y, int z) {
        const auto it = blocks.find({x, y, z});
        return it == blocks.end() ? BLOCK_AIR : it->second;
    };
    const auto set = [&blocks](int x, int y, int z, BlockId id) {
        blocks[{x, y, z}] = id;
        return true;
    };

    // 4x5 obsidian frame at z=0 with a 2x3 interior.
    for (int x = 0; x <= 3; ++x) {
        set(x, 0, 0, BLOCK_OBSIDIAN);
        set(x, 4, 0, BLOCK_OBSIDIAN);
    }
    for (int y = 0; y <= 4; ++y) {
        set(0, y, 0, BLOCK_OBSIDIAN);
        set(3, y, 0, BLOCK_OBSIDIAN);
    }

    PortalFrame frame;
    ASSERT_TRUE(findPortalFrame(get, glm::ivec3(1, 1, 0), frame));
    EXPECT_TRUE(frame.normalZ);
    EXPECT_EQ(frame.width(), 2);
    EXPECT_EQ(frame.height(), 3);

    ASSERT_TRUE(fillPortal(get, set, glm::ivec3(1, 1, 0), frame));
    EXPECT_EQ(get(1, 1, 0), BLOCK_PORTAL_Z);
    EXPECT_EQ(get(2, 2, 0), BLOCK_PORTAL_Z);
    EXPECT_EQ(get(2, 3, 0), BLOCK_PORTAL_Z);
    EXPECT_EQ(get(0, 0, 0), BLOCK_OBSIDIAN);
    EXPECT_EQ(get(-1, 1, 0), BLOCK_AIR);
}

TEST(Portal, RejectsBrokenRing) {
    std::map<std::tuple<int, int, int>, BlockId> blocks;
    const auto get = [&blocks](int x, int y, int z) {
        const auto it = blocks.find({x, y, z});
        return it == blocks.end() ? BLOCK_AIR : it->second;
    };
    const auto set = [&blocks](int x, int y, int z, BlockId id) {
        blocks[{x, y, z}] = id;
        return true;
    };

    for (int x = 0; x <= 3; ++x) {
        set(x, 0, 0, BLOCK_OBSIDIAN);
        set(x, 4, 0, BLOCK_OBSIDIAN);
    }
    for (int y = 0; y <= 4; ++y) {
        set(0, y, 0, BLOCK_OBSIDIAN);
        set(3, y, 0, BLOCK_OBSIDIAN);
    }
    set(3, 4, 0, BLOCK_AIR);  // missing corner

    PortalFrame frame;
    EXPECT_FALSE(findPortalFrame(get, glm::ivec3(1, 1, 0), frame));
}

TEST(Portal, RejectsTooNarrowInterior) {
    std::map<std::tuple<int, int, int>, BlockId> blocks;
    const auto get = [&blocks](int x, int y, int z) {
        const auto it = blocks.find({x, y, z});
        return it == blocks.end() ? BLOCK_AIR : it->second;
    };
    const auto set = [&blocks](int x, int y, int z, BlockId id) {
        blocks[{x, y, z}] = id;
        return true;
    };

    // 3x5 frame: interior is only one block wide.
    for (int x = 0; x <= 2; ++x) {
        set(x, 0, 0, BLOCK_OBSIDIAN);
        set(x, 4, 0, BLOCK_OBSIDIAN);
    }
    for (int y = 0; y <= 4; ++y) {
        set(0, y, 0, BLOCK_OBSIDIAN);
        set(2, y, 0, BLOCK_OBSIDIAN);
    }

    PortalFrame frame;
    EXPECT_FALSE(findPortalFrame(get, glm::ivec3(1, 1, 0), frame));
}

TEST(Portal, RejectsTooWideInterior) {
    std::map<std::tuple<int, int, int>, BlockId> blocks;
    const auto get = [&blocks](int x, int y, int z) {
        const auto it = blocks.find({x, y, z});
        return it == blocks.end() ? BLOCK_AIR : it->second;
    };
    const auto set = [&blocks](int x, int y, int z, BlockId id) {
        blocks[{x, y, z}] = id;
        return true;
    };

    // 22 blocks of interior space: beyond the vanilla 21 limit.
    for (int x = 0; x <= 23; ++x) {
        set(x, 0, 0, BLOCK_OBSIDIAN);
        set(x, 4, 0, BLOCK_OBSIDIAN);
    }
    for (int y = 0; y <= 4; ++y) {
        set(0, y, 0, BLOCK_OBSIDIAN);
        set(23, y, 0, BLOCK_OBSIDIAN);
    }

    PortalFrame frame;
    EXPECT_FALSE(findPortalFrame(get, glm::ivec3(1, 1, 0), frame));
}

TEST(Portal, RejectsSolidStart) {
    std::map<std::tuple<int, int, int>, BlockId> blocks;
    blocks[{1, 1, 0}] = BLOCK_STONE;
    const auto get = [&blocks](int x, int y, int z) {
        const auto it = blocks.find({x, y, z});
        return it == blocks.end() ? BLOCK_AIR : it->second;
    };
    PortalFrame frame;
    EXPECT_FALSE(findPortalFrame(get, glm::ivec3(1, 1, 0), frame));
}

TEST(Save, LoadsVersion2WithoutItems) {
    const char* path = "aimc_test_save_v2.aimc";
    gzFile file = gzopen(path, "wb6");
    ASSERT_NE(file, nullptr);

    const char magic[4] = {'A', 'I', 'M', 'C'};
    const std::uint32_t version = 2;
    const std::int32_t seed = 555;
    const float pose[5] = {1.0f, 2.0f, 3.0f, 45.0f, -10.0f};
    const std::int32_t selected = 1;
    const float time = 0.25f;
    const std::uint8_t hotbar[9] = {3, 0, 2, 0, 0, 0, 0, 0, 9};
    const std::uint32_t chunks = 0;
    ASSERT_EQ(gzwrite(file, magic, 4), 4);
    ASSERT_EQ(gzwrite(file, &version, 4), 4);
    ASSERT_EQ(gzwrite(file, &seed, 4), 4);
    ASSERT_EQ(gzwrite(file, pose, static_cast<unsigned int>(sizeof(pose))),
              static_cast<int>(sizeof(pose)));
    ASSERT_EQ(gzwrite(file, &selected, 4), 4);
    ASSERT_EQ(gzwrite(file, &time, 4), 4);
    ASSERT_EQ(gzwrite(file, hotbar, sizeof(hotbar)), static_cast<int>(sizeof(hotbar)));
    ASSERT_EQ(gzwrite(file, &chunks, 4), 4);
    ASSERT_EQ(gzclose(file), Z_OK);

    SaveData loaded;
    ASSERT_TRUE(loadWorld(path, loaded));
    std::remove(path);

    EXPECT_EQ(loaded.seed, 555);
    EXPECT_EQ(loaded.hotbar[0], ItemRef::ofBlock(BLOCK_STONE));
    EXPECT_TRUE(loaded.hotbar[1].empty());
    EXPECT_EQ(loaded.hotbar[2], ItemRef::ofBlock(BLOCK_DIRT));
    EXPECT_EQ(loaded.hotbar[8], ItemRef::ofBlock(BLOCK_WATER));
    for (const ItemRef& slot : loaded.hotbar) {
        EXPECT_FALSE(slot.isItem());
    }
}

TEST(Mesh, PortalBecomesASinglePlane) {
    Atlas atlas;
    Chunk chunk(glm::ivec2(0, 0));
    World world;
    chunk.set(4, 4, 4, static_cast<std::uint8_t>(BLOCK_PORTAL_Z));

    ChunkMeshData mesh;
    buildChunkMesh(chunk, world, atlas, mesh);

    EXPECT_TRUE(mesh.opaque.vertices.empty());
    EXPECT_TRUE(mesh.water.vertices.empty());
    ASSERT_EQ(mesh.portal.vertices.size(), 4u);
    ASSERT_EQ(mesh.portal.indices.size(), 6u);
    for (const Vertex& v : mesh.portal.vertices) {
        EXPECT_FLOAT_EQ(v.pos[2], 4.5f);
        EXPECT_FLOAT_EQ(v.light, 1.0f);
    }
}

TEST(Portal, LightsInARealWorld) {
    Atlas atlas;
    World world;
    world.init(7);
    world.setAtlas(&atlas);
    world.update(glm::vec3(0.5f, 64.0f, 0.5f), 1, false);
    for (int i = 0; i < 400 && world.getBlock(0, 0, 0) == BLOCK_AIR; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_NE(world.getBlock(0, 0, 0), BLOCK_AIR);

    const int surface = world.surfaceY(0, 0);
    ASSERT_GE(surface, 0);
    const int y0 = std::min(surface + 2, Chunk::SY - 6);
    const int z = 0;

    // Bottom and top rows, then the side columns: no block is set twice.
    for (int x = 0; x <= 3; ++x) {
        ASSERT_TRUE(world.setBlock(x, y0, z, BLOCK_OBSIDIAN));
        ASSERT_TRUE(world.setBlock(x, y0 + 4, z, BLOCK_OBSIDIAN));
    }
    for (int y = y0 + 1; y <= y0 + 3; ++y) {
        ASSERT_TRUE(world.setBlock(0, y, z, BLOCK_OBSIDIAN));
        ASSERT_TRUE(world.setBlock(3, y, z, BLOCK_OBSIDIAN));
    }
    for (int x = 1; x <= 2; ++x) {
        for (int y = y0 + 1; y <= y0 + 3; ++y) {
            (void)world.setBlock(x, y, z, BLOCK_AIR);
        }
    }

    EXPECT_TRUE(lightPortal(world, glm::ivec3(1, y0 + 1, z)));
    EXPECT_EQ(world.getBlock(1, y0 + 1, z), BLOCK_PORTAL_Z);
    EXPECT_EQ(world.getBlock(2, y0 + 3, z), BLOCK_PORTAL_Z);
    EXPECT_EQ(world.getBlock(0, y0, z), BLOCK_OBSIDIAN);
}

TEST(Dimension, ScalesCoordinates) {
    EXPECT_EQ(scaleBetweenDimensions(glm::ivec3(16, 30, -16), Dimension::Overworld,
                                     Dimension::Nether),
              glm::ivec3(2, 30, -2));
    EXPECT_EQ(scaleBetweenDimensions(glm::ivec3(15, 30, 15), Dimension::Overworld,
                                     Dimension::Nether),
              glm::ivec3(1, 30, 1));
    EXPECT_EQ(scaleBetweenDimensions(glm::ivec3(-1, 30, -9), Dimension::Overworld,
                                     Dimension::Nether),
              glm::ivec3(-1, 30, -2));
    EXPECT_EQ(scaleBetweenDimensions(glm::ivec3(2, 30, -2), Dimension::Nether,
                                     Dimension::Overworld),
              glm::ivec3(16, 30, -16));
    EXPECT_EQ(scaleBetweenDimensions(glm::ivec3(3, 30, 4), Dimension::Nether,
                                     Dimension::Nether),
              glm::ivec3(3, 30, 4));
    EXPECT_STREQ(dimensionName(Dimension::Nether), "Nether");
    EXPECT_STREQ(dimensionName(Dimension::Overworld), "Overworld");
}

TEST(NetherTerrain, GeneratesShelvesLavaAndRoof) {
    NetherTerrain terrain(7);
    int lavaCount = 0;

    for (int cz = 0; cz < 2; ++cz) {
        for (int cx = 0; cx < 2; ++cx) {
            Chunk chunk(glm::ivec2(cx, cz));
            terrain.generate(chunk);

            for (int x = 0; x < Chunk::SX; ++x) {
                for (int z = 0; z < Chunk::SZ; ++z) {
                    EXPECT_EQ(chunk.get(x, 0, z), static_cast<std::uint8_t>(BLOCK_BEDROCK));
                    EXPECT_EQ(chunk.get(x, Chunk::SY - 1, z),
                              static_cast<std::uint8_t>(BLOCK_BEDROCK));

                    const int h = terrain.heightAt(cx * Chunk::SX + x, cz * Chunk::SZ + z);
                    EXPECT_GE(h, NetherTerrain::LavaLevel - 4);
                    EXPECT_LE(h, Chunk::SY - 14);

                    const std::uint8_t surface = chunk.get(x, h, z);
                    EXPECT_TRUE(surface == static_cast<std::uint8_t>(BLOCK_NETHERRACK) ||
                                surface == static_cast<std::uint8_t>(BLOCK_SOUL_SAND));

                    if (h < NetherTerrain::LavaLevel) {
                        EXPECT_EQ(chunk.get(x, NetherTerrain::LavaLevel, z),
                                  static_cast<std::uint8_t>(BLOCK_LAVA));
                    }
                }
            }

            for (const std::uint8_t block : chunk.rawBlocks()) {
                EXPECT_NE(block, static_cast<std::uint8_t>(BLOCK_WATER));
                EXPECT_NE(block, static_cast<std::uint8_t>(BLOCK_LOG));
                lavaCount += block == static_cast<std::uint8_t>(BLOCK_LAVA) ? 1 : 0;
            }
        }
    }
    (void)lavaCount;

    // Find a valley below the lava level and make sure the sea is there.
    int valleyX = 0;
    int valleyZ = 0;
    bool found = false;
    for (int z = -64; z <= 64 && !found; ++z) {
        for (int x = -64; x <= 64 && !found; ++x) {
            if (terrain.heightAt(x, z) < NetherTerrain::LavaLevel) {
                valleyX = x;
                valleyZ = z;
                found = true;
            }
        }
    }
    ASSERT_TRUE(found);
    const int cx = floorDiv(valleyX, Chunk::SX);
    const int cz = floorDiv(valleyZ, Chunk::SZ);
    Chunk valley(glm::ivec2(cx, cz));
    terrain.generate(valley);
    EXPECT_EQ(valley.get(floorMod(valleyX, Chunk::SX), NetherTerrain::LavaLevel,
                         floorMod(valleyZ, Chunk::SZ)),
              static_cast<std::uint8_t>(BLOCK_LAVA));

    Chunk a(glm::ivec2(3, -2));
    Chunk b(glm::ivec2(3, -2));
    terrain.generate(a);
    terrain.generate(b);
    EXPECT_EQ(a.rawBlocks(), b.rawBlocks());
}

TEST(World, DimensionsKeepSeparateEdits) {
    World world;
    world.init(5);

    std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> overworldEdits;
    overworldEdits[0x0000000000000000ull][1] = static_cast<std::uint8_t>(BLOCK_STONE);
    std::map<std::uint64_t, std::map<std::uint32_t, std::uint8_t>> netherEdits;
    netherEdits[0x0000000000000000ull][2] = static_cast<std::uint8_t>(BLOCK_NETHERRACK);

    world.setDimensionEdits(Dimension::Overworld, overworldEdits);
    world.setDimensionEdits(Dimension::Nether, netherEdits);
    EXPECT_EQ(world.dimension(), Dimension::Overworld);
    EXPECT_EQ(world.editsFor(Dimension::Overworld), overworldEdits);
    EXPECT_EQ(world.editsFor(Dimension::Nether), netherEdits);

    world.setDimension(Dimension::Nether);
    EXPECT_EQ(world.dimension(), Dimension::Nether);
    EXPECT_EQ(world.editsFor(Dimension::Nether), netherEdits);
    EXPECT_EQ(world.editsFor(Dimension::Overworld), overworldEdits);

    world.setDimension(Dimension::Overworld);
    EXPECT_EQ(world.editsFor(Dimension::Overworld), overworldEdits);
}

TEST(Creative, NetherTabHasNetherBlocks) {
    const std::vector<ItemRef>& nether = creativeTabItems(CreativeTab::Nether);
    EXPECT_NE(std::find(nether.begin(), nether.end(), ItemRef::ofBlock(BLOCK_NETHERRACK)),
              nether.end());
    EXPECT_NE(std::find(nether.begin(), nether.end(), ItemRef::ofBlock(BLOCK_LAVA)),
              nether.end());
    EXPECT_NE(std::find(nether.begin(), nether.end(), ItemRef::ofBlock(BLOCK_GLOWSTONE)),
              nether.end());
}

TEST(Portal, BuildsDestinationPortal) {
    Atlas atlas;
    World world;
    world.init(11);
    world.setAtlas(&atlas);
    world.update(glm::vec3(0.5f, 64.0f, 0.5f), 1, false);
    for (int i = 0; i < 400 && world.getBlock(0, 0, 0) == BLOCK_AIR; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_NE(world.getBlock(0, 0, 0), BLOCK_AIR);

    glm::vec3 spawn{};
    ASSERT_TRUE(buildPortalAt(world, glm::ivec3(3, 0, 3), spawn));
    EXPECT_GE(spawn.y, 2.0f);

    glm::ivec3 found{};
    ASSERT_TRUE(findPortalNear(world, glm::ivec3(3, 0, 3), 4, found));
    EXPECT_TRUE(isPortalBlock(world.getBlock(found.x, found.y, found.z)));
    EXPECT_EQ(found.z, 3);
}

TEST(Portal, TravelSequenceBuildsNetherSide) {
    Atlas atlas;
    World world;
    world.init(3);
    world.setAtlas(&atlas);
    world.update(glm::vec3(0.5f, 64.0f, 0.5f), 2, false);
    for (int i = 0; i < 400 && world.getBlock(0, 0, 0) == BLOCK_AIR; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_NE(world.getBlock(0, 0, 0), BLOCK_AIR);

    glm::vec3 spawn{};
    ASSERT_TRUE(buildPortalAt(world, glm::ivec3(0, 0, 0), spawn));

    world.setDimension(Dimension::Nether);
    world.update(glm::vec3(0.5f, 40.0f, 0.5f), 2, false);
    for (int i = 0; i < 400 && world.getBlock(0, 0, 0) == BLOCK_AIR; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(world.getBlock(0, 0, 0), BLOCK_BEDROCK);

    glm::vec3 netherSpawn{};
    ASSERT_TRUE(buildPortalAt(world, glm::ivec3(0, 0, 0), netherSpawn));
    glm::ivec3 found{};
    ASSERT_TRUE(findPortalNear(world, glm::ivec3(0, 0, 0), 6, found));
    EXPECT_TRUE(isPortalBlock(world.getBlock(found.x, found.y, found.z)));
}

TEST(Save, LoadsVersion3WithItems) {
    const char* path = "aimc_test_save_v3.aimc";
    gzFile file = gzopen(path, "wb6");
    ASSERT_NE(file, nullptr);

    const char magic[4] = {'A', 'I', 'M', 'C'};
    const std::uint32_t version = 3;
    const std::int32_t seed = 99;
    const float pose[5] = {4.0f, 5.0f, 6.0f, -90.0f, -20.0f};
    const std::int32_t selected = 0;
    const float time = 0.75f;
    std::uint8_t hotbar[18] = {};
    hotbar[0] = 0;  // empty block
    hotbar[1] = 1;  // flint and steel
    hotbar[2] = 3;  // stone block
    hotbar[3] = 0;  // no item
    hotbar[16] = 9;  // water
    hotbar[17] = 0;
    const std::uint32_t chunks = 0;
    ASSERT_EQ(gzwrite(file, magic, 4), 4);
    ASSERT_EQ(gzwrite(file, &version, 4), 4);
    ASSERT_EQ(gzwrite(file, &seed, 4), 4);
    ASSERT_EQ(gzwrite(file, pose, static_cast<unsigned int>(sizeof(pose))),
              static_cast<int>(sizeof(pose)));
    ASSERT_EQ(gzwrite(file, &selected, 4), 4);
    ASSERT_EQ(gzwrite(file, &time, 4), 4);
    ASSERT_EQ(gzwrite(file, hotbar, sizeof(hotbar)), static_cast<int>(sizeof(hotbar)));
    ASSERT_EQ(gzwrite(file, &chunks, 4), 4);
    ASSERT_EQ(gzclose(file), Z_OK);

    SaveData loaded;
    ASSERT_TRUE(loadWorld(path, loaded));
    std::remove(path);

    EXPECT_EQ(loaded.seed, 99);
    EXPECT_EQ(loaded.current, Dimension::Overworld);
    EXPECT_EQ(loaded.hotbar[0], ItemRef::ofItem(ItemId::FlintAndSteel));
    EXPECT_EQ(loaded.hotbar[1], ItemRef::ofBlock(BLOCK_STONE));
    EXPECT_EQ(loaded.hotbar[8], ItemRef::ofBlock(BLOCK_WATER));
    EXPECT_TRUE(loaded.dimensions[0].edits.empty());
}

}  // namespace
