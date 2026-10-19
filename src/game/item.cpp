#include "game/item.h"

#include "world/block.h"

namespace aimc {
namespace {

const ItemDef kItems[] = {
    {"Air", 15, 0},                    // None: never shown
    {"Flint and Steel", 16, 1},        // FlintAndSteel
};

}  // namespace

const ItemDef& itemDef(ItemId id) {
    unsigned int i = static_cast<unsigned int>(id);
    if (i >= static_cast<unsigned int>(ItemId::Count)) {
        i = 0;
    }
    return kItems[i];
}

const char* ItemRef::name() const {
    if (isItem()) {
        return itemDef(item).name;
    }
    return blockDef(block).name;
}

int ItemRef::iconTile() const {
    if (isItem()) {
        return itemDef(item).iconTile;
    }
    return blockDef(block).tileSide;
}

int maxStackSize(const ItemRef& ref) {
    return ref.isItem() ? static_cast<int>(itemDef(ref.item).maxStack) : kMaxBlockStack;
}

}  // namespace aimc
