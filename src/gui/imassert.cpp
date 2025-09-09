#include "gui/imassert.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <iostream>
#include <source_location>
#include <string>

namespace recap::labeller::gui {

void imgui_assert_failed(const char *expr, const std::source_location loc)
{
    std::string msg = fmt::format(
        "{}:{}:{}:{}: Assertion failed: {}",
        loc.file_name(),
        loc.function_name(),
        loc.line(),
        loc.column(),
        expr
    );
    std::clog << msg << '\n';
    spdlog::critical(msg);
    std::abort();
}

}; // namespace recap::labeller::gui
