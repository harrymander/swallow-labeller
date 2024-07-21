#include "os.hpp"

#include <cstdlib>
#include <optional>
#include <string>

namespace recap::labeller::os {

std::optional<std::string> getenv(const char *name)
{
    char *buf = nullptr;
    size_t num_elements = 1;
    if (::_dupenv_s(&buf, &num_elements, name)) {
        return std::nullopt;
    }

    if (buf) {
        std::string var(buf, num_elements);
        std::free(buf);
        return var;
    }

    return std::nullopt;
}

}; // namespace recap::labeller::os
