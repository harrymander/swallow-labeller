#ifndef RECAP_LABELLER_IMGUI_UTIL_HPP_INCLUDE
#define RECAP_LABELLER_IMGUI_UTIL_HPP_INCLUDE

#include <imgui.h>

#include <initializer_list>
#include <variant>

namespace recap::imgui_util {

/**
 * RAII wrappers for various ImGui functions that require pushing contexts and then popping them.
 *
 * Don't forget to give objects a variable name when declaring, otherwise destructor will be called
 * immediately after construction!
 */

struct ImStyle {
    ImGuiStyleVar var;
    std::variant<ImVec2, float> val;
};

// Wraps ImGui::PushStyleVar and ImGui::PopStyleVar
class ScopedImStyle {
public:
    ScopedImStyle(ImGuiStyleVar style_var, const ImVec2& vec) noexcept;
    ScopedImStyle(ImGuiStyleVar style_var, float float_val) noexcept;
    ScopedImStyle(std::initializer_list<ImStyle> styles);

    ~ScopedImStyle() noexcept;

    ScopedImStyle(const ScopedImStyle&) = delete;
    ScopedImStyle(ScopedImStyle&&) = delete;
    ScopedImStyle& operator=(const ScopedImStyle&) = delete;
    ScopedImStyle& operator=(ScopedImStyle&&) = delete;

private:
    static void push(const ImStyle&) noexcept;

    int m_count = 1;
};

struct ImStyleColor {
    ImGuiCol var;
    ImColor color;
};

// Wraps ImGui::PushStyleColor and ImGui::PopStyleColor
class ScopedImColor {
public:
    ScopedImColor(ImGuiCol style_var, const ImColor& color) noexcept;
    ScopedImColor(std::initializer_list<ImStyleColor> color);

    ~ScopedImColor() noexcept;

    ScopedImColor(const ScopedImColor&) = delete;
    ScopedImColor(ScopedImColor&&) = delete;
    ScopedImColor& operator=(const ScopedImColor&) = delete;
    ScopedImColor& operator=(ScopedImColor&&) = delete;

private:
    int m_count = 1;
};

// Wraps ImGui::PushID and ImGui::PopID
class ScopedImID {
public:
    explicit ScopedImID(const char *str_id) noexcept;
    ScopedImID(const char *str_id_begin, const char *str_id_end) noexcept;
    explicit ScopedImID(const void *ptr_id) noexcept;
    explicit ScopedImID(int int_id) noexcept;

    ~ScopedImID() noexcept;

    ScopedImID(const ScopedImID&) = delete;
    ScopedImID(ScopedImID&&) = delete;
    ScopedImID& operator=(const ScopedImID&) = delete;
    ScopedImID& operator=(ScopedImID&&) = delete;
};

bool ButtonRed(const char *label, const ImVec2& size = {0, 0});

[[nodiscard]] bool item_disabled();

}; // namespace recap::imgui_util

#endif // RECAP_LABELLER_IMGUI_UTIL_HPP_INCLUDE
