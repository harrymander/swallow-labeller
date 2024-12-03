#ifndef RECAP_LABELLER_APP_INCLUDE_HPP
#define RECAP_LABELLER_APP_INCLUDE_HPP

#include "app/annotation-store.hpp"
#include "app/config.hpp"
#include "app/id-list.hpp"
#include "app/view-list.hpp"
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

using TimeRangeIDList = IDList<models::TimeRange>;
using EarClickLabel = TimeRangeIDList::Item;
using NonRespFlowLabel = TimeRangeIDList::Item;

class App;

class ActiveSwallowLabellingTaskView {
public:
    [[nodiscard]] const SwallowTaskData& data() const { return m_data; }

    [[nodiscard]] const models::SwallowTaskInfo& info() const { return m_task.info(); }

    [[nodiscard]] bool annotation_unsaved() const;

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
    [[nodiscard]] bool swallow_apnea_label_error() const;
    [[nodiscard]] std::optional<std::string> swallow_apnea_label_error_str() const;
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

    [[nodiscard]] bool can_add_new_non_resp_flow_label() const;
    [[nodiscard]] const std::vector<NonRespFlowLabel> *non_resp_flow_labels() const;
    [[nodiscard]] const models::TimeRange *non_resp_flow_label(NonRespFlowLabel::ID id) const;
    std::optional<NonRespFlowLabel::ID> add_non_resp_flow_label(double start, double end);
    void set_non_resp_flow_label(NonRespFlowLabel::ID id, double start, double end);
    void remove_non_resp_flow_label(NonRespFlowLabel::ID id);

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
    [[nodiscard]] const models::TimeRange *ear_click_label(EarClickLabel::ID id) const;
    std::optional<EarClickLabel::ID> add_ear_click_label(double start, double end);
    void set_ear_click_label(EarClickLabel::ID id, double start, double end);
    void remove_ear_click_label(EarClickLabel::ID id);

    [[nodiscard]] const std::string& note() const { return m_annotation.note; }

    void set_note(std::string note) { m_annotation.note = std::move(note); }

    // TODO
    [[nodiscard]] static constexpr bool can_delete_annotation() { return true; }

    [[nodiscard]] bool can_save_annotation() const;

    void delete_annotation();

private:
    friend class App;

    [[nodiscard]] bool save_annotation();
    [[nodiscard]] bool has_apnea_range() const;
    [[nodiscard]] bool valid_earclick_annotation() const;

    ActiveSwallowLabellingTaskView(
        const App& app,
        SwallowLabellingTask& task,
        SwallowTaskData data,
        SwallowAnnotationStore& annotation_store,
        const SwallowAnnotationResultMap& suggested_annotations
    );

    const App& m_app;
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
        TimeRangeIDList non_resp_flow_labels;

        EarClickAnnotationStatus ear_click_status = EarClickAnnotationStatus::Ok;
        TimeRangeIDList ear_click_labels;

        std::string note;
    };

    Annotation m_annotation;
    std::optional<Annotation> m_suggested_annotation = std::nullopt;
};

class App {
public:
    App(AppConfig config,
        const std::vector<models::SwallowTaskInfo>& swallow_tasks,
        SwallowAnnotationStore annotation_store,
        const std::filesystem::path& data_dir,
        bool shuffle_tasks,
        std::optional<SwallowAnnotationResultMap> suggested_annotations);

    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    void stop();

    [[nodiscard]] const AppConfig& config() const { return m_config; }

    [[nodiscard]] bool can_stop() const { return m_ready_to_stop; }

    using SwallowLabellingTaskList = ViewList<SwallowLabellingTask>;

    [[nodiscard]] const SwallowLabellingTaskList& tasks() const { return m_swallow_task_list; }

    [[nodiscard]] bool tasks_shuffled() const { return m_swallow_task_list.shuffled(); }

    void shuffle_tasks() { m_swallow_task_list.shuffle(); }

    void unshuffle_tasks() { m_swallow_task_list.unshuffle(); }

    [[nodiscard]] std::size_t active_task_index() const { return m_swallow_task_list.index(); }

    [[nodiscard]] bool can_go_to_previous_task() const { return m_swallow_task_list.can_go_back(); }

    [[nodiscard]] bool can_go_to_forward_task() const
    {
        return m_swallow_task_list.can_go_forward();
    }

    /**
     * [FIXME] The below functions may change the active task view, so any references returned from
     * active_task_view() may be invalidated and should not be used following a call to any of these
     * functions.
     */
    void set_active_task_index(std::size_t index);
    void go_to_next_unannotated_task();
    void go_to_previous_task_in_history();
    void go_to_next_task_in_history();
    void reload_active_task();
    void cancel_unsaved_task_switch();
    void save_unsaved_task_and_switch();
    void discard_unsaved_task_and_switch();
    void save_active_task();

    [[nodiscard]] bool unsaved_task_switch_blocked() const
    {
        return m_unsaved_task_handler != nullptr;
    }

    using ActiveTaskVariant =
        std::variant<ActiveSwallowLabellingTaskView, ActiveSwallowLabellingTaskErrorView>;
    using NewActiveTaskObservable = Observable<const ActiveTaskVariant&>;

    ActiveTaskVariant& active_task_view() { return *m_active_task; }

    NewActiveTaskObservable::Observer
    subscribe_new_active_task(NewActiveTaskObservable::Function&& func)
    {
        return m_new_active_task_observable.subscribe(func);
    }

    [[nodiscard]] bool task_has_annotation(const SwallowLabellingTask& task) const
    {
        return m_annotation_store.has_annotation(task.info().get_id());
    }

    [[nodiscard]] bool task_has_suggested_annotation(const SwallowLabellingTask& task) const
    {
        return m_suggested_annotations.contains(task.info().get_id());
    }

    [[nodiscard]] const std::optional<std::string>& critical_error() const
    {
        return m_critical_error;
    }

    [[nodiscard]] bool auto_advance_on_save() const { return m_auto_advance_on_save; }

    void set_auto_advance_on_save(bool advance) { m_auto_advance_on_save = advance; }

    [[nodiscard]] const std::filesystem::path& annotations_path() const
    {
        return m_annotation_store.path();
    }

    void save_annotations_to_path(const std::filesystem::path& path) const;

private:
    class UnsavedTaskHandler;
    friend class UnsavedTaskCloser;
    template <typename Submit> friend class UnsavedTaskSwitcher;

    AppConfig m_config;
    SwallowLabellingTaskList m_swallow_task_list;
    SwallowAnnotationStore m_annotation_store;
    SwallowAnnotationResultMap m_suggested_annotations;
    std::unique_ptr<ActiveTaskVariant> m_active_task;
    NewActiveTaskObservable m_new_active_task_observable;
    SwallowAnnotationStore::ErrorObservable::Observer m_annotation_store_error_observer;
    std::unique_ptr<UnsavedTaskHandler> m_unsaved_task_handler;

    bool m_stop_requested = false;
    bool m_ready_to_stop = false;
    std::optional<std::string> m_critical_error = std::nullopt;
    bool m_auto_advance_on_save = true;

    [[nodiscard]] bool active_task_unsaved() const;
    void load_active_task();
    void auto_advance_active_task();
    template <typename Submit> void switch_active_task_index(Submit&& submit);

    template <typename T, typename... Args>
    static std::unique_ptr<ActiveTaskVariant> make_unique_active_task(Args&&...);
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_INCLUDE_HPP
