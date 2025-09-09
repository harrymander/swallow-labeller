#include "gui/task-list.hpp"

#include "app/annotation-store.hpp"
#include "app/task-loader.hpp"
#include "fmt/core.h"
#include "gui/icons.h"
#include "gui/task-filter.hpp"
#include "gui/windows.hpp"
#include "models/task-info.hpp"

#include <imgui.h>

#include <cstddef>
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

bool draw_task_selectable(
    const models::SwallowTaskInfo& task,
    const std::filesystem::path& data_dir,
    app::TaskLoader& task_loader,
    bool has_annotation,
    bool selected
)
{
    const char *icon = has_annotation ? (ANNOTATED_TASK_ICON " ") : "";
    const char *tooltip = nullptr;
    const auto path = data_dir / task.npz_file.path;
    app::TaskLoader::Status data_status = task_loader.get_task_data_status(path);
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

std::size_t draw_in_window(
    TaskFilter& filter,
    const std::vector<models::SwallowTaskInfo>& tasks,
    std::size_t active_task_idx,
    const SwallowAnnotationStore& annotation_store,
    app::TaskLoader& task_loader,
    const std::filesystem::path& data_dir
)
{
    filter.draw("##task-list-filter");

    const ImVec2 task_counts_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos({
        task_counts_pos.x,
        task_counts_pos.y + ImGui::GetTextLineHeightWithSpacing() * (filter.enabled() ? 2 : 1),
    });

    std::size_t num_filtered = 0;
    std::size_t num_annotated = 0;
    if (ImGui::BeginListBox("##task-list", {-1, -1})) {
        for (std::size_t i = 0; i < tasks.size(); i++) {
            const auto& task = tasks[i];
            const auto *annotation = annotation_store.get_annotation(task.get_id());
            if (annotation) {
                num_annotated += 1;
            }
            if (!filter.enabled() || filter.passes(task, annotation)) {
                num_filtered += 1;
                if (draw_task_selectable(
                        task, data_dir, task_loader, annotation != nullptr, active_task_idx == i
                    ))
                {
                    active_task_idx = i;
                }
            }
        }
        ImGui::EndListBox();
    }

    ImGui::SetCursorPos(task_counts_pos);
    if (filter.enabled()) {
        ImGui::Text(
            FILTER_ICON ICON_TEXT_SPACE "Showing %zu task%s out of %zu",
            num_filtered,
            num_filtered == 1 ? "" : "s",
            tasks.size()
        );
    }
    ImGui::Text(
        ANNOTATED_TASK_ICON ICON_TEXT_SPACE "Annotated: %zu task%s out of %zu",
        num_annotated,
        num_annotated == 1 ? "" : "s",
        tasks.size()
    );

    return active_task_idx;
}

}; // namespace

std::size_t TaskList::draw(
    const std::vector<models::SwallowTaskInfo>& tasks,
    std::size_t active_task_idx,
    const SwallowAnnotationStore& annotation_store,
    app::TaskLoader& m_task_loader,
    const std::filesystem::path& data_dir
)
{
    if (ImGui::Begin(TaskListWindowId)) {
        active_task_idx = draw_in_window(
            m_filter, tasks, active_task_idx, annotation_store, m_task_loader, data_dir
        );
    }
    ImGui::End();
    return active_task_idx;
}

}; // namespace recap::labeller::gui
