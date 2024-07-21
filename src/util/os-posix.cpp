#include "os.hpp"

#include <cstdlib>
#include <optional>
#include <string>

namespace recap::labeller::os {

std::optional<std::string> getenv(const char *name)
{
    char *val = std::getenv(name);
    return val ? std::make_optional<std::string>(val) : std::nullopt;
}

}; // namespace recap::labeller::os
