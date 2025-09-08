#include "gui/task-view.hpp"

#include "gui/icons.h"
#include "gui/plot.hpp"
#include "gui/widgets/plot-range-dragger.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/util.hpp"
#include "imgui.h"
#include "implot.h"
#include "models/data.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

namespace recap::labeller::gui {

namespace {

struct RgbColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    constexpr ImU32 with_alpha(uint8_t alpha) const { return IM_COL32(red, green, blue, alpha); }
};

constexpr RgbColor EventLabelColor{0xFC, 0x65, 0x5A};
constexpr float LabelSummaryHeight = 8; // Same as default ImPlotStyle::DigitalBitHeight

// Alpha values for different label region states
constexpr uint8_t UnselectedLabelAlpha = 0x33;
constexpr uint8_t HoveredLabelAlpha = 0x44;
constexpr uint8_t SelectedLabelAlpha = 0x66;

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

class EarClicksAnnotator {
public:
    static constexpr RgbColor LabelColor{0xFC, 0x5A, 0xE1};
    static constexpr double MinSelectionRange = 0.001;

    EarClicksAnnotator() = default;

    explicit EarClicksAnnotator(const std::vector<models::TimeRange>& time_ranges)
    {
        m_ranges.reserve(time_ranges.size());
        for (const auto& range : time_ranges) {
            m_ranges.emplace_back( // cppcheck-suppress useStlAlgorithm
                range.start, range.end
            );
        }
    }

    // Call in the plot where the ranges can be edited
    void draw_range_editing()
    {
        auto new_range = m_range_selector.update(
            "##ear_clicks_new_range_selector",
            0,
            ImGuiMouseButton_Left,
            ImGuiKey_LeftCtrl,
            MinSelectionRange
        );
        if (new_range) {
            spdlog::info("Ear click label created: [{:g}, {:g}]", new_range->start, new_range->end);
            m_ranges.push_back(*new_range);
            m_active_id = m_ranges.size() - 1;
        }

        if (m_active_id.has_value()) {
            auto& active_range = m_ranges[*m_active_id];
            if (!m_range_dragger.is_editing()) {
                m_temp_range = active_range;
            }

            const bool updated = m_range_dragger.update(
                "##ear_clicks_active_range_dragger", m_temp_range, MinSelectionRange
            );
            if (updated) {
                spdlog::info(
                    "Ear click label {} updated to [{:g}, {:g}]",
                    *m_active_id,
                    m_temp_range.start,
                    m_temp_range.end
                );
                active_range = m_temp_range;
            }
        }
    }

    void draw_ranges(float height = 0) const
    {
        for (std::size_t i = 0; i < m_ranges.size(); i++) {
            const auto& range = m_ranges[i];
            const bool is_active = m_active_id == i;
            if (is_active && m_range_dragger.is_editing()) {
                widgets::draw_plot_range(
                    m_temp_range, LabelColor.with_alpha(SelectedLabelAlpha), height
                );
            } else {
                const bool is_hovered = m_hovered_id == i;
                const uint8_t alpha = is_active ?
                    SelectedLabelAlpha :
                    (is_hovered ? HoveredLabelAlpha : UnselectedLabelAlpha);
                widgets::draw_plot_range(range, LabelColor.with_alpha(alpha), height);
            }
        }

        const auto *selecting_range = m_range_selector.range();
        if (selecting_range) {
            widgets::draw_plot_range(
                *selecting_range, LabelColor.with_alpha(SelectedLabelAlpha), height
            );
        }
    }

    void draw_task_list_box()
    {
        ImGui::SeparatorText("Ear clicks");
        const float height = 4 * ImGui::GetTextLineHeightWithSpacing();
        m_hovered_id.reset();

        if (!ImGui::BeginListBox("##ear_clicks_labels_listbox", {-1, height})) {
            return;
        }

        for (std::size_t i = 0; i < m_ranges.size(); i++) {
            const auto& range = m_ranges[i];
            const bool is_active = m_active_id == i;

            std::string str = fmt::format("{:g}, {:g}", range.start, range.end);
            if (ImGui::Selectable(str.c_str(), is_active)) {
                m_active_id = i;
            }
            if (ImGui::IsItemHovered()) {
                m_hovered_id = i;
            }
        }
        ImGui::EndListBox();
    }

private:
    widgets::PlotRange m_temp_range = {NAN, NAN};
    std::vector<widgets::PlotRange> m_ranges;

    std::optional<std::size_t> m_active_id = std::nullopt;
    std::optional<std::size_t> m_hovered_id = std::nullopt;
    widgets::PlotRangeDragger m_range_dragger;
    widgets::PlotRangeSelector m_range_selector;
};

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

        if (ImGui::Begin("Labels")) {
            m_ear_clicks_annotator.draw_task_list_box();
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
            draw_plot("##flow_plot", m_flow_plot, data_plot_height, [this]() {
                m_ear_clicks_annotator.draw_ranges(LabelSummaryHeight);
            });
            draw_plot("##audio_plot", m_audio_plot, data_plot_height, [this]() {
                m_ear_clicks_annotator.draw_range_editing();
                m_ear_clicks_annotator.draw_ranges();
            });
            ImPlot::EndAlignedPlots();
        }

        if (ImPlot::BeginPlot("##summary_plot", {-1, SummaryPlotHeight}, ImPlotFlags_CanvasOnly)) {
            draw_plot_summary_selector();
            draw_event_labels();
            m_ear_clicks_annotator.draw_ranges(LabelSummaryHeight);
            ImPlot::EndPlot();
        }
    }

    template <typename Func>
        requires std::invocable<Func>
    void draw_plot(const char *id, Plot& plot, float height, const Func& extra_draw) const
    {
        constexpr ImPlotFlags Flags =
            ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus;
        widgets::ScopedImID scoped_id(id);
        if (ImPlot::BeginPlot("##plot", {-1, height}, Flags)) {
            plot.draw();
            draw_event_labels();
            extra_draw();
            ImPlot::EndPlot();
        }
    }

    void draw_event_labels() const
    {
        for (const auto& event : m_task_info.event_times) {
            widgets::draw_plot_range(
                event.start, event.end, EventLabelColor.with_alpha(0xFF), -LabelSummaryHeight
            );
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

    // TODO: pass in existing ear click labels if there is a saved annotation
    EarClicksAnnotator m_ear_clicks_annotator;

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
