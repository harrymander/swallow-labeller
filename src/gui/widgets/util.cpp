#include "util.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <initializer_list>
#include <variant>

namespace recap::labeller::gui::widgets {

namespace {

void push_color(ImGuiCol var, const ImColor& color)
{
    ImGui::PushStyleColor(var, static_cast<ImU32>(color));
}

}; // namespace

ScopedImStyle::ScopedImStyle(ImGuiStyleVar style_var, const ImVec2& vec) noexcept
{
    push({style_var, vec});
}

ScopedImStyle::ScopedImStyle(ImGuiStyleVar style_var, float float_val) noexcept
{
    push({style_var, float_val});
}

ScopedImStyle::ScopedImStyle(std::initializer_list<ImStyle> styles) :
    m_count(static_cast<decltype(m_count)>(styles.size()))
{
    for (const auto& style : styles) {
        push(style);
    }
}

void ScopedImStyle::push(const ImStyle& style) noexcept
{
    if (std::holds_alternative<float>(style.val)) {
        ImGui::PushStyleVar(style.var, std::get<float>(style.val));
    } else {
        ImGui::PushStyleVar(style.var, std::get<ImVec2>(style.val));
    }
}

ScopedImStyle::~ScopedImStyle() noexcept
{
    ImGui::PopStyleVar(m_count);
}

ScopedImColor::ScopedImColor(ImGuiCol style_var, const ImColor& color) noexcept
{
    push_color(style_var, color);
}

ScopedImColor::ScopedImColor(std::initializer_list<ImStyleColor> colors) :
    m_count(static_cast<decltype(m_count)>(colors.size()))
{
    for (const auto& color : colors) {
        push_color(color.var, color.color);
    }
}

ScopedImColor::~ScopedImColor() noexcept
{
    ImGui::PopStyleColor(m_count);
}

ScopedImID::ScopedImID(const char *str_id) noexcept
{
    ImGui::PushID(str_id);
}

ScopedImID::ScopedImID(const char *str_id_begin, const char *str_id_end) noexcept
{
    ImGui::PushID(str_id_begin, str_id_end);
}

ScopedImID::ScopedImID(const void *ptr_id) noexcept
{
    ImGui::PushID(ptr_id);
}

ScopedImID::ScopedImID(int int_id) noexcept
{
    ImGui::PushID(int_id);
}

ScopedImID::~ScopedImID() noexcept
{
    ImGui::PopID();
}

bool ButtonRed(const char *label, const ImVec2& size)
{
    constexpr ImU32 Color = 0x993D3DFF;
    constexpr ImU32 ColorHovered = 0xB33636FF;
    constexpr ImU32 ColorActive = 0xCC2929FF;
    ScopedImColor color_scope = {
        {ImGuiCol_Button, Color},
        {ImGuiCol_ButtonHovered, ColorHovered},
        {ImGuiCol_ButtonActive, ColorActive},
    };
    return ImGui::Button(label, size);
}

bool item_disabled()
{
    return ImGui::GetItemFlags() & ImGuiItemFlags_Disabled;
}

}; // namespace recap::labeller::gui::widgets
