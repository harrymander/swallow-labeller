#include "app.hpp"

#include "annotation-store.hpp"
#include "models/data.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace recap::labeller::app {

namespace fs = std::filesystem;

namespace {

std::vector<SwallowLabellingTask> labelling_tasks(
    const std::vector<models::SwallowTaskInfo>& tasks_info,
    const SwallowAnnotationStore& annotation_store,
    const fs::path& data_dir
)
{
    std::vector<SwallowLabellingTask> tasks;
    for (const auto& info : tasks_info) {
        const models::SwallowAnnotation *annotation =
            annotation_store.get_annotation(info.get_id());
        tasks.emplace_back(
            info, data_dir, annotation ? std::make_optional(*annotation) : std::nullopt
        );
    }
    return tasks;
}

}; // namespace

SwallowLabellingTask::SwallowLabellingTask(
    models::SwallowTaskInfo task_info,
    const fs::path& data_dir,
    std::optional<models::SwallowAnnotation> annotation
) :
    m_info(std::move(task_info)), m_annotation(std::move(annotation))
{
    fs::path path = (data_dir / fs::path(m_info.npz_file.path)).make_preferred();
    m_data_path = path.string();
    if (!fs::is_regular_file(path)) {
        std::string err = fmt::format("Data file not found at path '{}'", m_data_path);
        spdlog::error(err);
        m_error_msg = std::move(err);
    }
}

void SwallowLabellingTask::set_error_msg(std::string str)
{
    m_error_msg = std::move(str);
}

void SwallowLabellingTask::clear_error_msg()
{
    m_error_msg.reset();
}

App::App(
    const std::vector<models::SwallowTaskInfo>& swallow_tasks,
    SwallowAnnotationStore annotation_store,
    const fs::path& data_dir
) :
    m_swallow_tasks(labelling_tasks(swallow_tasks, annotation_store, data_dir)),
    m_annotation_store(std::move(annotation_store)),
    m_num_annotated_tasks(std::count_if(
        m_swallow_tasks.begin(),
        m_swallow_tasks.end(),
        [](const auto& task) { return task.is_annotated(); }
    ))
{
    load_active_task();
}

void App::set_active_task_index(std::size_t index)
{
    if (index < m_swallow_tasks.size()) {
        if (index != m_active_task_index) {
            spdlog::debug("Setting task index to {}", index);
            m_active_task_index = index;
            load_active_task();
        } else {
            spdlog::warn("Task index is already {}; not changing!", index);
        }
    } else {
        spdlog::error("Invalid task index: {}; not changing!", index);
    }
}

void App::increment_active_task_index()
{
    set_active_task_index((m_active_task_index + 1) % m_swallow_tasks.size());
}

void App::decrement_active_task_index()
{
    set_active_task_index(
        m_active_task_index > 0 ? m_active_task_index - 1 : m_swallow_tasks.size() - 1
    );
}

void App::reload_active_task()
{
    spdlog::info("Reloading active task...");
    load_active_task();
}

template <typename T, typename... Args>
std::unique_ptr<App::ActiveTaskVariant> App::make_unique_active_task(Args&&...args)
{
    auto *const ptr = new ActiveTaskVariant(T(std::forward<Args>(args)...));
    return std::unique_ptr<ActiveTaskVariant>(ptr);
}

void App::load_active_task()
{
    SwallowLabellingTask& task = m_swallow_tasks[m_active_task_index];
    const auto path = fs::path(task.data_path());
    if (!fs::is_regular_file(path)) {
        task.set_error_msg("Data file not found");
        m_active_task = make_unique_active_task<ActiveSwallowLabellingTaskErrorView>(task);
    } else {
        try {
            auto data = SwallowTaskData::from_numpy(cnpy::npz_load(task.data_path()));
            spdlog::debug("Loaded data from {}", task.data_path());
            task.clear_error_msg();
            m_active_task = make_unique_active_task<ActiveSwallowLabellingTaskView>(task, data);
        } catch (const std::exception& e) {
            spdlog::error("Error loading data file from {}: {}", task.data_path(), e.what());
            task.set_error_msg(fmt::format("Error loading data file: {}", e.what()));
            m_active_task = make_unique_active_task<ActiveSwallowLabellingTaskErrorView>(task);
        }
    }

    m_new_active_task_observable.notify(*m_active_task);
}

ActiveSwallowLabellingTaskView::ActiveSwallowLabellingTaskView(
    SwallowLabellingTask& task, SwallowTaskData data
) :
    m_task(task), m_data(std::move(data))
{}

namespace {

[[nodiscard]] inline bool apnea_status_is_src_pattern(SwallowApneaAnnotationStatus status)
{
    using enum SwallowApneaAnnotationStatus;
    return status == ExEx || status == InEx || status == ExIn || status == InIn;
}

[[nodiscard]] inline bool timerange_allnan(const models::TimeRange& range)
{
    return std::isnan(range.start) && std::isnan(range.end);
}

}; // namespace

bool ActiveSwallowLabellingTaskView::can_edit_swallow_apnea_range() const
{
    return apnea_status_is_src_pattern(m_swallow_apnea_status)
        && !timerange_allnan(m_swallow_apnea_range);
}

bool ActiveSwallowLabellingTaskView::can_add_new_swallow_apnea_range() const
{
    return apnea_status_is_src_pattern(m_swallow_apnea_status)
        && timerange_allnan(m_swallow_apnea_range);
}

std::optional<std::string_view> ActiveSwallowLabellingTaskView::swallow_apnea_label_error() const
{
    if (can_add_new_swallow_apnea_range()) {
        return "Missing swallow apnea label";
    }

    return std::nullopt;
}

bool ActiveSwallowLabellingTaskView::can_add_new_ear_click_range() const
{
    return m_ear_click_status == EarClickAnnotationStatus::Ok;
}

std::optional<std::string_view> ActiveSwallowLabellingTaskView::earclick_label_error() const
{
    if (can_add_new_ear_click_range() && m_ear_click_labels.empty()) {
        return "Missing ear click label(s)";
    }

    return std::nullopt;
}

const models::TimeRange *ActiveSwallowLabellingTaskView::swallow_anpea_range() const
{
    if (can_edit_swallow_apnea_range()) {
        return &m_swallow_apnea_range;
    }

    return nullptr;
}

void ActiveSwallowLabellingTaskView::set_swallow_apnea_range(models::TimeRange range)
{
    if (can_edit_swallow_apnea_range()) {
        m_swallow_apnea_range = range;
        spdlog::debug("Set swallow apnea range to: [{}, {}]", range.start, range.end);
    } else {
        spdlog::error("Cannot set swallow apnea range");
    }
}

void ActiveSwallowLabellingTaskView::add_swallow_apnea_range(models::TimeRange range)
{
    if (can_add_new_swallow_apnea_range()) {
        m_swallow_apnea_range = range;
        spdlog::debug("Set swallow apnea range to: [{}, {}]", range.start, range.end);
    } else {
        spdlog::error("Cannot add swallow apnea range");
    }
}

namespace {

std::vector<EarClickLabel>::const_iterator
find_ear_click(const std::vector<EarClickLabel>& ear_clicks, EarClickLabel::ID id)
{
    auto it = std::lower_bound(
        ear_clicks.begin(),
        ear_clicks.end(),
        id,
        [](const auto& l1, const auto val) { return l1.id < val; }
    );
    if (it != ear_clicks.end() && it->id == id) {
        return it;
    }
    return ear_clicks.end();
}

}; // namespace

const std::vector<EarClickLabel> *ActiveSwallowLabellingTaskView::ear_click_labels() const
{
    return can_add_new_ear_click_range() ? &m_ear_click_labels : nullptr;
}

const EarClickLabel *ActiveSwallowLabellingTaskView::ear_click_label(EarClickLabel::ID id) const
{
    if (!can_add_new_ear_click_range()) {
        return nullptr;
    }

    const auto it = find_ear_click(m_ear_click_labels, id);
    if (it == m_ear_click_labels.end()) {
        spdlog::error("No ear click label with ID {}", id);
        return nullptr;
    }

    return &(*it);
}

void ActiveSwallowLabellingTaskView::remove_ear_click_label(EarClickLabel::ID id)
{
    if (!can_add_new_ear_click_range()) {
        spdlog::error("Cannot delete ear click labels!");
        return;
    }

    const auto it = find_ear_click(m_ear_click_labels, id);
    if (it == m_ear_click_labels.end()) {
        spdlog::error("No ear click label with ID {} - nothing to remove!", id);
    } else {
        const auto& range = it->range;
        m_ear_click_labels.erase(it);
        spdlog::debug("Removed ear click label with ID {}: [{}, {}]", id, range.start, range.end);
    }
}

void ActiveSwallowLabellingTaskView::add_ear_click_label(double start, double end)
{
    if (can_add_new_ear_click_range()) {
        m_ear_click_labels.emplace_back(m_next_ear_click_label_id, models::TimeRange{start, end});
        spdlog::debug(
            "Added ear click with ID {}: [{}, {}]", m_next_ear_click_label_id, start, end
        );
        m_next_ear_click_label_id++;
    } else {
        spdlog::error("Cannot add ear click label");
    }
}

void ActiveSwallowLabellingTaskView::set_ear_click_label(
    EarClickLabel::ID id, double start, double end
)
{
    if (!can_add_new_ear_click_range()) {
        spdlog::error("Cannot change ear click labels");
        return;
    }

    const auto const_it = find_ear_click(m_ear_click_labels, id);
    if (const_it == m_ear_click_labels.end()) {
        spdlog::error("No ear click label with ID {} - nothing to change!", id);
    } else {
        auto it = m_ear_click_labels.begin() + (const_it - m_ear_click_labels.cbegin());
        it->range = {start, end};
        spdlog::debug("Changed ear click label with ID {}: [{}, {}]", id, start, end);
    }
}

}; // namespace recap::labeller::app
