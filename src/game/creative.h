#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "world/block.h"

namespace aimc {

constexpr int kHotbarSlots = 9;
constexpr int kMaxStack = 64;

// A stack carried on the inventory cursor. Creative mode never consumes blocks,
// so only the cursor and the hotbar carry stacks and the count is cosmetic.
struct ItemStack {
    BlockId block = BLOCK_AIR;
    std::uint8_t count = 0;

    bool empty() const { return block == BLOCK_AIR || count == 0; }
    void clear() {
        block = BLOCK_AIR;
        count = 0;
    }
};

// Content tabs, loosely following the vanilla order. Tabs are filled from the
// blocks this game actually has; empty vanilla tabs are not shown.
enum class CreativeTab : int {
    Building,
    Natural,
    Search,
    Count,
};

struct CreativeTabInfo {
    const char* name;
    BlockId icon;
    bool search;
};

CreativeTabInfo creativeTabInfo(CreativeTab tab);

// Fixed per-tab ordering, like vanilla's registration order. Search is empty.
const std::vector<BlockId>& creativeTabItems(CreativeTab tab);

// Every placeable block (everything except air), in menu order.
const std::vector<BlockId>& creativeAllBlocks();

std::array<BlockId, kHotbarSlots> defaultHotbar();

bool blockMatchesQuery(BlockId block, const std::string& query);

// Items shown for a tab, optionally filtered by the search query.
std::vector<BlockId> creativeItems(CreativeTab tab, const std::string& query);

int creativeRowCount(int itemCount, int columns);
int creativeMaxScrollRow(int itemCount, int columns, int visibleRows);

int firstEmptyHotbarSlot(const std::array<BlockId, kHotbarSlots>& hotbar);

// Puts a block in the first empty slot. Returns the slot index, or -1 when the
// hotbar is full.
int putInHotbar(std::array<BlockId, kHotbarSlots>& hotbar, BlockId block);

// Click rules, modelled on the vanilla creative inventory.
void takeItem(ItemStack& cursor, BlockId block);     // left click: replace, or add one
void takeOneItem(ItemStack& cursor, BlockId block);  // right click: one, or put one back
void clickHotbarSlot(std::array<BlockId, kHotbarSlots>& hotbar, ItemStack& cursor, int slot);

}  // namespace aimc
