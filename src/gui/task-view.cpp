#include "gui/task-view.hpp"

#include "gui/icons.h"
#include "gui/plot.hpp"
#include "gui/widgets/plot-range-dragger.hpp"
#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/util.hpp"
#include "imgui.h"
#include "implot.h"
#include "models/data.hpp"
#include "models/task-info.hpp"

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <utility>

namespace recap::labeller::gui {

namespace {

constexpr ImU32 EventLabelColor = IM_COL32(0xFC, 0x65, 0x5A, 0xFF);
constexpr float LabelSummaryHeight = 8; // Same as default ImPlotStyle::DigitalBitHeight

widgets::PlotRange initial_plot_range(double t0, double t1, const models::SwallowTaskInfo& info)
{
    constexpr double Margin = 3;

    if (info.event_times.size() > 0) {
        const auto first_event = info.event_times[0];
        t0 = std::max(t0, first_event.start - Margin);
        t1 = std::min(t1, first_event.end + Margin);
    }
    return {t0, t1};
}

class TaskLabellingView : public TaskView {
public:
    TaskLabellingView(models::SwallowTaskData&& data, models::SwallowTaskInfo info) :
        m_data(std::move(data)),
        m_task_info(std::move(info)),
        m_plot_x_range(
            initial_plot_range(m_data.flow_time.front(), m_data.flow_time.back(), m_task_info)
        ),
        m_flow_plot(m_data.flow_time, m_data.flow, m_plot_x_range, "Flow (L/min)", "{:g} L/min"),
        m_audio_plot(m_data.audio_time, m_data.audio, m_plot_x_range, "Audio (V)", "{:g} V")
    {}

    void draw() override
    {
        if (ImGui::Begin("Task labelling")) {
            draw_plots();
        }
        ImGui::End();
    }

private:
    void draw_plots()
    {
        constexpr float SummaryPlotHeight = 75;
        constexpr unsigned int NumPlots = 2;
        const float data_plot_height =
            ((ImGui::GetContentRegionAvail().y - SummaryPlotHeight) / NumPlots)
            - ImGui::GetStyle().ItemSpacing.y;

        if (ImPlot::BeginAlignedPlots("##aligned_plots")) {
            draw_plot("##flow_plot", m_flow_plot, data_plot_height);
            draw_plot("##audio_plot", m_audio_plot, data_plot_height);
            ImPlot::EndAlignedPlots();
        }

        if (ImPlot::BeginPlot("##summary_plot", {-1, SummaryPlotHeight}, ImPlotFlags_CanvasOnly)) {
            draw_plot_summary_selector();
            draw_event_labels();
            ImPlot::EndPlot();
        }
    }

    void draw_plot(const char *id, Plot& plot, float height) const
    {
        constexpr ImPlotFlags Flags =
            ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus;
        widgets::ScopedImID scoped_id(id);
        if (ImPlot::BeginPlot("##plot", {-1, height}, Flags)) {
            plot.draw();
            draw_event_labels();
            ImPlot::EndPlot();
        }
    }

    void draw_event_labels() const
    {
        for (const auto& event : m_task_info.event_times) {
            widgets::draw_plot_range(event.start, event.end, EventLabelColor, -LabelSummaryHeight);
        }
    }

    void draw_plot_summary_selector()
    {
        constexpr ImPlotAxisFlags AxFlags = ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit;
        constexpr ImColor SummaryColor = {.5F, .5F, .5F, .6F};

        ImPlot::SetupAxes(nullptr, nullptr, AxFlags, AxFlags);

        ImPlot::PlotLine(
            "##summary_flow_plot_line",
            m_data.flow_time.data(),
            m_data.flow.data(),
            static_cast<int>(m_data.flow_time.size())
        );

        m_plot_summary_selector.update("##plot_summary_selector");
        const widgets::PlotRange *new_range = m_plot_summary_selector.range();
        if (new_range) {
            m_plot_x_range = *new_range;
        } else {
            m_plot_summary_dragger.update("##plot_summary_dragger", m_plot_x_range);
        }
        widgets::draw_plot_range(m_plot_x_range, SummaryColor);
    }

    widgets::PlotRangeDragger m_plot_summary_dragger;
    widgets::PlotRangeSelector m_plot_summary_selector;

    models::SwallowTaskData m_data;
    models::SwallowTaskInfo m_task_info;
    widgets::PlotRange m_plot_x_range;
    Plot m_flow_plot;
    Plot m_audio_plot;
};

class TaskLoadErrorView : public TaskView {
public:
    explicit TaskLoadErrorView(std::filesystem::path path) : m_path(std::move(path)) {}

    void draw() override
    {
        if (ImGui::Begin("Task data loading error")) {
            ImGui::TextWrapped(
                ERR_ICON ICON_TEXT_SPACE "Error loading data for path %s", m_path.string().c_str()
            );
        }
        ImGui::End();
    }

private:
    std::filesystem::path m_path;
};

}; // namespace

std::unique_ptr<TaskView> load_task_view(
    app::TaskLoader& loader,
    const std::filesystem::path& data_dir,
    const models::SwallowTaskInfo& task
)
{
    std::filesystem::path path = data_dir / task.npz_file.path;
    spdlog::debug("Loading task data {}...", path);
    auto task_data = loader.load_task_data(path);
    if (task_data.has_value()) {
        return std::make_unique<TaskLabellingView>(std::move(*task_data), task);
    }

    return std::make_unique<TaskLoadErrorView>(path);
}
}; // namespace recap::labeller::gui
