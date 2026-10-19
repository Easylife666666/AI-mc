#pragma once

#include <cstdint>

#include "world/block.h"

namespace aimc {

constexpr int kHotbarSlots = 9;
constexpr int kMaxBlockStack = 64;

// Items that are not placeable blocks. Blocks keep their own id space; a
// hotbar slot holds either one block or one item.
enum class ItemId : std::uint8_t {
    None = 0,
    FlintAndSteel,
    Count,
};

struct ItemDef {
    const char* name;
    int iconTile;  // atlas tile used for the flat inventory icon
    std::uint8_t maxStack;
};

const ItemDef& itemDef(ItemId id);

// A reference to a block or an item, never both.
struct ItemRef {
    BlockId block = BLOCK_AIR;
    ItemId item = ItemId::None;

    static ItemRef ofBlock(BlockId id) { return {id, ItemId::None}; }
    static ItemRef ofItem(ItemId id) { return {BLOCK_AIR, id}; }

    bool empty() const { return block == BLOCK_AIR && item == ItemId::None; }
    bool isItem() const { return item != ItemId::None; }
    const char* name() const;
    int iconTile() const;

    bool operator==(const ItemRef&) const = default;
};

int maxStackSize(const ItemRef& ref);

// A stack carried on the inventory cursor. The hotbar stores bare refs, so the
// count only matters while dragging something around.
struct ItemStack {
    ItemRef ref;
    std::uint8_t count = 0;

    bool empty() const { return ref.empty() || count == 0; }
    void clear() {
        ref = {};
        count = 0;
    }
};

}  // namespace aimc
