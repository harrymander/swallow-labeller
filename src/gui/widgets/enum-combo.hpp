#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_ENUM_COMBO_HPP
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_ENUM_COMBO_HPP

#include "gui/widgets/enum-utils.hpp"

#include <imgui.h>
#include <magic_enum.hpp>

#include <cstddef>

namespace recap::labeller::gui::widgets {

template <typename Enum>
inline bool enum_combo(const char *label, const EnumLabels<Enum>& labels, Enum& current)
{
    bool changed = false;
    if (ImGui::BeginCombo(label, labels[current])) {
        for (std::size_t i = 0; i < labels.size(); i++) {
            const auto enum_i = magic_enum::enum_value<Enum>(i);
            bool selected = current == enum_i;
            bool selected_before = selected;
            ImGui::Selectable(labels[enum_i], &selected);
            if (selected_before != selected) {
                changed = true;
            }
            if (selected) {
                current = enum_i;
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

} // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_ENUM_COMBO_HPP
