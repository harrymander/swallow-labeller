#include "integer-range-input.hpp"

#include "util/strutil.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <charconv>
#include <string_view>
#include <system_error>
#include <tuple>

namespace recap::labeller::gui::widgets {

namespace {

bool parse_int(std::string_view s, unsigned int& val)
{
    s = strutil::trimmed(s);
    if (s.empty()) {
        return false;
    }
    const char *const last = s.data() + s.size();
    const auto res = std::from_chars(s.data(), last, val);
    return res.ec == std::errc() && res.ptr == last;
}

}; // namespace

bool IntegerRangeInput::parse_pair(std::string_view str, Pair& pair)
{
    const auto dash_pos = str.find('-');
    if (dash_pos == std::string_view::npos) {
        if (!parse_int(str, pair.first)) {
            return false;
        }
        pair.second = pair.first;
    }

    if (!(parse_int(str.substr(0, dash_pos), pair.first)
          && parse_int(str.substr(dash_pos + 1), pair.second)))
    {
        return false;
    }

    std::tie(pair.first, pair.second) = std::minmax(pair.first, pair.second);
    return true;
}

void IntegerRangeInput::draw(const char *id)
{
    const bool updated = ImGui::InputText(id, &m_input);
    ImGui::SetItemTooltip("Input numbers or ranges separated by commas, e.g. \"1,3-12\"");
    if (updated) {
        update();
    }
}

bool IntegerRangeInput::contains(unsigned int val) const
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

void IntegerRangeInput::update()
{
    m_error = true;
    m_items.clear();
    std::vector<Pair> items;

    std::size_t start = 0;
    while (true) {
        const std::size_t end = m_input.find(',', start);
        const auto substr = strutil::trimmed(std::string_view(m_input).substr(start, end - start));
        if (substr.empty()) {
            // Allow a single trailing comma, otherwise error
            if (end == std::string::npos) {
                break;
            }
            return;
        }

        Pair pair;
        if (!parse_pair(substr, pair)) {
            return;
        }
        items.push_back(pair);

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
