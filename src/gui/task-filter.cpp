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
#include <optional>
#include <regex>
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

class AnnotationNoteFilter : public TaskFilter::Filter {
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
        if (ImGui::InputText("##notes-text-input", &m_input)) {
            update_input();
        }
        if (regex_error) {
            ImGui::SameLine();
            ImGui::TextUnformatted(ICON_FA_TRIANGLE_EXCLAMATION);
            ImGui::SetItemTooltip("Invalid regex");
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

        const auto& note = annotation->note;
        if (m_use_regex) {
            return m_regex.has_value() && std::regex_search(note.value_or(""), *m_regex);
        }

        if (note.has_value()) {
            return note->find(m_input) != std::string::npos;
        }
        return m_input.empty();
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
            if (m_input.empty()) {
                m_regex = std::regex(".*");
            } else {
                try {
                    m_regex = std::regex(m_input);
                } catch (const std::regex_error&) {
                    m_regex = std::nullopt;
                }
            }
        } else {
            m_input = strutil::trimmed(m_input);
        }
    }

    std::string m_input;
    std::optional<std::regex> m_regex = std::nullopt;
    bool m_use_regex = false;
};

template <typename Filter> std::unique_ptr<TaskFilter::Filter> FilterFactory()
{
    return std::make_unique<Filter>();
}

using FilterFactoryFunction = std::unique_ptr<TaskFilter::Filter> (*)();
constexpr std::array<std::pair<const char *, FilterFactoryFunction>, 9> Filters = {{
    {"Subject#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::subject>>},
    {"Repeat#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::repeatnum>>},
    {"Swallow#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::swallownum>>},
    {"Swallow type", FilterFactory<SwallowTypeFilter>},
    {"SRC pattern", FilterFactory<SwallowPatternFilter>},
    {"Has annotation", FilterFactory<HasAnnotationFilter>},
    {"Is ambiguous", FilterFactory<AmbiguityFilter>},
    {"Has ear clicks", FilterFactory<HasEarClicksFilter>},
    {"Note", FilterFactory<AnnotationNoteFilter>},
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
