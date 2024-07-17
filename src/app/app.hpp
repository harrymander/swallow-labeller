#ifndef RECAP_LABELLER_APP_INCLUDE_HPP
#define RECAP_LABELLER_APP_INCLUDE_HPP

#include "app/annotation-store.hpp"
#include "models/annotation.hpp"
#include "models/data.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"
#include "util/observable.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace recap::labeller::app {

class SwallowLabellingTask {
public:
    SwallowLabellingTask(
        models::SwallowTaskInfo task_info,
        const std::filesystem::path& data_dir,
        std::optional<models::SwallowAnnotation> annotation
    );

    [[nodiscard]] const std::optional<std::string>& error_msg() const { return m_error_msg; }

    [[nodiscard]] const models::SwallowTaskInfo& info() const { return m_info; }

    [[nodiscard]] const std::string& data_path() const { return m_data_path; }

    [[nodiscard]] bool is_annotated() const { return m_annotation.has_value(); }

    [[nodiscard]] const std::optional<models::SwallowAnnotation>& annotation() const
    {
        return m_annotation;
    }

    void set_error_msg(std::string str);
    void clear_error_msg();

private:
    std::optional<std::string> m_error_msg = std::nullopt;

    models::SwallowTaskInfo m_info;
    std::optional<models::SwallowAnnotation> m_annotation;
    std::string m_data_path;
};

class ActiveSwallowLabellingTaskErrorView {
public:
    [[nodiscard]] const models::SwallowTaskInfo& info() const { return m_task.info(); }

    [[nodiscard]] const std::string& error_msg() const { return *m_task.error_msg(); }

private:
    friend class App;

    explicit ActiveSwallowLabellingTaskErrorView(const SwallowLabellingTask& task) : m_task(task) {}

    const SwallowLabellingTask& m_task;
};

enum class SwallowApneaAnnotationStatus {
    ExEx,
    ExIn,
    InEx,
    InIn,
    FlowError,
    NoSwallow,
    ApneaCutoff,
};

enum class EarClickAnnotationStatus {
    Ok,
    AudioError,
    NoEarClick,
};

struct EarClickLabel {
    using ID = std::size_t;

    ID id;
    models::TimeRange range;
};

class ActiveSwallowLabellingTaskView {
public:
    [[nodiscard]] const SwallowTaskData& data() const { return m_data; }

    [[nodiscard]] const models::SwallowTaskInfo& info() const { return m_task.info(); }

    [[nodiscard]] SwallowApneaAnnotationStatus swallow_apnea_annotation_status() const
    {
        return m_swallow_apnea_status;
    }

    void set_swallow_apnea_annotation_status(SwallowApneaAnnotationStatus status)
    {
        m_swallow_apnea_status = status;
    }

    [[nodiscard]] bool swallow_is_ambiguous() const { return m_swallow_is_ambiguous; }

    void set_swallow_is_ambiguous(bool is_ambiguous) { m_swallow_is_ambiguous = is_ambiguous; }

    [[nodiscard]] bool can_add_new_swallow_apnea_range() const;
    [[nodiscard]] bool can_edit_swallow_apnea_range() const;
    [[nodiscard]] std::optional<std::string_view> swallow_apnea_label_error() const;
    [[nodiscard]] const models::TimeRange *swallow_anpea_range() const;
    void set_swallow_apnea_range(models::TimeRange range);
    void add_swallow_apnea_range(models::TimeRange range);

    void set_swallow_apnea_range(double start, double end)
    {
        set_swallow_apnea_range({start, end});
    }

    void add_swallow_apnea_range(double start, double end)
    {
        add_swallow_apnea_range({start, end});
    }

    [[nodiscard]] EarClickAnnotationStatus ear_click_annotation_status() const
    {
        return m_ear_click_status;
    }

    void set_ear_click_annotation_status(EarClickAnnotationStatus status)
    {
        m_ear_click_status = status;
    }

    [[nodiscard]] bool can_add_new_ear_click_range() const;
    [[nodiscard]] std::optional<std::string_view> earclick_label_error() const;

    [[nodiscard]] const std::vector<EarClickLabel> *ear_click_labels() const;
    [[nodiscard]] const EarClickLabel *ear_click_label(EarClickLabel::ID id) const;
    void add_ear_click_label(double start, double end);
    void set_ear_click_label(EarClickLabel::ID id, double start, double end);
    void remove_ear_click_label(EarClickLabel::ID id);

private:
    friend class App;

    ActiveSwallowLabellingTaskView(SwallowLabellingTask& task, SwallowTaskData data);

    SwallowLabellingTask& m_task;
    SwallowTaskData m_data;

    SwallowApneaAnnotationStatus m_swallow_apnea_status = SwallowApneaAnnotationStatus::ExEx;
    bool m_swallow_is_ambiguous = false;
    models::TimeRange m_swallow_apnea_range = {NAN, NAN};

    EarClickAnnotationStatus m_ear_click_status = EarClickAnnotationStatus::Ok;
    std::vector<EarClickLabel> m_ear_click_labels;
    EarClickLabel::ID m_next_ear_click_label_id = 0;
};

class App {
public:
    App(const std::vector<models::SwallowTaskInfo>& swallow_tasks,
        SwallowAnnotationStore annotation_store,
        const std::filesystem::path& data_dir);

    [[nodiscard]] const std::vector<SwallowLabellingTask>& tasks() const { return m_swallow_tasks; }

    [[nodiscard]] std::size_t num_annotated_tasks() const { return m_num_annotated_tasks; }

    [[nodiscard]] std::size_t active_task_index() const { return m_active_task_index; }

    void set_active_task_index(std::size_t index);
    void increment_active_task_index();
    void decrement_active_task_index();

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
    SwallowAnnotationStore m_annotation_store;
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
