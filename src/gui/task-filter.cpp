#include "task-filter.hpp"

#include "gui/widgets/enum-checkboxes.hpp"
#include "gui/widgets/enum-combo.hpp"
#include "gui/widgets/enum-utils.hpp"
#include "gui/widgets/integer-range-input.hpp"
#include "gui/widgets/util.hpp"
#include "models/annotation.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"
#include "util/strutil.hpp"
#include "util/variant-visitor.hpp"

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <optional>
#include <regex>
#include <string_view>
#include <utility>

namespace recap::labeller::gui {

namespace {

class IntegerFilter : public TaskFilter::Filter {
public:
    void draw() final
    {
        const bool error = m_input.error();
        m_input.draw(
            "##input", error ? -ImGui::GetFontSize() - ImGui::GetStyle().ItemSpacing.x : -1
        );
        if (error) {
            ImGui::SameLine();
            ImGui::TextUnformatted(ICON_FA_TRIANGLE_EXCLAMATION);
            ImGui::SetItemTooltip("Invalid input");
        }
    }

    bool integer_passes(unsigned int val) const
    {
        return m_input.empty() || (!m_input.error() && m_input.contains(val));
    }

private:
    widgets::IntegerRangeInput m_input;
};

template <unsigned int models::SwallowTaskInfo::*IntMember>
struct TaskIntegerFilter : public IntegerFilter {
    bool passes(
        const app::SwallowLabellingTask& task,
        [[maybe_unused]] const models::SwallowAnnotation *annotation
    ) const override
    {
        return integer_passes(task.info().*IntMember);
    }
};

class SwallowTypeFilter : public TaskFilter::Filter {
public:
    void draw() override
    {
        constexpr auto Labels = []() {
            using enum models::SwallowTestType;
            widgets::EnumLabels<models::SwallowTestType> labels;
            labels[TidalBreathing] = "Tidal";
            labels[Cued] = "Cued";
            return labels;
        }();
        ImGui::SetNextItemWidth(-1);
        widgets::enum_combo("##swallow-test-type", Labels, m_value);
    }

    bool passes(
        const app::SwallowLabellingTask& task,
        [[maybe_unused]] const models::SwallowAnnotation *annotation
    ) const override
    {
        return m_value == task.info().test_type;
    }

private:
    models::SwallowTestType m_value = models::SwallowTestType::TidalBreathing;
};

class BooleanFilter : public TaskFilter::Filter {
public:
    void draw() final { ImGui::Checkbox("##boolean_filter", &m_value); }

    [[nodiscard]] bool bool_passes(bool value) const { return value == m_value; }

private:
    bool m_value = true;
};

struct HasAnnotationFilter : public BooleanFilter {
    bool passes(
        [[maybe_unused]] const app::SwallowLabellingTask& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        return bool_passes(annotation != nullptr);
    }
};

struct AmbiguityFilter : public BooleanFilter {
    bool passes(
        [[maybe_unused]] const app::SwallowLabellingTask& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        if (annotation == nullptr) {
            return false;
        }

        const bool is_ambiguous = VariantVisitor{
            [](const models::SwallowApneaAnnotation& apnea) { return apnea.is_ambiguous; },
            [](auto) { return false; },
        }(annotation->swallow_apnea);
        return bool_passes(is_ambiguous);
    }
};

struct HasEarClicksFilter : public BooleanFilter {
    bool passes(
        [[maybe_unused]] const app::SwallowLabellingTask& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        if (annotation == nullptr) {
            return false;
        }

        const bool has_ear_clicks = VariantVisitor{
            [](const std::vector<models::TimeRange>& clicks) { return !clicks.empty(); },
            [](auto) { return false; },
        }(annotation->ear_clicks);
        return bool_passes(has_ear_clicks);
    }
};

class SwallowPatternFilter : public TaskFilter::Filter {
public:
    SwallowPatternFilter() { m_src.fill(true); }

    void draw() override
    {
        ImGui::SetNextItemWidth(-1);
        bool updated = false;
        if (ImGui::BeginCombo("##src-pattern-combo", m_preview.c_str())) {
            if (ImGui::Checkbox("No swallow", &m_no_swallow)) {
                updated = true;
            }
            if (widgets::enum_checkboxes("##src-pattern-checkboxes", SrcLabels, m_src)) {
                updated = true;
            }
            ImGui::EndCombo();
        }
        if (updated) {
            m_preview = preview_string();
        }
    }

    bool passes(
        [[maybe_unused]] const app::SwallowLabellingTask& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        if (annotation == nullptr) {
            return false;
        }

        return VariantVisitor{
            [this](const models::SwallowApneaAnnotation& apnea) { return m_src[apnea.pattern]; },
            [this](auto) { return m_no_swallow; },
        }(annotation->swallow_apnea);
    }

private:
    static constexpr auto SrcLabels = []() {
        using enum models::SrcPattern;
        widgets::EnumLabels<models::SrcPattern> labels;
        labels[ExEx] = "ex-ex";
        labels[InEx] = "in-ex";
        labels[ExIn] = "ex-in";
        labels[InIn] = "in-in";
        return labels;
    }();

    std::string preview_string() const
    {
        if (m_no_swallow && std::ranges::all_of(m_src, [](auto v) { return v; })) {
            return "All";
        }

        std::string preview = m_no_swallow ? "NS" : "";
        for (std::size_t i = 0; i < m_src.size(); i++) {
            const auto e = magic_enum::enum_value<models::SrcPattern>(i);
            if (m_src[e]) {
                if (!preview.empty()) {
                    preview += ", ";
                }
                preview += SrcLabels[e];
            }
        }

        if (preview.empty()) {
            preview = "(none)";
        }

        return preview;
    }

    bool m_no_swallow = true;
    widgets::EnumCheckboxValues<models::SrcPattern> m_src{};
    std::string m_preview = "All";
};

class TextFilter : public TaskFilter::Filter {
public:
    void draw() override
    {
        if (draw_regex_button()) {
            update_input();
        }

        ImGui::SameLine();
        const bool regex_error = m_use_regex && !m_regex.has_value();
        ImGui::SetNextItemWidth(
            regex_error ? -ImGui::GetFontSize() - ImGui::GetStyle().ItemSpacing.x : -1
        );
        if (ImGui::InputText("##text-filter-input", &m_search)) {
            update_input();
        }
        if (regex_error) {
            ImGui::SameLine();
            ImGui::TextUnformatted(ICON_FA_TRIANGLE_EXCLAMATION);
            ImGui::SetItemTooltip("Invalid regex");
        }
    }

    [[nodiscard]] bool string_passes(std::string_view s) const
    {
        if (m_use_regex) {
            return m_regex.has_value() && std::regex_search(s.begin(), s.end(), *m_regex);
        }
        return s.find(m_search) != std::string_view::npos;
    }

private:
    bool draw_regex_button()
    {
        const ImU32 color =
            m_use_regex ? ImGui::GetColorU32(ImGuiCol_Button) : IM_COL32(0, 0, 0, 0);
        widgets::ScopedImColor button_color = {
            {ImGuiCol_Button, color},
            {ImGuiCol_ButtonActive, color},
            {ImGuiCol_ButtonHovered, color},
        };
        const bool clicked = ImGui::Button(".*");
        if (clicked) {
            m_use_regex = !m_use_regex;
        }
        ImGui::SetItemTooltip("Use regular expression");
        return clicked;
    }

    void update_input()
    {
        if (m_use_regex) {
            if (m_search.empty()) {
                m_regex = std::regex(".*");
            } else {
                try {
                    m_regex = std::regex(m_search);
                } catch (const std::regex_error&) {
                    m_regex = std::nullopt;
                }
            }
        } else {
            m_search = strutil::trimmed(m_search);
        }
    }

    std::string m_search;
    std::optional<std::regex> m_regex = std::nullopt;
    bool m_use_regex = false;
};

struct AnnotationNoteFilter : public TextFilter {
    bool passes(
        [[maybe_unused]] const app::SwallowLabellingTask& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        if (annotation == nullptr) {
            return false;
        }
        const auto& note = annotation->note;
        return string_passes(note.has_value() ? *note : std::string_view{});
    }
};

struct DatapathFilter : public TextFilter {
    bool passes(
        const app::SwallowLabellingTask& task,
        [[maybe_unused]] const models::SwallowAnnotation *annotation
    ) const override
    {
        return string_passes(task.data_path());
    }
};

class DoubleComparator {
public:
    enum class Op {
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        NotEqual,
        Equal,
    };

    DoubleComparator(Op op, double val) : m_compare_func(get_compare_func(op)), m_val(val) {}

    bool operator()(double val) const { return m_compare_func(val, m_val); }

private:
    using CompareFunc = bool (*)(double, double);

    static constexpr double Atol = 1e-3;

    static bool less(double a, double b) { return a < b; }

    static bool equal(double a, double b) { return std::abs(a - b) < Atol; }

    static bool not_equal(double a, double b) { return !equal(a, b); }

    static bool less_equal(double a, double b) { return less(a, b) || equal(a, b); }

    static bool greater(double a, double b) { return a > b; }

    static bool greater_equal(double a, double b) { return greater(a, b) || equal(a, b); }

    static CompareFunc get_compare_func(Op op)
    {
        using enum Op;

        switch (op) {
        case Less:
            return less;
        case LessEqual:
            return less_equal;
        case Greater:
            return greater;
        case GreaterEqual:
            return greater_equal;
        case NotEqual:
            return not_equal;
        case Equal:
            break;
        }
        return equal;
    }

    CompareFunc m_compare_func;
    double m_val;
};

class DoubleRangeFilter : public TaskFilter::Filter {
public:
    void draw() override
    {
        const bool regex_error = !m_comparator.has_value();
        ImGui::SetNextItemWidth(
            regex_error ? -ImGui::GetFontSize() - ImGui::GetStyle().ItemSpacing.x : -1
        );
        const bool updated = ImGui::InputText("##float-range-filter-input", &m_input);
        if (regex_error) {
            ImGui::SameLine();
            ImGui::TextUnformatted(ICON_FA_TRIANGLE_EXCLAMATION);
            ImGui::SetItemTooltip("Invalid input");
        }

        if (updated) {
            update_comparator();
        }
    }

    [[nodiscard]] bool double_passes(double val) const
    {
        return m_comparator ? (*m_comparator)(val) : false;
    }

private:
    static DoubleComparator::Op parse_op(std::ssub_match s)
    {
        using enum DoubleComparator::Op;

        if (!s.matched) {
            return Equal;
        }
        if (s == "<") {
            return Less;
        }
        if (s == "<=") {
            return LessEqual;
        }
        if (s == ">") {
            return Greater;
        }
        if (s == ">=") {
            return GreaterEqual;
        }
        if (s == "!=") {
            return NotEqual;
        }
        return Equal;
    }

    static double parse_double(
        const std::string& s,
        std::smatch::difference_type position,
        std::smatch::difference_type len
    )
    {
        double val;
        const char *start = s.data() + position;
        const char *end = start + len;
        [[maybe_unused]] auto res = std::from_chars(start, end, val, std::chars_format::fixed);

        // Match should already be validated as a float by regex
        assert(res.ec == std::errc{} && res.ptr == end);
        return val;
    }

    void update_comparator()
    {
        static const std::regex Re(R"(\s*([<>=]=?|!=)?\s*(\d+(?:\.\d*)?|\.\d+)\s*)");
        std::smatch matches;
        if (!std::regex_match(m_input, matches, Re)) {
            m_comparator.reset();
            return;
        }

        auto op = parse_op(matches[1]);
        double val = parse_double(m_input, matches.position(2), matches.length(2));
        m_comparator = DoubleComparator(op, val);
    }

    std::string m_input;
    std::optional<DoubleComparator> m_comparator = std::nullopt;
};

struct SnrfTimeFilter : public DoubleRangeFilter {
    bool passes(
        [[maybe_unused]] const app::SwallowLabellingTask& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        if (!annotation) {
            return false;
        }

        return VariantVisitor{
            [](auto) { return false; },
            [this](const models::SwallowApneaAnnotation apnea) {
                return std::ranges::any_of(apnea.non_respiratory_flow, [this](const auto& time) {
                    return double_passes(std::abs(time.end - time.start));
                });
            },
        }(annotation->swallow_apnea);
    }
};

template <typename Filter> std::unique_ptr<TaskFilter::Filter> FilterFactory()
{
    return std::make_unique<Filter>();
}

using FilterFactoryFunction = std::unique_ptr<TaskFilter::Filter> (*)();
constexpr std::array<std::pair<const char *, FilterFactoryFunction>, 11> Filters = {{
    {"Subject#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::subject>>},
    {"Repeat#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::repeatnum>>},
    {"Swallow#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::swallownum>>},
    {"Swallow type", FilterFactory<SwallowTypeFilter>},
    {"File path", FilterFactory<DatapathFilter>},
    {"SRC pattern", FilterFactory<SwallowPatternFilter>},
    {"Has annotation", FilterFactory<HasAnnotationFilter>},
    {"Is ambiguous", FilterFactory<AmbiguityFilter>},
    {"Has ear clicks", FilterFactory<HasEarClicksFilter>},
    {"Note", FilterFactory<AnnotationNoteFilter>},
    {"SNRF duration", FilterFactory<SnrfTimeFilter>},
}};

bool bool_combo(const char *label, bool& value, const char *true_text, const char *false_text)
{
    const bool old_value = value;
    if (ImGui::BeginCombo(label, value ? true_text : false_text)) {
        if (ImGui::Selectable(true_text, value)) {
            value = true;
        }
        if (ImGui::Selectable(false_text, !value)) {
            value = false;
        }
        ImGui::EndCombo();
    }
    return old_value != value;
}

bool draw_delete_button()
{
    constexpr ImU32 Transparent = IM_COL32(0, 0, 0, 0);
    widgets::ScopedImColor color_scope = {
        {ImGuiCol_Button, Transparent},
        {ImGuiCol_ButtonActive, Transparent},
        {ImGuiCol_ButtonHovered, Transparent},
    };
    const bool clicked = ImGui::Button(ICON_FA_TRASH_CAN);
    ImGui::SetItemTooltip("Remove filter");
    return clicked;
}

}; // namespace

void TaskFilter::draw(const char *id)
{
    widgets::ScopedImID id_scope(id);
    draw_filters();
    draw_new_filter_control();
}

void TaskFilter::draw_filters()
{
    auto it = m_filters.begin();
    while (it != m_filters.end()) {
        widgets::ScopedImID filter_id(&(it->second));
        if (draw_delete_button()) {
            spdlog::debug("Deleted '{}' filter", it->first);
            it = m_filters.erase(it);
        } else {
            ImGui::SameLine();
            ImGui::TextUnformatted(it->first);
            ImGui::SameLine();
            it->second->draw();
            it++;
        }
    }
}

void TaskFilter::draw_new_filter_control()
{
    static const char *AddButtonText = ICON_FA_PLUS " Add";
    static float AddButtonWidth =
        ImGui::CalcTextSize(AddButtonText).x + 2 * ImGui::GetStyle().ItemSpacing.x;

    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.5F);
    bool_combo("##and_or_combo", m_and, "AND", "OR");

    ImGui::SetNextItemWidth(-AddButtonWidth);
    ImGui::SameLine();
    if (ImGui::BeginCombo(
            "##new_filter_combo", Filters[m_new_filter_index].first, ImGuiComboFlags_HeightLarge
        ))
    {
        for (std::size_t i = 0; i < Filters.size(); i++) {
            bool selected = i == m_new_filter_index;
            if (ImGui::Selectable(Filters[i].first, selected)) {
                m_new_filter_index = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button(AddButtonText)) {
        const auto& new_filter = Filters[m_new_filter_index];
        m_filters.emplace_back(new_filter.first, new_filter.second());
        m_new_filter_index = 0;
        spdlog::debug("Added '{}' filter", new_filter.first);
    }
}

bool TaskFilter::passes(
    const app::SwallowLabellingTask& task, const models::SwallowAnnotation *annotation
) const
{
    const auto pred = [&](const auto& p) { return p.second->passes(task, annotation); };
    if (m_and) {
        return std::ranges::all_of(m_filters, pred);
    }
    return m_filters.empty() || std::ranges::any_of(m_filters, pred);
}

}; // namespace recap::labeller::gui
