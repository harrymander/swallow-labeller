#ifndef RECAP_LABELLER_UTIL_CHRONO_UTIL_HPP_INCLUDE
#define RECAP_LABELLER_UTIL_CHRONO_UTIL_HPP_INCLUDE

#include <utility>
#ifdef USE_HOWARDHINNANT_DATE
#include <date/date.h>
#else
#include <chrono>
#endif

namespace recap::labeller::chrono_util {

template <class Fmt, class Parsable> inline auto parse(Fmt&& fmt, Parsable&& parsable)
{
#ifdef USE_HOWARDHINNANT_DATE
    return date::parse
#else
    return std::chrono::parse
#endif
        (std::forward<Fmt>(fmt), std::forward<Parsable>(parsable));
}

}; // namespace recap::labeller::chrono_util

#endif // RECAP_LABELLER_UTIL_CHRONO_UTIL_HPP_INCLUDE
