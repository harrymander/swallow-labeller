#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_ENUM_UTILS_HPP
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_ENUM_UTILS_HPP

#include <magic_enum/magic_enum_containers.hpp>

namespace recap::labeller::gui::widgets {

template <typename Enum> using EnumLabels = magic_enum::containers::array<Enum, const char *>;

};

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_ENUM_UTILS_HPP
