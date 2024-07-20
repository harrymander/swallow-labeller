#ifndef RECAP_LABELLER_UTIL_OPTUTIL_HPP_INCLUDE
#define RECAP_LABELLER_UTIL_OPTUTIL_HPP_INCLUDE

#include <optional>

namespace recap::labeller::optutil {

template <class T> inline T value_or_default(const std::optional<T>& opt)
{
    return opt.value_or(T{});
}

template <class T> inline bool has_value_and_equal(const std::optional<T>& opt, const T& val)
{
    return opt.has_value() && *opt == val;
}

/**
 * Returns default_value if opt is nullopt, else return the result of applying map_func on the
 * contained value.
 */
template <class T, class Map, class R>
inline R map_or(const std::optional<T>& opt, Map map_func, const R& default_value)
{
    return opt.has_value() ? map_func(*opt) : default_value;
}

/**
 * Returns default_value if ptr is nullptr, else return the result of applying map_func on the
 * dereferenced ptr.
 */
template <class T, class Map, class R>
inline R map_or(const T *ptr, Map map_func, const R& default_value)
{
    return ptr == nullptr ? default_value : map_func(*ptr);
}

}; // namespace recap::labeller::optutil

#endif // RECAP_LABELLER_UTIL_OPTUTIL_HPP_INCLUDE
