#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_INTEGER_RANGE_INPUT
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_INTEGER_RANGE_INPUT

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace recap::labeller::gui::widgets {

class IntegerRangeInput {
public:
    void draw(const char *id);

    [[nodiscard]] bool contains(unsigned int val) const;

    [[nodiscard]] bool error() const { return !m_input.empty() && m_error; }

    [[nodiscard]] bool empty() const { return m_input.empty(); }

    void clear() { m_items.clear(); }

private:
    using Pair = std::pair<unsigned int, unsigned int>;

    void update();
    static bool parse_pair(std::string_view str, Pair& pair);

    bool m_error = false;
    std::vector<Pair> m_items;
    std::string m_input;
};

} // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_INTEGER_RANGE_INPUT
