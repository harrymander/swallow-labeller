#include "task-filter.hpp"

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
        m_input.draw("##input");
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

class BooleanFilter : public TaskFilter::Filter {
public:
    void draw() final { ImGui::Checkbox("##boolean_filter", &m_value); }

    bool passes(const app::SwallowLabellingTask& task, const models::SwallowAnnotation *annotation)
        const final
    {
        return bool_passes(task, annotation) == m_value;
    }

    virtual bool bool_passes(
        const app::SwallowLabellingTask& task, const models::SwallowAnnotation *annotation
    ) const = 0;

private:
    bool m_value = true;
};

struct HasAnnotationFilter : public BooleanFilter {
    bool bool_passes(
        [[maybe_unused]] const app::SwallowLabellingTask& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        return annotation != nullptr;
    }
};

struct AmbiguityFilter : public BooleanFilter {
    bool bool_passes(
        [[maybe_unused]] const app::SwallowLabellingTask& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        if (annotation == nullptr) {
            return false;
        }

        return VariantVisitor{
            [](const models::SwallowApneaAnnotation& apnea) { return apnea.is_ambiguous; },
            [](auto) { return false; },
        }(annotation->swallow_apnea);
    }
};

struct HasEarClicksFilter : BooleanFilter {
    bool bool_passes(
        [[maybe_unused]] const app::SwallowLabellingTask& task,
        const models::SwallowAnnotation *annotation
    ) const override
    {
        if (annotation == nullptr) {
            return false;
        }

        return VariantVisitor{
            [](const std::vector<models::TimeRange>& clicks) { return !clicks.empty(); },
            [](auto) { return false; },
        }(annotation->ear_clicks);
    }
};

template <typename Filter> std::unique_ptr<TaskFilter::Filter> filter_factory()
{
    return std::make_unique<Filter>();
}

using FilterFactory = std::unique_ptr<TaskFilter::Filter> (*)();
constexpr std::array<std::pair<const char *, FilterFactory>, 6> Filters = {{
    {"Subject#", filter_factory<TaskIntegerFilter<&models::SwallowTaskInfo::subject>>},
    {"Repeat#", filter_factory<TaskIntegerFilter<&models::SwallowTaskInfo::repeatnum>>},
    {"Swallow#", filter_factory<TaskIntegerFilter<&models::SwallowTaskInfo::swallownum>>},
    {"Has annotation", filter_factory<HasAnnotationFilter>},
    {"Is ambiguous", filter_factory<AmbiguityFilter>},
    {"Has ear clicks", filter_factory<HasEarClicksFilter>},
}};

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
        if (ImGui::Button(ICON_FA_TRASH_CAN)) {
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
    if (ImGui::Button("Add")) {
        const auto& new_filter = Filters[m_new_filter_index];
        m_filters.emplace_back(new_filter.first, new_filter.second());
        m_new_filter_index = 0;
        spdlog::debug("Added '{}' filter", new_filter.first);
    }

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
}

bool TaskFilter::passes(
    const app::SwallowLabellingTask& task, const models::SwallowAnnotation *annotation
) const
{
    const auto pred = [&](const auto& p) { return p.second->passes(task, annotation); };
    if (m_and) {
        return std::all_of(m_filters.begin(), m_filters.end(), pred);
    }
    return std::any_of(m_filters.begin(), m_filters.end(), pred);
}

}; // namespace recap::labeller::gui
