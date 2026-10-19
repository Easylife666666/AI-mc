#include "game/creative.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace aimc {
namespace {

const std::vector<ItemRef> kBuilding = {
    ItemRef::ofBlock(BLOCK_STONE),
    ItemRef::ofBlock(BLOCK_COBBLE),
    ItemRef::ofBlock(BLOCK_STONE_BRICKS),
    ItemRef::ofBlock(BLOCK_BRICKS),
    ItemRef::ofBlock(BLOCK_PLANKS),
    ItemRef::ofBlock(BLOCK_OBSIDIAN),
};

const std::vector<ItemRef> kNatural = {
    ItemRef::ofBlock(BLOCK_GRASS),
    ItemRef::ofBlock(BLOCK_DIRT),
    ItemRef::ofBlock(BLOCK_SAND),
    ItemRef::ofBlock(BLOCK_GRAVEL),
    ItemRef::ofBlock(BLOCK_LOG),
    ItemRef::ofBlock(BLOCK_LEAVES),
    ItemRef::ofBlock(BLOCK_SNOW),
    ItemRef::ofBlock(BLOCK_WATER),
};

const std::vector<ItemRef> kTools = {
    ItemRef::ofItem(ItemId::FlintAndSteel),
};

const std::vector<ItemRef> kAll = {
    ItemRef::ofBlock(BLOCK_GRASS),        ItemRef::ofBlock(BLOCK_DIRT),
    ItemRef::ofBlock(BLOCK_STONE),        ItemRef::ofBlock(BLOCK_COBBLE),
    ItemRef::ofBlock(BLOCK_STONE_BRICKS), ItemRef::ofBlock(BLOCK_BRICKS),
    ItemRef::ofBlock(BLOCK_PLANKS),       ItemRef::ofBlock(BLOCK_SAND),
    ItemRef::ofBlock(BLOCK_GRAVEL),       ItemRef::ofBlock(BLOCK_LOG),
    ItemRef::ofBlock(BLOCK_LEAVES),       ItemRef::ofBlock(BLOCK_SNOW),
    ItemRef::ofBlock(BLOCK_WATER),        ItemRef::ofBlock(BLOCK_OBSIDIAN),
    ItemRef::ofItem(ItemId::FlintAndSteel),
};

std::string toLower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

const std::vector<ItemRef>& itemsForTab(CreativeTab tab) {
    switch (tab) {
    case CreativeTab::Building:
        return kBuilding;
    case CreativeTab::Natural:
        return kNatural;
    case CreativeTab::Tools:
        return kTools;
    case CreativeTab::Search:
    case CreativeTab::Count:
        break;
    }
    return kAll;
}

}  // namespace

CreativeTabInfo creativeTabInfo(CreativeTab tab) {
    switch (tab) {
    case CreativeTab::Building:
        return {"Building Blocks", ItemRef::ofBlock(BLOCK_STONE), false};
    case CreativeTab::Natural:
        return {"Natural Blocks", ItemRef::ofBlock(BLOCK_GRASS), false};
    case CreativeTab::Tools:
        return {"Tools & Utilities", ItemRef::ofItem(ItemId::FlintAndSteel), false};
    case CreativeTab::Search:
        return {"Search Items", {}, true};
    case CreativeTab::Count:
        break;
    }
    return {"Building Blocks", ItemRef::ofBlock(BLOCK_STONE), false};
}

const std::vector<ItemRef>& creativeTabItems(CreativeTab tab) { return itemsForTab(tab); }

const std::vector<ItemRef>& creativeAllItems() { return kAll; }

std::array<ItemRef, kHotbarSlots> defaultHotbar() {
    return {ItemRef::ofBlock(BLOCK_GRASS),  ItemRef::ofBlock(BLOCK_DIRT),
            ItemRef::ofBlock(BLOCK_STONE),  ItemRef::ofBlock(BLOCK_SAND),
            ItemRef::ofBlock(BLOCK_LOG),    ItemRef::ofBlock(BLOCK_LEAVES),
            ItemRef::ofBlock(BLOCK_PLANKS), ItemRef::ofBlock(BLOCK_COBBLE),
            ItemRef::ofBlock(BLOCK_WATER)};
}

bool itemMatchesQuery(const ItemRef& item, const std::string& query) {
    if (query.empty()) {
        return true;
    }
    return toLower(item.name()).find(toLower(query)) != std::string::npos;
}

std::vector<ItemRef> creativeItems(CreativeTab tab, const std::string& query) {
    std::vector<ItemRef> items;
    const std::vector<ItemRef>& source =
        (tab == CreativeTab::Search) ? kAll : itemsForTab(tab);
    items.reserve(source.size());
    for (const ItemRef& item : source) {
        if (itemMatchesQuery(item, query)) {
            items.push_back(item);
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

int firstEmptyHotbarSlot(const std::array<ItemRef, kHotbarSlots>& hotbar) {
    for (int i = 0; i < kHotbarSlots; ++i) {
        if (hotbar[static_cast<std::size_t>(i)].empty()) {
            return i;
        }
    }
    return -1;
}

int putInHotbar(std::array<ItemRef, kHotbarSlots>& hotbar, const ItemRef& item) {
    if (item.empty()) {
        return -1;
    }
    const int slot = firstEmptyHotbarSlot(hotbar);
    if (slot < 0) {
        return -1;
    }
    hotbar[static_cast<std::size_t>(slot)] = item;
    return slot;
}

void takeItem(ItemStack& cursor, const ItemRef& item) {
    if (item.empty()) {
        return;
    }
    if (cursor.ref == item) {
        const int limit = maxStackSize(item);
        cursor.count = static_cast<std::uint8_t>(
            std::min(limit, static_cast<int>(cursor.count) + 1));
    } else {
        cursor.ref = item;
        cursor.count = 1;
    }
}

void takeOneItem(ItemStack& cursor, const ItemRef& item) {
    if (item.empty()) {
        return;
    }
    if (cursor.ref == item) {
        cursor.count =
            static_cast<std::uint8_t>(std::max(0, static_cast<int>(cursor.count) - 1));
        if (cursor.count == 0) {
            cursor.clear();
        }
    } else {
        cursor.ref = item;
        cursor.count = 1;
    }
}

void clickHotbarSlot(std::array<ItemRef, kHotbarSlots>& hotbar, ItemStack& cursor, int slot) {
    if (slot < 0 || slot >= kHotbarSlots) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(slot);
    if (!cursor.empty()) {
        if (hotbar[index].empty() || hotbar[index] == cursor.ref) {
            hotbar[index] = cursor.ref;
            cursor.clear();
        } else {
            std::swap(hotbar[index], cursor.ref);
            cursor.count = static_cast<std::uint8_t>(maxStackSize(cursor.ref));
        }
    } else if (!hotbar[index].empty()) {
        cursor.ref = hotbar[index];
        cursor.count = static_cast<std::uint8_t>(maxStackSize(cursor.ref));
        hotbar[index] = {};
    }
}

}  // namespace aimc
