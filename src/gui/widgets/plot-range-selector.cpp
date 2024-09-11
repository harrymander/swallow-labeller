#include "plot-range-selector.hpp"

#include "gui/widgets/util.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <implot_internal.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <tuple>

namespace recap::labeller::gui::widgets {

namespace {

bool key_down_or_none(ImGuiKey key)
{
    return key == ImGuiKey_None || ImGui::IsKeyDown(key);
}

}; // namespace

std::optional<PlotRange> PlotRangeSelector::update(
    ImGuiID id, Flags flags, ImGuiMouseButton mouse_button, ImGuiKey key, double min_range
)
{
    IM_ASSERT_USER_ERROR(ImPlot::GetCurrentPlot(), "update() needs to be called inside a plot");
    IM_ASSERT_USER_ERROR(
        ImHasFlag(ImPlot::GetCurrentPlot()->Flags, ImPlotFlags_NoBoxSelect),
        "Plot box select must be disabled"
    );
    ImPlot::SetupLock();

    const ImPlotRect plot_limits = ImPlot::GetPlotLimits();
    const auto set_active = [id]() {
        ImGui::KeepAliveID(id);
        ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
    };

    const bool last_selecting = m_selecting;
    bool cancelled = false;
    const bool key_down = key_down_or_none(key);
    const float mouse_pos = ImGui::GetMousePos().x;
    float xmin_px = NAN;
    float xmax_px = NAN;
    if (m_selecting) {
        if (ImHasFlag(flags, CancelOnKeyRelease) && !key_down) {
            cancelled = true;
            m_selecting = false;
        } else if (ImGui::IsMouseDragging(mouse_button)) {
            set_active();
            const float clicked_pos = mouse_pos - ImGui::GetMouseDragDelta(mouse_button).x;
            const float position_clamped = std::clamp(
                mouse_pos,
                plot_xaxis_to_pixels(plot_limits.X.Min),
                plot_xaxis_to_pixels(plot_limits.X.Max)
            );
            std::tie(xmin_px, xmax_px) = std::minmax(clicked_pos, position_clamped);
            ImGui::ClearActiveID();
        } else if (!ImGui::IsMouseDown(mouse_button)) {
            m_selecting = false;
        }
    } else if (key_down && ImPlot::IsPlotHovered() && ImGui::IsMouseDown(mouse_button)) {
        set_active();
        m_selecting = true;
        xmin_px = xmax_px = mouse_pos;
        ImGui::ClearActiveID();
    }

    if (m_selecting) {
        if (!(std::isnan(xmin_px) || std::isnan(xmax_px))) {
            m_range.start = plot_pixels_to_xaxis(xmin_px);
            m_range.end = plot_pixels_to_xaxis(xmax_px);
        }
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    if (!cancelled && last_selecting && !m_selecting
        && std::fabs(m_range.end - m_range.start) > min_range)
    {
        // TODO: not sure if this is needed...
        if (m_range.end < m_range.start) {
            std::swap(m_range.start, m_range.end);
        }
        return m_range;
    }
    return std::nullopt;
}

void PlotRangeSelector::reset()
{
    m_selecting = false;
    m_range = {NAN, NAN};
}

}; // namespace recap::labeller::gui::widgets
