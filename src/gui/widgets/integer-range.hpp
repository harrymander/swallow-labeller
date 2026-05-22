#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_INTEGER_RANGE
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_INTEGER_RANGE

#include "util/strutil.hpp"

#include <algorithm>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace recap::labeller::gui::widgets {

template <std::unsigned_integral T> using IntegerRange = std::pair<T, T>;

namespace detail {

template <std::unsigned_integral T> bool parse_int(std::string_view s, T& val)
{
    const char *const last = s.data() + s.size();
    const auto res = std::from_chars(s.data(), last, val);
    return res.ec == std::errc() && res.ptr == last;
}

} // namespace detail

/**
 * Parse a single token like "5", "3-7", or "10-" into `range`.
 *
 * For "N-M" with M < N the bounds are swapped. A leading dash ("-N") is
 * rejected since it could be mistaken for a negative number. The input is
 * expected to have leading/trailing whitespace already removed; internal
 * whitespace around the dash is tolerated.
 *
 * Returns false on any parse error.
 */
template <std::unsigned_integral T>
bool parse_integer_range(std::string_view str, IntegerRange<T>& range)
{
    const auto dash_pos = str.find('-');

    // No dash: expect just a single number
    if (dash_pos == std::string_view::npos) {
        if (!detail::parse_int(str, range.first)) {
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
        if (!detail::parse_int(first, range.first)) {
            return false;
        }
        range.second = std::numeric_limits<T>::max();
    } else { // "first-second": admit all numbers in [first, second]
        if (!(detail::parse_int(first, range.first) && detail::parse_int(second, range.second))) {
            return false;
        }
        if (range.second < range.first) {
            std::swap(range.first, range.second);
        }
    }

    return true;
}

/**
 * Parse a comma-separated list of integer ranges like "1, 3-12, 15-".
 *
 * On success `ranges` is replaced with the parsed entries sorted ascending.
 * An empty or whitespace-only input parses successfully to an empty vector,
 * and a single trailing comma is permitted. Any malformed token causes the
 * entire parse to fail and `ranges` to be left unchanged.
 */
template <std::unsigned_integral T>
bool parse_integer_range_list(std::string_view str, std::vector<IntegerRange<T>>& ranges)
{
    std::vector<IntegerRange<T>> items;

    std::size_t start = 0;
    while (true) {
        const std::size_t end = str.find(',', start);
        const auto substr = strutil::trimmed(str.substr(start, end - start));
        if (substr.empty()) {
            // Allow a single trailing comma, otherwise error
            if (end == std::string_view::npos) {
                break;
            }
            return false;
        }

        IntegerRange<T> range;
        if (!parse_integer_range(substr, range)) {
            return false;
        }
        items.push_back(range);

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    std::sort(items.begin(), items.end());
    ranges = std::move(items);
    return true;
}

} // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_INTEGER_RANGE
