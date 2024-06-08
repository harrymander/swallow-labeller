#include <algorithm>
#include <iterator>

namespace util {

template <class BidirIt, class T>
BidirIt binary_search_closest(BidirIt first, BidirIt last, const T& value)
{
    BidirIt found = std::lower_bound(first, last, value);
    if (found != last && found != first) {
        const auto prev = std::prev(found);
        if (value - *prev < *found - value)
            found = prev;
    }
    return found;
}

}; // namespace util
