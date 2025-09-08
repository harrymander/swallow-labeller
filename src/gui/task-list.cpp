#include "gui/task-list.hpp"

#include "app/annotation-store.hpp"
#include "app/task-loader.hpp"
#include "fmt/core.h"
#include "gui/icons.h"
#include "gui/windows.hpp"
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
    return "???";
}

}; // namespace

class TaskList::Impl {
public:
    Impl(
        const std::vector<models::SwallowTaskInfo>& tasks,
        const SwallowAnnotationStore& annotation_store,
        app::TaskLoader& task_loader,
        const std::filesystem::path& data_dir
    ) :
        m_tasks(tasks),
        m_annotation_store(annotation_store),
        m_task_loader(task_loader),
        m_data_dir(data_dir)
    {}

    const models::SwallowTaskInfo *draw()
    {
        const models::SwallowTaskInfo *new_task = nullptr;
        if (ImGui::BeginListBox("##task-list", {-1, -1})) {
            for (std::size_t i = 0; i < m_tasks.size(); i++) {
                const auto& task = m_tasks[i];
                if (draw_task_selectable(task, m_active_idx == i)) {
                    if (m_active_idx != i) {
                        new_task = &task;
                    }
                    m_active_idx = i;
                }
            }
        }

        return new_task;
    }

    const models::SwallowTaskInfo& current_task() const { return m_tasks[m_active_idx]; }

private:
    const std::vector<models::SwallowTaskInfo>& m_tasks;
    const SwallowAnnotationStore& m_annotation_store;
    app::TaskLoader& m_task_loader;
    std::filesystem::path m_data_dir;

    std::size_t m_active_idx = 0;

    bool draw_task_selectable(const models::SwallowTaskInfo& task, bool selected)
    {
        const bool has_annotation = m_annotation_store.has_annotation(task.get_id());
        const char *icon = has_annotation ? (ANNOTATED_TASK_ICON " ") : "";
        const char *tooltip = nullptr;
        const auto path = m_data_dir / task.npz_file.path;
        app::TaskLoader::Status data_status = m_task_loader.get_task_data_status(path);
        switch (data_status) {
        case app::TaskLoader::Status::Ok:
        case app::TaskLoader::Status::NotLoaded:
            break;
        case app::TaskLoader::Status::FileNotFound:
            tooltip = "File not found";
            icon = FILE_ERR_ICON " ";
            break;
        case app::TaskLoader::Status::FileLoadError:
            tooltip = "Error loading file";
            icon = FILE_ERR_ICON " ";
            break;
        }

        auto num_events = task.event_times.size();
        std::string info_str = fmt::format(
            "{}Subject #{}, {}\nRepeat #{}, {} event{}",
            icon,
            task.subject,
            swallow_test_type_str(task.test_type),
            task.repeatnum,
            num_events,
            num_events == 1 ? "" : "s"
        );
        const bool ret = ImGui::Selectable(info_str.c_str(), selected);
        if (tooltip) {
            ImGui::SetItemTooltip("%s", tooltip);
        }
        return ret;
    }
};

TaskList::TaskList(
    const std::vector<models::SwallowTaskInfo>& tasks,
    const SwallowAnnotationStore& annotation_store,
    app::TaskLoader& task_loader,
    const std::filesystem::path& data_dir
) :
    m_pimpl(std::make_unique<TaskList::Impl>(tasks, annotation_store, task_loader, data_dir))
{}

TaskList::~TaskList() = default;

const models::SwallowTaskInfo *TaskList::draw()
{
    const models::SwallowTaskInfo *new_task = nullptr;
    if (ImGui::Begin(TaskListWindowId)) {
        new_task = m_pimpl->draw();
    }
    ImGui::End();
    return new_task;
}

const models::SwallowTaskInfo& TaskList::currently_selected_task() const
{
    return m_pimpl->current_task();
}

}; // namespace recap::labeller::gui
