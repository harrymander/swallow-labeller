#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETSENUM_RADIO_BUTTON_HPP
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETSENUM_RADIO_BUTTON_HPP

#include "gui/widgets/util.hpp"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <type_traits>

namespace recap::labeller::gui::widgets {

inline bool colored_radio_button(const char *label, bool selected, const ImColor& base_color)
{
    float hue;
    float sat;
    float val;
    ImGui::ColorConvertRGBtoHSV(
        base_color.Value.x, base_color.Value.y, base_color.Value.z, hue, sat, val
    );
    const auto check_color = ImColor::HSV(hue, sat, val + 0.3F);
    const auto bg_color = ImColor::HSV(hue, sat, val - 0.4F);
    const auto hover_color = ImColor::HSV(hue, sat, val - 0.3F);
    const auto active_color = ImColor::HSV(hue, sat, val - 0.25F);
    widgets::ScopedImColor color_scope{
        {ImGuiCol_FrameBg, bg_color},
        {ImGuiCol_FrameBgHovered, hover_color},
        {ImGuiCol_FrameBgActive, active_color},
        {ImGuiCol_CheckMark, check_color},
    };

    return ImGui::RadioButton(label, selected);
}

template <class T> struct RadioButtonField;

template <class T, class ConstIt>
inline bool
enum_radio_buttons(const char *id, T& value, ConstIt begin, ConstIt end, bool horizontal = false)
{
    ScopedImID scoped_id(id);
    bool changed = false;
    for (auto it = begin; it != end; it++) {
        if (it != begin && horizontal) {
            ImGui::SameLine();
        }
        const RadioButtonField<T>& field = *it;
        const bool enabled = field.value == value;
        const bool pressed = field.color.has_value() ?
            colored_radio_button(field.label, enabled, *field.color) :
            ImGui::RadioButton(field.label, enabled);
        if (!enabled
            && (pressed
                || (field.key != ImGuiKey_None && !item_disabled() && global_shortcut(field.key))))
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

    constexpr RadioButtonField(
        const char *label,
        T value,
        ImGuiKey key = ImGuiKey_None,
        std::optional<ImColor> color = std::nullopt
    ) :
        label(label), value(value), key(key), color(color)
    {}

    const char *label;
    T value;
    ImGuiKey key;
    std::optional<ImColor> color;
};

template <class T, class Container>
inline bool
enum_radio_buttons(const char *id, T& value, const Container& options, bool horizontal = false)
{
    return enum_radio_buttons(id, value, options.begin(), options.end(), horizontal);
}

}; // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETSENUM_RADIO_BUTTON_HPP
