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

#include <IconsFontAwesome6.h>
#include <fmt/std.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <implot.h>
#include <magic_enum/magic_enum.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

namespace recap::labeller::gui {

namespace {

// Pass ImGui size arguments to fill remaining X/Y space
constexpr float SizeFill = -std::numeric_limits<float>::min();

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

    static FlowAnnotation from_nrf_time_range(const models::TimeRange range)
    {
        return {
            .plot_range = {range.start, range.end},
            .choice = Choice::Nrf,
            .is_ambiguous = false,
        };
    }

    static FlowAnnotation
    from_apnea_annotation(const models::SwallowApneaAnnotation& apnea_annotation)
    {
        return {
            .plot_range = {apnea_annotation.time.start, apnea_annotation.time.end},
            .choice = src_pattern_to_choice(apnea_annotation.pattern),
            .is_ambiguous = apnea_annotation.is_ambiguous,
        };
    }

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

    const char *error_description() const
    {
        if (choice == Choice::Nrf) {
            const double duration = plot_range.end - plot_range.start;
            if (duration > app::get_global_app_config().max_snrf_time) {
                return "Non-resp. flow label too long";
            }
        }
        return nullptr;
    }

    bool valid() const { return error_description() == nullptr; }

    // TODO
    widgets::PlotRange plot_range = {NAN, NAN};
    Choice choice = Choice::ExEx;
    bool is_ambiguous = false;
};

// Adapter for models::SwallowAnnotation
struct Annotation {
    Annotation() = default;

    explicit Annotation(const models::SwallowAnnotation& annotation) :
        notes(annotation.notes),
        ear_clicks(transform_to_vector(annotation.ear_clicks, [](const auto& click) {
            return widgets::PlotRange{click.start, click.end};
        }))
    {
        std::ranges::transform(
            annotation.swallow_apneas, std::back_inserter(flow_annotations), [](const auto& a) {
                return FlowAnnotation::from_apnea_annotation(a);
            }
        );
        std::ranges::transform(
            annotation.non_respiratory_flow_events,
            std::back_inserter(flow_annotations),
            [](const auto& a) { return FlowAnnotation::from_nrf_time_range(a); }
        );
        std::ranges::sort(flow_annotations, [](const auto& a, const auto& b) {
            const auto& range_a = a.plot_range;
            const auto& range_b = b.plot_range;
            return std::tie(range_a.start, range_a.end) < std::tie(range_b.start, range_b.end);
        });
    }

    models::SwallowAnnotation to_annotation_model() const
    {
        models::SwallowAnnotation annotation = {
            .swallow_apneas = {}, // updated below
            .ear_clicks = transform_to_vector(
                ear_clicks,
                [](const auto& click) { return models::TimeRange{click.start, click.end}; }
            ),
            .non_respiratory_flow_events = {},
            .notes = notes,
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

    bool valid() const
    {
        return std::ranges::all_of(flow_annotations, [](const auto& a) { return a.valid(); });
    }

    std::vector<std::string> notes;
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
class EditAnnotationCommand : public AnnotationCommand {
public:
    EditAnnotationCommand(std::size_t idx, const T& new_value) :
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

template <typename T, std::vector<T> Annotation::*Member>
class DeleteAnnotationCommand : public AnnotationCommand {
public:
    explicit DeleteAnnotationCommand(std::size_t idx) :
        m_idx_to_delete(static_cast<difference_type>(idx))
    {}

    void execute(Annotation& annotation) override
    {
        auto& vec = annotation.*Member;
        m_deleted_value = vec[m_idx_to_delete];
        vec.erase(vec.begin() + m_idx_to_delete);
    }

    void undo(Annotation& annotation) override
    {
        auto& vec = annotation.*Member;
        vec.insert(vec.begin() + m_idx_to_delete, *m_deleted_value);
    }

private:
    using difference_type = std::vector<T>::difference_type;

    std::optional<T> m_deleted_value;
    difference_type m_idx_to_delete;
};

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
        m_modified_since_last_save = true;
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
            m_modified_since_last_save = true;
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
            m_modified_since_last_save = true;
        }
    }

    void save_annotation(const SaveAnnotationCallback& callback)
    {
        callback(m_annotation.to_annotation_model());
        m_modified_since_last_save = false;
    }

    bool modified_since_last_save() const { return m_modified_since_last_save; }

private:
    using CommandList = std::list<std::unique_ptr<AnnotationCommand>>;

    bool m_modified_since_last_save = false;

    CommandList m_executed_commands;
    CommandList::iterator m_next_command_it;
    Annotation m_annotation;
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

// FIXME: this is a mess... Need to avoid replicating state between the PlotRanges here and in
// Annotation
class PlotRangesEditor {
public:
    // Call inside BeginPlot/EndPlot
    template <typename GetRange, typename NewRange, typename EditRange>
        requires std::
                     convertible_to<std::invoke_result_t<GetRange, std::size_t>, widgets::PlotRange>
        && std::same_as<std::invoke_result_t<NewRange, widgets::PlotRange>, std::size_t>
        && std::invocable<EditRange, std::size_t, widgets::PlotRange>
    void edit(
        const char *id,
        const GetRange& get_range,
        const NewRange& add_new_range,
        const EditRange& edit_range
    )
    {
        widgets::ScopedImID scoped_id(id);

        // Add a new range
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
            m_active_idx = add_new_range(*new_range);
        }

        // Edit current active range
        if (m_active_idx.has_value()) {
            if (!m_range_dragger.is_editing()) {
                m_active_annotation_temp_range = get_range(*m_active_idx);
            }

            const bool updated = m_range_dragger.update(
                "##active_range_dragger", m_active_annotation_temp_range, MinSelectionDuration
            );
            if (updated) {
                spdlog::info(
                    "{}: label {} updated to [{:g}, {:g}]",
                    id,
                    *m_active_idx,
                    m_active_annotation_temp_range.start,
                    m_active_annotation_temp_range.end
                );
                edit_range(*m_active_idx, m_active_annotation_temp_range);
            }
        }
    }

    // Call inside BeginListBox/EndListBox
    template <typename T, typename AnnotationDescription, typename Delete, typename CenterRange>
        requires std::convertible_to<std::invoke_result_t<AnnotationDescription, T>, std::string>
        && std::invocable<Delete, std::size_t> && std::invocable<CenterRange, const T&>
    void draw_list_box_items(
        const std::vector<T>& items,
        const AnnotationDescription& annotation_description,
        const Delete& delete_func,
        const CenterRange& center_range
    )
    {
        m_hovered_idx.reset();
        static const char *remove_button_str = DELETE_ICON;
        static const char *center_range_button_str = ICON_FA_LOCATION_CROSSHAIRS;

        const float line_height = ImGui::GetTextLineHeightWithSpacing();
        const float x_spacing = ImGui::GetStyle().ItemSpacing.x;
        const float label_width = ImGui::GetContentRegionAvail().x - 4 * x_spacing
            - ImGui::CalcTextSize(remove_button_str).x
            - ImGui::CalcTextSize(center_range_button_str).x;

        std::optional<std::size_t> delete_idx = std::nullopt;
        for (std::size_t i = 0; i < items.size(); i++) {
            widgets::ScopedImID idx_id_scope(static_cast<int>(i));

            const auto& annotation = items[i];
            std::string description = annotation_description(annotation);
            const bool is_active = m_active_idx == i;
            bool is_hovered = false;

            if (ImGui::Selectable(description.c_str(), is_active, 0, {label_width, line_height})) {
                if (is_active) {
                    m_active_idx.reset();
                } else {
                    m_active_idx = i;
                }
            }
            if (ImGui::IsItemHovered()) {
                is_hovered = true;
            }

            widgets::RoundedButtonStyleScope rounded_button;
            ImGui::SameLine();
            if (ImGui::Button(center_range_button_str)) {
                center_range(annotation);
            }
            ImGui::SetItemTooltip("Centre plot on annotation");
            if (ImGui::IsItemHovered()) {
                is_hovered = true;
            }

            ImGui::SameLine();
            {
                widgets::RedButtonColorScope red_button;
                if (ImGui::Button(remove_button_str)) {
                    delete_idx = i;
                }
            }
            if (ImGui::IsItemHovered()) {
                is_hovered = true;
            }
            ImGui::SetItemTooltip("Delete annotation");

            if (is_hovered) {
                m_hovered_idx = i;
            }
        }

        if (delete_idx.has_value()) {
            if (m_active_idx.has_value()) {
                if (*delete_idx < *m_active_idx) {
                    --(*m_active_idx);
                } else if (*delete_idx == *m_active_idx) {
                    m_active_idx.reset();
                }
            }
            delete_func(*delete_idx);
        }
    }

    template <typename T, typename GetRange, typename GetColor>
        requires std::convertible_to<std::invoke_result_t<GetRange, T>, const widgets::PlotRange&>
        && std::convertible_to<std::invoke_result_t<GetColor, T>, RgbColor>
    void draw_plot_ranges(
        const std::vector<T>& annotations,
        const GetRange& get_range,
        const GetColor& get_color,
        float height
    ) const
    {
        for (std::size_t i = 0; i < annotations.size(); i++) {
            const bool is_active = m_active_idx == i;
            const uint8_t alpha = is_active ?
                SelectedLabelAlpha :
                (m_hovered_idx == i ? HoveredLabelAlpha : UnselectedLabelAlpha);

            const auto& annotation = annotations[i];
            const widgets::PlotRange& range = is_active && m_range_dragger.is_editing() ?
                m_active_annotation_temp_range :
                get_range(annotation);
            widgets::draw_plot_range(range, get_color(annotation).with_alpha(alpha), height);
        }
    }

    const std::optional<std::size_t>& active_idx() const { return m_active_idx; }

    const widgets::PlotRange *creating_range() const { return m_range_selector.range(); }

private:
    static constexpr double MinSelectionDuration = 0.001;

    std::optional<std::size_t> m_active_idx = std::nullopt;
    std::optional<std::size_t> m_hovered_idx = std::nullopt;
    widgets::PlotRange m_active_annotation_temp_range = {NAN, NAN};
    widgets::PlotRangeDragger m_range_dragger;
    widgets::PlotRangeSelector m_range_selector;
};

class AudioAnnotator {
public:
    explicit AudioAnnotator(Annotator& annotator) : m_annotator(annotator) {}

    // Call inside BeginPlot/EndPlot
    void edit(const char *id)
    {
        m_ranges_editor.edit(
            id,
            [this](auto idx) { return ear_clicks()[idx]; },
            [this](const widgets::PlotRange& range) {
                m_annotator.execute_command<AddCommand>(range);
                return ear_clicks().size() - 1;
            },
            [this](auto idx, const auto& range) {
                m_annotator.execute_command<EditCommand>(idx, range);
            }
        );
    }

    // Call inside BeginPlot/EndPlot
    void draw_plot_ranges(float height = 0)
    {
        m_ranges_editor.draw_plot_ranges(
            ear_clicks(),
            [](const auto& range) { return range; },
            [](const auto&) { return LabelColor; },
            height
        );
        const auto *new_range = m_ranges_editor.creating_range();
        if (new_range) {
            auto color = LabelColor.with_alpha(UnselectedLabelAlpha);
            widgets::draw_plot_range(*new_range, color, height);
        }
    }

    template <typename CenterRange>
        requires std::invocable<CenterRange, const widgets::PlotRange&>
    void draw_labels_editor(const char *id, const CenterRange& center_range)
    {
        widgets::ScopedImID scoped_id(id);

        auto num_ear_clicks = ear_clicks().size();
        ImGui::SeparatorText(fmt::format("Audio labels [{}]", num_ear_clicks).c_str());
        if (num_ear_clicks == 0) {
            ImGui::TextWrapped("No audio labels: Ctrl + click on audio plot to add one");
        } else {
            draw_list_box(center_range);
        }
    }

private:
    using EditCommand = EditAnnotationCommand<widgets::PlotRange, &Annotation::ear_clicks>;
    using AddCommand = AddAnnotationCommand<widgets::PlotRange, &Annotation::ear_clicks>;
    using DeleteCommand = DeleteAnnotationCommand<widgets::PlotRange, &Annotation::ear_clicks>;

    template <typename CenterRange> void draw_list_box(const CenterRange& center_range)
    {
        if (ImGui::BeginListBox("##labels_list_box", {SizeFill, SizeFill})) {
            m_ranges_editor.draw_list_box_items(
                ear_clicks(),
                [](const auto& r) {
                    return fmt::format("[{:g}, {:g}], Δ = {:g} s", r.start, r.end, r.end - r.start);
                },
                [this](auto idx) { m_annotator.execute_command<DeleteCommand>(idx); },
                center_range
            );

            ImGui::EndListBox();
        }
    }

    const std::vector<widgets::PlotRange>& ear_clicks() const
    {
        return m_annotator.annotation().ear_clicks;
    }

    static constexpr RgbColor LabelColor{0xFC, 0x5A, 0xE1};

    PlotRangesEditor m_ranges_editor;

    Annotator& m_annotator;
};

class FlowAnnotator {
public:
    explicit FlowAnnotator(Annotator& annotator) : m_annotator(annotator) {}

    // Call inside BeginPlot/EndPlot
    void edit(const char *id)
    {
        m_ranges_editor.edit(
            id,
            [this](auto idx) { return flow_annotations()[idx].plot_range; },
            [this](const auto& range) {
                FlowAnnotation new_annotation = m_temp_annotation;
                new_annotation.plot_range = range;
                m_annotator.execute_command<AddCommand>(new_annotation);
                return flow_annotations().size() - 1;
            },
            [this](auto idx, const auto& range) {
                FlowAnnotation new_annotation = flow_annotations()[idx];
                new_annotation.plot_range = range;
                m_annotator.execute_command<EditCommand>(idx, new_annotation);
            }
        );
    }

    // Call inside BeginPlot/EndPlot
    void draw_plot_ranges(float height = 0) const
    {
        m_ranges_editor.draw_plot_ranges(
            flow_annotations(),
            [](const auto& annotation) { return annotation.plot_range; },
            [](const auto& annotation) { return choice_to_color(annotation.choice); },
            height
        );
        const auto *new_range = m_ranges_editor.creating_range();
        if (new_range) {
            auto color = choice_to_color(m_temp_annotation.choice);
            widgets::draw_plot_range(*new_range, color.with_alpha(UnselectedLabelAlpha), height);
        }
    }

    template <typename CenterRange>
        requires std::invocable<CenterRange, const FlowAnnotation&>
    void draw_labels_editor(const char *id, const CenterRange& center_range)
    {
        widgets::ScopedImID scoped_id(id);

        auto num_annotations = flow_annotations().size();
        ImGui::SeparatorText(fmt::format("Flow labels [{}]", num_annotations).c_str());
        draw_annotation_editor();

        if (num_annotations == 0) {
            ImGui::TextWrapped("No flow labels: Ctrl + click and drag on flow plot to add one");
        } else {
            draw_list_box(center_range);
        }
    }

private:
    using EditCommand = EditAnnotationCommand<FlowAnnotation, &Annotation::flow_annotations>;
    using AddCommand = AddAnnotationCommand<FlowAnnotation, &Annotation::flow_annotations>;
    using DeleteCommand = DeleteAnnotationCommand<FlowAnnotation, &Annotation::flow_annotations>;

    void draw_annotation_editor()
    {
        // TODO: should there be separate controls for creating a new label vs editing an existing
        // one?

        using enum FlowAnnotation::Choice;
        using Option = widgets::RadioButtonField<FlowAnnotation::Choice>;
        constexpr std::array Options = {
            Option("ex-ex [1]", ExEx, ImGuiKey_1),
            Option("ex-in [2]", ExIn, ImGuiKey_2),
            Option("in-ex [3]", InEx, ImGuiKey_3),
            Option("in-in [4]", InIn, ImGuiKey_4),
            Option("non-resp. flow [5]", Nrf, ImGuiKey_5),
        };

        const auto& active_idx = m_ranges_editor.active_idx();
        FlowAnnotation current_annotation =
            active_idx.has_value() ? flow_annotations()[*active_idx] : m_temp_annotation;

        bool changed = false;
        for (const auto& opt : Options) {
            FlowAnnotation::Choice choice = opt.value;
            bool selected = choice == current_annotation.choice;
            const bool radio_clicked = widgets::colored_radio_button(
                opt.label, selected, choice_to_color(choice).with_alpha(0xFF)
            );
            if (radio_clicked || widgets::global_shortcut(opt.key)) {
                if (!selected) {
                    current_annotation.choice = choice;
                    selected = true;
                    changed = true;
                    spdlog::info("Apnea SRC selection changed to {}", opt.label);
                }
            }
            if (selected && choice != Nrf) {
                ImGui::SameLine();
                if (ImGui::Checkbox("Ambiguous [a]", &current_annotation.is_ambiguous)
                    || widgets::global_shortcut_toggle(ImGuiKey_A, current_annotation.is_ambiguous))
                {
                    spdlog::info(
                        "Swallow apnea ambiguity changed: {}", current_annotation.is_ambiguous
                    );
                    changed = true;
                }
            }
        }

        if (changed) {
            if (active_idx.has_value()) {
                m_annotator.execute_command<EditCommand>(*active_idx, current_annotation);

                // Maintain the current label choice for new annotation, but without ambiguity
                m_temp_annotation.choice = current_annotation.choice;
                m_temp_annotation.is_ambiguous = false;
            } else {
                m_temp_annotation = current_annotation;
            }
        }
    }

    template <typename CenterRange> void draw_list_box(const CenterRange& center_range)
    {
        if (ImGui::BeginListBox("##flow-labels-list-box", {SizeFill, SizeFill})) {
            m_ranges_editor.draw_list_box_items(
                flow_annotations(),
                annotation_description,
                [this](std::size_t i) { m_annotator.execute_command<DeleteCommand>(i); },
                center_range
            );
            ImGui::EndListBox();
        }

        const auto& active_idx = m_ranges_editor.active_idx();
        if (active_idx.has_value()) {
            const auto& annotation = flow_annotations()[*active_idx];
            const char *err = annotation.error_description();
            if (err) {
                ImGui::TextWrapped(ERR_ICON ICON_TEXT_SPACE "%s", err);
            }
        }
    }

    static std::string annotation_description(const FlowAnnotation& annotation)
    {
        const auto& range = annotation.plot_range;
        return fmt::format(
            "{}{}{}, [{:g}, {:g}], Δ = {:g}",
            annotation.valid() ? "" : ERR_ICON ICON_TEXT_SPACE,
            magic_enum::enum_name(annotation.choice),
            annotation.choice != FlowAnnotation::Choice::Nrf && annotation.is_ambiguous ? "?" : "",
            range.start,
            range.end,
            range.start - range.end
        );
    }

    const std::vector<FlowAnnotation>& flow_annotations() const
    {
        return m_annotator.annotation().flow_annotations;
    }

    static RgbColor choice_to_color(FlowAnnotation::Choice choice)
    {
        using enum FlowAnnotation::Choice;
        switch (choice) {
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

    FlowAnnotation m_temp_annotation; // values used for new annotation
    PlotRangesEditor m_ranges_editor;
    Annotator& m_annotator;
};

class NotesEditor {
public:
    explicit NotesEditor(Annotator& annotator) : m_annotator(annotator) {}

    void draw(const char *id)
    {
        widgets::ScopedImID id_scope(id);
        ImGui::SeparatorText("Notes");

        draw_new_note_input();
        draw_notes_list(m_annotator.annotation().notes);
    }

private:
    // TODO: command to edit existing notes
    using AddCommand = AddAnnotationCommand<std::string, &Annotation::notes>;
    using DeleteCommand = DeleteAnnotationCommand<std::string, &Annotation::notes>;

    void draw_new_note_input()
    {
        static const char *btn_text = ICON_FA_PLUS ICON_TEXT_SPACE "Add";
        float button_width = ImGui::CalcTextSize(btn_text).x + 2 * ImGui::GetStyle().ItemSpacing.x;

        ImGui::PushItemWidth(-button_width);
        bool submitted = ImGui::InputText(
            "##new_note_input_text",
            &m_new_note_val,
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll
        );
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button(btn_text)) {
            submitted = true;
        }

        if (submitted) {
            auto trimmed = strutil::trimmed(m_new_note_val);
            if (!trimmed.empty()) {
                spdlog::info("Adding new note {:?}", trimmed);
                m_annotator.execute_command<AddCommand>(std::string(trimmed));
                m_new_note_val.clear();
            }
        }
    }

    void draw_notes_list(const std::vector<std::string>& notes)
    {
        std::optional<std::size_t> delete_idx = std::nullopt;
        for (std::size_t i = 0; i < notes.size(); i++) {
            widgets::ScopedImID id_scope(static_cast<int>(i));
            const auto& note = notes[i];
            {
                widgets::RedButtonColorScope red_button;
                widgets::RoundedButtonStyleScope rounded_button;
                if (ImGui::Button(DELETE_ICON)) {
                    delete_idx = i;
                }
                ImGui::SetItemTooltip("Delete");
            }

            // TODO: put the delete button to right of text?
            // TODO: truncate long note with ellipsis?
            ImGui::SameLine();
            ImGui::TextUnformatted(note.c_str());
            ImGui::SetItemTooltip("%s", note.c_str());
        }

        if (delete_idx.has_value()) {
            spdlog::info("Deleting note {:?}", notes[*delete_idx]);
            m_annotator.execute_command<DeleteCommand>(*delete_idx);
        }
    }

    std::string m_new_note_val;
    Annotator& m_annotator;
};

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
        m_save_annotation_callback(std::move(save_annotation_callback)),
        m_annotator(existing_annotation ? Annotation(*existing_annotation) : Annotation{})
    {}

    void draw() override
    {
        m_num_plot_points = 0;
        if (ImGui::Begin(TaskViewDataPlotsWindowId)) {
            draw_plots();
        }
        ImGui::End();

        if (ImGui::Begin(TaskViewLabelControlsWindowId)) {
            draw_labels_editor();
        }
        ImGui::End();
    }

    bool has_unsaved_changes() const override { return m_annotator.modified_since_last_save(); }

    bool can_save_annotation() const override { return m_annotator.annotation().valid(); }

    void save_annotation() const override
    {
        if (can_save_annotation()) {
            m_save_annotation_callback(m_annotator.annotation().to_annotation_model());
        } else {
            spdlog::error("Cannot save annotation");
        }
    }

    std::string debug_info() const override
    {
        return fmt::format("Plotting {} points.", m_num_plot_points);
    }

private:
    template <typename Function>
    void child_window(const char *name, float num_lines, const Function& draw)
    {
        constexpr ImGuiChildFlags ChildFlags = ImGuiChildFlags_ResizeY;
        float line_height = ImGui::GetTextLineHeightWithSpacing();
        if (ImGui::BeginChild(name, {SizeFill, num_lines * line_height}, ChildFlags)) {
            draw();
        }
        ImGui::EndChild();
    }

    void draw_labels_editor()
    {
        draw_save_button();
        draw_undo_redo();

        child_window("##event_child_window", 6, [this]() {
            ImGui::SeparatorText("Events");
            draw_event_list();
        });

        child_window("##flow_label_editor_child", 12, [this]() {
            m_flow_annotator.draw_labels_editor("##flow_labels_editor", [this](const auto& ann) {
                center_plots_on_range(ann.plot_range);
            });
        });

        child_window("##audio_label_editor_child", 8, [this]() {
            m_audio_annotator.draw_labels_editor("##audio_labels_editor", [this](const auto& r) {
                center_plots_on_range(r);
            });
        });

        child_window("##notes_editor_child", 8, [this]() {
            m_notes_editor.draw("##notes_editor");
        });
    }

    static bool draw_button_with_shortcut(
        const char *label, bool enabled, const char *tooltip, ImGuiKeyChord shortcut_chord
    )
    {
        ImGui::BeginDisabled(!enabled);
        bool clicked = ImGui::Button(label);
        if (enabled) {
            ImGui::SetItemTooltip("%s", tooltip);
            if (widgets::global_shortcut(shortcut_chord)) {
                clicked = true;
            }
        }
        ImGui::EndDisabled();
        return clicked;
    }

    void draw_undo_redo()
    {
        bool undo = draw_button_with_shortcut(
            ICON_FA_ARROW_ROTATE_LEFT ICON_TEXT_SPACE "Undo",
            m_annotator.can_undo_last_command(),
            "Ctrl+Z",
            ImGuiMod_Ctrl | ImGuiKey_Z
        );
        if (undo) {
            m_annotator.undo_last_command();
        }

        ImGui::SameLine();
        bool redo = draw_button_with_shortcut(
            ICON_FA_ARROW_ROTATE_RIGHT ICON_TEXT_SPACE "Redo",
            m_annotator.can_redo_last_undone_command(),
            "Ctrl+Y",
            ImGuiMod_Ctrl | ImGuiKey_Y
        );
        if (redo) {
            m_annotator.redo_last_undone_command();
        }
    }

    void draw_save_button()
    {
        const float height = ImGui::GetTextLineHeightWithSpacing() * 2;
        constexpr ImGuiKeyChord Shortcut = ImGuiMod_Ctrl | ImGuiKey_S;
        const bool can_save = m_annotator.modified_since_last_save() && can_save_annotation();
        ImGui::BeginDisabled(!can_save);
        if (ImGui::Button(ICON_FA_FLOPPY_DISK ICON_TEXT_SPACE "Save [Ctrl+S]", {SizeFill, height})
            || (can_save && widgets::global_shortcut(Shortcut)))
        {
            spdlog::info("Saving annotation");
            m_annotator.save_annotation(m_save_annotation_callback);
        }

        ImGui::EndDisabled();
    }

    // Range must have start/end fields (e.g. widgets::PlotRange, or models::TimeRange)
    template <typename Range> void center_plots_on_range(const Range& range, double margin = 3.0)
    {
        const auto& time = m_data.flow_time;
        m_plot_x_range = {
            std::max(time.front(), range.start - margin),
            std::min(time.back(), range.end + margin),
        };
    }

    void draw_event_list()
    {
        const auto& events = m_task_info.event_times;
        if (events.empty()) {
            ImGui::TextWrapped("No events defined for this task.");
            return;
        }

        ImGui::TextWrapped(HINT_ICON ICON_TEXT_SPACE "Click on event to centre it in plot");
        if (!ImGui::BeginListBox("##events_listbox", {SizeFill, SizeFill})) {
            return;
        }

        for (std::size_t i = 0; i < events.size(); i++) {
            const auto& event = events[i];
            std::string label = fmt::format("{}: [{:g}, {:g}]", i + 1, event.start, event.end);
            if (ImGui::Selectable(label.c_str())) {
                center_plots_on_range(event);
            }
        }
        ImGui::EndListBox();
    }

    void draw_plots()
    {
        constexpr float SummaryPlotHeight = 75;
        constexpr unsigned int NumPlots = 2;
        const float data_plot_height =
            ((ImGui::GetContentRegionAvail().y - SummaryPlotHeight) / NumPlots)
            - ImGui::GetStyle().ItemSpacing.y;

        if (ImPlot::BeginAlignedPlots("##aligned_plots")) {
            m_num_plot_points += draw_plot("##flow_plot", m_flow_plot, data_plot_height, [this]() {
                m_flow_annotator.edit("##flow_annotator_edit");
                m_audio_annotator.draw_plot_ranges(LabelSummaryHeight);
                m_flow_annotator.draw_plot_ranges();
            });
            m_num_plot_points +=
                draw_plot("##audio_plot", m_audio_plot, data_plot_height, [this]() {
                    m_flow_annotator.draw_plot_ranges(LabelSummaryHeight);
                    m_audio_annotator.draw_plot_ranges();
                    m_audio_annotator.edit("##audio_annotator_edit");
                });
            ImPlot::EndAlignedPlots();
        }

        if (ImPlot::BeginPlot(
                "##summary_plot", {SizeFill, SummaryPlotHeight}, ImPlotFlags_CanvasOnly
            ))
        {
            draw_plot_summary_selector();
            draw_event_labels();
            m_flow_annotator.draw_plot_ranges(LabelSummaryHeight);
            ImPlot::EndPlot();
        }
    }

    template <typename Func>
        requires std::invocable<Func>
    std::size_t draw_plot(const char *id, Plot& plot, float height, const Func& extra_draw) const
    {
        constexpr ImPlotFlags Flags =
            ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus;
        widgets::ScopedImID scoped_id(id);
        std::size_t num_points = 0;
        if (ImPlot::BeginPlot("##plot", {SizeFill, height}, Flags)) {
            num_points = plot.draw();
            draw_event_labels();
            extra_draw();
            ImPlot::EndPlot();
        }
        return num_points;
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
        constexpr ImPlotLineFlags LineFlags = 0;
        constexpr int PlotOffset = 0;

        ImPlot::SetupAxes(nullptr, nullptr, AxFlags, AxFlags);

        auto size = static_cast<int>(m_data.flow_time.size());
        int max_points = app::get_global_app_config().max_num_plot_points;
        int stride = std::max(size / max_points, 1);
        size /= stride;
        ImPlot::PlotLine(
            "##summary_flow_plot_line",
            m_data.flow_time.data(),
            m_data.flow.data(),
            size,
            LineFlags,
            PlotOffset,
            stride * static_cast<int>(sizeof(decltype(m_data.flow_time)::value_type))
        );
        m_num_plot_points += static_cast<std::size_t>(size);

        m_plot_summary_selector.update("##plot_summary_selector");
        const widgets::PlotRange *new_range = m_plot_summary_selector.range();
        if (new_range) {
            m_plot_x_range = *new_range;
        } else {
            m_plot_summary_dragger.update("##plot_summary_dragger", m_plot_x_range);
        }
        widgets::draw_plot_range(m_plot_x_range, SummaryColor);
    }

    std::size_t m_num_plot_points = 0;
    widgets::PlotRangeDragger m_plot_summary_dragger;
    widgets::PlotRangeSelector m_plot_summary_selector;

    models::SwallowTaskData m_data;
    models::SwallowTaskInfo m_task_info;
    widgets::PlotRange m_plot_x_range;
    Plot m_flow_plot;
    Plot m_audio_plot;
    SaveAnnotationCallback m_save_annotation_callback;
    Annotator m_annotator;
    FlowAnnotator m_flow_annotator{m_annotator};
    AudioAnnotator m_audio_annotator{m_annotator};
    NotesEditor m_notes_editor{m_annotator};
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
    spdlog::info("Loading task data {}...", path);
    auto task_data = loader.load_task_data(path);
    if (task_data.has_value()) {
        return std::make_unique<TaskLabellingView>(
            std::move(*task_data), task, existing_annotation, save_annotation_callback
        );
    }

    return std::make_unique<TaskLoadErrorView>(path);
}
}; // namespace recap::labeller::gui
