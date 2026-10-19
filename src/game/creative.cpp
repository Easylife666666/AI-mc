#include "game/creative.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace aimc {
namespace {

const std::vector<BlockId> kBuilding = {
    BLOCK_STONE,        // stone
    BLOCK_COBBLE,       // cobblestone
    BLOCK_STONE_BRICKS, // stone bricks
    BLOCK_BRICKS,       // bricks
    BLOCK_PLANKS,       // oak planks
};

const std::vector<BlockId> kNatural = {
    BLOCK_GRASS,   // grass block
    BLOCK_DIRT,    // dirt
    BLOCK_SAND,    // sand
    BLOCK_GRAVEL,  // gravel
    BLOCK_LOG,     // oak log
    BLOCK_LEAVES,  // oak leaves
    BLOCK_SNOW,    // snow block
    BLOCK_WATER,   // water
};

const std::vector<BlockId> kSearch;  // filled at runtime from creativeAllBlocks()

const std::vector<BlockId> kAll = {
    BLOCK_GRASS,        BLOCK_DIRT,  BLOCK_STONE,  BLOCK_COBBLE, BLOCK_STONE_BRICKS,
    BLOCK_BRICKS,       BLOCK_PLANKS, BLOCK_SAND,  BLOCK_GRAVEL, BLOCK_LOG,
    BLOCK_LEAVES,       BLOCK_SNOW,  BLOCK_WATER,
};

std::string toLower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

const std::vector<BlockId>& itemsForTab(CreativeTab tab) {
    switch (tab) {
    case CreativeTab::Building:
        return kBuilding;
    case CreativeTab::Natural:
        return kNatural;
    case CreativeTab::Search:
    case CreativeTab::Count:
        break;
    }
    return kSearch;
}

}  // namespace

CreativeTabInfo creativeTabInfo(CreativeTab tab) {
    switch (tab) {
    case CreativeTab::Building:
        return {"Building Blocks", BLOCK_STONE, false};
    case CreativeTab::Natural:
        return {"Natural Blocks", BLOCK_GRASS, false};
    case CreativeTab::Search:
        return {"Search Items", BLOCK_AIR, true};
    case CreativeTab::Count:
        break;
    }
    return {"Building Blocks", BLOCK_STONE, false};
}

const std::vector<BlockId>& creativeTabItems(CreativeTab tab) { return itemsForTab(tab); }

const std::vector<BlockId>& creativeAllBlocks() { return kAll; }

std::array<BlockId, kHotbarSlots> defaultHotbar() {
    return {BLOCK_GRASS,  BLOCK_DIRT,   BLOCK_STONE,  BLOCK_SAND,   BLOCK_LOG,
            BLOCK_LEAVES, BLOCK_PLANKS, BLOCK_COBBLE, BLOCK_WATER};
}

bool blockMatchesQuery(BlockId block, const std::string& query) {
    if (query.empty()) {
        return true;
    }
    const std::string name = toLower(blockDef(block).name);
    return name.find(toLower(query)) != std::string::npos;
}

std::vector<BlockId> creativeItems(CreativeTab tab, const std::string& query) {
    std::vector<BlockId> items;
    const std::vector<BlockId>& source =
        (tab == CreativeTab::Search) ? kAll : itemsForTab(tab);
    items.reserve(source.size());
    for (const BlockId block : source) {
        if (blockMatchesQuery(block, query)) {
            items.push_back(block);
        }
    }
    return items;
}

int creativeRowCount(int itemCount, int columns) {
    if (itemCount <= 0 || columns <= 0) {
        return 0;
    }
    return (itemCount + columns - 1) / columns;
}

int creativeMaxScrollRow(int itemCount, int columns, int visibleRows) {
    return std::max(0, creativeRowCount(itemCount, columns) - std::max(0, visibleRows));
}

int firstEmptyHotbarSlot(const std::array<BlockId, kHotbarSlots>& hotbar) {
    for (int i = 0; i < kHotbarSlots; ++i) {
        if (hotbar[static_cast<std::size_t>(i)] == BLOCK_AIR) {
            return i;
        }
    }
    return -1;
}

int putInHotbar(std::array<BlockId, kHotbarSlots>& hotbar, BlockId block) {
    if (block == BLOCK_AIR) {
        return -1;
    }
    const int slot = firstEmptyHotbarSlot(hotbar);
    if (slot < 0) {
        return -1;
    }
    hotbar[static_cast<std::size_t>(slot)] = block;
    return slot;
}

void takeItem(ItemStack& cursor, BlockId block) {
    if (block == BLOCK_AIR) {
        return;
    }
    if (cursor.block == block) {
        cursor.count = static_cast<std::uint8_t>(
            std::min(kMaxStack, static_cast<int>(cursor.count) + 1));
    } else {
        cursor.block = block;
        cursor.count = 1;
    }
}

void takeOneItem(ItemStack& cursor, BlockId block) {
    if (block == BLOCK_AIR) {
        return;
    }
    if (cursor.block == block) {
        cursor.count =
            static_cast<std::uint8_t>(std::max(0, static_cast<int>(cursor.count) - 1));
        if (cursor.count == 0) {
            cursor.clear();
        }
    } else {
        cursor.block = block;
        cursor.count = 1;
    }
}

void clickHotbarSlot(std::array<BlockId, kHotbarSlots>& hotbar, ItemStack& cursor, int slot) {
    if (slot < 0 || slot >= kHotbarSlots) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(slot);
    if (!cursor.empty()) {
        if (hotbar[index] == BLOCK_AIR || hotbar[index] == cursor.block) {
            hotbar[index] = cursor.block;
            cursor.clear();
        } else {
            std::swap(hotbar[index], cursor.block);
            cursor.count = static_cast<std::uint8_t>(kMaxStack);
        }
    } else if (hotbar[index] != BLOCK_AIR) {
        cursor.block = hotbar[index];
        cursor.count = static_cast<std::uint8_t>(kMaxStack);
        hotbar[index] = BLOCK_AIR;
    }
}

}  // namespace aimc
