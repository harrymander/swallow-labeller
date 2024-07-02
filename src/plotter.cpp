#define IMGUI_DEFINE_MATH_OPERATORS

#include "plotter.hpp"

#include "data.hpp"
#include "drag-range.hpp"
#include "imgui-util.hpp"
#include "implot-util.hpp"
#include "labelling-task.hpp"
#include "optutil.hpp"
#include "strutil.hpp"
#include "util.hpp"

#include <IconsFontAwesome6.h>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <implot.h>
#include <implot_internal.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#define ERROR_ICON ICON_FA_TRIANGLE_EXCLAMATION
#define PLOT_HELP_ICON ICON_FA_LIGHTBULB

namespace recap::labeller::plotter {

using labeller::data::SwallowTaskData;
using namespace labeller::task;
using namespace imgui_util;

namespace {

// Add text in top left corner of plot
void add_plot_text(const char *str)
{
    ImPlot::PushPlotClipRect();
    ImPlot::GetPlotDrawList()->AddText(
        ImPlot::GetPlotPos() + ImGui::GetStyle().ItemSpacing,
        ImPlot::GetStyleColorU32(ImPlotCol_InlayText),
        str
    );
    ImPlot::PopPlotClipRect();
}

bool begin_data_plot(const char *id)
{
    return ImPlot::BeginPlot(
        id, {-1, 0}, ImPlotFlags_NoMouseText | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMenus
    );
}

void setup_axis_links(ImAxis axis, double *v1, double *v2)
{
    double *vmin;
    double *vmax;
    std::tie(vmin, vmax) = util::minmax_pointers(v1, v2);
    ImPlot::SetupAxisLinks(axis, vmin, vmax);
}

bool mouse_inside_plot()
{
    if (!ImGui::IsMousePosValid()) {
        return false;
    }

    const ImVec2 bbmin = ImPlot::GetPlotPos();
    const ImVec2 bbmax = bbmin + ImPlot::GetPlotSize();
    const ImVec2 pos = ImGui::GetMousePos();
    return pos.x >= bbmin.x && pos.x <= bbmax.x && pos.y >= bbmin.y && pos.y <= bbmax.y;
}

void add_plot_marker(ImDrawList *draw_list, const ImVec2& pos)
{
    constexpr float half_width = 4;
    draw_list->AddRect(
        ImVec2(pos.x - half_width, pos.y - half_width),
        ImVec2(pos.x + half_width, pos.y + half_width),
        ImColor(128, 128, 128)
    );
}

/**
 * Add text in position (xp, yp), automatically right-aligining text if it would be greater than
 * xend
 */
void add_text_autoalign(ImDrawList *draw_list, const char *text, float xp, float yp, float xend)
{
    constexpr float align_margin = 15;
    constexpr float padding = 6;
    const auto text_size = ImGui::CalcTextSize(text);
    if (xp + text_size.x + align_margin > xend) {
        xp -= text_size.x + padding;
    } else {
        xp += padding;
    }
    draw_list->AddText(ImVec2(xp, yp), ImGui::GetColorU32(ImGuiCol_Text), text);
}

void add_plot_vline(ImDrawList *draw_list, double xplot, double yplot, const ImVec2& pospx)
{
    const ImVec2 plot_pos = ImPlot::GetPlotPos();
    const ImVec2 plot_size = ImPlot::GetPlotSize();
    const ImVec2 top(pospx.x, plot_pos.y);
    const ImVec2 bottom(pospx.x, top.y + plot_size.y);
    draw_list->AddLine(top, bottom, ImColor(128, 128, 128));

    const float xend = plot_pos.x + plot_size.x;
    add_text_autoalign(
        draw_list,
        fmt::format("x={:g}", xplot).c_str(),
        bottom.x,
        bottom.y - ImGui::GetTextLineHeightWithSpacing(),
        xend
    );
    add_text_autoalign(draw_list, fmt::format("y={:g}", yplot).c_str(), top.x, top.y, xend);
}

void draw_plot_cursor(double xplot, double yplot)
{
    ImDrawList *draw_list = ImPlot::GetPlotDrawList();
    const auto pospx = ImPlot::PlotToPixels(xplot, yplot);
    add_plot_vline(draw_list, xplot, yplot, pospx);
    add_plot_marker(draw_list, pospx);
}

void draw_plot_hovered(const double *x, size_t n, const double *y)
{
    const auto mouse = ImPlot::GetPlotMousePos();
    if (mouse.x > x[0]) {
        const double *const end = x + n;
        const double *xclosest = util::binary_search_closest(x, end, mouse.x);
        if (xclosest != end) {
            draw_plot_cursor(*xclosest, y[xclosest - x]);
        }
    }
}

void plot_line(const char *id, const std::vector<double>& x, const std::vector<double>& y)
{
    ImPlot::PlotLine(id, x.data(), y.data(), static_cast<int>(y.size()));
}

template <class T> struct RadioButtonField {
    const char *label;
    T value;
    ImGuiKey key;

    RadioButtonField(const char *label, T value, ImGuiKey key = ImGuiKey_None) :
        label(label), value(value), key(key)
    {}
};

template <class T>
bool radio_button_enums(const char *id, T& value, const RadioButtonField<T> *fields, std::size_t n)
{
    ScopedImID scoped_id(id);
    bool changed = false;
    for (std::size_t i = 0; i < n; i++) {
        if (i > 0) {
            ImGui::SameLine();
        }
        const RadioButtonField<T>& field = fields[i];
        const bool enabled = field.value == value;
        const bool pressed = ImGui::RadioButton(field.label, enabled);
        if (!enabled
            && (pressed
                || (field.key != ImGuiKey_None && !item_disabled() && ImGui::Shortcut(field.key))))
        {
            value = field.value;
            changed = true;
            spdlog::debug("Radio buttons {}: changed to '{}'", id, field.label);
        }
    }

    return changed;
}

template <class T, class Container>
bool radio_button_enums(const char *id, T& value, const Container& options)
{
    return radio_button_enums(id, value, options.data(), options.size());
}

const char *src_pattern_str(SRCPattern pattern)
{
    using enum SRCPattern;
    switch (pattern) {
    case ExEx:
        return "ex-ex";
    case ExIn:
        return "ex-in";
    case InEx:
        return "in-ex";
    case InIn:
        break;
    }
    return "in-in";
}

bool radio_button_src_patterns(const char *id, SRCPattern& pattern)
{
    using enum SRCPattern;
    static std::array<RadioButtonField<SRCPattern>, 4> fields = {{
        {"ex-ex [1]", ExEx, ImGuiKey_1},
        {"ex-in [2]", ExIn, ImGuiKey_2},
        {"in-ex [3]", InEx, ImGuiKey_3},
        {"in-in [4]", InIn, ImGuiKey_4},
    }};
    return radio_button_enums(id, pattern, fields);
}

bool radio_button_swallow_label_info(const char *id, SwallowLabelInfo& info)
{
    static std::array<RadioButtonField<SwallowLabelInfo>, 5> fields = {{
        {"Ok", SwallowLabelInfo::Ok},
        {"Ambiguous [a]", SwallowLabelInfo::AmbiguousPattern, ImGuiKey_A},
        {"No swallow", SwallowLabelInfo::NoSwallow},
        {"Apnoea cut-off", SwallowLabelInfo::ApneaCutOff},
        {"Flow error", SwallowLabelInfo::FlowError},
    }};
    return radio_button_enums(id, info, fields);
}

bool radio_button_earclick_label_info(const char *id, EarClickLabelInfo& info)
{
    static std::array<RadioButtonField<EarClickLabelInfo>, 3> fields = {{
        {"Ok [e]", EarClickLabelInfo::Ok, ImGuiKey_E},
        {"No ear click [w]", EarClickLabelInfo::NoEarClick, ImGuiKey_W},
        {"Audio error", EarClickLabelInfo::AudioError},
    }};
    return radio_button_enums(id, info, fields);
}

void text_input_trim(const char *label, std::string& text, std::optional<std::string>& output)
{
    ImGui::InputText(label, &text);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        std::string trimmed = strutil::trimmed(text);
        if (trimmed.empty()) {
            spdlog::debug("Text input '{}' cleared", label);
            output = std::nullopt;
        } else {
            spdlog::debug("Text input '{}' = {}", label, trimmed);
            output.emplace(std::move(trimmed));
        }
    }
}

bool range_isnan(const ImPlotRange& range)
{
    return std::isnan(range.Min) && std::isnan(range.Max);
}

std::vector<ImPlotRange> range_vector_from_time_ranges(const std::vector<TimeRange>& ranges)
{
    std::vector<ImPlotRange> result;
    result.reserve(ranges.size());
    for (const auto& range : ranges) {
        const auto [start, end] = std::minmax(range.start, range.end);
        result.emplace_back(start, end);
    }
    return result;
}

// Container for managing TimeRange/ImPlotRange
class SelectionRanges {
private:
    std::vector<TimeRange>& time_ranges;
    std::vector<ImPlotRange> plot_ranges_;

public:
    explicit SelectionRanges(std::vector<TimeRange>& time_ranges) :
        time_ranges(time_ranges), plot_ranges_(range_vector_from_time_ranges(time_ranges))
    {}

    [[nodiscard]] const std::vector<ImPlotRange>& plot_ranges() const { return plot_ranges_; }

    [[nodiscard]] std::vector<ImPlotRange>& plot_ranges() { return plot_ranges_; }

    [[nodiscard]] std::size_t size() const { return time_ranges.size(); }

    void push_back(const ImPlotRange& range)
    {
        time_ranges.emplace_back(range.Min, range.Max);
        plot_ranges_.push_back(range);
    }

    void erase_at(std::size_t index)
    {
        time_ranges.erase(
            time_ranges.begin() + static_cast<std::vector<TimeRange>::difference_type>(index)
        );
        plot_ranges_.erase(
            plot_ranges_.begin() + static_cast<std::vector<ImPlotRange>::difference_type>(index)
        );
    }
};

class PlotSelectionsEditor {
private:
    std::string name;
    ImColor color;
    ImColor hovered_color;
    ImColor selected_color;
    SelectionRanges ranges;
    std::vector<std::string> labels;

    plot::PlotXSelector selector;
    ImPlotRange next_range = {NAN, NAN};
    std::optional<std::size_t> selected_index = std::nullopt;
    std::optional<std::size_t> hovered_index = std::nullopt;

    void remove_selection(std::size_t i)
    {
        if (selected_index.has_value()) {
            if (*selected_index == i) {
                selected_index.reset();
            } else if (i < *selected_index) {
                *selected_index -= 1;
            }
        }

        const auto& range = ranges.plot_ranges()[i];
        spdlog::debug("{}: removing label [{}, {}] (#{})", name, range.Min, range.Max, i + 1);
        ranges.erase_at(i);
        set_labels();
    }

    [[nodiscard]] std::string label_str(std::size_t i) const
    {
        return fmt::format("{} #{}", name, i + 1);
    }

    void set_labels()
    {
        labels.clear();
        labels.reserve(ranges.size());
        for (std::size_t i = 0; i < ranges.size(); i++) {
            labels.push_back(label_str(i));
        }
    }

public:
    PlotSelectionsEditor(
        std::string_view name,
        ImColor color,
        ImColor hovered_color,
        ImColor selected_color,
        std::vector<TimeRange>& time_ranges
    ) :
        name(name),
        color(color),
        hovered_color(hovered_color),
        selected_color(selected_color),
        ranges(time_ranges)
    {
        set_labels();
    }

    void draw_plot_selection(const char *id)
    {
        ScopedImID scoped_id(id);
        if (selector.is_selecting() || range_isnan(next_range)) {
            const bool finished =
                selector.draw(0, next_range, color, 0, ImGuiMouseButton_Left, ImGuiKey_LeftCtrl);

            if (finished) {
                spdlog::debug("{}: placed new label: [{}, {}]", id, next_range.Min, next_range.Max);
                selected_index = ranges.size();
                ranges.push_back(next_range);
                set_labels();
                next_range = {NAN, NAN};
            }
        }

        for (std::size_t i = 0; i < ranges.size(); i++) {
            const bool selected = optutil::value_and_equal(selected_index, i);
            plot::drag_xrange(
                static_cast<ImGuiID>(i) + 1,
                ranges.plot_ranges()[i],
                selected ? selected_color :
                           (optutil::value_and_equal(hovered_index, i) ? hovered_color : color),
                selected && !selector.is_selecting() ? plot::DragXRangeFlag::None :
                                                       plot::DragXRangeFlag::NoInput
            );
        }
    }

    void draw_regions_readonly() const
    {
        for (std::size_t i = 0; i < ranges.size(); i++) {
            const bool selected = optutil::value_and_equal(selected_index, i);
            const ImPlotRange& range = ranges.plot_ranges()[i];
            implot_util::plot_vspan(
                range,
                selected ? selected_color :
                           (optutil::value_and_equal(hovered_index, i) ? hovered_color : color)
            );
        }
    }

    void draw_list(const char *id)
    {
        static const char *remove_button_str = ICON_FA_TRASH_CAN;
        constexpr float ButtonCornerRadius = 5;
        constexpr ImVec2 SelectableTextAlign = {0, 0.5};
        ScopedImStyle styles{
            {ImGuiStyleVar_SelectableTextAlign, SelectableTextAlign},
            {ImGuiStyleVar_FrameRounding, ButtonCornerRadius},
        };

        const float label_height = ImGui::GetTextLineHeightWithSpacing();
        const float label_width = ImGui::GetContentRegionAvail().x
            - (ImGui::CalcTextSize(remove_button_str).x + 2 * ImGui::GetStyle().ItemSpacing.x);

        ScopedImID scoped_id(id);
        hovered_index.reset();
        for (std::size_t i = 0; i < ranges.size(); i++) {
            ScopedImID task_id(static_cast<int>(i));
            const bool selected = optutil::value_and_equal(selected_index, i);
            if (ImGui::Selectable(labels[i].c_str(), selected, 0, {label_width, label_height})) {
                if (selected) {
                    selected_index.reset();
                } else {
                    selected_index = i;
                }
                const auto& range = ranges.plot_ranges()[i];
                spdlog::debug(
                    "{}: {} label #{} [{}, {}]",
                    name,
                    selected ? "deselected" : "selected",
                    i + 1,
                    range.Min,
                    range.Max
                );
            }
            if (ImGui::IsItemHovered()) {
                hovered_index = i;
            }

            ImGui::SameLine();
            if (ButtonRed(remove_button_str)) {
                remove_selection(i);
            }
            if (ImGui::IsItemHovered()) {
                hovered_index = i;
            }
        }
    }

    [[nodiscard]] const std::vector<ImPlotRange>& plot_ranges() const
    {
        return ranges.plot_ranges();
    }
};

}; // namespace

class SwallowTaskPlotter::AnnotationEditor {
private:
    static constexpr ImColor ApneaLabelColorSelecting = ImColor(1.0F, 1.0F, 0.0F, 0.1F);
    static constexpr ImColor ApneaLabelColorSelected = ImColor(1.0F, 1.0F, 0.0F, 0.4F);
    static constexpr ImColor EarclickLabelColor = ImColor(0.0F, 1.0F, 0.0F, 0.1F);
    static constexpr ImColor EarclickLabelColorHovered = ImColor(0.0F, 1.0F, 0.0F, 0.25F);
    static constexpr ImColor EarclickLabelColorSelected = ImColor(0.0F, 1.0F, 0.0F, 0.4F);

    static constexpr ImColor ApneaLabelSummaryColor = ApneaLabelColorSelected;
    static constexpr ImColor EarclickLabelSummaryColor = EarclickLabelColorSelected;

    static constexpr float LabelSummaryHeight = 8; // Same as default ImPlotStyle::DigitalBitHeight

    ImPlotRange apnea_range = {NAN, NAN};
    plot::PlotXSelector apnea_selector;

    SwallowAnnotation annotation_;
    SRCPattern src_pattern;
    std::string swallow_notes;
    std::string ear_click_notes;
    PlotSelectionsEditor earclick_selections;

    [[nodiscard]] bool draw_apnea_selector()
    {
        const bool new_label = apnea_selector.draw(
            0, apnea_range, ApneaLabelColorSelecting, 0, ImGuiMouseButton_Left, ImGuiKey_LeftCtrl
        );
        if (new_label) {
            spdlog::debug(
                "Placed new swallow apnea label: [{}, {}]", apnea_range.Min, apnea_range.Max
            );
        }
        return new_label;
    }

    [[nodiscard]] bool valid_apnea_label() const
    {
        using enum SwallowLabelInfo;
        const SwallowLabelInfo info = annotation_.swallow_info;
        return !(info == Ok || info == AmbiguousPattern) || apnea_selected();
    }

    [[nodiscard]] bool swallow_label_requires_note() const
    {
        using enum SwallowLabelInfo;
        const auto info = annotation_.swallow_info;
        return (info == FlowError || info == ApneaCutOff) && !annotation_.swallow_notes.has_value();
    }

    [[nodiscard]] bool earclick_label_requires_note() const
    {
        return annotation_.ear_click_info == EarClickLabelInfo::AudioError
            && !annotation_.ear_click_notes.has_value();
    }

    void update_apnea_label()
    {
        if (!valid_apnea_label()) {
            return;
        }
        if (can_edit_apnea_label()) {
            TimeRange time_range{apnea_range.Min, apnea_range.Max};
            annotation_.swallow_apnea.emplace(time_range, src_pattern);
            spdlog::debug(
                "Updated swallow apnea label ({}, {}), pattern={}",
                time_range.start,
                time_range.end,
                src_pattern_str(src_pattern)
            );
        } else {
            annotation_.swallow_apnea.reset();
            spdlog::debug("Cleared swallow apnea label");
        }
    }

    static void draw_label_summary(const ImPlotRange& range, const ImColor& color)
    {
        implot_util::plot_vspan(range.Min, range.Max, color, LabelSummaryHeight);
    }

public:
    explicit AnnotationEditor(SwallowAnnotation annotation = {}) :
        annotation_(std::move(annotation)),
        src_pattern(optutil::map_or(
            annotation_.swallow_apnea, [](const auto& a) { return a.pattern; }, SRCPattern{}
        )),
        swallow_notes(optutil::value_or_default(annotation_.swallow_notes)),
        ear_click_notes(optutil::value_or_default(annotation_.ear_click_notes)),
        earclick_selections(
            "Ear click",
            EarclickLabelColor,
            EarclickLabelColorHovered,
            EarclickLabelColorSelected,
            annotation_.ear_clicks
        )
    {
        if (annotation_.swallow_apnea.has_value()) {
            const auto& region = annotation_.swallow_apnea->time;
            std::tie(apnea_range.Min, apnea_range.Max) = std::minmax(region.start, region.end);
        }
    }

    [[nodiscard]] const SwallowAnnotation& annotation() const { return annotation_; }

    [[nodiscard]] bool is_valid() const
    {
        return valid_apnea_label() && !earclick_label_requires_note()
            && !swallow_label_requires_note();
    }

    [[nodiscard]] bool can_add_earclick_labels() const
    {
        return annotation_.ear_click_info == EarClickLabelInfo::Ok;
    }

    [[nodiscard]] bool apnea_selected() const { return !range_isnan(apnea_range); }

    [[nodiscard]] bool can_edit_apnea_label() const
    {
        using enum SwallowLabelInfo;
        const SwallowLabelInfo info = annotation_.swallow_info;
        return info == Ok || info == AmbiguousPattern;
    }

    [[nodiscard]] bool can_add_apnea_label() const
    {
        return can_edit_apnea_label() && !apnea_selected();
    }

    void draw_swallow_label_info()
    {
        if (radio_button_swallow_label_info("##swallow_label_info", annotation_.swallow_info)) {
            update_apnea_label();
        }
        if (swallow_label_requires_note()) {
            ImGui::SameLine();
            ImGui::TextUnformatted(ERROR_ICON "  Please add note explaining error!");
        }
    }

    void draw_swallow_apnea_info()
    {
        if (can_edit_apnea_label()) {
            const bool invalid = !apnea_selected();
            if (invalid) {
                ImGui::SameLine();
                ImGui::TextUnformatted(ERROR_ICON "  Error: apnea label is required!");
            }
            ImGui::BeginDisabled(invalid);
        } else {
            ImGui::BeginDisabled();
        }
        if (radio_button_src_patterns("##src_pattern", src_pattern)) {
            update_apnea_label();
        }
        ImGui::EndDisabled();
    }

    void draw_earclick_label_info()
    {
        radio_button_earclick_label_info("##earclick_label_info", annotation_.ear_click_info);
        if (earclick_label_requires_note()) {
            ImGui::SameLine();
            ImGui::TextUnformatted(ERROR_ICON "  Please add note explaining error!");
        }
    }

    void draw_earclick_notes()
    {
        text_input_trim("Ear click notes", ear_click_notes, annotation_.ear_click_notes);
    }

    void draw_swallow_notes()
    {
        text_input_trim("Swallow notes", swallow_notes, annotation_.swallow_notes);
    }

    void draw_apnea_selection()
    {
        if (!can_edit_apnea_label()) {
            return;
        }

        ScopedImID scoped_id("##apnea_selection");
        bool changed = false;
        if (apnea_selector.is_selecting() || !apnea_selected()) {
            changed = draw_apnea_selector();
        }
        if (apnea_selected()) {
            if (plot::drag_xrange(0, apnea_range, ApneaLabelColorSelected)) {
                changed = true;
            }
        }
        if (changed) {
            update_apnea_label();
        }
    }

    void draw_apnea_regions_summary() const
    {
        if (can_edit_apnea_label() && apnea_selected()) {
            draw_label_summary(apnea_range, ApneaLabelSummaryColor);
        }
    }

    void draw_earclick_regions_summary() const
    {
        if (can_add_earclick_labels()) {
            for (const auto& range : earclick_selections.plot_ranges()) {
                draw_label_summary(range, EarclickLabelSummaryColor);
            }
        }
    }

    void draw_earclick_selection()
    {
        if (!can_add_earclick_labels()) {
            return;
        }

        earclick_selections.draw_plot_selection("##earclick_selections");
    }

    void draw_earclick_selection_list()
    {
        ImGui::BeginDisabled(!can_add_earclick_labels());
        if (ImGui::BeginListBox("##earclick_selection_list", {-1, -1})) {
            earclick_selections.draw_list("##earclick_selection_list_items");

            ImGui::EndListBox();
        }
        ImGui::EndDisabled();
    }

    void draw_regions_readonly() const
    {
        if (can_edit_apnea_label()) {
            implot_util::plot_vspan(apnea_range, ApneaLabelColorSelecting);
        }

        if (can_add_earclick_labels()) {
            earclick_selections.draw_regions_readonly();
        }
    }
};

ImPlotRange SwallowTaskPlotter::initial_range(
    const std::vector<double>& time, const std::vector<uint8_t>& event
)
{
    constexpr double EventBufferSecs = 6;
    constexpr auto is_non_zero = [](auto e) { return e != 0; };

    const auto& event_start = std::find_if(event.begin(), event.end(), is_non_zero);
    if (event_start == event.end()) {
        return {time.front(), time.back()};
    }
    const double event_start_time = time[std::distance(event.begin(), event_start)];

    const auto& event_end = std::find_if(event.rbegin(), event.rend(), is_non_zero);
    double end_time;
    if (event_end == event.rend()) {
        end_time = time.back();
    } else {
        const double event_end_time = time[std::distance(event.begin(), event_end.base()) - 1];
        end_time = std::min(time.back(), event_end_time + EventBufferSecs);
    }

    return {
        std::max(event_start_time - EventBufferSecs, time.front()),
        end_time,
    };
}

SwallowTaskPlotter::SwallowTaskPlotter(SwallowTaskData data_, SwallowAnnotation annotation) :
    data(std::move(data_)),
    event(data.event.begin(), data.event.end()),
    summary_range(initial_range(data.flow_time, data.event)),
    annotation_editor(std::make_unique<AnnotationEditor>(std::move(annotation)))
{}

SwallowTaskPlotter::~SwallowTaskPlotter() noexcept = default;
SwallowTaskPlotter::SwallowTaskPlotter(SwallowTaskPlotter&&) noexcept = default;
SwallowTaskPlotter& SwallowTaskPlotter::operator=(SwallowTaskPlotter&&) noexcept = default;

void SwallowTaskPlotter::draw(const char *id)
{
    ScopedImID scoped_id(id);

    const float plotter_width = ImGui::GetContentRegionAvail().x / 6;
    if (ImGui::BeginChild("##plotter", {-plotter_width, -1}, ImGuiChildFlags_ResizeX)) {
        draw_plots();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("##earclick_selection_list", {-1, -1}, ImGuiChildFlags_Border)) {
        ImGui::TextUnformatted("Ear click labels:");
        annotation_editor->draw_earclick_selection_list();
    }
    ImGui::EndChild();
}

const SwallowAnnotation& SwallowTaskPlotter::annotation() const
{
    return annotation_editor->annotation();
}

void SwallowTaskPlotter::reset_annotation()
{
    annotation_editor = std::make_unique<AnnotationEditor>();
}

bool SwallowTaskPlotter::valid_annotation() const
{
    return annotation_editor->is_valid();
}

void SwallowTaskPlotter::draw_plots()
{
    if (ImPlot::BeginAlignedPlots("##aligned_plots")) {
        annotation_editor->draw_swallow_label_info();
        annotation_editor->draw_swallow_apnea_info();
        if (begin_data_plot("##flow")) {
            draw_flow_plot();
            annotation_editor->draw_apnea_selection();
            if (annotation_editor->can_add_apnea_label()) {
                add_plot_text(PLOT_HELP_ICON
                              "  Hold Ctrl and left click and drag to add apnea label");
            }
            annotation_editor->draw_earclick_regions_summary();
            ImPlot::EndPlot();
        }
        annotation_editor->draw_swallow_notes();

        annotation_editor->draw_earclick_label_info();
        if (begin_data_plot("##ear_audio")) {
            ImPlot::SetupAxis(ImAxis_X1, "Time (s)");
            draw_audio_plot();
            annotation_editor->draw_earclick_selection();
            if (annotation_editor->can_add_earclick_labels()) {
                add_plot_text(PLOT_HELP_ICON
                              "  Hold Ctrl and left click and drag to add ear click label(s)");
            }
            annotation_editor->draw_apnea_regions_summary();
            ImPlot::EndPlot();
        }
        annotation_editor->draw_earclick_notes();

        ImPlot::EndAlignedPlots();
    }

    if (ImPlot::BeginPlot("##summary", ImVec2(-1, 75), ImPlotFlags_CanvasOnly)) {
        draw_summary_plot();
        annotation_editor->draw_regions_readonly();
        ImPlot::EndPlot();
    }
}

void SwallowTaskPlotter::plot_event_digital() const
{
    ImPlot::PlotDigital(
        "##event", data.flow_time.data(), event.data(), static_cast<int>(event.size())
    );
}

void SwallowTaskPlotter::plot_data(
    const char *id, const std::vector<double>& x, const std::vector<double>& y, const char *ylabel
)
{
    ImPlot::SetupAxis(ImAxis_Y1, ylabel, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, x[0], x.back());
    setup_axis_links(ImAxis_X1, &summary_range.Min, &summary_range.Max);
    plot_line(id, x, y);
    plot_event_digital();
    if (mouse_inside_plot()) {
        draw_plot_hovered(x.data(), x.size(), y.data());
    }
}

void SwallowTaskPlotter::draw_flow_plot()
{
    plot_data("##flow_plot_line", data.flow_time, data.flow, "Flow rate (L/min)");
}

void SwallowTaskPlotter::draw_audio_plot()
{
    plot_data("##audio_plot_line", data.audio_time, data.audio, "Ear audio (V)");
}

void SwallowTaskPlotter::draw_summary_plot()
{
    constexpr ImPlotAxisFlags ax_flags = ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit;
    ImPlot::SetupAxes(nullptr, nullptr, ax_flags, ax_flags);

    constexpr ImColor summary_color = {.5F, .5F, .5F, .6F};
    summary_selector.draw(
        0, summary_range, summary_color, plot::PlotXSelector::NoCursor, ImGuiMouseButton_Left
    );
    if (!summary_selector.is_selecting()) {
        plot::drag_xrange(0, summary_range, summary_color);
    }

    plot_line("##summary_flow_plot_line", data.flow_time, data.flow);
    plot_event_digital();
}
}; // namespace recap::labeller::plotter
