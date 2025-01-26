#include "task-filter.hpp"

#include "gui/widgets/enum-checkboxes.hpp"
#include "gui/widgets/enum-combo.hpp"
#include "gui/widgets/enum-utils.hpp"
#include "gui/widgets/integer-range-input.hpp"
#include "gui/widgets/util.hpp"
#include "models/annotation.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"
#include "util/variant-visitor.hpp"

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <utility>

namespace recap::labeller::gui {

namespace {

class IntegerFilter : public TaskFilter::Filter {
public:
    void draw() final
    {
        m_input.draw("##input", ImGui::GetFontSize() * 6);
        if (m_input.error()) {
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
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
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

struct HasEarClicksFilter : BooleanFilter {
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

struct SwallowPatternFilter : TaskFilter::Filter {
public:
    SwallowPatternFilter() { m_src.fill(true); }

    void draw() override
    {
        constexpr auto SrcLabels = []() {
            using enum models::SrcPattern;
            widgets::EnumLabels<models::SrcPattern> labels;
            labels[ExEx] = "ex-ex";
            labels[InEx] = "in-ex";
            labels[ExIn] = "ex-in";
            labels[InIn] = "in-in";
            return labels;
        }();

        ImGui::BeginGroup();
        ImGui::Checkbox("No swallow", &m_no_swallow);
        widgets::enum_checkboxes("##src-pattern-checkboxes", SrcLabels, m_src, false);
        ImGui::EndGroup();
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
    bool m_no_swallow = true;
    widgets::EnumCheckboxValues<models::SrcPattern> m_src{};
};

template <typename Filter> std::unique_ptr<TaskFilter::Filter> FilterFactory()
{
    return std::make_unique<Filter>();
}

using FilterFactoryFunction = std::unique_ptr<TaskFilter::Filter> (*)();
constexpr std::array<std::pair<const char *, FilterFactoryFunction>, 8> Filters = {{
    {"Subject#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::subject>>},
    {"Repeat#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::repeatnum>>},
    {"Swallow#", FilterFactory<TaskIntegerFilter<&models::SwallowTaskInfo::swallownum>>},
    {"Swallow type", FilterFactory<SwallowTypeFilter>},
    {"SRC pattern", FilterFactory<SwallowPatternFilter>},
    {"Has annotation", FilterFactory<HasAnnotationFilter>},
    {"Is ambiguous", FilterFactory<AmbiguityFilter>},
    {"Has ear clicks", FilterFactory<HasEarClicksFilter>},
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
        const bool to_delete = ImGui::Button(ICON_FA_TRASH_CAN);
        ImGui::SetItemTooltip("Remove filter");
        if (to_delete) {
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
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.5F);
    bool_combo("##and_or_combo", m_and, "AND", "OR");

    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
    ImGui::SameLine();
    if (ImGui::BeginCombo("##new_filter_combo", Filters[m_new_filter_index].first)) {
        for (std::size_t i = 0; i < Filters.size(); i++) {
            bool selected = i == m_new_filter_index;
            if (ImGui::Selectable(Filters[i].first, selected)) {
                m_new_filter_index = i;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Add")) {
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
        return std::all_of(m_filters.begin(), m_filters.end(), pred);
    }
    return m_filters.empty() || std::any_of(m_filters.begin(), m_filters.end(), pred);
}

}; // namespace recap::labeller::gui
