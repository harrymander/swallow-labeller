#ifndef RECAP_LABELLER_UTIL_CHRONO_UTIL_HPP_INCLUDE
#define RECAP_LABELLER_UTIL_CHRONO_UTIL_HPP_INCLUDE

#ifdef USE_HOWARDHINNANT_DATE
#include <date/date.h>
#define DATETIME_PARSE_FUNC date::parse
#else
#include <chrono>
#define DATETIME_PARSE_FUNC std::chrono::parse
#endif

#include <utility>

namespace recap::labeller::chrono_util {

template <typename... Args> inline auto parse(Args&&...args)
{
    return DATETIME_PARSE_FUNC(std::forward<Args>(args)...);
}

}; // namespace recap::labeller::chrono_util

#undef DATETIME_PARSE_FUNC

#endif // RECAP_LABELLER_UTIL_CHRONO_UTIL_HPP_INCLUDE
