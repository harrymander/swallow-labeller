#include "app.hpp"

#include "app/annotation-store.hpp"
#include "app/id-list.hpp"
#include "models/annotation.hpp"
#include "models/data.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"
#include "util/optutil.hpp"
#include "util/strutil.hpp"
#include "util/variant-visitor.hpp"

#include <fmt/core.h>
#include <magic_enum.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace recap::labeller::app {

namespace fs = std::filesystem;

namespace {

std::vector<SwallowLabellingTask>
labelling_tasks(const std::vector<models::SwallowTaskInfo>& tasks_info, const fs::path& data_dir)
{
    std::vector<SwallowLabellingTask> tasks;
    tasks.reserve(tasks_info.size());
    for (const auto& info : tasks_info) {
        tasks.emplace_back( // cppcheck-suppress useStlAlgorithm
            info,
            data_dir
        );
    }
    return tasks;
}

}; // namespace

SwallowLabellingTask::SwallowLabellingTask(
    models::SwallowTaskInfo task_info, const fs::path& data_dir
) :
    m_info(std::move(task_info))
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
    AppConfig config,
    const std::vector<models::SwallowTaskInfo>& swallow_tasks,
    SwallowAnnotationStore annotation_store,
    const fs::path& data_dir,
    std::optional<SwallowAnnotationResultMap> suggested_annotations
) :
    m_config(config),
    m_swallow_task_list(
        labelling_tasks(swallow_tasks, data_dir),
        m_config.default_shuffle_tasks,
        [](const auto& a, const auto& b) { return a.info() < b.info(); }
    ),
    m_annotation_store(std::move(annotation_store)),
    m_suggested_annotations(
        suggested_annotations ? std::move(*suggested_annotations) : SwallowAnnotationResultMap{}
    ),
    m_annotation_store_error_observer(
        m_annotation_store.subscribe_sync_error([this](const std::string& err) {
            m_critical_error = fmt::format("Error syncing to annotation file: {}", err);
            spdlog::critical(*m_critical_error);
        })
    ),
    m_unsaved_task_handler(nullptr)
{
    load_active_task();
}

App::~App() = default;

class App::UnsavedTaskHandler {
public:
    UnsavedTaskHandler() = default;
    virtual ~UnsavedTaskHandler() = default;

    virtual void cancel() = 0;
    virtual void submit() = 0;

    UnsavedTaskHandler(const UnsavedTaskHandler&) = delete;
    UnsavedTaskHandler& operator=(const UnsavedTaskHandler&) = delete;
    UnsavedTaskHandler(UnsavedTaskHandler&&) = delete;
    UnsavedTaskHandler& operator=(UnsavedTaskHandler&&) = delete;
};

template <typename Submit> class UnsavedTaskSwitcher : public App::UnsavedTaskHandler {
public:
    UnsavedTaskSwitcher(App& app, Submit submit) : m_app(app), m_submit(std::move(submit)) {}

    void cancel() override
    {
        spdlog::debug("Cancelling task switch, staying on {}", m_app.m_swallow_task_list.index());
    }

    void submit() override
    {
        const auto old_index = m_app.m_swallow_task_list.index();
        m_submit();
        spdlog::debug("Switched task index {} -> {}", old_index, m_app.m_swallow_task_list.index());
        m_app.load_active_task();
    }

private:
    App& m_app;
    Submit m_submit;
};

class UnsavedTaskCloser : public App::UnsavedTaskHandler {
public:
    explicit UnsavedTaskCloser(App& app) : m_app(app) {}

    void cancel() override
    {
        spdlog::info("App close cancelled");
        m_app.m_stop_requested = false;
    }

    void submit() override
    {
        spdlog::info("Closing app");
        m_app.m_ready_to_stop = true;
    }

private:
    App& m_app;
};

void App::stop()
{
    m_stop_requested = true;
    if (m_critical_error) {
        spdlog::critical("Force-stopping application due to critical error");
        m_ready_to_stop = true;
    } else if (active_task_unsaved()) {
        spdlog::debug("Application stop requested, but there is an unsaved annotation; blocking");
        m_unsaved_task_handler = std::make_unique<UnsavedTaskCloser>(*this);
    } else {
        spdlog::info("Stopping application");
        m_ready_to_stop = true;
    }
}

template <typename Submit> void App::switch_active_task_index(Submit&& submit)
{
    auto switcher =
        std::make_unique<UnsavedTaskSwitcher<Submit>>(*this, std::forward<Submit>(submit));
    if (active_task_unsaved()) {
        spdlog::debug(
            "Task switch from index={} requested, but annotation unsaved; blocking switch",
            m_swallow_task_list.index()
        );
        m_unsaved_task_handler = std::move(switcher);
    } else {
        switcher->submit();
    }
}

void App::set_active_task_index(std::size_t index)
{
    if (m_unsaved_task_handler) {
        spdlog::error("There is already an unsaved task action pending, not switching");
        return;
    }
    if (index >= m_swallow_task_list.size()) {
        spdlog::error("Invalid task index {}; not changing!", index);
        return;
    }
    if (index == m_swallow_task_list.index()) {
        spdlog::warn("Task index is already {}; not changing!", index);
        return;
    }

    switch_active_task_index([this, index]() { m_swallow_task_list.set_index(index); });
}

void App::go_to_next_unannotated_task()
{
    auto_advance_active_task();
}

void App::go_to_next_task_in_history()
{
    if (!m_swallow_task_list.can_go_forward()) {
        spdlog::error("Cannot go forward in task history");
        return;
    }
    switch_active_task_index([this]() { m_swallow_task_list.go_forward(); });
}

void App::go_to_previous_task_in_history()
{
    if (!m_swallow_task_list.can_go_back()) {
        spdlog::error("Cannot go backwards in task history");
        return;
    }
    switch_active_task_index([this]() { m_swallow_task_list.go_back(); });
}

void App::cancel_unsaved_task_switch()
{
    if (m_unsaved_task_handler) {
        spdlog::debug("Cancelling unsaved task change");
        m_unsaved_task_handler->cancel();
        m_unsaved_task_handler.reset();
    } else {
        spdlog::warn("No task switch to cancel, staying on index {}", m_swallow_task_list.index());
    }
}

void App::save_unsaved_task_and_switch()
{
    if (m_unsaved_task_handler) {
        spdlog::debug("Saving unsaved task");
        save_active_task();
        m_unsaved_task_handler->submit();
        m_unsaved_task_handler.reset();
    } else {
        spdlog::warn("No unsaved task to save, staying on index {}", m_swallow_task_list.index());
    }
}

void App::discard_unsaved_task_and_switch()
{
    if (m_unsaved_task_handler) {
        spdlog::debug("Discarding unsaved task");
        m_unsaved_task_handler->submit();
        m_unsaved_task_handler.reset();
    } else {
        spdlog::warn(
            "No unsaved task to discard, staying on index {}", m_swallow_task_list.index()
        );
    }
}

void App::reload_active_task()
{
    if (active_task_unsaved()) {
        spdlog::error("Current task is unsaved, cannot reload");
    } else {
        spdlog::info("Reloading active task...");
        load_active_task();
    }
}

void App::save_active_task()
{
    auto *task_view = active_task_labelling_view();
    if (task_view) {
        spdlog::debug("Saving annotation for active task");
        if (m_auto_advance_on_save && !m_swallow_task_list.can_go_forward()) {
            auto_advance_active_task();
        }
    } else {
        spdlog::error("Cannot save annotation for active task: in error state");
    }
}

void App::save_annotations_to_path(const std::filesystem::path& path) const
{
    try {
        m_annotation_store.sync_to_file(path);
    } catch (const std::runtime_error& error) {
        spdlog::error("Error saving annotations to {}: {}", path, error.what());
    }
}

template <typename T, typename... Args>
std::unique_ptr<App::ActiveTaskVariant> App::make_unique_active_task(Args&&...args)
{
    return std::make_unique<ActiveTaskVariant>(T{std::forward<Args>(args)...});
}

void App::auto_advance_active_task()
{
    spdlog::debug("Finding next unannotated task...");
    const std::size_t active_index = m_swallow_task_list.index();
    std::size_t index = active_index + 1;
    while (index != active_index) {
        if (index == m_swallow_task_list.size()) {
            // If the active index is 0 and we have reached end of list, then we have completed a
            // full loop
            if (active_index == 0) {
                break;
            }
            index = 0;
        }

        const auto& task = m_swallow_task_list.at(index);
        if (!task.error_msg().has_value()) {
            set_active_task_index(index);
            return;
        }

        index += 1;
    }

    spdlog::info("No more un-annotated tasks to auto-advance to!");
}

void App::load_active_task()
{
    SwallowLabellingTask& task = m_swallow_task_list.index_item();
    const auto path = fs::path(task.data_path());
    if (!fs::is_regular_file(path)) {
        task.set_error_msg("Data file not found");
        m_active_task = make_unique_active_task<ActiveSwallowLabellingTaskErrorView>(task);
    } else {
        try {
            auto data = SwallowTaskData::from_numpy(cnpy::npz_load(task.data_path()));
            spdlog::debug("Loaded data from {}", task.data_path());
            task.clear_error_msg();
            m_active_task = make_unique_active_task<ActiveSwallowLabellingTaskView>(
                *this, task, data, m_annotation_store, m_suggested_annotations
            );
        } catch (const std::exception& e) {
            spdlog::error("Error loading data file from {}: {}", task.data_path(), e.what());
            task.set_error_msg(fmt::format("Error loading data file: {}", e.what()));
            m_active_task = make_unique_active_task<ActiveSwallowLabellingTaskErrorView>(task);
        }
    }

    m_new_active_task_observable.notify(*m_active_task);
}

ActiveSwallowLabellingTaskView::ActiveSwallowLabellingTaskView(
    const App& app,
    SwallowLabellingTask& task,
    SwallowTaskData data,
    SwallowAnnotationStore& annotation_store,
    const SwallowAnnotationResultMap& suggested_annotations
) :
    m_app(app), m_task(task), m_data(std::move(data)), m_annotation_store(annotation_store)
{}

}; // namespace recap::labeller::app
