#include "world/world.h"

#include <cstddef>

#include <GL/glew.h>
#include <spdlog/spdlog.h>

#include "core/math.h"
#include "render/atlas.h"
#include "render/chunk_mesh.h"
#include "world/chunk.h"
#include "world/terrain.h"

namespace aimc {

World::World() = default;
World::~World() = default;

std::uint64_t World::key(int cx, int cz) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32) |
           static_cast<std::uint32_t>(cz);
}

void World::init(int seed, int radius) {
    m_chunks.clear();
    m_seed = seed;
    m_radius = radius;
    m_terrain = std::make_unique<Terrain>(seed);

    int generated = 0;
    for (int cz = -radius; cz <= radius; ++cz) {
        for (int cx = -radius; cx <= radius; ++cx) {
            auto chunk = std::make_unique<Chunk>(glm::ivec2(cx, cz));
            m_terrain->generate(*chunk);
            m_chunks.emplace(key(cx, cz), std::move(chunk));
            ++generated;
        }
    }
    spdlog::info("world generated: seed={} radius={} chunks={}", seed, radius, generated);
}

BlockId World::getBlock(int worldX, int worldY, int worldZ) const {
    if (worldY < 0 || worldY >= Chunk::SY) {
        return BLOCK_AIR;
    }
    const int cx = floorDiv(worldX, Chunk::SX);
    const int cz = floorDiv(worldZ, Chunk::SZ);
    const auto it = m_chunks.find(key(cx, cz));
    if (it == m_chunks.end()) {
        return BLOCK_AIR;
    }
    return static_cast<BlockId>(
        it->second->get(floorMod(worldX, Chunk::SX), worldY, floorMod(worldZ, Chunk::SZ)));
}

Chunk* World::getChunk(int cx, int cz) const {
    const auto it = m_chunks.find(key(cx, cz));
    return it == m_chunks.end() ? nullptr : it->second.get();
}

void World::buildAllMeshes(const Atlas& atlas) {
    MeshData mesh;
    for (auto& [k, chunk] : m_chunks) {
        mesh.vertices.clear();
        mesh.indices.clear();
        buildChunkMesh(*chunk, *this, atlas, mesh);

        if (chunk->vao == 0) {
            glGenVertexArrays(1, &chunk->vao);
            glGenBuffers(1, &chunk->vbo);
            glGenBuffers(1, &chunk->ibo);
        }

        glBindVertexArray(chunk->vao);
        glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                     mesh.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk->ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                     mesh.indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(offsetof(Vertex, pos)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(offsetof(Vertex, uv)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(offsetof(Vertex, light)));

        chunk->indexCount = static_cast<int>(mesh.indices.size());
    }
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void World::destroyGpu() {
    for (auto& [k, chunk] : m_chunks) {
        if (chunk->vao != 0) {
            glDeleteVertexArrays(1, &chunk->vao);
        }
        if (chunk->vbo != 0) {
            glDeleteBuffers(1, &chunk->vbo);
        }
        if (chunk->ibo != 0) {
            glDeleteBuffers(1, &chunk->ibo);
        }
        chunk->vao = 0;
        chunk->vbo = 0;
        chunk->ibo = 0;
        chunk->indexCount = 0;
    }
}

DrawStats World::draw() const {
    DrawStats stats;
    for (const auto& [k, chunk] : m_chunks) {
        if (chunk->indexCount <= 0) {
            continue;
        }
        glBindVertexArray(chunk->vao);
        glDrawElements(GL_TRIANGLES, chunk->indexCount, GL_UNSIGNED_INT, nullptr);
        ++stats.drawCalls;
        stats.triangles += chunk->indexCount / 3;
    }
    glBindVertexArray(0);
    return stats;
}

}  // namespace aimc
