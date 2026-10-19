#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "game/item.h"
#include "world/block.h"

namespace aimc {

// Content tabs, loosely following the vanilla order. Tabs are filled from the
// blocks and items this game actually has; empty vanilla tabs are not shown.
enum class CreativeTab : int {
    Building,
    Natural,
    Tools,
    Search,
    Count,
};

struct CreativeTabInfo {
    const char* name;
    ItemRef icon;
    bool search;
};

CreativeTabInfo creativeTabInfo(CreativeTab tab);

// Fixed per-tab ordering, like vanilla's registration order. Search is empty.
const std::vector<ItemRef>& creativeTabItems(CreativeTab tab);

// Every placeable block plus every item, in menu order.
const std::vector<ItemRef>& creativeAllItems();

std::array<ItemRef, kHotbarSlots> defaultHotbar();

bool itemMatchesQuery(const ItemRef& item, const std::string& query);

// Items shown for a tab, optionally filtered by the search query.
std::vector<ItemRef> creativeItems(CreativeTab tab, const std::string& query);

int creativeRowCount(int itemCount, int columns);
int creativeMaxScrollRow(int itemCount, int columns, int visibleRows);

int firstEmptyHotbarSlot(const std::array<ItemRef, kHotbarSlots>& hotbar);

// Puts an item in the first empty slot. Returns the slot index, or -1 when the
// hotbar is full.
int putInHotbar(std::array<ItemRef, kHotbarSlots>& hotbar, const ItemRef& item);

// Click rules, modelled on the vanilla creative inventory.
void takeItem(ItemStack& cursor, const ItemRef& item);     // left click: replace, or add one
void takeOneItem(ItemStack& cursor, const ItemRef& item);  // right click: one, or put one back
void clickHotbarSlot(std::array<ItemRef, kHotbarSlots>& hotbar, ItemStack& cursor, int slot);

}  // namespace aimc
