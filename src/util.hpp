#include <algorithm>
#include <iterator>
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

}; // namespace util
