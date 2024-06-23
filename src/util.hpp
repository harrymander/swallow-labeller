#include <algorithm>
#include <cctype>
#include <iterator>
#include <locale>
#include <optional>
#include <string>
#include <utility>

namespace util {

template <class Comparable>
inline std::pair<Comparable *, Comparable *> minmax_pointers(Comparable *v1, Comparable *v2)
{
    return (*v1 <= *v2) ? std::make_pair(v1, v2) : std::make_pair(v2, v1);
}

template <class BidirIt, class T>
inline BidirIt binary_search_closest(BidirIt first, BidirIt last, const T& value)
{
    BidirIt found = std::lower_bound(first, last, value);
    if (found != last && found != first) {
        const auto prev = std::prev(found);
        if (value - *prev < *found - value) {
            found = prev;
        }
    }
    return found;
}

template <class T> inline T value_or_default(const std::optional<T>& opt)
{
    return opt.value_or(T{});
}

template <class T> inline bool has_value_and_equal(const std::optional<T>& opt, const T& val)
{
    return opt.has_value() && *opt == val;
}

// trim from start (in place)
inline void ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
}

// trim from end (in place)
inline void rtrim(std::string& s)
{
    s.erase(
        std::find_if(
            s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }
        ).base(),
        s.end()
    );
}

inline void trim(std::string& s)
{
    ltrim(s);
    rtrim(s);
}

inline std::string trimmed(std::string s)
{
    trim(s);
    return s;
}

}; // namespace util
