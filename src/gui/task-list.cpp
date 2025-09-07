#include "gui/task-list.hpp"

#include "app/annotation-store.hpp"
#include "fmt/format.h"
#include "gui/icons.h"
#include "models/task-info.hpp"

#include <imgui.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace recap::labeller::gui {

namespace {

const char *swallow_test_type_str(models::SwallowTestType test_type)
{
    switch (test_type) {
    case models::SwallowTestType::TidalBreathing:
        return "tidal Breathing";
    case models::SwallowTestType::Cued:
        return "cued swallow";
    }
    assert(false);
}

std::string swallow_task_info_string(const models::SwallowTaskInfo& info, bool has_label)
{
    auto num_events = info.event_times.size();
    return fmt::format(
        "{}Subject #{}, {}\nRepeat #{}, {} event{}",
        has_label ? (ANNOTATED_TASK_ICON " ") : "",
        info.subject,
        swallow_test_type_str(info.test_type),
        info.repeatnum,
        num_events,
        num_events == 1 ? "" : "s"
    );
}

}; // namespace

class TaskList::Impl {
public:
    Impl(
        const std::vector<models::SwallowTaskInfo>& tasks,
        const SwallowAnnotationStore& annotation_store
    ) :
        m_tasks(tasks), m_annotation_store(annotation_store)
    {}

    void draw()
    {
        if (ImGui::BeginListBox("##task-list", {-1, -1})) {
            for (std::size_t i = 0; i < m_tasks.size(); i++) {
                const bool selected = i == m_active_idx;
                const auto& task = m_tasks[i];
                const bool has_label = m_annotation_store.has_annotation(task.get_id());
                const auto info_str = swallow_task_info_string(task, has_label);
                if (ImGui::Selectable(info_str.c_str(), selected)) {
                    m_active_idx = i;
                }
            }
        }
    }

private:
    const std::vector<models::SwallowTaskInfo>& m_tasks;
    const SwallowAnnotationStore& m_annotation_store;

    std::size_t m_active_idx = 0;
};

TaskList::TaskList(
    const std::vector<models::SwallowTaskInfo>& tasks,
    const SwallowAnnotationStore& annotation_store
) :
    m_pimpl(std::make_unique<TaskList::Impl>(tasks, annotation_store))
{}

TaskList::~TaskList() = default;

void TaskList::draw(const char *id)
{
    if (ImGui::Begin(id)) {
        m_pimpl->draw();
    }
    ImGui::End();
}

}; // namespace recap::labeller::gui
