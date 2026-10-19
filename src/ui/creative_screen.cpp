#include "ui/creative_screen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "render/atlas.h"
#include "world/block.h"

namespace aimc {
namespace {

// Palette taken from the vanilla inventory textures: a light grey panel with a
// white top-left bevel, sunken item slots and dark grey labels.
constexpr ImU32 kPanelBg = IM_COL32(198, 198, 198, 255);
constexpr ImU32 kPanelLight = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kPanelDark = IM_COL32(78, 78, 78, 255);
constexpr ImU32 kSlotBg = IM_COL32(55, 55, 55, 255);
constexpr ImU32 kSlotShadow = IM_COL32(32, 32, 32, 255);
constexpr ImU32 kSlotEdge = IM_COL32(98, 98, 98, 255);
constexpr ImU32 kTextDark = IM_COL32(64, 64, 64, 255);
constexpr ImU32 kTextLight = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kTextShadow = IM_COL32(0, 0, 0, 170);
constexpr ImU32 kTabInactive = IM_COL32(148, 148, 148, 255);
constexpr ImU32 kTabHover = IM_COL32(172, 172, 172, 255);
constexpr ImU32 kTabEdge = IM_COL32(78, 78, 78, 255);
constexpr ImU32 kHoverOutline = IM_COL32(255, 255, 255, 190);
constexpr ImU32 kSelectedOutline = IM_COL32(255, 255, 255, 250);
constexpr ImU32 kDestroyMark = IM_COL32(196, 64, 64, 230);
constexpr ImU32 kNumberLabel = IM_COL32(196, 196, 196, 180);

void bevel(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1, ImU32 fill, ImU32 light, ImU32 dark,
           float thickness) {
    dl->AddRectFilled(p0, p1, fill);
    dl->AddRectFilled(p0, ImVec2(p1.x, p0.y + thickness), light);
    dl->AddRectFilled(p0, ImVec2(p0.x + thickness, p1.y), light);
    dl->AddRectFilled(ImVec2(p0.x, p1.y - thickness), p1, dark);
    dl->AddRectFilled(ImVec2(p1.x - thickness, p0.y), p1, dark);
}

void drawSlot(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1, float thickness) {
    dl->AddRectFilled(p0, p1, kSlotBg);
    dl->AddRectFilled(p0, ImVec2(p1.x, p0.y + thickness), kSlotShadow);
    dl->AddRectFilled(p0, ImVec2(p0.x + thickness, p1.y), kSlotShadow);
    dl->AddRectFilled(ImVec2(p0.x, p1.y - thickness), p1, kSlotEdge);
    dl->AddRectFilled(ImVec2(p1.x - thickness, p0.y), p1, kSlotEdge);
}

// Isometric cube icon: the vanilla inventory renders blocks as small 3D cubes
// rather than flat tiles. Top face bright, left face mid, right face dark.
void drawBlockIcon(ImDrawList* dl, const ImTextureRef& tex, const Atlas& atlas, BlockId block,
                   const ImVec2& center, float size) {
    if (block == BLOCK_AIR || size <= 0.0f) {
        return;
    }
    const BlockDef& def = blockDef(block);
    const glm::vec4 top = atlas.tileUV(def.tileTop);
    const glm::vec4 side = atlas.tileUV(def.tileSide);

    const auto uvTL = [](const glm::vec4& r) { return ImVec2(r.x, r.w); };
    const auto uvTR = [](const glm::vec4& r) { return ImVec2(r.z, r.w); };
    const auto uvBR = [](const glm::vec4& r) { return ImVec2(r.z, r.y); };
    const auto uvBL = [](const glm::vec4& r) { return ImVec2(r.x, r.y); };

    const float half = size * 0.5f;
    const float height = size * 0.92f;
    const ImVec2 c = center;

    const ImVec2 a(c.x, c.y - height * 0.5f - half);  // far corner of the top face
    const ImVec2 b(c.x + size, c.y - height * 0.5f);  // right corner
    const ImVec2 cc(c.x, c.y - height * 0.5f + half); // near corner
    const ImVec2 d(c.x - size, c.y - height * 0.5f);  // left corner
    const ImVec2 db(d.x, d.y + height);
    const ImVec2 cb(cc.x, cc.y + height);
    const ImVec2 bb(b.x, b.y + height);

    dl->AddImageQuad(tex, d, cc, cb, db, uvTL(side), uvTR(side), uvBR(side), uvBL(side),
                     IM_COL32(172, 172, 172, 255));
    dl->AddImageQuad(tex, b, cc, cb, bb, uvTL(side), uvTR(side), uvBR(side), uvBL(side),
                     IM_COL32(124, 124, 124, 255));
    dl->AddImageQuad(tex, a, b, cc, d, uvTL(top), uvTR(top), uvBR(top), uvBL(top),
                     IM_COL32(255, 255, 255, 255));
}

// Items use a flat 16x16 sprite; blocks use the isometric cube above.
void drawItemIcon(ImDrawList* dl, const ImTextureRef& tex, const Atlas& atlas,
                  const ImVec2& center, float halfSize, int tile) {
    const glm::vec4 uv = atlas.tileUV(tile);
    dl->AddImage(tex, ImVec2(center.x - halfSize, center.y - halfSize),
                 ImVec2(center.x + halfSize, center.y + halfSize), ImVec2(uv.x, uv.w),
                 ImVec2(uv.z, uv.y));
}

void drawRefIcon(ImDrawList* dl, const ImTextureRef& tex, const Atlas& atlas, const ItemRef& item,
                 const ImVec2& center, float halfSize) {
    if (item.empty()) {
        return;
    }
    if (item.isItem()) {
        drawItemIcon(dl, tex, atlas, center, halfSize, item.iconTile());
    } else {
        drawBlockIcon(dl, tex, atlas, item.block, center, halfSize);
    }
}

}  // namespace

void CreativeScreen::setOpen(bool open) {
    m_open = open;
    m_textFocus = false;
    m_cursor.clear();
}

void CreativeScreen::setTab(CreativeTab tab) {
    m_tab = tab;
    m_scrollRow = 0;
}

bool CreativeScreen::draw(const Atlas& atlas, std::array<ItemRef, kHotbarSlots>& hotbar,
                          int& selected, bool acceptInput) {
    if (!m_open) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = io.DisplaySize;
    if (display.x < 80.0f || display.y < 80.0f) {
        return true;
    }

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("##creative_inventory", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImTextureRef tex(static_cast<ImTextureID>(atlas.texture()));

    const float slotSize = std::clamp(std::floor(display.y / 20.0f), 28.0f, 46.0f);
    const float pad = std::round(slotSize * 0.30f);
    const float headerH = std::round(slotSize * 0.60f);
    const float gridW = static_cast<float>(Columns) * slotSize;
    const float gridH = static_cast<float>(Rows) * slotSize;
    const float scrollW = std::max(12.0f, std::round(slotSize * 0.34f));
    const float rowGap = std::round(slotSize * 0.26f);
    const float slotBevel = std::max(1.0f, std::round(slotSize / 18.0f));
    const float panelBevel = std::max(2.0f, std::round(slotSize / 18.0f));
    const float panelW = pad * 2.0f + gridW + 6.0f + scrollW;
    const float panelH = pad * 2.0f + headerH + rowGap * 2.0f + gridH + slotSize;
    const float panelX = std::round((display.x - panelW) * 0.5f);
    const float panelY = std::round((display.y - panelH) * 0.5f);
    const float contentX = panelX + pad;
    const float gridX = contentX;
    const float gridY = panelY + pad + headerH + rowGap;
    const float hotbarY = gridY + gridH + rowGap;
    const float scrollX = gridX + gridW + 6.0f;

    m_textFocus = false;
    bool consumed = false;

    dl->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(0, 0, 0, 130));
    bevel(dl, ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH), kPanelBg,
          kPanelLight, kPanelDark, panelBevel);

    const std::vector<ItemRef> items = creativeItems(m_tab, std::string(m_query));
    const CreativeTabInfo current = creativeTabInfo(m_tab);
    const int maxScroll = creativeMaxScrollRow(static_cast<int>(items.size()), Columns, Rows);
    m_scrollRow = std::clamp(m_scrollRow, 0, maxScroll);

    // Tab strip, overlapping the top edge of the panel like vanilla.
    const float tabW = std::round(slotSize * 0.86f);
    const float tabH = std::round(slotSize * 0.84f);
    const float tabY = panelY - tabH + 3.0f;
    float tabLeft = contentX;

    for (int i = 0; i < static_cast<int>(CreativeTab::Count); ++i) {
        const CreativeTab tab = static_cast<CreativeTab>(i);
        const CreativeTabInfo info = creativeTabInfo(tab);
        const float x = info.search ? panelX + panelW - pad - tabW : tabLeft;
        if (!info.search) {
            tabLeft += tabW + 2.0f;
        }
        const bool active = tab == m_tab;
        const ImVec2 p0(x, tabY);
        const ImVec2 p1(x + tabW, active ? panelY + 3.0f : panelY + 2.0f);

        char id[32];
        std::snprintf(id, sizeof(id), "##creative_tab%d", i);
        ImGui::SetCursorScreenPos(p0);
        ImGui::InvisibleButton(id, ImVec2(p1.x - p0.x, p1.y - p0.y));
        const bool hovered = ImGui::IsItemHovered();
        consumed = consumed || hovered;
        if (acceptInput && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            setTab(tab);
        }

        dl->AddRectFilled(p0, p1, active ? kPanelBg : (hovered ? kTabHover : kTabInactive));
        dl->AddLine(ImVec2(p0.x, p0.y), ImVec2(p1.x - 1.0f, p0.y), kPanelLight);
        dl->AddLine(ImVec2(p0.x, p0.y), ImVec2(p0.x, p1.y - 1.0f), kPanelLight);
        dl->AddLine(ImVec2(p1.x - 1.0f, p0.y), ImVec2(p1.x - 1.0f, p1.y - 1.0f), kTabEdge);
        if (!active) {
            dl->AddLine(ImVec2(p0.x, p1.y - 1.0f), ImVec2(p1.x - 1.0f, p1.y - 1.0f), kTabEdge);
        }

        if (info.search) {
            const ImVec2 c(x + tabW * 0.5f, tabY + tabH * 0.5f + 1.0f);
            const float r = std::round(slotSize * 0.15f);
            dl->AddCircle(c, r, kTextDark, 16, 2.0f);
            dl->AddLine(ImVec2(c.x + r * 0.72f, c.y + r * 0.72f),
                        ImVec2(c.x + r * 1.7f, c.y + r * 1.7f), kTextDark, 2.0f);
        } else {
            drawRefIcon(dl, tex, atlas, info.icon,
                        ImVec2(x + tabW * 0.5f, tabY + tabH * 0.5f + 1.0f),
                        slotSize * (info.icon.isItem() ? 0.38f : 0.27f));
        }

        if (hovered) {
            ImGui::SetTooltip("%s", info.name);
        }
    }

    // Header: tab title, or the search box on the search tab.
    if (current.search) {
        ImGui::SetCursorScreenPos(
            ImVec2(contentX, panelY + pad + (headerH - ImGui::GetFontSize() - 6.0f) * 0.5f));
        ImGui::SetNextItemWidth(gridW);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.93f, 0.93f, 0.93f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.97f, 0.97f, 0.97f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.31f, 0.31f, 0.31f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.14f, 0.14f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.52f, 0.52f, 0.52f, 1.0f));
        if (ImGui::InputTextWithHint("##creative_search", "Search items...", m_query,
                                     IM_ARRAYSIZE(m_query))) {
            m_scrollRow = 0;
        }
        m_textFocus = ImGui::IsItemActive();
        consumed = consumed || ImGui::IsItemHovered();
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(3);
    } else {
        dl->AddText(ImVec2(contentX, panelY + pad + (headerH - ImGui::GetFontSize()) * 0.5f),
                    kTextDark, current.name);
    }

    // Item grid.
    for (int row = 0; row < Rows; ++row) {
        for (int col = 0; col < Columns; ++col) {
            const int index = (m_scrollRow + row) * Columns + col;
            const ImVec2 p0(gridX + static_cast<float>(col) * slotSize,
                            gridY + static_cast<float>(row) * slotSize);
            const ImVec2 p1(p0.x + slotSize, p0.y + slotSize);
            drawSlot(dl, p0, p1, slotBevel);
            if (index < 0 || index >= static_cast<int>(items.size())) {
                continue;
            }
            const ItemRef item = items[static_cast<std::size_t>(index)];
            drawRefIcon(dl, tex, atlas, item,
                        ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f),
                        slotSize * (item.isItem() ? 0.37f : 0.33f));

            char id[32];
            std::snprintf(id, sizeof(id), "##creative_item%d", index);
            ImGui::SetCursorScreenPos(p0);
            ImGui::InvisibleButton(id, ImVec2(slotSize, slotSize));
            const bool hovered = ImGui::IsItemHovered();
            consumed = consumed || hovered;

            if (hovered) {
                dl->AddRect(ImVec2(p0.x + 1.0f, p0.y + 1.0f), ImVec2(p1.x - 1.0f, p1.y - 1.0f),
                            kHoverOutline, 0.0f, 0, 1.5f);
                if (m_cursor.empty()) {
                    ImGui::SetTooltip("%s", item.name());
                }
                if (acceptInput && !m_textFocus) {
                    for (int k = 0; k < kHotbarSlots; ++k) {
                        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + k))) {
                            hotbar[static_cast<std::size_t>(k)] = item;
                            selected = k;
                            break;
                        }
                    }
                }
            }

            const bool left = acceptInput && ImGui::IsItemClicked(ImGuiMouseButton_Left);
            const bool right = acceptInput && ImGui::IsItemClicked(ImGuiMouseButton_Right);
            if (left && io.KeyShift) {
                const int placed = putInHotbar(hotbar, item);
                if (placed >= 0) {
                    selected = placed;
                }
            } else if (left) {
                takeItem(m_cursor, item);
            } else if (right) {
                takeOneItem(m_cursor, item);
            }
        }
    }

    // Hotbar row.
    for (int i = 0; i < kHotbarSlots; ++i) {
        const ImVec2 p0(contentX + static_cast<float>(i) * slotSize, hotbarY);
        const ImVec2 p1(p0.x + slotSize, p0.y + slotSize);
        drawSlot(dl, p0, p1, slotBevel);

        const ItemRef item = hotbar[static_cast<std::size_t>(i)];
        if (!item.empty()) {
            drawRefIcon(dl, tex, atlas, item,
                        ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f),
                        slotSize * (item.isItem() ? 0.37f : 0.33f));
        }
        const char label[2] = {static_cast<char>('1' + i), '\0'};
        dl->AddText(ImVec2(p0.x + 2.0f, p0.y + 1.0f), kNumberLabel, label);

        char id[32];
        std::snprintf(id, sizeof(id), "##creative_hotbar%d", i);
        ImGui::SetCursorScreenPos(p0);
        ImGui::InvisibleButton(id, ImVec2(slotSize, slotSize));
        const bool hovered = ImGui::IsItemHovered();
        consumed = consumed || hovered;

        if (i == selected) {
            dl->AddRect(ImVec2(p0.x - 2.0f, p0.y - 2.0f), ImVec2(p1.x + 2.0f, p1.y + 2.0f),
                        kSelectedOutline, 0.0f, 0, 2.0f);
        }
        if (hovered && !item.empty() && m_cursor.empty()) {
            ImGui::SetTooltip("%s", item.name());
        }

        const bool clicked = acceptInput && (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                                             ImGui::IsItemClicked(ImGuiMouseButton_Right));
        if (clicked) {
            clickHotbarSlot(hotbar, m_cursor, i);
            selected = i;
        }
    }

    // Destroy slot, below the scrollbar.
    const float destroySize = std::round(slotSize * 0.55f);
    const ImVec2 d0(scrollX + (scrollW - destroySize) * 0.5f,
                    hotbarY + (slotSize - destroySize) * 0.5f);
    const ImVec2 d1(d0.x + destroySize, d0.y + destroySize);
    drawSlot(dl, d0, d1, slotBevel);
    dl->AddLine(ImVec2(d0.x + 3.0f, d0.y + 3.0f), ImVec2(d1.x - 3.0f, d1.y - 3.0f), kDestroyMark,
                2.0f);
    dl->AddLine(ImVec2(d1.x - 3.0f, d0.y + 3.0f), ImVec2(d0.x + 3.0f, d1.y - 3.0f), kDestroyMark,
                2.0f);
    ImGui::SetCursorScreenPos(d0);
    ImGui::InvisibleButton("##creative_destroy", ImVec2(destroySize, destroySize));
    {
        const bool hovered = ImGui::IsItemHovered();
        consumed = consumed || hovered;
        if (hovered) {
            dl->AddRect(d0, d1, IM_COL32(255, 160, 160, 200), 0.0f, 0, 1.5f);
            ImGui::SetTooltip("Destroy item");
        }
        if (acceptInput && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_cursor.clear();
        }
    }

    // Scrollbar.
    const int totalRows = creativeRowCount(static_cast<int>(items.size()), Columns);
    float thumbY = gridY;
    float thumbH = gridH;
    if (maxScroll > 0) {
        thumbH = std::max(slotSize * 0.6f,
                          gridH * static_cast<float>(Rows) / static_cast<float>(totalRows));
        thumbY = gridY + (gridH - thumbH) *
                             (static_cast<float>(m_scrollRow) / static_cast<float>(maxScroll));
    }
    if (maxScroll > 0) {
        dl->AddRectFilled(ImVec2(scrollX, gridY), ImVec2(scrollX + scrollW, gridY + gridH),
                          IM_COL32(96, 96, 96, 255));
        dl->AddRectFilled(ImVec2(scrollX + 1.0f, gridY + 1.0f),
                          ImVec2(scrollX + scrollW - 1.0f, gridY + gridH - 1.0f),
                          IM_COL32(52, 52, 52, 255));
        bevel(dl, ImVec2(scrollX + 1.0f, thumbY + 1.0f),
              ImVec2(scrollX + scrollW - 1.0f, thumbY + thumbH - 1.0f), kPanelBg, kPanelLight,
              kPanelDark, 1.5f);
        ImGui::SetCursorScreenPos(ImVec2(scrollX, thumbY));
        ImGui::InvisibleButton("##creative_scroll", ImVec2(scrollW, thumbH));
        consumed = consumed || ImGui::IsItemHovered();
        if (acceptInput && ImGui::IsItemActive()) {
            const float travel = gridH - thumbH;
            if (travel > 0.0f) {
                const float t =
                    std::clamp((io.MousePos.y - gridY - thumbH * 0.5f) / travel, 0.0f, 1.0f);
                m_scrollRow = static_cast<int>(std::lround(t * static_cast<float>(maxScroll)));
            }
        }
    }

    if (acceptInput && maxScroll > 0 && !m_textFocus &&
        ImGui::IsMouseHoveringRect(ImVec2(gridX, gridY), ImVec2(gridX + gridW, gridY + gridH))) {
        const float wheel = io.MouseWheel;
        if (wheel != 0.0f) {
            const int step = wheel > 0.0f ? -1 : 1;
            m_scrollRow = std::clamp(m_scrollRow + step, 0, maxScroll);
        }
    }

    // A click that is not on any widget drops whatever the cursor carries.
    if (acceptInput && !consumed && !m_cursor.empty() &&
        (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        m_cursor.clear();
    }

    if (!m_cursor.empty()) {
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        const ImVec2 mouse = io.MousePos;
        drawRefIcon(fg, tex, atlas, m_cursor.ref, mouse, slotSize * 0.38f);
        if (m_cursor.count > 1) {
            char count[8];
            std::snprintf(count, sizeof(count), "%d", static_cast<int>(m_cursor.count));
            fg->AddText(ImVec2(mouse.x + 6.0f, mouse.y + 2.0f), kTextShadow, count);
            fg->AddText(ImVec2(mouse.x + 5.0f, mouse.y + 1.0f), kTextLight, count);
        }
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    return true;
}

}  // namespace aimc
