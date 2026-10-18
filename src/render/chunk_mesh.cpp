#include "render/chunk_mesh.h"

#include <array>

#include <glm/glm.hpp>

#include "render/atlas.h"
#include "world/block.h"
#include "world/chunk.h"
#include "world/world.h"

namespace aimc {
namespace {

struct FaceDef {
    glm::ivec3 normal;
    glm::ivec3 corner[4];
    glm::vec2 uv[4];
    float light;
    int tileSlot;  // 0 = top, 1 = bottom, 2 = side
};

const std::array<FaceDef, 6> kFaces = {{
    // +X
    {{1, 0, 0}, {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
     {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}}, 0.80f, 2},
    // -X
    {{-1, 0, 0}, {{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}},
     {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}}, 0.80f, 2},
    // +Y
    {{0, 1, 0}, {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}},
     {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}}, 1.00f, 0},
    // -Y
    {{0, -1, 0}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
     {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}}, 0.50f, 1},
    // +Z
    {{0, 0, 1}, {{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}},
     {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}}, 0.65f, 2},
    // -Z
    {{0, 0, -1}, {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
     {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}}, 0.65f, 2},
}};

BlockId blockAt(const Chunk& chunk, const World& world, int x, int y, int z) {
    if (chunk.contains(x, y, z)) {
        return static_cast<BlockId>(chunk.get(x, y, z));
    }
    const glm::ivec3 origin = chunk.origin();
    return world.getBlock(origin.x + x, origin.y + y, origin.z + z);
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

}  // namespace

void buildChunkMesh(const Chunk& chunk, const World& world, const Atlas& atlas, MeshData& out) {
    out.vertices.clear();
    out.indices.clear();

    const glm::ivec3 origin = chunk.origin();

    for (int y = 0; y < Chunk::SY; ++y) {
        for (int z = 0; z < Chunk::SZ; ++z) {
            for (int x = 0; x < Chunk::SX; ++x) {
                const BlockId block = static_cast<BlockId>(chunk.get(x, y, z));
                if (block == BLOCK_AIR) {
                    continue;
                }
                const BlockDef& def = blockDef(block);

                for (const FaceDef& face : kFaces) {
                    const BlockId neighbour =
                        blockAt(chunk, world, x + face.normal.x, y + face.normal.y,
                                z + face.normal.z);

                    // Same block behind the face (water/water) or any opaque
                    // neighbour hides the face completely.
                    if (neighbour == block || isOpaque(neighbour)) {
                        continue;
                    }

                    const int tile = face.tileSlot == 0 ? def.tileTop
                                   : face.tileSlot == 1 ? def.tileBottom
                                                        : def.tileSide;
                    const glm::vec4 uvRect = atlas.tileUV(tile);
                    const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());

                    for (int c = 0; c < 4; ++c) {
                        Vertex v{};
                        v.pos[0] = static_cast<float>(origin.x + x + face.corner[c].x);
                        v.pos[1] = static_cast<float>(origin.y + y + face.corner[c].y);
                        v.pos[2] = static_cast<float>(origin.z + z + face.corner[c].z);
                        v.uv[0] = lerp(uvRect.x, uvRect.z, face.uv[c].x);
                        v.uv[1] = lerp(uvRect.y, uvRect.w, face.uv[c].y);
                        v.light = face.light;
                        out.vertices.push_back(v);
                    }

                    out.indices.push_back(base + 0);
                    out.indices.push_back(base + 1);
                    out.indices.push_back(base + 2);
                    out.indices.push_back(base + 0);
                    out.indices.push_back(base + 2);
                    out.indices.push_back(base + 3);
                }
            }
        }
    }
}

}  // namespace aimc
