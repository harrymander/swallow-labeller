#ifndef RECAP_OPTUTIL_HPP_INCLUDE
#define RECAP_OPTUTIL_HPP_INCLUDE

#include <optional>

namespace recap::optutil {

template <class T> inline T value_or_default(const std::optional<T>& opt)
{
    return opt.value_or(T{});
}

template <class T> inline bool value_and_equal(const std::optional<T>& opt, const T& val)
{
    return opt.has_value() && *opt == val;
}

}; // namespace recap::optutil

#endif // RECAP_OPTUTIL_HPP_INCLUDE
