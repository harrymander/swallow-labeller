#ifndef RECAP_LABELLER_GUI_WIDGETS_COLOR_SCHEME_SELECTOR_HPP_INCLUDE
#define RECAP_LABELLER_GUI_WIDGETS_COLOR_SCHEME_SELECTOR_HPP_INCLUDE

#include <imgui.h>

namespace recap::labeller::gui::widgets {

enum ColorScheme : int {
    Dark = 0,
    Light,
    Classic,
};

class ColorSchemeSelector {
public:
    explicit ColorSchemeSelector(ColorScheme scheme = ColorScheme::Dark) : m_color_scheme(scheme) {}

    void draw(const char *id = "Color scheme")
    {
        if (ImGui::Combo(id, reinterpret_cast<int *>(&m_color_scheme), "Dark\0Light\0Classic\0")) {
            switch (m_color_scheme) {
            case ColorScheme::Dark:
                ImGui::StyleColorsDark();
                break;
            case ColorScheme::Light:
                ImGui::StyleColorsLight();
                break;
            case ColorScheme::Classic:
                ImGui::StyleColorsClassic();
                break;
            }
        }
    }

private:
    ColorScheme m_color_scheme;
};

}; // namespace recap::labeller::gui::widgets

#endif // RECAP_LABELLER_GUI_WIDGETS_COLOR_SCHEME_SELECTOR_HPP_INCLUDE
