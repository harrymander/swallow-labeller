#include "gui/task-view.hpp"

#include "app/config.hpp"
#include "gui/icons.h"
#include "gui/plot.hpp"
#include "gui/widgets/enum-radio-button.hpp"
#include "gui/widgets/plot-range-dragger.hpp"
#include "gui/widgets/plot-range-selector.hpp"
#include "gui/widgets/plot-range.hpp"
#include "gui/widgets/util.hpp"
#include "gui/windows.hpp"
#include "models/annotation.hpp"
#include "models/data.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"
#include "util/strutil.hpp"
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
#include <execution>
#include <filesystem>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace recap::labeller::gui {

namespace {

template <std::ranges::range R, typename Op> auto transform_to_vector(const R& range, const Op& op)
{
    using T = std::decay_t<std::invoke_result_t<Op, std::ranges::range_value_t<R>>>;
    std::vector<T> result;
    if constexpr (std::ranges::sized_range<R>) {
        result.reserve(std::ranges::distance(range));
    }
    std::transform(
        std::ranges::begin(range), std::ranges::end(range), std::back_inserter(result), op
    );
    return result;
}

struct FlowAnnotation {
    enum class Choice : unsigned char {
        ExEx,
        ExIn,
        InEx,
        InIn,
        Nrf,
    };

    explicit FlowAnnotation(const models::TimeRange range) :
        plot_range(range.start, range.end), choice(Choice::Nrf), is_ambiguous(false)
    {}

    explicit FlowAnnotation(const models::SwallowApneaAnnotation& apnea_annotation) :
        plot_range(apnea_annotation.time.start, apnea_annotation.time.end),
        choice(src_pattern_to_choice(apnea_annotation.pattern)),
        is_ambiguous(apnea_annotation.is_ambiguous)
    {}

    std::variant<models::SwallowApneaAnnotation, models::TimeRange> to_annotation_model() const
    {
        if (choice == Choice::Nrf) {
            return models::TimeRange{plot_range.start, plot_range.end};
        }

        return models::SwallowApneaAnnotation{
            .is_ambiguous = is_ambiguous,
            .pattern = choice_to_src_pattern(choice),
            .time = models::TimeRange{plot_range.start, plot_range.end},
        };
    }

    static models::SrcPattern choice_to_src_pattern(Choice choice)
    {
        auto pattern = magic_enum::enum_cast<models::SrcPattern>(magic_enum::enum_name(choice));
        return pattern.value();
    }

    static Choice src_pattern_to_choice(models::SrcPattern pattern)
    {
        auto choice = magic_enum::enum_cast<Choice>(magic_enum::enum_name(pattern));
        return choice.value();
    }

    widgets::PlotRange plot_range;
    Choice choice;
    bool is_ambiguous;
};

// Adapter for models::SwallowAnnotation
struct Annotation {
    Annotation() = default;

    explicit Annotation(const models::SwallowAnnotation& annotation) :
        note(annotation.note.value_or("")),
        ear_clicks(transform_to_vector(annotation.ear_clicks, [](const auto& click) {
            return widgets::PlotRange{click.start, click.end};
        }))
    {
        std::ranges::transform(
            annotation.swallow_apneas, std::back_inserter(flow_annotations), [](const auto& a) {
                return FlowAnnotation(a);
            }
        );
        std::ranges::transform(
            annotation.non_respiratory_flow_events,
            std::back_inserter(flow_annotations),
            [](const auto& a) { return FlowAnnotation(a); }
        );
        std::ranges::sort(flow_annotations, [](const auto& a, const auto& b) {
            const auto& range_a = a.plot_range;
            const auto& range_b = b.plot_range;
            return std::tie(range_a.start, range_a.end) < std::tie(range_b.start, range_b.end);
        });
    }

    models::SwallowAnnotation to_annotation_model() const
    {
        auto trimmed_note = strutil::trimmed(note);
        models::SwallowAnnotation annotation = {
            .swallow_apneas = {},
            .ear_clicks = transform_to_vector(
                ear_clicks,
                [](const auto& click) { return models::TimeRange{click.start, click.end}; }
            ),
            .non_respiratory_flow_events = {},
            .note =
                trimmed_note.empty() ? std::nullopt : std::make_optional<std::string>(trimmed_note),
        };

        for (const auto& flow_annotation : flow_annotations) {
            VariantVisitor{
                [&](const models::SwallowApneaAnnotation& apnea) {
                    annotation.swallow_apneas.push_back(apnea);
                },
                [&](const models::TimeRange& nrf) {
                    annotation.non_respiratory_flow_events.push_back(nrf);
                },
            }(flow_annotation.to_annotation_model());
        }

        std::ranges::sort(annotation.ear_clicks);
        std::ranges::sort(annotation.non_respiratory_flow_events);
        std::ranges::sort(annotation.swallow_apneas, [](const auto& a, const auto& b) {
            return a.time < b.time;
        });

        return annotation;
    }

    std::string note;
    std::vector<widgets::PlotRange> ear_clicks;
    std::vector<FlowAnnotation> flow_annotations;
};

class AnnotationCommand {
public:
    virtual ~AnnotationCommand() = default;
    virtual void execute(Annotation& annotation) = 0;
    virtual void undo(Annotation& annotation) = 0;
};

template <typename T, std::vector<T> Annotation::*Member>
class AnnotationEditCommand : public AnnotationCommand {
public:
    AnnotationEditCommand(std::size_t idx, const T& new_value) :
        m_idx_to_edit(idx), m_new_value(new_value)
    {}

    void execute(Annotation& annotation) override
    {
        auto& item = (annotation.*Member)[m_idx_to_edit];
        m_old_value = item;
        item = m_new_value;
    }

    void undo(Annotation& annotation) override
    {
        (annotation.*Member)[m_idx_to_edit] = *m_old_value;
    }

private:
    std::optional<T> m_old_value = std::nullopt;
    std::size_t m_idx_to_edit;
    T m_new_value;
};

using EarClickAnnotationEditCommand =
    AnnotationEditCommand<widgets::PlotRange, &Annotation::ear_clicks>;
using FlowAnnotationEditCommand =
    AnnotationEditCommand<FlowAnnotation, &Annotation::flow_annotations>;

template <typename T, std::vector<T> Annotation::*Member>
class AddAnnotationCommand : public AnnotationCommand {
public:
    explicit AddAnnotationCommand(const T& value) : m_value(value) {}

    void execute(Annotation& annotation) override
    {
        auto& vec = annotation.*Member;
        vec.push_back(m_value);
    }

    void undo(Annotation& annotation) override
    {
        auto& vec = annotation.*Member;
        vec.pop_back();
    }

private:
    T m_value;
};

using EarClickAnnotationAddCommand =
    AddAnnotationCommand<widgets::PlotRange, &Annotation::ear_clicks>;
using FlowAnnotationAddCommand =
    AddAnnotationCommand<FlowAnnotation, &Annotation::flow_annotations>;

class Annotator {
public:
    explicit Annotator(Annotation annotation) :
        m_next_command_it(m_executed_commands.end()), m_annotation(std::move(annotation))
    {}

    const Annotation& annotation() const { return m_annotation; }

    void execute_command(std::unique_ptr<AnnotationCommand>&& command)
    {
        // Clear any undone commands
        m_executed_commands.erase(m_next_command_it, m_executed_commands.end());

        command->execute(m_annotation);
        m_executed_commands.push_back(std::move(command));
        m_next_command_it = m_executed_commands.end();
    }

    template <typename Command, typename... Args> void execute_command(Args&&...args)
    {
        execute_command(std::make_unique<Command>(std::forward<Args>(args)...));
    }

    bool can_undo_last_command() const { return m_next_command_it != m_executed_commands.begin(); }

    void undo_last_command()
    {
        if (can_undo_last_command()) {
            --m_next_command_it;
            (*m_next_command_it)->undo(m_annotation);
        }
    }

    bool can_redo_last_undone_command() const
    {
        return m_next_command_it != m_executed_commands.end();
    }

    void redo_last_undone_command()
    {
        if (can_redo_last_undone_command()) {
            (*m_next_command_it)->execute(m_annotation);
            ++m_next_command_it;
        }
    }

private:
    using CommandList = std::list<std::unique_ptr<AnnotationCommand>>;

    CommandList m_executed_commands;
    CommandList::iterator m_next_command_it;
    Annotation m_annotation;
};

// FIXME - we are duplicating state by storing PlotRange objects here that are also kept in
// Annotation. This won't work with the command queue.
class PlotRangesAnnotator {
public:
    static constexpr double MinSelectionDuration = 0.001;

    template <std::ranges::range R>
    explicit PlotRangesAnnotator(const R& r) : m_ranges(r.begin(), r.end())
    {}

    // Call in the BeginPlot/EndPlot block of the plot(s) where the annotations can be edited.
    // Returns
    template <typename Add, typename Edit>
        requires std::invocable<Add, const widgets::PlotRange&>
        && std::invocable<Edit, std::size_t, const widgets::PlotRange&>
    void edit(const char *id, const Add& add_func, const Edit& edit_func)
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
            m_ranges.emplace_back(*new_range);
            m_active_idx = m_ranges.size() - 1;
            add_func(*new_range);
        }

        if (m_active_idx.has_value()) {
            auto& range = m_ranges[*m_active_idx];
            if (!m_range_dragger.is_editing()) {
                m_temp_range = range;
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
                range = m_temp_range;
                edit_func(*m_active_idx, range);
            }
        }
    }

    struct PlotRangeInfo {
        std::size_t idx;
        const widgets::PlotRange& range;
        bool is_hovered;
        bool is_active;
    };

    template <typename Function>
        requires std::invocable<Function, const PlotRangeInfo&>
    void for_each_range(const Function& function) const
    {
        for (std::size_t i = 0; i < m_ranges.size(); i++) {
            bool is_active = m_active_idx == i;
            const widgets::PlotRange& range =
                is_active && m_range_dragger.is_editing() ? m_temp_range : m_ranges[i];
            function(
                PlotRangeInfo{
                    .idx = i,
                    .range = range,
                    .is_hovered = m_hovered_idx == i,
                    .is_active = is_active,
                }
            );
        }
    }

    template <typename Describe, typename Delete>
        requires std::convertible_to<
                     std::invoke_result_t<Describe, std::size_t, const widgets::PlotRange&>,
                     std::string>
        && std::invocable<Delete, std::size_t>
    void draw_list_box(const char *id, const Describe& describe_func, const Delete& delete_func)
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

        std::optional<std::size_t> delete_idx = std::nullopt;
        for (std::size_t i = 0; i < m_ranges.size(); i++) {
            widgets::ScopedImID idx_id_scope(static_cast<int>(i));
            std::string description = describe_func(i, m_ranges[i]);
            const bool is_active = m_active_idx == i;
            if (ImGui::Selectable(description.c_str(), is_active, 0, {label_width, line_height})) {
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

        if (delete_idx.has_value()) {
            delete_func(*delete_idx);
        }
    }

    template <typename Function>
        requires std::invocable<Function, widgets::PlotRange>
    void for_new_range(const Function& function) const
    {
        const auto *selecting_range = m_range_selector.range();
        if (selecting_range) {
            function(*selecting_range);
        }
    }

private:
    static bool draw_delete_button()
    {
        widgets::RedButtonColorScope color_scope;
        return draw_rounded_button(DELETE_ICON);
    }

    static bool draw_rounded_button(const char *label)
    {
        constexpr float ButtonCornerRadius = 5;
        widgets::ScopedImStyle style(ImGuiStyleVar_FrameRounding, ButtonCornerRadius);
        return ImGui::Button(label);
    }

    widgets::PlotRange m_temp_range = {NAN, NAN};
    std::optional<std::size_t> m_active_idx = std::nullopt;
    std::optional<std::size_t> m_hovered_idx = std::nullopt;
    widgets::PlotRangeDragger m_range_dragger;
    widgets::PlotRangeSelector m_range_selector;

    std::vector<widgets::PlotRange> m_ranges;
};

class FlowAnnotator {
public:
    explicit FlowAnnotator(Annotator& annotator) :
        m_annotator(annotator),
        m_ranges_annotator(
            std::views::transform(
                m_annotator.annotation().flow_annotations,
                [](const auto& annotation) { return annotation.plot_range; }
            )
        )
    {}

    void edit()
    {
        m_ranges_annotator.edit(
            "##flow_plot_label_editing",
            [this](const widgets::PlotRange& new_range) {
                // TODO
                // m_annotator.execute_command<FlowAnnotationAddCommand>(new_range);
            },
            [this](std::size_t idx, const widgets::PlotRange& new_range) {
                auto new_annotation = m_annotator.annotation().flow_annotations[idx];
                new_annotation.plot_range = new_range;
                m_annotator.execute_command<FlowAnnotationEditCommand>(idx, new_annotation);
            }
        );
    }

    void draw_ranges(float height = 0) const
    {
        m_ranges_annotator.for_each_range([this, height](const auto& info) {
            const auto& annotation = m_annotator.annotation().flow_annotations[info.idx];
            // TODO: color
            widgets::draw_plot_range(info.range, IM_COL32(128, 128, 128, 128), height);
        });

        m_ranges_annotator.for_new_range([height](const auto& range) {
            widgets::draw_plot_range(range, IM_COL32(128, 128, 128, 128), height);
        });
    }

private:
    Annotator& m_annotator;
    PlotRangesAnnotator m_ranges_annotator;
};

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

class TaskLabellingView : public TaskView {
public:
    TaskLabellingView(
        models::SwallowTaskData&& data,
        models::SwallowTaskInfo info,
        const models::SwallowAnnotation *existing_annotation,
        SaveAnnotationCallback save_annotation_callback
    ) :
        m_data(std::move(data)),
        m_task_info(std::move(info)),
        m_plot_x_range(m_data.flow_time.front(), m_data.flow_time.back()),
        m_flow_plot(m_data.flow_time, m_data.flow, m_plot_x_range, "Flow (L/min)", "{:g} L/min"),
        m_audio_plot(m_data.audio_time, m_data.audio, m_plot_x_range, "Audio (V)", "{:g} V"),
        m_save_annotation(std::move(save_annotation_callback)),
        m_annotator(existing_annotation ? Annotation(*existing_annotation) : Annotation{}),
        m_flow_annotator(m_annotator)
    {}

    void draw() override
    {
        if (ImGui::Begin(TaskViewDataPlotsWindowId)) {
            draw_plots();
        }
        ImGui::End();

        if (ImGui::Begin(TaskViewLabelControlsWindowId)) {
            draw_labels_editor();
        }
        ImGui::End();
    }

private:
    void draw_labels_editor()
    {
        draw_undo_redo();

        draw_save_button();

        ImGui::SeparatorText("Events");
        draw_event_list();

        ImGui::SeparatorText("Note");
        draw_note_input();
    }

    void draw_undo_redo()
    {
        // TODO: icons
        ImGui::BeginDisabled(!m_annotator.can_undo_last_command());
        if (ImGui::Button("Undo")) {
            m_annotator.undo_last_command();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_annotator.can_redo_last_undone_command());
        if (ImGui::Button("Redo")) {
            m_annotator.redo_last_undone_command();
        }
        ImGui::EndDisabled();
    }

    bool can_save() const { return false; }

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
        }

        ImGui::EndDisabled();
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
                m_flow_annotator.edit();
                m_flow_annotator.draw_ranges();
            });
            draw_plot("##audio_plot", m_audio_plot, data_plot_height, [this]() {
                m_flow_annotator.draw_ranges(LabelSummaryHeight);
            });
            ImPlot::EndAlignedPlots();
        }

        if (ImPlot::BeginPlot("##summary_plot", {-1, SummaryPlotHeight}, ImPlotFlags_CanvasOnly)) {
            draw_plot_summary_selector();
            draw_event_labels();
            m_flow_annotator.draw_ranges(LabelSummaryHeight);
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
    std::string m_note;

    models::SwallowTaskData m_data;
    models::SwallowTaskInfo m_task_info;
    widgets::PlotRange m_plot_x_range;
    Plot m_flow_plot;
    Plot m_audio_plot;
    SaveAnnotationCallback m_save_annotation;
    Annotator m_annotator;
    FlowAnnotator m_flow_annotator;
};

class TaskLoadErrorView : public TaskView {
public:
    explicit TaskLoadErrorView(std::filesystem::path path) : m_path(std::move(path)) {}

    void draw() override
    {
        if (ImGui::Begin(TaskViewDataPlotsWindowId)) {
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
    const models::SwallowAnnotation *existing_annotation,
    const SaveAnnotationCallback& save_annotation_callback
)
{
    std::filesystem::path path = data_dir / task.npz_file.path;
    spdlog::debug("Loading task data {}...", path);
    auto task_data = loader.load_task_data(path);
    if (task_data.has_value()) {
        return std::make_unique<TaskLabellingView>(
            std::move(*task_data), task, existing_annotation, save_annotation_callback
        );
    }

    return std::make_unique<TaskLoadErrorView>(path);
}
}; // namespace recap::labeller::gui
