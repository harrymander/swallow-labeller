#ifndef RECAP_LABELLER_GUI_IMASSERT_HPP_INCLUDE
#define RECAP_LABELLER_GUI_IMASSERT_HPP_INCLUDE

#include <source_location>

namespace recap::labeller::gui {

[[noreturn]]
void imgui_assert_failed(
    const char *expr, std::source_location loc = std::source_location::current()
);

};

#endif // RECAP_LABELLER_GUI_IMASSERT_HPP_INCLUDE
