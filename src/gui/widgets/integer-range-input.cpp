#include "integer-range-input.hpp"

#include "util/strutil.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <charconv>
#include <concepts>
#include <cstddef>
#include <limits>
#include <string_view>
#include <system_error>

namespace recap::labeller::gui::widgets {

template class UnsignedIntegerRangeInput<std::size_t>;
template class UnsignedIntegerRangeInput<unsigned int>;

namespace {

template <std::unsigned_integral T> bool parse_int(std::string_view s, T& val)
{
    const char *const last = s.data() + s.size();
    const auto res = std::from_chars(s.data(), last, val);
    return res.ec == std::errc() && res.ptr == last;
}

}; // namespace

template <std::unsigned_integral T>
bool UnsignedIntegerRangeInput<T>::parse_range(std::string_view str, Range& range)
{
    const auto dash_pos = str.find('-');

    // No dash: expect just a single number
    if (dash_pos == std::string_view::npos) {
        if (!parse_int(str, range.first)) {
            return false;
        }
        range.second = range.first;
        return true;
    }

    // "-second": rejected since it could be confused for a negative number
    const auto first = strutil::trimmed(str.substr(0, dash_pos));
    if (first.empty()) {
        return false;
    }

    const auto second = strutil::trimmed(str.substr(dash_pos + 1));
    if (second.empty()) { // "first-": admit all numbers >= first
        if (!parse_int(first, range.first)) {
            return false;
        }
        range.second = std::numeric_limits<T>::max();
    } else { // "first-second": admit all numbers in [first, second]
        if (!(parse_int(first, range.first) && parse_int(second, range.second))) {
            return false;
        }
        if (range.second < range.first) {
            std::swap(range.first, range.second);
        }
    }

    return true;
}

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
    m_error = true;
    m_items.clear();
    std::vector<Range> items;

    std::size_t start = 0;
    while (true) {
        const std::size_t end = m_input.find(',', start);
        const auto substr = strutil::trimmed(m_input.substr(start, end - start));
        if (substr.empty()) {
            // Allow a single trailing comma, otherwise error
            if (end == std::string::npos) {
                break;
            }
            return;
        }

        Range range;
        if (!parse_range(substr, range)) {
            return;
        }
        items.push_back(range);

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    std::sort(items.begin(), items.end());
    m_items = std::move(items);
    m_error = false;
}

}; // namespace recap::labeller::gui::widgets
