#include "task-filter.hpp"

#include "gui/icons.h"
#include "gui/widgets/combo-forced.hpp"
#include "gui/widgets/enum-combo.hpp"
#include "gui/widgets/enum-utils.hpp"
#include "gui/widgets/integer-range-input.hpp"
#include "gui/widgets/util.hpp"
#include "models/annotation.hpp"
#include "models/task-info.hpp"
#include "util/strutil.hpp"

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <optional>
#include <regex>
#include <string_view>

namespace recap::labeller::gui {

namespace {

class IntegerFilter : public TaskFilter::Filter {
public:
    using TaskFilter::Filter::Filter;

    void draw() final
    {
        const bool error = m_input.error();
        m_input.draw(
            "##input", error ? -ImGui::GetFontSize() - ImGui::GetStyle().ItemSpacing.x : -1
        );
        if (error) {
            ImGui::SameLine();
            ImGui::TextUnformatted(ERR_ICON);
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

struct TaskEventCountFilter : public IntegerFilter {
    using IntegerFilter::IntegerFilter;

    bool passes(
        const models::SwallowTaskInfo& task,
        [[maybe_unused]] const models::SwallowAnnotation *annotation
    ) const override
    {
        return integer_passes(task.event_times.size());
    }
};

template <unsigned int models::SwallowTaskInfo::*IntMember>
struct TaskIntegerFilter : public IntegerFilter {
    using IntegerFilter::IntegerFilter;

    bool passes(
        const models::SwallowTaskInfo& task,
        [[maybe_unused]] const models::SwallowAnnotation *annotation
    ) const override
    {
        return integer_passes(task.*IntMember);
    }
};

class SwallowTypeFilter : public TaskFilter::Filter {
public:
    using TaskFilter::Filter::Filter;

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
        const models::SwallowTaskInfo& task,
        [[maybe_unused]] const models::SwallowAnnotation *annotation
    ) const override
    {
        return m_value == task.test_type;
    }

private:
    models::SwallowTestType m_value = models::SwallowTestType::TidalBreathing;
};

class BooleanFilter : public TaskFilter::Filter {
public:
    using TaskFilter::Filter::Filter;

    void draw() final { ImGui::Checkbox("##boolean_filter", &m_value); }

    [[nodiscard]] bool bool_passes(bool value) const { return value == m_value; }

private:
    bool m_value = true;
};

struct HasAnnotationFilter : public BooleanFilter {
    using BooleanFilter::BooleanFilter;

    bool passes(
        [[maybe_unused]] const models::SwallowTaskInfo& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        return bool_passes(annotation != nullptr);
    }
};

class TextFilter : public TaskFilter::Filter {
public:
    using TaskFilter::Filter::Filter;

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
            ImGui::TextUnformatted(ERR_ICON);
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
    using TextFilter::TextFilter;

    bool passes(
        [[maybe_unused]] const models::SwallowTaskInfo& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        if (annotation == nullptr) {
            return false;
        }
        const auto& notes = annotation->notes;
        if (notes.empty()) {
            return false;
        }

        return std::ranges::any_of(notes, [this](const auto& note) { return string_passes(note); });
    }
};

struct DatapathFilter : public TextFilter {
    using TextFilter::TextFilter;

    bool passes(
        const models::SwallowTaskInfo& task,
        [[maybe_unused]] const models::SwallowAnnotation *annotation
    ) const override
    {
        return string_passes(task.npz_file.path);
    }
};

template <typename Filter> std::unique_ptr<TaskFilter::Filter> FilterFactory(const char *name)
{
    return std::make_unique<Filter>(name);
}

class FilterChoice {
public:
    using Factory = std::unique_ptr<TaskFilter::Filter> (*)(const char *);

    constexpr FilterChoice(const char *name, Factory factory) : m_name(name), m_factory(factory) {}

    [[nodiscard]] constexpr const char *name() const { return m_name; }

    [[nodiscard]] std::unique_ptr<TaskFilter::Filter> create() const { return m_factory(m_name); }

private:
    const char *m_name;
    Factory m_factory;
};

constexpr std::array<FilterChoice, 7> FilterChoices = {{
    {"Num. events", FilterFactory<TaskEventCountFilter>},
    {"Has annotation", FilterFactory<HasAnnotationFilter>},
    {"Swallow type", FilterFactory<SwallowTypeFilter>},
    {"Subject#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::subject>>},
    {"Repeat#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::repeatnum>>},
    {"Note", FilterFactory<AnnotationNoteFilter>},
    {"File path", FilterFactory<DatapathFilter>},
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
    const bool clicked = ImGui::Button(DELETE_ICON);
    ImGui::SetItemTooltip("Remove filter");
    return clicked;
}

const FilterChoice *draw_filter_change_combo(const char *current, bool& open, bool appearing)
{
    const FilterChoice *new_filter = nullptr;
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
    if (widgets::BeginComboForced(
            "##filter_change_combo", current, appearing, ImGuiComboFlags_HeightLarge
        ))
    {
        for (const auto& filter : FilterChoices) {
            if (ImGui::Selectable(filter.name())) { // cppcheck-suppress useStlAlgorithm
                new_filter = &filter;
                break;
            }
        }
        ImGui::EndCombo();
    } else {
        open = false;
    }

    return new_filter;
}

bool draw_filter(std::unique_ptr<TaskFilter::Filter>& filter, bool changing, bool appearing)
{
    ImGui::SameLine();
    if (changing) {
        const auto *new_filter = draw_filter_change_combo(filter->name(), changing, appearing);
        if (new_filter) {
            changing = false;
            if (new_filter->name() != filter->name()) {
                spdlog::debug("Changing filter '{}' -> '{}'", new_filter->name(), filter->name());
                filter = new_filter->create();
            }
        }
    } else {
        ImGui::TextUnformatted(filter->name());
        changing = ImGui::IsItemClicked();
    }

    ImGui::SameLine();
    filter->draw();
    return changing;
}

}; // namespace

void TaskFilter::draw(const char *id)
{
    widgets::ScopedImID id_scope(id);

    if (m_filters.size() > 1) {
        if (ImGui::Button(FILTER_CANCEL_ICON ICON_TEXT_SPACE "Clear all filters")) {
            spdlog::debug("Cleared all {} filters", m_filters.size());
            m_filters.clear();
            m_and = true;
        }
    }

    draw_filters();
    draw_new_filter_control();
}

void TaskFilter::draw_filters()
{
    auto it = m_filters.begin();
    while (it != m_filters.end()) {
        auto& filter = *it;
        widgets::ScopedImID filter_id(filter.get());
        if (draw_delete_button()) {
            spdlog::debug("Deleted '{}' filter", filter->name());
            if (m_changing_filter_it == it) {
                reset_changing_filter();
            }
            it = m_filters.erase(it);
        } else {
            bool changing = m_changing_filter_it == it;
            if (draw_filter(filter, changing, m_changing_filter_appearing)) {
                set_changing_filter(it);
            } else if (changing) {
                reset_changing_filter();
            }
            it++;
        }
    }

    // If all filters have been deleted, reset the AND/OR control
    if (m_filters.empty()) {
        m_and = true;
    }
}

void TaskFilter::draw_new_filter_control()
{
    if (!m_filters.empty()) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.5F);
        bool_combo("##and_or_combo", m_and, "AND", "OR");
        ImGui::SameLine();
    }

    const FilterChoice *new_filter = nullptr;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo(
            "##new_filter_combo",
            FILTER_ICON ICON_TEXT_SPACE "Add task filter...",
            ImGuiComboFlags_HeightLarge
        ))
    {
        for (const auto& filter : FilterChoices) {
            if (ImGui::Selectable(filter.name())) {
                new_filter = &filter;
            }
        }
        ImGui::EndCombo();
    }

    if (new_filter) {
        m_filters.emplace_back(new_filter->create());
        reset_changing_filter();
        spdlog::debug("Added '{}' filter", new_filter->name());
    }
}

void TaskFilter::reset_changing_filter()
{
    m_changing_filter_it.reset();
    m_changing_filter_appearing = true;
}

void TaskFilter::set_changing_filter(TaskFilter::FilterList::const_iterator it)
{
    if (it != m_changing_filter_it) {
        m_changing_filter_it = it;
        m_changing_filter_appearing = true;
    } else {
        m_changing_filter_appearing = false;
    }
}

bool TaskFilter::passes(
    const models::SwallowTaskInfo& task, const models::SwallowAnnotation *annotation
) const
{
    const auto pred = [&](const auto& filter) { return filter->passes(task, annotation); };
    if (m_and) {
        return std::ranges::all_of(m_filters, pred);
    }
    return m_filters.empty() || std::ranges::any_of(m_filters, pred);
}

}; // namespace recap::labeller::gui
