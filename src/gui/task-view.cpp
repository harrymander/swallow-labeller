#include "gui/task-view.hpp"

#include "gui/icons.h"
#include "gui/plot.hpp"
#include "gui/widgets/enum-radio-button.hpp"
#include "gui/widgets/plot-range-dragger.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/util.hpp"
#include "models/annotation.hpp"
#include "models/data.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"
#include "util/variant-visitor.hpp"

#include <fmt/std.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <implot.h>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

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

    void draw_labels_list_box(const char *id)
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

class ApneaAnnotation {
public:
    static constexpr RgbColor DefaultAnnotationColor = {128, 128, 128};
    static constexpr double MaxSnrfTime = 0.246; // TODO: get this from config

    explicit ApneaAnnotation(widgets::PlotRange range) : m_range(range) {}

    const widgets::PlotRange& range() const { return m_range; }

    void set_range(widgets::PlotRange range) { m_range = range; }

    RgbColor color() const { return label_color(m_choice); }

    void draw_pattern_selector()
    {
        using enum LabelChoice;
        using Option = widgets::RadioButtonField<LabelChoice>;
        constexpr std::array Options = {
            Option("ex-ex [1]", ExEx, ImGuiKey_1),
            Option("ex-in [2]", ExIn, ImGuiKey_2),
            Option("in-ex [3]", InEx, ImGuiKey_3),
            Option("in-in [4]", InIn, ImGuiKey_4),
            Option("non-resp. flow [5]", Nrf, ImGuiKey_5),
        };
        for (const auto& opt : Options) {
            LabelChoice choice = opt.value;
            bool selected = choice == m_choice;
            const bool radio_clicked = widgets::colored_radio_button(
                opt.label, selected, label_color(choice).with_alpha(0xFF)
            );
            if (radio_clicked || widgets::global_shortcut(opt.key)) {
                if (!selected) {
                    m_choice = choice;
                    selected = true;
                    spdlog::debug("Apnea SRC selection changed to {}", opt.label);
                }
            }
            if (selected && choice != Nrf) {
                ImGui::SameLine();
                if (ImGui::Checkbox("Ambiguous [a]", &m_is_ambiguous)
                    || widgets::global_shortcut_toggle(ImGuiKey_A, m_is_ambiguous))
                {
                    spdlog::debug("Swallow apnea ambiguity changed: {}", m_is_ambiguous);
                }
            }
        }
    }

    std::string description() const
    {
        return fmt::format(
            "{}{}{}, [{:g}, {:g}] Δ={:g} s",
            error_description() == nullptr ? "" : ERR_ICON ICON_TEXT_SPACE,
            magic_enum::enum_name(m_choice),
            m_choice != LabelChoice::Nrf && m_is_ambiguous ? "?" : "",
            m_range.start,
            m_range.end,
            m_range.range()
        );
    }

    bool valid() const { return error_description() == nullptr; }

    const char *error_description() const
    {
        if (m_choice == LabelChoice::Nrf && m_range.range() > MaxSnrfTime) {
            return "Non-resp. flow label too long";
        }
        return nullptr;
    }

    std::variant<models::SwallowApneaAnnotation, models::TimeRange> to_annotation() const
    {
        models::TimeRange time{m_range.start, m_range.end};
        if (m_choice == LabelChoice::Nrf) {
            return time;
        }

        return models::SwallowApneaAnnotation{
            .is_ambiguous = m_is_ambiguous,
            .pattern = choice_to_src_pattern(),
            .time = time,
        };
    }

private:
    enum class LabelChoice {
        ExEx,
        ExIn,
        InEx,
        InIn,
        Nrf,
    };

    models::SrcPattern choice_to_src_pattern() const
    {
        switch (m_choice) {
        case LabelChoice::ExEx:
            return models::SrcPattern::ExEx;
        case LabelChoice::ExIn:
            return models::SrcPattern::ExIn;
        case LabelChoice::InEx:
            return models::SrcPattern::InEx;
        case LabelChoice::InIn:
            return models::SrcPattern::InIn;
        default:
            break;
        }

        std::string msg = fmt::format(
            "cannot convert apnea annotation choice '{}' to SrcPattern",
            magic_enum::enum_name(m_choice)
        );
        spdlog::critical(msg);
        throw std::runtime_error(msg);
    }

    static RgbColor label_color(LabelChoice pattern)
    {
        using enum LabelChoice;
        switch (pattern) {
        case ExEx:
            return {0xFC, 0xEE, 0x5A};
        case ExIn:
            return {0x80, 0xFC, 0x5A};
        case InEx:
            return {0x5A, 0xFC, 0xBE};
        case InIn:
            return {0x5A, 0xB0, 0xFC};
        case Nrf:
            break;
        default:
            spdlog::error("apnea_label_color: invalid SrcPattern!");
            break;
        }

        return {0x8D, 0x5A, 0xFC};
    }

    widgets::PlotRange m_range;
    LabelChoice m_choice = LabelChoice::ExEx;
    bool m_is_ambiguous = false;
};

class TaskLabellingView : public TaskView {
public:
    TaskLabellingView(
        models::SwallowTaskData&& data,
        models::SwallowTaskInfo info,
        SaveAnnotationCallback save_annotation_callback
    ) :
        m_data(std::move(data)),
        m_task_info(std::move(info)),
        m_plot_x_range(m_data.flow_time.front(), m_data.flow_time.back()),
        m_flow_plot(m_data.flow_time, m_data.flow, m_plot_x_range, "Flow (L/min)", "{:g} L/min"),
        m_audio_plot(m_data.audio_time, m_data.audio, m_plot_x_range, "Audio (V)", "{:g} V"),
        m_save_annotation(std::move(save_annotation_callback))
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
        draw_save_button();

        ImGui::SeparatorText("Events");
        draw_event_list();

        ImGui::SeparatorText("Note");
        draw_note_input();

        ImGui::SeparatorText("Swallow apnea");
        if (m_apnea_annotator.annotations().empty()) {
            ImGui::TextWrapped(
                "No swallow labels: Ctrl + click and drag in the flow plot to create one."
            );
        } else {
            m_apnea_annotator.draw_labels_list_box("##apnea_labels_listbox");
            auto *active = m_apnea_annotator.active_annotation();
            if (active) {
                active->draw_pattern_selector();
                const char *error = active->error_description();
                if (error) {
                    ImGui::TextWrapped(ERR_ICON ICON_TEXT_SPACE "%s", error);
                }
            }
        }

        ImGui::SeparatorText("Ear clicks");
        if (m_ear_clicks_annotator.annotations().empty()) {
            ImGui::TextWrapped(
                "No ear click labels: Ctrl + click and drag in the audio plot to create one."
            );
        } else {
            m_ear_clicks_annotator.draw_labels_list_box("##ear_clicks_labels_listbox");
        }
    }

    bool can_save() const
    {
        return std::ranges::all_of(m_apnea_annotator.annotations(), [](const auto& annotation) {
            return annotation.valid();
        });
    }

    void draw_save_button()
    {
        const float height = ImGui::GetTextLineHeightWithSpacing() * 2;
        constexpr ImGuiKeyChord Shortcut = ImGuiMod_Ctrl | ImGuiKey_S;
        const bool disabled = !can_save();
        ImGui::BeginDisabled(disabled);
        if (ImGui::Button("Submit [Ctrl+S]", {-1, height})
            || (!disabled && widgets::global_shortcut(Shortcut)))
        {
            spdlog::info("Saving annotation");
            m_save_annotation(labels_to_swallow_annotation());
        }

        ImGui::EndDisabled();
    }

    [[nodiscard]] models::SwallowAnnotation labels_to_swallow_annotation() const
    {
        models::SwallowAnnotation swallow_annotation;
        swallow_annotation.note = m_note;
        for (const auto& ear_click_annotation : m_ear_clicks_annotator.annotations()) {
            const auto& range = ear_click_annotation.range();
            swallow_annotation.ear_clicks.emplace_back(range.start, range.end);
        }

        for (const auto& apnea_annotation : m_apnea_annotator.annotations()) {
            VariantVisitor{
                [&](const models::SwallowApneaAnnotation& apnea) {
                    swallow_annotation.swallow_apneas.push_back(apnea);
                },
                [&](const models::TimeRange& nrf) {
                    swallow_annotation.non_respiratory_flow_events.push_back(nrf);
                },
            }(apnea_annotation.to_annotation());
        }

        std::ranges::sort(swallow_annotation.ear_clicks);
        std::ranges::sort(swallow_annotation.non_respiratory_flow_events);
        std::ranges::sort(swallow_annotation.swallow_apneas, [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
        return swallow_annotation;
    }

    void draw_event_list()
    {
        constexpr double Margin = 3;

        const auto& events = m_task_info.event_times;
        if (events.empty()) {
            ImGui::TextWrapped("No events defined for this task.");
            return;
        }

        ImGui::TextWrapped(HINT_ICON ICON_TEXT_SPACE "Click on event to zoom into it in plot");
        const float list_height = ImGui::GetTextLineHeightWithSpacing() * 4;
        if (!ImGui::BeginListBox("##events_listbox", {-1, list_height})) {
            return;
        }

        for (std::size_t i = 0; i < events.size(); i++) {
            const auto& event = events[i];
            std::string label = fmt::format("{}: [{:g}, {:g}]", i + 1, event.start, event.end);
            if (ImGui::Selectable(label.c_str())) {
                const auto& time = m_data.flow_time;
                m_plot_x_range = {
                    std::max(time.front(), event.start - Margin),
                    std::min(time.back(), event.end + Margin),
                };
            }
        }
        ImGui::EndListBox();
    }

    void draw_note_input()
    {
        constexpr float HeightNumLines = 3;

        const float height = (HeightNumLines - 1) * ImGui::GetTextLineHeightWithSpacing()
            + ImGui::GetTextLineHeight();

        ImGui::InputTextMultiline("##annotation_note_input", &m_note, {-1, height});
        if (ImGui::SmallButton("Clear##clear_note_text")) {
            m_note.clear();
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
    std::string m_note;

    models::SwallowTaskData m_data;
    models::SwallowTaskInfo m_task_info;
    widgets::PlotRange m_plot_x_range;
    Plot m_flow_plot;
    Plot m_audio_plot;
    SaveAnnotationCallback m_save_annotation;
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
    const models::SwallowTaskInfo& task,
    const SaveAnnotationCallback& save_annotation_callback
)
{
    std::filesystem::path path = data_dir / task.npz_file.path;
    spdlog::debug("Loading task data {}...", path);
    auto task_data = loader.load_task_data(path);
    if (task_data.has_value()) {
        return std::make_unique<TaskLabellingView>(
            std::move(*task_data), task, save_annotation_callback
        );
    }

    return std::make_unique<TaskLoadErrorView>(path);
}
}; // namespace recap::labeller::gui
