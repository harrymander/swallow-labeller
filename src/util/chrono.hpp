#ifndef RECAP_LABELLER_UTIL_CHRONO_HPP_INCLUDE
#define RECAP_LABELLER_UTIL_CHRONO_HPP_INCLUDE

/**
 * C++20 std::chrono support.
 *
 * GCC still doesn't support all C++20 chrono features. So use https://github.com/HowardHinnant/date
 * instead. The date library has a near-equivalent API to C++20 std::chrono (the C++20 API was based
 * off this library).
 */

#ifdef USE_HOWARDHINNANT_DATE
#include <date/date.h>
#define CHRONO_NAMESPACE date
#else
#include <chrono>
#define CHRONO_NAMESPACE std::chrono
#endif

namespace recap::labeller {

namespace chrono = CHRONO_NAMESPACE;

}; // namespace recap::labeller

#undef CHRONO_NAMESPACE

#endif // RECAP_LABELLER_UTIL_CHRONO_HPP_INCLUDE
