#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_PLOT_RANGE_MAKER_HPP
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_PLOT_RANGE_MAKER_HPP

#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/util.hpp"

#include <imgui.h>

#include <cassert>
#include <cmath>
#include <optional>

namespace recap::labeller::gui::widgets {

/**
 * Allows selecting ranges in plot x-axis coordinates by clicking, dragging, and releasing on plot.
 * Once created, PlotRangeDragger can be used to move and resize the range. User is responsible for
 * rendering the range.
 */
class PlotRangeSelector {
public:
    static constexpr ImGuiMouseButton DefaultMouseButton = ImGuiMouseButton_Left;
    static constexpr ImGuiKey DefaultKey = ImGuiKey_None;

    using Flags = unsigned int;

    enum Flag : Flags {
        Default = 0,

        // If a key is passed to draw and it is released while dragging, cancel the current
        // selection (i.e. draw will return false). Has no effect if ImGuiKey_None is passed to key
        // argument of draw.
        CancelOnKeyRelease = 1 << 0,
    };

    /**
     * Returns new range if one was created, else nullopt. User is responsible for drawing the range
     * after update.
     *
     * Must be called inside ImPlot::PlotBegin/End. The plot must have ImPlotFlags_NoBoxSelect flags
     * set.
     *
     * Note this changes the cursor to reflect the selection state. TODO: since the user is
     * responsible for rendering the range, they probably should also be responsible for setting the
     * cursor...?
     */
    template <class Id>
    std::optional<PlotRange> update(
        const Id& id,
        Flags flags = Flag::Default,
        ImGuiMouseButton mouse_button = DefaultMouseButton,
        ImGuiKey key = DefaultKey,
        double min_range = 0
    )
    {
        assert(min_range >= 0);
        ScopedImID id_scope(id);
        return update(ImGui::GetID("##plot_range_maker"), flags, mouse_button, key, min_range);
    }

    /**
     * Returns pointer to range if currently being selected (i.e. needs rendering). Else returns
     * nullptr if there is no range to draw.
     */
    [[nodiscard]] const PlotRange *range() const { return m_selecting ? &m_range : nullptr; }

    [[nodiscard]] bool is_selecting() const { return m_selecting; }

    void reset();

private:
    [[nodiscard]] std::optional<PlotRange>
    update(ImGuiID id, Flags flags, ImGuiMouseButton mouse_button, ImGuiKey key, double min_range);

    bool m_selecting = false;
    PlotRange m_range = {NAN, NAN};
};

}; // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_PLOT_RANGE_MAKER_HPP
