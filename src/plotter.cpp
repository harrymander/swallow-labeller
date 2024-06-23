#include <sstream>
#define IMGUI_DEFINE_MATH_OPERATORS

#include "data.hpp"
#include "drag-range.hpp"
#include "labelling-task.hpp"
#include "plotter.hpp"
#include "util.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <implot.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <optional>
#include <vector>

namespace recap::labeller::plotter {

using plot::SwallowTaskData;
using namespace labelling_task;

namespace {

ImPlotRange initial_range(const std::vector<double>& time, const std::vector<uint8_t>& event)
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

void add_plot_vline(ImDrawList *draw_list, const ImVec2& posplot, const ImVec2& pospx)
{
    const ImVec2 plot_pos = ImPlot::GetPlotPos();
    const ImVec2 plot_size = ImPlot::GetPlotSize();
    const ImVec2 top(pospx.x, plot_pos.y);
    const ImVec2 bottom(pospx.x, top.y + plot_size.y);
    draw_list->AddLine(top, bottom, ImColor(128, 128, 128));

    const float xend = plot_pos.x + plot_size.x;
    char xtext[20];
    (void) std::snprintf(xtext, sizeof(xtext), "x=%g", posplot.x);
    add_text_autoalign(
        draw_list, xtext, bottom.x, bottom.y - ImGui::GetTextLineHeightWithSpacing(), xend
    );

    char ytext[20];
    (void) std::snprintf(ytext, sizeof(ytext), "y=%g", posplot.y);
    add_text_autoalign(draw_list, ytext, top.x, top.y, xend);
}

void draw_plot_cursor(float xplot, float yplot)
{
    ImDrawList *draw_list = ImPlot::GetPlotDrawList();
    const auto pospx = ImPlot::PlotToPixels(xplot, yplot);
    add_plot_vline(draw_list, ImVec2(xplot, yplot), pospx);
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
};

template <class T>
bool radio_button_enums(const char *id, T& value, const RadioButtonField<T> *options, std::size_t n)
{
    ImGui::PushID(id);
    bool changed = false;
    for (std::size_t i = 0; i < n; i++) {
        if (i > 0) {
            ImGui::SameLine();
        }

        const bool enabled = options[i].value == value;
        if (ImGui::RadioButton(options[i].label, enabled) && !enabled) {
            value = options[i].value;
            changed = true;
            spdlog::debug("Radio buttons {}: changed to '{}'", id, options[i].label);
        }
    }
    ImGui::PopID();
    return changed;
}

template <class T, class Container>
bool radio_button_enums(const char *id, T& value, const Container& options)
{
    return radio_button_enums(id, value, options.data(), options.size());
}

bool radio_button_src_patterns(const char *id, SRCPattern& pattern)
{
    static std::array<RadioButtonField<SRCPattern>, 4> fields = {{
        {"ex-ex", SRCPattern::ExEx},
        {"ex-in", SRCPattern::ExIn},
        {"in-ex", SRCPattern::InEx},
        {"in-in", SRCPattern::InIn},
    }};
    return radio_button_enums(id, pattern, fields);
}

bool radio_button_swallow_label_info(const char *id, SwallowLabelInfo& info)
{
    static std::array<RadioButtonField<SwallowLabelInfo>, 4> fields = {{
        {"Ok", SwallowLabelInfo::Ok},
        {"No swallow", SwallowLabelInfo::NoSwallow},
        {"Apnoea cut-off", SwallowLabelInfo::ApneaCutOff},
        {"Flow error", SwallowLabelInfo::FlowError},
    }};
    return radio_button_enums(id, info, fields);
}

bool radio_button_earclick_label_info(const char *id, EarClickLabelInfo& info)
{
    static std::array<RadioButtonField<EarClickLabelInfo>, 3> fields = {{
        {"Ok", EarClickLabelInfo::Ok},
        {"No ear click", EarClickLabelInfo::NoEarClick},
        {"Audio error", EarClickLabelInfo::AudioError},
    }};
    return radio_button_enums(id, info, fields);
}

void text_input_trim(const char *label, std::string& text, std::optional<std::string>& output)
{
    ImGui::InputText(label, &text);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        std::string trimmed = util::trimmed(text);
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

}; // namespace

SwallowAnnotationEditor::SwallowAnnotationEditor(SwallowAnnotation annotation_) :
    annotation(std::move(annotation_)),
    src_pattern(
        annotation.swallow_apnea.has_value() ? annotation.swallow_apnea->pattern : SRCPattern{}
    ),
    is_ambiguous(
        annotation.swallow_apnea.has_value() ? annotation.swallow_apnea->is_ambiguous : false
    ),
    swallow_notes(util::value_or_default(annotation.swallow_notes)),
    ear_click_notes(util::value_or_default(annotation.ear_click_notes)),
    earclick_selections(
        "Ear click",
        EarclickLabelColor,
        EarclickLabelColorHovered,
        EarclickLabelColorSelected,
        annotation.ear_clicks
    )
{
    if (annotation.swallow_apnea.has_value()) {
        const auto& region = annotation.swallow_apnea->time;
        std::tie(apnea_range.Min, apnea_range.Max) = std::minmax(region.start, region.end);
    }
}

void SwallowAnnotationEditor::draw_swallow_label_info()
{
    radio_button_swallow_label_info("##swallow_label_info", annotation.swallow_info);
}

void SwallowAnnotationEditor::draw_swallow_apnea_info()
{
    ImGui::BeginDisabled(annotation.swallow_info != SwallowLabelInfo::Ok);
    radio_button_src_patterns("##src_pattern", src_pattern);
    ImGui::SameLine();
    ImGui::Checkbox("Ambiguous swallow", &is_ambiguous);
    ImGui::EndDisabled();
}

void SwallowAnnotationEditor::draw_earclick_label_info()
{
    radio_button_earclick_label_info("##earclick_label_info", annotation.ear_click_info);
}

void SwallowAnnotationEditor::draw_earclick_notes()
{
    text_input_trim("Ear click notes", ear_click_notes, annotation.ear_click_notes);
}

void SwallowAnnotationEditor::draw_swallow_notes()
{
    text_input_trim("Swallow notes", swallow_notes, annotation.swallow_notes);
}

bool SwallowAnnotationEditor::draw_apnea_selector()
{
    const bool new_label = apnea_selector.draw(
        0, apnea_range, ApneaLabelColor, 0, ImGuiMouseButton_Left, ImGuiKey_LeftCtrl
    );
    if (new_label) {
        spdlog::debug("Placed new swallow apnea label: [{}, {}]", apnea_range.Min, apnea_range.Max);
    }
    return new_label;
}

void SwallowAnnotationEditor::draw_apnea_selection()
{
    if (annotation.swallow_info != SwallowLabelInfo::Ok) {
        return;
    }
    ImGui::PushID("##apnea_selection");
    if (!(apnea_selector.is_selecting() || range_isnan(apnea_range)) || draw_apnea_selector()) {
        plot::drag_xrange(0, apnea_range, ApneaLabelColor);
    }
    ImGui::PopID();
}

void SwallowAnnotationEditor::draw_earclick_selection()
{
    if (annotation.ear_click_info != EarClickLabelInfo::Ok) {
        return;
    }

    earclick_selections.draw_plot_selection("##earclick_selections");
}

void SwallowAnnotationEditor::draw_earclick_selection_list()
{
    earclick_selections.draw_list("##earclick_selection_list");
}

PlotSelections::PlotSelections(
    std::string_view name,
    ImColor color,
    ImColor hovered_color,
    ImColor selected_color,
    const std::vector<TimeRange>& ranges
) :
    name(name),
    color(color),
    hovered_color(hovered_color),
    selected_color(selected_color),
    ranges(range_vector_from_time_ranges(ranges))
{
    set_labels();
}

void PlotSelections::set_labels()
{
    labels.clear();
    labels.reserve(ranges.size());
    for (std::size_t i = 0; i < ranges.size(); i++) {
        std::stringstream ss;
        ss << name << " #" << i + 1;
        labels.push_back(ss.str());
    }
}

void PlotSelections::draw_plot_selection(const char *id)
{
    ImGui::PushID(id);
    if (selector.is_selecting() || range_isnan(next_range)) {
        const bool finished =
            selector.draw(0, next_range, color, 0, ImGuiMouseButton_Left, ImGuiKey_LeftCtrl);

        if (finished) {
            spdlog::debug("{}: placed new label: [{}, {}]", id, next_range.Min, next_range.Max);
            ranges.push_back(next_range);
            set_labels();
            next_range = {NAN, NAN};
        }
    }

    for (std::size_t i = 0; i < ranges.size(); i++) {
        const bool selected = util::has_value_and_equal(selected_index, i);
        plot::drag_xrange(
            static_cast<ImGuiID>(i) + 1,
            ranges[i],
            selected ? selected_color :
                       (util::has_value_and_equal(hovered_index, i) ? hovered_color : color),
            selected && !selector.is_selecting() ? 0 : plot::DragXRangeFlag::NoInput
        );
    }
    ImGui::PopID();
}

void PlotSelections::draw_list(const char *id)
{
    if (ranges.empty()) {
        return;
    }

    ImGui::PushID(id);

    hovered_index.reset();
    if (ImGui::BeginListBox("##listbox", {-1, -1})) {
        for (std::size_t i = 0; i < ranges.size(); i++) {
            ImGui::PushID(static_cast<int>(i));
            const bool remove = ImGui::Button("Remove");
            if (ImGui::IsItemHovered()) {
                hovered_index = i;
            }
            if (remove) {
                remove_selection(i);
            } else {
                ImGui::SameLine();
                const bool selected = util::has_value_and_equal(selected_index, i);
                if (ImGui::Selectable(labels[i].c_str(), selected)) {
                    if (selected) {
                        selected_index.reset();
                    } else {
                        selected_index = i;
                    }
                    spdlog::debug(
                        "{}: {} label #{} [{}, {}]",
                        name,
                        selected ? "deselected" : "selected",
                        i + 1,
                        ranges[i].Min,
                        ranges[i].Max
                    );
                }
                if (ImGui::IsItemHovered()) {
                    hovered_index = i;
                }
            }
            ImGui::PopID();
        }

        ImGui::EndListBox();
    }

    ImGui::PopID();
}

void PlotSelections::remove_selection(std::size_t i)
{
    if (selected_index.has_value()) {
        if (*selected_index == i) {
            selected_index.reset();
        } else if (i < *selected_index) {
            *selected_index -= 1;
        }
    }

    const auto range = ranges.begin() + static_cast<decltype(ranges)::difference_type>(i);
    spdlog::debug("{}: removing label [{}, {}] ({})", name, range->Min, range->Max, labels[i]);
    ranges.erase(range);
    set_labels();
}

SwallowTaskPlotter::SwallowTaskPlotter(SwallowTaskData data_) :
    data(std::move(data_)),
    event(data.event.begin(), data.event.end()),
    summary_range(initial_range(data.flow_time, data.event))
{}

void SwallowTaskPlotter::draw(const char *id)
{
    ImGui::PushID(id);

    if (ImPlot::BeginAlignedPlots("##aligned_plots")) {
        annotation_editor.draw_swallow_label_info();
        annotation_editor.draw_swallow_apnea_info();
        if (begin_data_plot("##flow")) {
            draw_flow_plot();
            annotation_editor.draw_apnea_selection();
            ImPlot::EndPlot();
        }
        annotation_editor.draw_swallow_notes();

        annotation_editor.draw_earclick_label_info();
        if (begin_data_plot("##ear_audio")) {
            ImPlot::SetupAxis(ImAxis_X1, "Time (s)");
            draw_audio_plot();
            annotation_editor.draw_earclick_selection();
            ImPlot::EndPlot();
        }
        annotation_editor.draw_earclick_notes();

        ImPlot::EndAlignedPlots();
    }

    if (ImPlot::BeginPlot("##summary", ImVec2(-1, 75), ImPlotFlags_CanvasOnly)) {
        draw_summary_plot();
        ImPlot::EndPlot();
    }

    annotation_editor.draw_earclick_selection_list();

    ImGui::PopID();
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

    constexpr ImColor summary_color = {.5f, .5, .5, .6};
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
