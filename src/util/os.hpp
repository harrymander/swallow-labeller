#ifndef RECAP_LABELLER_UTIL_OS_HPP_INCLUDE
#define RECAP_LABELLER_UTIL_OS_HPP_INCLUDE

#include <optional>
#include <string>

namespace recap::labeller::os {

// Not thread safe
std::optional<std::string> getenv(const char *name);

}; // namespace recap::labeller::os

#endif // RECAP_LABELLER_UTIL_OS_HPP_INCLUDE
