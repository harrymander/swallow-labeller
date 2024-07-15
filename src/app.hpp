#ifndef RECAP_LABELLER_APP_INCLUDE_HPP
#define RECAP_LABELLER_APP_INCLUDE_HPP

#include "annotation-manager.hpp"
#include "annotation.hpp"
#include "data.hpp"
#include "labelling-task.hpp"
#include "observable.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace recap::labeller::app {

enum class SwallowLabellingTaskState {
    Unannotated,
    Annotated,
    DataFileNotFound,
    DataFileReadError,
};

class SwallowLabellingTask {
public:
    SwallowLabellingTask(
        recap::labeller::SwallowTaskInfo task_info,
        const std::filesystem::path& data_dir,
        std::optional<SwallowAnnotation> annotation
    );

    [[nodiscard]] SwallowLabellingTaskState state() const { return m_state; }

    [[nodiscard]] const recap::labeller::SwallowTaskInfo& info() const { return m_info; }

    [[nodiscard]] const std::string& data_path() const { return m_data_path; }

    void set_state(SwallowLabellingTaskState state) { m_state = state; }

private:
    SwallowLabellingTaskState m_state = SwallowLabellingTaskState::Unannotated;

    recap::labeller::SwallowTaskInfo m_info;
    std::optional<SwallowAnnotation> m_annotation;
    std::string m_data_path;
};

struct SwallowLabellingTaskError {
    std::string message;
};

class SwallowLabellingTaskManager {
public:
    SwallowLabellingTaskManager(SwallowLabellingTask& task, SwallowTaskData data);

    [[nodiscard]] const SwallowTaskData& data() const { return m_data; }

    [[nodiscard]] SwallowLabellingTaskState state() const { return m_task.state(); }

    [[nodiscard]] const recap::labeller::SwallowTaskInfo& info() const { return m_task.info(); }

private:
    SwallowLabellingTask& m_task;
    SwallowTaskData m_data;
};

class App {
public:
    App(const std::vector<SwallowTaskInfo>& swallow_tasks,
        SwallowAnnotationManager annotation_manager,
        const std::filesystem::path& data_dir);

    [[nodiscard]] const std::vector<SwallowLabellingTask>& tasks() const { return m_swallow_tasks; }

    [[nodiscard]] std::size_t num_annotated_tasks() const { return m_num_annotated_tasks; }

    [[nodiscard]] std::size_t active_task_index() const { return m_active_task_index; }

    void set_active_task_index(std::size_t index);

    using ActiveTaskVariant = std::variant<SwallowLabellingTaskError, SwallowLabellingTaskManager>;
    using NewActiveTaskObservable = Observable<const ActiveTaskVariant&>;

    ActiveTaskVariant& active_task() { return *m_active_task; }

    NewActiveTaskObservable::Observer
    subscribe_new_active_task(NewActiveTaskObservable::Function&& func)
    {
        return m_new_active_task_observable.subscribe(func);
    }

    void reload_active_task();

private:
    std::vector<SwallowLabellingTask> m_swallow_tasks;
    SwallowAnnotationManager m_annotation_manager;
    std::size_t m_num_annotated_tasks;
    std::unique_ptr<ActiveTaskVariant> m_active_task;
    NewActiveTaskObservable m_new_active_task_observable;

    std::size_t m_active_task_index = 0;

    void load_active_task();
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_INCLUDE_HPP
