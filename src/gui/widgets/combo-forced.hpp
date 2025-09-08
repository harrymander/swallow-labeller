#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_HPP
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_HPP

#include <imgui.h>

namespace recap::labeller::gui::widgets {

/**
 * Same as ImGui::BeginCombo, except that if `force` is true, always open the combo dropdown (even
 * if not clicked). Only call ImGui::EndCombo if this function returns true.
 */
bool BeginComboForced(
    const char *label, const char *preview_value, bool force, ImGuiComboFlags flags = 0
);

}; // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_HPP
