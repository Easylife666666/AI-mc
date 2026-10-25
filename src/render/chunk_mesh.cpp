#include "render/chunk_mesh.h"

#include <array>
#include <cstdlib>

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

constexpr float kAoLevels[4] = {0.45f, 0.62f, 0.80f, 1.0f};

inline BlockId blockAt(const Chunk& chunk, const World& world, int x, int y, int z) {
    if (chunk.contains(x, y, z)) {
        return static_cast<BlockId>(chunk.get(x, y, z));
    }
    const glm::ivec3 origin = chunk.origin();
    return world.getBlock(origin.x + x, origin.y + y, origin.z + z);
}

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Portals are a single translucent plane in the middle of their block, not a
// full cube. The transparent pass disables culling, so one quad is enough.
void emitPortalQuad(const Chunk& chunk, MeshData& mesh, int x, int y, int z, bool normalZ,
                    const glm::vec4& uvRect) {
    const glm::ivec3 origin = chunk.origin();
    const float baseX = static_cast<float>(origin.x + x);
    const float baseY = static_cast<float>(origin.y + y);
    const float baseZ = static_cast<float>(origin.z + z);
    const float planeX = baseX + 0.5f;
    const float planeZ = baseZ + 0.5f;

    struct Corner {
        float x, y, z, u, v;
    };
    std::array<Corner, 4> corners{};
    if (normalZ) {
        corners = {{{baseX, baseY, planeZ, 0.0f, 0.0f},
                    {baseX + 1.0f, baseY, planeZ, 1.0f, 0.0f},
                    {baseX + 1.0f, baseY + 1.0f, planeZ, 1.0f, 1.0f},
                    {baseX, baseY + 1.0f, planeZ, 0.0f, 1.0f}}};
    } else {
        corners = {{{planeX, baseY, baseZ, 0.0f, 0.0f},
                    {planeX, baseY, baseZ + 1.0f, 1.0f, 0.0f},
                    {planeX, baseY + 1.0f, baseZ + 1.0f, 1.0f, 1.0f},
                    {planeX, baseY + 1.0f, baseZ, 0.0f, 1.0f}}};
    }

    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const Corner& c : corners) {
        Vertex v{};
        v.pos[0] = c.x;
        v.pos[1] = c.y;
        v.pos[2] = c.z;
        v.uv[0] = lerp(uvRect.x, uvRect.z, c.u);
        v.uv[1] = lerp(uvRect.y, uvRect.w, c.v);
        v.light = 1.0f;
        mesh.vertices.push_back(v);
    }
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
}


// Classic voxel AO: two edge neighbours and the diagonal corner of the face.
float vertexAo(const Chunk& chunk, const World& world, int x, int y, int z, const FaceDef& face,
               int cornerIndex) {
    const int axis = face.normal.x != 0 ? 0 : (face.normal.y != 0 ? 1 : 2);
    const int tangentA = (axis + 1) % 3;
    const int tangentB = (axis + 2) % 3;

    const int signA = face.corner[cornerIndex][tangentA] == 1 ? 1 : -1;
    const int signB = face.corner[cornerIndex][tangentB] == 1 ? 1 : -1;

    glm::ivec3 offsetA = face.normal;
    offsetA[tangentA] += signA;
    glm::ivec3 offsetB = face.normal;
    offsetB[tangentB] += signB;

    const bool sideA = isOpaque(blockAt(chunk, world, x + offsetA.x, y + offsetA.y, z + offsetA.z));
    const bool sideB = isOpaque(blockAt(chunk, world, x + offsetB.x, y + offsetB.y, z + offsetB.z));
    const bool diagonal = isOpaque(blockAt(chunk, world, x + offsetA.x + offsetB.x,
                                          y + offsetA.y + offsetB.y,
                                          z + offsetA.z + offsetB.z));

    const int level = (sideA && sideB) ? 0 : 3 - (static_cast<int>(sideA) +
                                                  static_cast<int>(sideB) +
                                                  static_cast<int>(diagonal));
    return kAoLevels[level];
}

template <typename Mesh>
void emitFace(const Chunk& chunk, Mesh& mesh, int x, int y, int z, const FaceDef& face,
              const glm::vec4& uvRect, float light, const float ao[4]) {
    const glm::ivec3 origin = chunk.origin();
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());

    for (int c = 0; c < 4; ++c) {
        Vertex v{};
        v.pos[0] = static_cast<float>(origin.x + x + face.corner[c].x);
        v.pos[1] = static_cast<float>(origin.y + y + face.corner[c].y);
        v.pos[2] = static_cast<float>(origin.z + z + face.corner[c].z);
        v.uv[0] = lerp(uvRect.x, uvRect.z, face.uv[c].x);
        v.uv[1] = lerp(uvRect.y, uvRect.w, face.uv[c].y);
        v.light = light * ao[c];
        mesh.vertices.push_back(v);
    }

    // Split along the darker diagonal to avoid visible anisotropy artefacts.
    if (ao[0] + ao[2] < ao[1] + ao[3]) {
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 3);
        mesh.indices.push_back(base + 0);
    } else {
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
    }
}

}  // namespace

void buildChunkMesh(const Chunk& chunk, const World& world, const Atlas& atlas,
                    ChunkMeshData& out) {
    out.opaque.vertices.clear();
    out.opaque.indices.clear();
    out.water.vertices.clear();
    out.water.indices.clear();
    out.lava.vertices.clear();
    out.lava.indices.clear();
    out.portal.vertices.clear();
    out.portal.indices.clear();

    auto lock = chunk.readLock();

    for (int y = 0; y < Chunk::SY; ++y) {
        for (int z = 0; z < Chunk::SZ; ++z) {
            for (int x = 0; x < Chunk::SX; ++x) {
                const BlockId block = static_cast<BlockId>(chunk.get(x, y, z));
                if (block == BLOCK_AIR) {
                    continue;
                }
                const BlockDef& def = blockDef(block);
                if (block == BLOCK_PORTAL_X || block == BLOCK_PORTAL_Z) {
                    emitPortalQuad(chunk, out.portal, x, y, z, block == BLOCK_PORTAL_Z,
                                   atlas.tileUV(def.tileSide));
                    continue;
                }
                const bool water = isWater(block);
                const bool lava = isLava(block);

                for (const FaceDef& face : kFaces) {
                    const BlockId neighbour =
                        blockAt(chunk, world, x + face.normal.x, y + face.normal.y,
                                z + face.normal.z);

                    const bool sameFluid = (water && isWater(neighbour)) ||
                                           (lava && isLava(neighbour));
                    if (neighbour == block || isOpaque(neighbour) || sameFluid) {
                        continue;
                    }

                    const int tile = face.tileSlot == 0 ? def.tileTop
                                   : face.tileSlot == 1 ? def.tileBottom
                                                        : def.tileSide;
                    const glm::vec4 uvRect = atlas.tileUV(tile);

                    if (water || lava) {
                        const float ao[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                        MeshData& target = lava ? out.lava : out.water;
                        const float light = lava ? 1.0f : face.light * 0.9f;
                        emitFace(chunk, target, x, y, z, face, uvRect, light, ao);
                    } else {
                        float ao[4];
                        for (int c = 0; c < 4; ++c) {
                            ao[c] = vertexAo(chunk, world, x, y, z, face, c);
                        }
                        emitFace(chunk, out.opaque, x, y, z, face, uvRect, face.light, ao);
                    }
                }
            }
        }
    }
}

}  // namespace aimc
