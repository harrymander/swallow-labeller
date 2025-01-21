#ifndef RECAP_LABELLER_UTIL_STRUTIL_HPP_INCLUDE
#define RECAP_LABELLER_UTIL_STRUTIL_HPP_INCLUDE

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace recap::labeller::strutil {

/**
 * Return a view over `s` with leading and trailing whitespace removed.
 */
constexpr std::string_view trimmed(std::string_view s)
{
    constexpr auto not_space = [](auto c) { return !std::isspace(static_cast<int>(c)); };
    const auto start = std::find_if(s.begin(), s.end(), not_space);
    if (start == s.end()) {
        return {};
    }

    return {start, std::find_if(s.rbegin(), s.rend(), not_space).base()};
}

/**
 * Remove leading and trailing whitespace from `s`
 */
inline void trim(std::string& s)
{
    s = trimmed(s);
}

}; // namespace recap::labeller::strutil

#endif // RECAP_LABELLER_UTIL_STRUTIL_HPP_INCLUDE
