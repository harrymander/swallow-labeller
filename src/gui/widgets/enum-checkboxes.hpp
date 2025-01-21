#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_ENUM_CHECKBOXES_HPP
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_ENUM_CHECKBOXES_HPP

#include "gui/widgets/enum-utils.hpp"
#include "gui/widgets/util.hpp"
#include "imgui.h"

#include <magic_enum.hpp>
#include <magic_enum_containers.hpp>

namespace recap::labeller::gui::widgets {

template <typename Enum> using EnumCheckboxValues = magic_enum::containers::array<Enum, bool>;

template <typename Enum>
inline bool enum_checkboxes(
    const char *id,
    const EnumLabels<Enum>& labels,
    EnumCheckboxValues<Enum>& selected,
    bool horizontal = true
)
{
    ScopedImID scoped_id(id);
    bool changed = false;

    for (std::size_t i = 0; i < labels.size(); i++) {
        const auto enum_i = magic_enum::enum_value<Enum>(i);
        if (horizontal && i > 0) {
            ImGui::SameLine();
        }
        if (ImGui::Checkbox(labels[enum_i], &selected[enum_i])) {
            changed = true;
        }
    }

    return changed;
}

}; // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_ENUM_CHECKBOXES_HPP
