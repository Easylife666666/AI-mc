#pragma once

#include <cstdint>
#include <vector>

namespace aimc {

class Chunk;
class World;
class Atlas;

// Tightly packed so the GPU layout is obvious (24 bytes).
struct Vertex {
    float pos[3];
    float uv[2];
    float light;
};
static_assert(sizeof(Vertex) == 24, "Vertex must stay tightly packed");

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct ChunkMeshData {
    MeshData opaque;
    MeshData water;
    MeshData lava;    // emissive liquid, drawn opaque
    MeshData portal;  // emissive, so it keeps glowing at night
};

// Emits only visible faces with per-vertex ambient occlusion. Opaque geometry
// and water go into separate buffers so water can be drawn transparent.
void buildChunkMesh(const Chunk& chunk, const World& world, const Atlas& atlas,
                    ChunkMeshData& out);

}  // namespace aimc
