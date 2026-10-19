#include "world/portal.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <vector>

#include "world/chunk.h"
#include "world/world.h"

namespace aimc {
namespace {

constexpr int kMinWidth = 2;
constexpr int kMinHeight = 3;
constexpr int kMaxSpan = 21;
constexpr int kScanRadius = kMaxSpan + 2;  // ring included

struct ScanCell {
    int u = 0;  // offset along the first interior axis
    int v = 0;  // offset along Y
};

// Flood fills the air pocket in the vertical plane at start[w] and validates
// both the rectangle and its obsidian ring.
bool scanPlane(const BlockGetter& get, const glm::ivec3& start, bool normalZ, PortalFrame& out) {
    const int uAxis = normalZ ? 0 : 2;
    const int wAxis = normalZ ? 2 : 0;
    const int fixed = start[wAxis];

    std::vector<ScanCell> region;
    std::vector<ScanCell> queue;
    queue.push_back({0, 0});

    auto inBounds = [](const ScanCell& c) {
        return c.u >= -kScanRadius && c.u <= kScanRadius && c.v >= -kScanRadius &&
               c.v <= kScanRadius;
    };
    auto toWorld = [&](const ScanCell& c) {
        glm::ivec3 p = start;
        p[uAxis] += c.u;
        p.y += c.v;
        p[wAxis] = fixed;
        return p;
    };
    auto contains = [&](const ScanCell& c) {
        return std::any_of(region.begin(), region.end(),
                           [&](const ScanCell& r) { return r.u == c.u && r.v == c.v; });
    };

    // The grid is small (45x45), so linear scans are fine at this scale.
    while (!queue.empty()) {
        const ScanCell cell = queue.back();
        queue.pop_back();
        if (!inBounds(cell) || contains(cell)) {
            continue;
        }
        const glm::ivec3 world = toWorld(cell);
        if (world.y < 1 || world.y >= Chunk::SY - 1) {
            continue;
        }
        if (get(world.x, world.y, world.z) != BLOCK_AIR) {
            continue;
        }
        region.push_back(cell);
        if (static_cast<int>(region.size()) > kMaxSpan * kMaxSpan) {
            return false;
        }
        queue.push_back({cell.u + 1, cell.v});
        queue.push_back({cell.u - 1, cell.v});
        queue.push_back({cell.u, cell.v + 1});
        queue.push_back({cell.u, cell.v - 1});
    }

    if (region.empty()) {
        return false;
    }

    int minU = region.front().u;
    int maxU = minU;
    int minV = region.front().v;
    int maxV = minV;
    for (const ScanCell& c : region) {
        minU = std::min(minU, c.u);
        maxU = std::max(maxU, c.u);
        minV = std::min(minV, c.v);
        maxV = std::max(maxV, c.v);
    }

    const int width = maxU - minU + 1;
    const int height = maxV - minV + 1;
    if (width < kMinWidth || width > kMaxSpan || height < kMinHeight || height > kMaxSpan) {
        return false;
    }
    if (static_cast<int>(region.size()) != width * height) {
        return false;  // holes or a non-rectangular pocket
    }

    for (int du = -1; du <= width; ++du) {
        for (int dv = -1; dv <= height; ++dv) {
            if (du >= 0 && du < width && dv >= 0 && dv < height) {
                continue;
            }
            const ScanCell ring{minU + du, minV + dv};
            const glm::ivec3 p = toWorld(ring);
            if (get(p.x, p.y, p.z) != BLOCK_OBSIDIAN) {
                return false;
            }
        }
    }

    PortalFrame frame;
    frame.normalZ = normalZ;
    frame.min = toWorld({minU, minV});
    frame.max = toWorld({maxU, maxV});
    out = frame;
    return true;
}

}  // namespace

int PortalFrame::width() const {
    return normalZ ? max.x - min.x + 1 : max.z - min.z + 1;
}

int PortalFrame::height() const { return max.y - min.y + 1; }

bool findPortalFrame(const BlockGetter& get, const glm::ivec3& start, PortalFrame& out) {
    if (!get || get(start.x, start.y, start.z) != BLOCK_AIR) {
        return false;
    }
    if (scanPlane(get, start, true, out)) {
        return true;
    }
    return scanPlane(get, start, false, out);
}

bool fillPortal(const BlockGetter& get, const BlockSetter& set, const glm::ivec3& start,
                PortalFrame& out) {
    PortalFrame frame;
    if (!findPortalFrame(get, start, frame)) {
        return false;
    }

    const BlockId portal = frame.normalZ ? BLOCK_PORTAL_Z : BLOCK_PORTAL_X;
    if (frame.normalZ) {
        for (int x = frame.min.x; x <= frame.max.x; ++x) {
            for (int y = frame.min.y; y <= frame.max.y; ++y) {
                if (!set(x, y, frame.min.z, portal)) {
                    return false;
                }
            }
        }
    } else {
        for (int z = frame.min.z; z <= frame.max.z; ++z) {
            for (int y = frame.min.y; y <= frame.max.y; ++y) {
                if (!set(frame.min.x, y, z, portal)) {
                    return false;
                }
            }
        }
    }

    out = frame;
    return true;
}

bool lightPortal(World& world, const glm::ivec3& start) {
    PortalFrame frame;
    const BlockGetter get = [&world](int x, int y, int z) { return world.getBlock(x, y, z); };
    const BlockSetter set = [&world](int x, int y, int z, BlockId block) {
        return world.setBlock(x, y, z, block);
    };
    return fillPortal(get, set, start, frame);
}

}  // namespace aimc
