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
#include <string>
#include <string_view>
#include <vector>

namespace recap::labeller::app {

class SwallowLabellingTask {
public:
    SwallowLabellingTask(models::SwallowTaskInfo task_info, const std::filesystem::path& data_dir);

    [[nodiscard]] const std::optional<std::string>& error_msg() const { return m_error_msg; }

    [[nodiscard]] const models::SwallowTaskInfo& info() const { return m_info; }

    [[nodiscard]] const std::string& data_path() const { return m_data_path; }

    [[nodiscard]] const std::string& annotation_id() const { return m_info.get_id(); }

    void set_error_msg(std::string str);
    void clear_error_msg();

private:
    std::optional<std::string> m_error_msg = std::nullopt;

    models::SwallowTaskInfo m_info;
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
        return m_annotation.swallow_apnea_status;
    }

    void set_swallow_apnea_annotation_status(SwallowApneaAnnotationStatus status)
    {
        m_annotation.swallow_apnea_status = status;
    }

    [[nodiscard]] bool swallow_is_ambiguous() const { return m_annotation.swallow_is_ambiguous; }

    void set_swallow_is_ambiguous(bool is_ambiguous)
    {
        m_annotation.swallow_is_ambiguous = is_ambiguous;
    }

    [[nodiscard]] bool can_add_new_swallow_apnea_range() const;
    [[nodiscard]] bool can_edit_swallow_apnea_range() const;
    [[nodiscard]] bool can_delete_swallow_apnea_range() const;
    [[nodiscard]] std::optional<std::string_view> swallow_apnea_label_error() const;
    [[nodiscard]] const models::TimeRange *swallow_anpea_range() const;
    void add_swallow_apnea_range(models::TimeRange range);
    void set_swallow_apnea_range(models::TimeRange range);
    void delete_swallow_apnea_range();

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
        return m_annotation.ear_click_status;
    }

    void set_ear_click_annotation_status(EarClickAnnotationStatus status)
    {
        m_annotation.ear_click_status = status;
    }

    [[nodiscard]] bool can_add_new_ear_click_range() const;
    [[nodiscard]] std::optional<std::string_view> earclick_label_error() const;

    [[nodiscard]] const std::vector<EarClickLabel> *ear_click_labels() const;
    [[nodiscard]] const EarClickLabel *ear_click_label(EarClickLabel::ID id) const;
    std::optional<EarClickLabel::ID> add_ear_click_label(double start, double end);
    void set_ear_click_label(EarClickLabel::ID id, double start, double end);
    void remove_ear_click_label(EarClickLabel::ID id);

    // TODO
    [[nodiscard]] bool can_delete_annotation() const { return true; }

    [[nodiscard]] bool can_save_annotation() const;

    void save_annotation();

    void delete_annotation();

private:
    friend class App;

    [[nodiscard]] bool has_apnea_range() const;
    [[nodiscard]] bool valid_apnea_annotation() const;
    [[nodiscard]] bool valid_earclick_annotation() const;

    ActiveSwallowLabellingTaskView(
        SwallowLabellingTask& task, SwallowTaskData data, SwallowAnnotationStore& annotation_store
    );

    SwallowLabellingTask& m_task;
    SwallowTaskData m_data;
    SwallowAnnotationStore& m_annotation_store;

    struct Annotation {
        Annotation() = default;
        explicit Annotation(const models::SwallowAnnotation& annotation);

        [[nodiscard]] models::SwallowAnnotation to_model() const;

        SwallowApneaAnnotationStatus swallow_apnea_status = SwallowApneaAnnotationStatus::ExEx;
        bool swallow_is_ambiguous = false;
        models::TimeRange swallow_apnea_range = {NAN, NAN};

        EarClickAnnotationStatus ear_click_status = EarClickAnnotationStatus::Ok;
        std::vector<EarClickLabel> ear_click_labels;
        EarClickLabel::ID next_ear_click_label_id = 1;
    };

    Annotation m_annotation;
};

class App {
public:
    App(const std::vector<models::SwallowTaskInfo>& swallow_tasks,
        SwallowAnnotationStore annotation_store,
        const std::filesystem::path& data_dir);

    [[nodiscard]] const std::vector<SwallowLabellingTask>& tasks() const { return m_swallow_tasks; }

    [[nodiscard]] std::size_t num_annotated_tasks() const { return m_annotation_store.size(); }

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

    [[nodiscard]] bool task_has_annotation(const SwallowLabellingTask& task)
    {
        return m_annotation_store.has_annotation(task.info().get_id());
    }

    [[nodiscard]] const std::optional<std::string>& critical_error() const
    {
        return m_critical_error;
    }

private:
    std::vector<SwallowLabellingTask> m_swallow_tasks;
    SwallowAnnotationStore m_annotation_store;
    std::unique_ptr<ActiveTaskVariant> m_active_task;
    NewActiveTaskObservable m_new_active_task_observable;
    SwallowAnnotationStore::ErrorObservable::Observer m_annotation_store_error_observer;

    std::size_t m_active_task_index = 0;
    std::optional<std::string> m_critical_error = std::nullopt;

    void load_active_task();

    template <typename T, typename... Args>
    static std::unique_ptr<ActiveTaskVariant> make_unique_active_task(Args&&...);
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_INCLUDE_HPP
