#include "world/chunk.h"

namespace aimc {

Chunk::Chunk(glm::ivec2 coord)
    : m_coord(coord), m_blocks(static_cast<std::size_t>(VOLUME), 0) {}

}  // namespace aimc
