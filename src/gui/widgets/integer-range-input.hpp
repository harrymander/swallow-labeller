#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_INTEGER_RANGE_INPUT
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_INTEGER_RANGE_INPUT

#include "integer-range.hpp"

#include <concepts>
#include <string>
#include <vector>

namespace recap::labeller::gui::widgets {

template <std::unsigned_integral T> class UnsignedIntegerRangeInput {
public:
    void draw(const char *id, float width = 0);

    [[nodiscard]] bool contains(T val) const;

    [[nodiscard]] bool error() const { return !m_input.empty() && m_error; }

    [[nodiscard]] bool empty() const { return m_input.empty(); }

    void clear() { m_items.clear(); }

private:
    void update();

    bool m_error = false;
    std::vector<IntegerRange<T>> m_items;
    std::string m_input;
};

} // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_INTEGER_RANGE_INPUT
