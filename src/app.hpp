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
#include <utility>
#include <vector>

namespace recap::labeller::app {

class SwallowLabellingTask {
public:
    SwallowLabellingTask(
        recap::labeller::SwallowTaskInfo task_info,
        const std::filesystem::path& data_dir,
        std::optional<SwallowAnnotation> annotation
    );

    [[nodiscard]] const std::optional<std::string>& error_msg() const { return m_error_msg; }

    [[nodiscard]] const recap::labeller::SwallowTaskInfo& info() const { return m_info; }

    [[nodiscard]] const std::string& data_path() const { return m_data_path; }

    [[nodiscard]] bool is_annotated() const { return m_annotation.has_value(); }

    [[nodiscard]] const std::optional<SwallowAnnotation>& annotation() const
    {
        return m_annotation;
    }

    void set_error_msg(std::string str);
    void clear_error_msg();

private:
    std::optional<std::string> m_error_msg = std::nullopt;

    recap::labeller::SwallowTaskInfo m_info;
    std::optional<SwallowAnnotation> m_annotation;
    std::string m_data_path;
};

class ActiveSwallowLabellingTaskErrorView {
public:
    [[nodiscard]] const SwallowTaskInfo& info() const { return m_task.info(); }

    [[nodiscard]] const std::string& error_msg() const { return *m_task.error_msg(); }

private:
    friend class App;

    explicit ActiveSwallowLabellingTaskErrorView(const SwallowLabellingTask& task) : m_task(task) {}

    const SwallowLabellingTask& m_task;
};

class ActiveSwallowLabellingTaskView {
public:
    [[nodiscard]] const SwallowTaskData& data() const { return m_data; }

    [[nodiscard]] const SwallowTaskInfo& info() const { return m_task.info(); }

private:
    friend class App;

    ActiveSwallowLabellingTaskView(SwallowLabellingTask& task, SwallowTaskData data);

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

    using ActiveTaskVariant =
        std::variant<ActiveSwallowLabellingTaskView, ActiveSwallowLabellingTaskErrorView>;
    using NewActiveTaskObservable = Observable<const ActiveTaskVariant&>;

    ActiveTaskVariant& active_task_view() { return *m_active_task; }

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

    template <typename T, typename... Args>
    static std::unique_ptr<ActiveTaskVariant> make_unique_active_task(Args&&...);
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_INCLUDE_HPP
