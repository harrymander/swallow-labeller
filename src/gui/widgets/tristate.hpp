#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_TRISTATE_HPP
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_TRISTATE_HPP

#include "gui/widgets/enum-combo.hpp"
#include "gui/widgets/enum-utils.hpp"

namespace recap::labeller::gui::widgets {

class TriState {
public:
    TriState() : m_val(-1) {}

    explicit TriState(bool val) : m_val(val ? 1 : 0) {}

    void unset() { m_val = -1; }

    void set(bool val) { m_val = val ? 1 : 0; }

    TriState& operator=(bool val)
    {
        set(val);
        return *this;
    }

    explicit operator bool() const { return is_true(); }

    [[nodiscard]] bool is_set() const { return m_val >= 0; }

    [[nodiscard]] bool is_unset() const { return !is_set(); }

    [[nodiscard]] bool is_false() const { return m_val == 0; }

    [[nodiscard]] bool is_true() const { return m_val > 0; }

private:
    signed char m_val;
};

inline bool tristate_combo(
    const char *label,
    TriState& current,
    const char *false_label,
    const char *true_label,
    const char *unset_label = "Any"
)
{
    enum class TriStateValue : unsigned char {
        Unset,
        False,
        True,
    };
    using enum TriStateValue;

    const EnumLabels<TriStateValue> labels = {unset_label, false_label, true_label};
    TriStateValue val = current.is_true() ? True : (current.is_false() ? False : Unset);
    const bool updated = enum_combo(label, labels, val);
    if (val == Unset) {
        current.unset();
    } else {
        current = val == True;
    }
    return updated;
}

}; // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_TRISTATE_HPP
