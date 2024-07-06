#ifndef INCLUDE_PLOT_DRAG_RANGE_HPP
#define INCLUDE_PLOT_DRAG_RANGE_HPP

#include <imgui.h>
#include <implot.h>

namespace recap::plot {

using DragXRangeFlags = unsigned int;

enum DragXRangeFlag : DragXRangeFlags {
    None = 0,

    // Disable cursors on hover
    NoCursor = 1 << 0,

    // Disable moving/resizing, will still show cursor and report click, hold, or hovered
    NoMove = 1 << 1,

    // Disable all input but still show rectangle. Will not show cursor and values of report click,
    // hold, or hovered will be unchanged
    NoInput = 1 << 2,
};

/**
 * Draggable region that vertically spans axis.
 *
 * Must be called between PlotBegin/End.
 *
 * @param id ID of the region, must be unique within a PlotBegin/End.
 * @param xrange Reference to the range to drag. If the region is being resized, xrange.Min may be
 *     greater than xrange.Max; the ordering will be fixed when the region is released.
 * @param color Color of the region.
 * @param flags Flags to customize the behavior.
 * @param clicked Pointer to store if the region was clicked.
 * @param hovered Pointer to store if the region is hovered.
 * @param held Pointer to store if the region is held.
 *
 * @returns true if the region was modified
 */
bool drag_xrange(
    ImGuiID id,
    ImPlotRange& xrange,
    const ImColor& color,
    recap::plot::DragXRangeFlags flags = 0,
    bool *clicked = nullptr,
    bool *hovered = nullptr,
    bool *held = nullptr
);

bool drag_xrange(
    ImGuiID id,
    double& xmin,
    double& xmax,
    const ImColor& color,
    recap::plot::DragXRangeFlags flags = 0,
    bool *clicked = nullptr,
    bool *hovered = nullptr,
    bool *held = nullptr
);

class DragXRangeWrapper {
public:
    explicit DragXRangeWrapper(ImPlotRange& xrange);

    /**
     * Same as calling drag_xrange on the range passed at construction, but only returns true when
     * the range has been modified *and mouse button has been released*.
     */
    bool draw(ImGuiID id, const ImColor& color, recap::plot::DragXRangeFlags flags = 0);

    [[nodiscard]] bool is_editing() const { return m_modified; }

private:
    ImPlotRange& range;
    bool m_modified = false;
};

} // namespace recap::plot

#endif // INCLUDE_PLOT_DRAG_RANGE_HPP
