#ifndef INCLUDE_RECAP_LABELLER_GUI_WIDGETS_DRAG_RANGE
#define INCLUDE_RECAP_LABELLER_GUI_WIDGETS_DRAG_RANGE

#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/util.hpp"

#include <imgui.h>

namespace recap::labeller::gui::widgets {

/**
 * Manages a draggable and resizable range in plot x-axis coordinates. User is responsible for
 * actually rendering the range.
 */
class PlotRangeDragger {
public:
    /**
     * Returns true when finished editing, that is, is_editing() returns false and the range has
     * been changed since last update.
     *
     * User is responsible for rendering the range.
     *
     * Note this changes the cursor to reflect the update state. TODO: since the user is
     * responsible for rendering the range, they probably should also be responsible for setting the
     * cursor...?
     *
     * Must be called inside ImPlot::PlotBegin/End.
     */
    template <class Id> bool update(const Id& id, PlotRange& range)
    {
        recap::labeller::gui::widgets::ScopedImID id_scope(id);
        return update(ImGui::GetID("##plot_range_dragger"), range);
    }

    /**
     * Returns true if currently editing the range.
     *
     * The start/end fields of the bound range may be out of order (i.e. end before start) while the
     * range is being edited.
     */
    [[nodiscard]] bool is_editing() const { return m_modified; }

private:
    bool update(ImGuiID id, PlotRange& range);
    bool m_modified = false;
};

} // namespace recap::labeller::gui::widgets

#endif // INCLUDE_RECAP_LABELLER_GUI_WIDGETS_DRAG_RANGE
