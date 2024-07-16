#ifndef RECAP_LABELLER_GUI_WIDGETS_RADIO_BUTTON_ENUM_HPP_INCLUDE
#define RECAP_LABELLER_GUI_WIDGETS_RADIO_BUTTON_ENUM_HPP_INCLUDE

#include "gui/widgets/util.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <initializer_list>
#include <type_traits>

namespace recap::labeller::gui::widgets {

template <class T> struct RadioButtonField;

template <class T, class ConstIt>
bool radio_button_enums(
    const char *id, T& value, ConstIt begin, ConstIt end, bool horizontal = false
)
{
    ScopedImID scoped_id(id);
    bool changed = false;
    for (auto it = begin; it != end; it++) {
        if (it != begin && horizontal) {
            ImGui::SameLine();
        }
        const RadioButtonField<T>& field = *it;
        const bool enabled = field.value == value;
        const bool pressed = ImGui::RadioButton(field.label, enabled);
        if (!enabled
            && (pressed
                || (field.key != ImGuiKey_None && !item_disabled() && ImGui::Shortcut(field.key))))
        {
            value = field.value;
            changed = true;
            spdlog::debug("Radio buttons {}: changed to '{}'", id, field.label);
        }
    }

    return changed;
}

template <class T> struct RadioButtonField {
    static_assert(std::is_enum_v<T>, "T must be an enum");

    RadioButtonField(const char *label, T value, ImGuiKey key = ImGuiKey_None) :
        label(label), value(value), key(key)
    {}

    const char *label;
    T value;
    ImGuiKey key;
};

template <class T, class Container>
bool radio_button_enums(const char *id, T& value, const Container& options, bool horizontal = false)
{
    return radio_button_enums(id, value, options.begin(), options.end(), horizontal);
}

}; // namespace recap::labeller::gui::widgets

#endif // RECAP_LABELLER_GUI_WIDGETS_RADIO_BUTTON_ENUM_HPP_INCLUDE
