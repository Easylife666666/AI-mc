#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "game/creative.h"

namespace aimc {

class Atlas;

// Vanilla-style creative inventory: a tab strip, a 9x5 item grid with a
// scrollbar and search box, the hotbar row and a destroy slot. Rendered with
// ImGui inside the existing UI frame; owns the stack carried on the cursor.
class CreativeScreen {
public:
    static constexpr int Columns = 9;
    static constexpr int Rows = 5;

    bool open() const { return m_open; }
    void setOpen(bool open);
    void close() { setOpen(false); }
    void toggle() { setOpen(!m_open); }

    // True while the search box owns the keyboard (so "e" types instead of
    // closing the screen).
    bool capturesText() const { return m_textFocus; }

    void setTab(CreativeTab tab);
    CreativeTab tab() const { return m_tab; }
    const ItemStack& cursor() const { return m_cursor; }

    // Draws the screen. Mutates the hotbar (blocks dropped there) and selected
    // (the slot the player places from). Returns true while open.
    bool draw(const Atlas& atlas, std::array<ItemRef, kHotbarSlots>& hotbar, int& selected,
              bool acceptInput);

private:
    bool m_open = false;
    bool m_textFocus = false;
    CreativeTab m_tab = CreativeTab::Building;
    char m_query[64] = {};
    int m_scrollRow = 0;
    ItemStack m_cursor;
};

}  // namespace aimc
