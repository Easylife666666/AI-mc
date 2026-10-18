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

// Emits only visible faces (neighbour-aware), in world-space coordinates.
void buildChunkMesh(const Chunk& chunk, const World& world, const Atlas& atlas, MeshData& out);

}  // namespace aimc
