#include "integer-range-input.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cstddef>

namespace recap::labeller::gui::widgets {

template class UnsignedIntegerRangeInput<std::size_t>;
template class UnsignedIntegerRangeInput<unsigned int>;

template <std::unsigned_integral T>
void UnsignedIntegerRangeInput<T>::draw(const char *id, float width)
{
    if (width != 0) {
        ImGui::SetNextItemWidth(width);
    }
    const bool updated = ImGui::InputText(id, &m_input);
    ImGui::SetItemTooltip("Input numbers or ranges separated by commas, e.g. \"1, 3-12, 15-\"");
    if (updated) {
        update();
    }
}

template <std::unsigned_integral T> bool UnsignedIntegerRangeInput<T>::contains(T val) const
{
    // m_items is sorted so can return false early if val < start. A linear search is not
    // necessarily efficient but since the vector of items is expected to be fairly small it doesn't
    // really matter.
    for (const auto& [start, end] : m_items) {
        if (val < start) {
            return false;
        }
        if (val <= end) {
            return true;
        }
    }
    return false;
}

template <std::unsigned_integral T> void UnsignedIntegerRangeInput<T>::update()
{
    std::vector<IntegerRange<T>> items;
    if (parse_integer_range_list<T>(m_input, items)) {
        m_items = std::move(items);
        m_error = false;
    } else {
        m_items.clear();
        m_error = true;
    }
}

}; // namespace recap::labeller::gui::widgets
