#ifndef INCLUDE_PLOT_SELECTOR_HPP
#define INCLUDE_PLOT_SELECTOR_HPP

#include "imgui.h"
#include "implot.h"

namespace recap::plot {

using PlotSelectorFlags = unsigned int;

class PlotXSelector {
public:
    static constexpr ImU32 DefaultColor = IM_COL32(255, 255, 0, 50);
    static constexpr ImGuiMouseButton DefaultMouseButton = ImGuiMouseButton_Right;
    static constexpr ImGuiKey DefaultKey = ImGuiKey_None;

    bool draw(
        ImGuiID id,
        ImPlotRange& range,
        const ImColor& color = DefaultColor,
        PlotSelectorFlags flags = 0,
        ImGuiMouseButton button = DefaultMouseButton,
        ImGuiKey key = DefaultKey
    );
    [[nodiscard]] bool is_selecting() const;

    enum : PlotSelectorFlags {
        // Disable cursor change when dragging
        NoCursor = 1 << 0,

        // If a key is passed to draw and it is released while dragging, cancel the current
        // selection (i.e. draw will return false). Has no effect if ImGuiKey_None is passed to key
        // argument of draw.
        CancelOnKeyRelease = 1 << 1,
    };

private:
    bool selecting = false;
};

}; // namespace recap::plot

#endif // INCLUDE_PLOT_SELECTOR_HPP
