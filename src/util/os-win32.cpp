#include "os.hpp"

#include <cstdlib>
#include <optional>
#include <string>

namespace recap::labeller::os {

std::optional<std::string> getenv(const char *name)
{
    char *buf = nullptr;
    size_t buflen = 0;
    if (::_dupenv_s(&buf, &buflen, name)) {
        return std::nullopt;
    }

    if (buf) {
        std::string var(buf, buflen);
        std::free(buf);
        return var;
    }

    return std::nullopt;
}

}; // namespace recap::labeller::os
