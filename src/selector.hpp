#ifndef INCLUDE_PLOT_SELECTOR_HPP
#define INCLUDE_PLOT_SELECTOR_HPP

#include "imgui.h"
#include "implot.h"

#include <optional>

namespace plot {

typedef unsigned int PlotSelectorFlags;

class PlotXSelector {
public:
    static constexpr ImU32 DefaultColor = IM_COL32(255, 255, 0, 50);

    void draw(
        const ImColor& color = DefaultColor,
        PlotSelectorFlags flags = 0,
        ImGuiMouseButton button = ImGuiMouseButton_Right,
        ImGuiKey key = ImGuiKey_None
    );
    bool is_selecting() const;
    bool has_selected() const;
    std::optional<ImPlotRange> last_selection() const;

    void clear_selection();

    enum {
        // Disable cursor change when dragging
        NoCursor = 1 << 0,
    };

private:
    bool selecting = false;
    double xmouse_start = 0;
    double xmouse_drag = 0;
    std::optional<ImPlotRange> last_selection_ = std::nullopt;

    void draw_selection(const ImColor& color) const;
};

}; // namespace plot

#endif // INCLUDE_PLOT_SELECTOR_HPP
