#include "gui/task-view.hpp"

#include "gui/icons.h"
#include "gui/plot.hpp"
#include "gui/widgets/enum-radio-button.hpp"
#include "gui/widgets/plot-range-dragger.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/util.hpp"
#include "imgui.h"
#include "implot.h"
#include "magic_enum.hpp"
#include "models/annotation.hpp"
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

template <typename T>
concept RangeAnnotation = requires(T annotation, widgets::PlotRange range) {
    { T(range) };
    { T::DefaultAnnotationColor } -> std::convertible_to<RgbColor>;
    { annotation.color() } -> std::convertible_to<RgbColor>;
    { annotation.range() } -> std::convertible_to<widgets::PlotRange>;
    { annotation.description() } -> std::convertible_to<std::string>;
    { annotation.set_range(range) };
};

template <RangeAnnotation Annotation> class RangesAnnotator {
public:
    static constexpr double MinSelectionDuration = 0.001;

    RangesAnnotator() = default;

    // Call in the BeginPlot/EndPlot block of the plot(s) where the annotations can be edited.
    void edit(const char *id)
    {
        widgets::ScopedImID id_scope(id);
        auto new_range = m_range_selector.update(
            "##new_range_selector",
            0,
            ImGuiMouseButton_Left,
            ImGuiKey_LeftCtrl,
            MinSelectionDuration
        );
        if (new_range) {
            spdlog::info(
                "{}: new label created: [{:g}, {:g}]", id, new_range->start, new_range->end
            );
            m_annotations.emplace_back(*new_range);
            m_active_idx = m_annotations.size() - 1;
        }

        if (m_active_idx.has_value()) {
            Annotation& annotation = m_annotations[*m_active_idx];
            if (!m_range_dragger.is_editing()) {
                m_temp_range = annotation.range();
            }

            const bool updated = m_range_dragger.update(
                "##active_range_dragger", m_temp_range, MinSelectionDuration
            );
            if (updated) {
                spdlog::info(
                    "{}: label {} updated to [{:g}, {:g}]",
                    id,
                    *m_active_idx,
                    m_temp_range.start,
                    m_temp_range.end
                );
                annotation.set_range(m_temp_range);
            }
        }
    }

    // Call in BeginPlot/EndPlot block
    void draw_ranges(float height = 0) const
    {
        for (std::size_t i = 0; i < m_annotations.size(); i++) {
            const Annotation& annotation = m_annotations[i];
            const bool is_active = m_active_idx == i;
            const auto& color = annotation.color();
            if (is_active && m_range_dragger.is_editing()) {
                widgets::draw_plot_range(
                    m_temp_range, color.with_alpha(SelectedLabelAlpha), height
                );
            } else {
                const bool is_hovered = m_hovered_idx == i;
                const uint8_t alpha = is_active ?
                    SelectedLabelAlpha :
                    (is_hovered ? HoveredLabelAlpha : UnselectedLabelAlpha);
                widgets::draw_plot_range(annotation.range(), color.with_alpha(alpha), height);
            }
        }

        const auto *selecting_range = m_range_selector.range();
        if (selecting_range) {
            widgets::draw_plot_range(
                *selecting_range,
                Annotation::DefaultAnnotationColor.with_alpha(SelectedLabelAlpha),
                height
            );
        }
    }

    void draw_task_list_box(const char *id)
    {
        widgets::ScopedImID id_scope(id);

        static const char *remove_button_str = DELETE_ICON;
        const float line_height = ImGui::GetTextLineHeightWithSpacing();
        const float list_height = 4 * line_height;
        m_hovered_idx.reset();
        if (!ImGui::BeginListBox("##ear_clicks_labels_listbox", {-1, list_height})) {
            return;
        }

        const float x_padding = 2 * ImGui::GetStyle().ItemSpacing.x;
        const float label_width = ImGui::GetContentRegionAvail().x
            - (ImGui::CalcTextSize(remove_button_str).x + x_padding);

        std::optional<std::size_t> delete_idx;
        for (std::size_t i = 0; i < m_annotations.size(); i++) {
            widgets::ScopedImID scoped_id(static_cast<int>(i));
            const bool is_active = m_active_idx == i;
            std::string str = m_annotations[i].description();
            if (ImGui::Selectable(str.c_str(), is_active, 0, {label_width, line_height})) {
                if (is_active) {
                    m_active_idx.reset();
                } else {
                    m_active_idx = i;
                }
            }
            if (ImGui::IsItemHovered()) {
                m_hovered_idx = i;
            }

            ImGui::SameLine();
            if (draw_delete_button()) {
                delete_idx = i;
            }
            ImGui::SetItemTooltip("Delete label");
            if (ImGui::IsItemHovered()) {
                m_hovered_idx = i;
            }
        }
        ImGui::EndListBox();

        if (delete_idx.has_value()) {
            delete_label(id, *delete_idx);
        }
    }

    const std::vector<Annotation>& annotations() const { return m_annotations; }

    Annotation *active_annotation()
    {
        if (m_active_idx.has_value()) {
            return &m_annotations[*m_active_idx];
        }
        return nullptr;
    }

private:
    static bool draw_delete_button()
    {
        constexpr float ButtonCornerRadius = 5;
        widgets::ScopedImStyle style(ImGuiStyleVar_FrameRounding, ButtonCornerRadius);
        return widgets::ButtonRed(DELETE_ICON);
    }

    void delete_label(const char *id, std::size_t idx)
    {
        const auto& range = m_annotations[idx].range();
        spdlog::info("{}: deleting task [{:g}, {:g}] (idx = {})", id, range.start, range.end, idx);
        if (m_active_idx.has_value()) {
            if (*m_active_idx > idx) {
                *m_active_idx -= 1;
            }
            m_range_selector.reset();
        }
        if (m_hovered_idx.has_value() && *m_hovered_idx > idx) {
            *m_hovered_idx -= 1;
        }

        auto diff = static_cast<decltype(m_annotations)::difference_type>(idx);
        m_annotations.erase(m_annotations.begin() + diff);
    }

    std::vector<Annotation> m_annotations;

    widgets::PlotRange m_temp_range = {NAN, NAN};
    std::optional<std::size_t> m_active_idx = std::nullopt;
    std::optional<std::size_t> m_hovered_idx = std::nullopt;
    widgets::PlotRangeDragger m_range_dragger;
    widgets::PlotRangeSelector m_range_selector;
};

class EarClickAnnotation {
public:
    static constexpr RgbColor LabelColor{0xFC, 0x5A, 0xE1};
    static constexpr RgbColor DefaultAnnotationColor = LabelColor;

    explicit EarClickAnnotation(widgets::PlotRange range) : m_range(range) {}

    const widgets::PlotRange& range() const { return m_range; }

    void set_range(widgets::PlotRange range) { m_range = range; }

    static constexpr RgbColor color() { return LabelColor; }

    std::string description() const
    {
        return fmt::format("[{:g}, {:g}] Δ={:g} s", m_range.start, m_range.end, m_range.range());
    }

private:
    widgets::PlotRange m_range;
};

bool shortcut_toggle(ImGuiKeyChord chord, bool& val)
{
    if (widgets::global_shortcut(chord)) {
        val = !val;
        return true;
    }
    return false;
}

class ApneaAnnotation {
public:
    static constexpr RgbColor DefaultAnnotationColor = {128, 128, 128};

    explicit ApneaAnnotation(widgets::PlotRange range) : m_range(range) {}

    const widgets::PlotRange& range() const { return m_range; }

    void set_range(widgets::PlotRange range) { m_range = range; }

    RgbColor color() const { return pattern_label_color(m_pattern); }

    void draw_pattern_selector()
    {
        using enum models::SrcPattern;
        using Option = widgets::RadioButtonField<models::SrcPattern>;
        constexpr std::array Options = {
            Option("ex-ex [1]", ExEx, ImGuiKey_1),
            Option("ex-in [2]", ExIn, ImGuiKey_2),
            Option("in-ex [3]", InEx, ImGuiKey_3),
            Option("in-in [4]", InIn, ImGuiKey_4),
        };
        for (const auto& opt : Options) {
            bool selected = opt.value == m_pattern;
            const bool radio_clicked = widgets::colored_radio_button(
                opt.label, selected, pattern_label_color(opt.value).with_alpha(0xFF)
            );
            if (radio_clicked || widgets::global_shortcut(opt.key)) {
                if (!selected) {
                    m_pattern = opt.value;
                    selected = true;
                    spdlog::debug("Apnea SRC selection changed to {}", opt.label);
                }
            }
            if (selected) {
                ImGui::SameLine();
                if (ImGui::Checkbox("Ambiguous [a]", &m_is_ambiguous)
                    || shortcut_toggle(ImGuiKey_A, m_is_ambiguous))
                {
                    spdlog::debug("Swallow apnea ambiguity changed: {}", m_is_ambiguous);
                }
            }
        }
    }

    std::string description() const
    {
        return fmt::format(
            "{}{}, [{:g}, {:g}] Δ={:g} s",
            magic_enum::enum_name(m_pattern),
            m_is_ambiguous ? "?" : "",
            m_range.start,
            m_range.end,
            m_range.range()
        );
    }

private:
    static RgbColor pattern_label_color(models::SrcPattern pattern)
    {
        using enum models::SrcPattern;
        switch (pattern) {
        case ExEx:
            return {0xFC, 0xEE, 0x5A};
        case ExIn:
            return {0x80, 0xFC, 0x5A};
        case InEx:
            return {0x5A, 0xFC, 0xBE};
        case InIn:
            break;
        default:
            spdlog::error("apnea_label_color: invalid SwallowApneaAnnotationStatus!");
            break;
        }
        return {0x5A, 0xB0, 0xFC};
    }

    widgets::PlotRange m_range;
    models::SrcPattern m_pattern = models::SrcPattern::ExEx;
    bool m_is_ambiguous = false;
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
            draw_labels_editor();
        }
        ImGui::End();
    }

private:
    void draw_labels_editor()
    {
        ImGui::SeparatorText("Swallow apnea");
        if (m_apnea_annotator.annotations().empty()) {
            ImGui::TextWrapped(
                "No swallow labels: Ctrl + click and drag in the flow plot to create one."
            );
        } else {
            m_apnea_annotator.draw_task_list_box("##apnea_labels_listbox");
            auto *active = m_apnea_annotator.active_annotation();
            if (active) {
                active->draw_pattern_selector();
            }
        }

        ImGui::SeparatorText("Ear clicks");
        if (m_ear_clicks_annotator.annotations().empty()) {
            ImGui::TextWrapped(
                "No ear click labels: Ctrl + click and drag in the audio plot to create one."
            );
        } else {
            m_ear_clicks_annotator.draw_task_list_box("##ear_clicks_labels_listbox");
        }
    }

    void draw_plots()
    {
        constexpr float SummaryPlotHeight = 75;
        constexpr unsigned int NumPlots = 2;
        const float data_plot_height =
            ((ImGui::GetContentRegionAvail().y - SummaryPlotHeight) / NumPlots)
            - ImGui::GetStyle().ItemSpacing.y;

        if (ImPlot::BeginAlignedPlots("##aligned_plots")) {
            draw_plot("##flow_plot", m_flow_plot, data_plot_height, [this]() {
                m_apnea_annotator.edit("##apnea_ranges_edit");
                m_apnea_annotator.draw_ranges();
                m_ear_clicks_annotator.draw_ranges(LabelSummaryHeight);
            });
            draw_plot("##audio_plot", m_audio_plot, data_plot_height, [this]() {
                m_ear_clicks_annotator.edit("##ear_clicks_ranges_edit");
                m_ear_clicks_annotator.draw_ranges();
                m_apnea_annotator.draw_ranges(LabelSummaryHeight);
            });
            ImPlot::EndAlignedPlots();
        }

        if (ImPlot::BeginPlot("##summary_plot", {-1, SummaryPlotHeight}, ImPlotFlags_CanvasOnly)) {
            draw_plot_summary_selector();
            draw_event_labels();
            m_apnea_annotator.draw_ranges(LabelSummaryHeight);
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
    RangesAnnotator<EarClickAnnotation> m_ear_clicks_annotator;
    RangesAnnotator<ApneaAnnotation> m_apnea_annotator;

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
