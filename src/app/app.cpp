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
#include <spdlog/spdlog.h>

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
    const std::vector<models::SwallowTaskInfo>& swallow_tasks,
    SwallowAnnotationStore annotation_store,
    const fs::path& data_dir,
    bool shuffle_tasks
) :
    m_swallow_task_list(
        labelling_tasks(swallow_tasks, data_dir),
        shuffle_tasks,
        [](const auto& a, const auto& b) { return a.info() < b.info(); }
    ),
    m_annotation_store(std::move(annotation_store)),
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
    auto *task_view = std::get_if<ActiveSwallowLabellingTaskView>(m_active_task.get());
    if (task_view) {
        spdlog::debug("Saving annotation for active task");
        if (task_view->save_annotation() && m_auto_advance_on_save) {
            auto_advance_active_task();
        }
    } else {
        spdlog::error("Cannot save annotation for active task: in error state");
    }
}

template <typename T, typename... Args>
std::unique_ptr<App::ActiveTaskVariant> App::make_unique_active_task(Args&&...args)
{
    auto *const ptr = new ActiveTaskVariant(T(std::forward<Args>(args)...));
    return std::unique_ptr<ActiveTaskVariant>(ptr);
}

bool App::active_task_unsaved() const
{
    const auto *task_view = std::get_if<ActiveSwallowLabellingTaskView>(m_active_task.get());
    if (task_view) {
        return task_view->annotation_unsaved();
    }
    return false;
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
        if (!(task_has_annotation(task) || task.error_msg().has_value())) {
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
                task, data, m_annotation_store
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
    SwallowLabellingTask& task, SwallowTaskData data, SwallowAnnotationStore& annotation_store
) :
    m_task(task), m_data(std::move(data)), m_annotation_store(annotation_store)
{
    const auto *annotation = m_annotation_store.get_annotation(task.annotation_id());
    if (annotation) {
        m_annotation = Annotation(*annotation);
    }
}

namespace {

SwallowApneaAnnotationStatus apnea_status_from_swallow_apnea_model(
    const std::variant<models::SwallowApneaAnnotation, models::SwallowApneaError>& v
)
{
    auto name = VariantVisitor{
        [](const models::SwallowApneaAnnotation& annotation) {
            return magic_enum::enum_name(annotation.pattern);
        },
        [](const models::SwallowApneaError& error) { return magic_enum::enum_name(error); },
    }(v);
    return magic_enum::enum_cast<SwallowApneaAnnotationStatus>(name).value();
}

EarClickAnnotationStatus ear_click_status_from_ear_click_model(
    const std::variant<std::vector<models::TimeRange>, models::EarClickError>& v
)
{
    return VariantVisitor{
        [](const std::vector<models::TimeRange>&) { return EarClickAnnotationStatus::Ok; },
        [](const models::EarClickError& error) {
            return magic_enum::enum_cast<EarClickAnnotationStatus>(magic_enum::enum_name(error))
                .value();
        },
    }(v);
}

}; // namespace

ActiveSwallowLabellingTaskView::Annotation::Annotation(const models::SwallowAnnotation& annotation
) :
    swallow_apnea_status(apnea_status_from_swallow_apnea_model(annotation.swallow_apnea)),

    swallow_is_ambiguous(VariantVisitor{
        [](const models::SwallowApneaAnnotation& annotation) { return annotation.is_ambiguous; },
        [](auto) { return false; },
    }(annotation.swallow_apnea)),

    swallow_apnea_range(VariantVisitor{
        [](const models::SwallowApneaAnnotation& annotation) { return annotation.time; },
        [](auto) {
            return models::TimeRange{NAN, NAN};
        },
    }(annotation.swallow_apnea)),

    non_resp_flow_labels(VariantVisitor{
        [](const models::SwallowApneaAnnotation& annotation) {
            return annotation.non_respiratory_flow;
        },
        [](auto) { return std::vector<models::TimeRange>{}; },
    }(annotation.swallow_apnea)),

    ear_click_status(ear_click_status_from_ear_click_model(annotation.ear_clicks)),

    ear_click_labels(VariantVisitor{
        [](const std::vector<models::TimeRange>& ranges) { return ranges; },
        [](auto) { return std::vector<models::TimeRange>{}; },
    }(annotation.ear_clicks)),

    note(optutil::value_or_default(annotation.note))
{}

namespace {

[[nodiscard]] inline bool apnea_status_is_src_pattern(SwallowApneaAnnotationStatus status)
{
    using enum SwallowApneaAnnotationStatus;
    return status == ExEx || status == InEx || status == ExIn || status == InIn;
}

template <typename T> std::vector<T> id_list_items_vector(const IDList<T>& id_list)
{
    std::vector<T> v;
    v.reserve(id_list.size());
    for (const auto& item : id_list.items()) {
        v.push_back(item.item);
    }
    return v;
}

[[nodiscard]] std::variant<models::SwallowApneaAnnotation, models::SwallowApneaError>
swallow_apnea_annotation_model(
    bool is_ambiguous,
    SwallowApneaAnnotationStatus status,
    models::TimeRange range,
    const TimeRangeIDList& non_resp_flow_labels
)
{
    auto pattern = magic_enum::enum_cast<models::SRCPattern>(magic_enum::enum_name(status)).value();
    return models::SwallowApneaAnnotation{
        .is_ambiguous = is_ambiguous,
        .pattern = pattern,
        .time = range,
        .non_respiratory_flow = id_list_items_vector(non_resp_flow_labels),
    };
}

[[nodiscard]] std::variant<models::SwallowApneaAnnotation, models::SwallowApneaError>
swallow_apnea_error_model(SwallowApneaAnnotationStatus status)
{
    return magic_enum::enum_cast<models::SwallowApneaError>(magic_enum::enum_name(status)).value();
}

[[nodiscard]] std::variant<std::vector<models::TimeRange>, models::EarClickError>
ear_clicks_annotation_model(EarClickAnnotationStatus status, const TimeRangeIDList& labels)
{
    switch (status) {
    case EarClickAnnotationStatus::NoEarClick:
        return models::EarClickError::NoEarClick;
    case EarClickAnnotationStatus::AudioError:
        return models::EarClickError::AudioError;
    case EarClickAnnotationStatus::Ok:
        break;
    }

    if (labels.empty()) {
        // Could throw an exception like above, but will play it safe
        spdlog::error("Ear click status is Ok, but no labels! Returning NoEarClick error");
        return models::EarClickError::NoEarClick;
    };

    return id_list_items_vector(labels);
}

std::optional<std::string> note_annotation_model(std::string note)
{
    strutil::trim(note);
    if (note.empty()) {
        return std::nullopt;
    }
    return note;
}

}; // namespace

models::SwallowAnnotation ActiveSwallowLabellingTaskView::Annotation::to_model() const
{
    return {
        .swallow_apnea = apnea_status_is_src_pattern(swallow_apnea_status) ?
            swallow_apnea_annotation_model(
                swallow_is_ambiguous,
                swallow_apnea_status,
                swallow_apnea_range,
                non_resp_flow_labels
            ) :
            swallow_apnea_error_model(swallow_apnea_status),
        .ear_clicks = ear_clicks_annotation_model(ear_click_status, ear_click_labels),
        .note = note_annotation_model(note),
    };
}

namespace {

[[nodiscard]] inline bool timerange_allnan(const models::TimeRange& range)
{
    return std::isnan(range.start) && std::isnan(range.end);
}

}; // namespace

bool ActiveSwallowLabellingTaskView::has_apnea_range() const
{
    return !timerange_allnan(m_annotation.swallow_apnea_range);
}

bool ActiveSwallowLabellingTaskView::can_edit_swallow_apnea_range() const
{
    return apnea_status_is_src_pattern(m_annotation.swallow_apnea_status) && has_apnea_range();
}

bool ActiveSwallowLabellingTaskView::can_delete_swallow_apnea_range() const
{
    return can_edit_swallow_apnea_range();
}

bool ActiveSwallowLabellingTaskView::can_add_new_swallow_apnea_range() const
{
    return apnea_status_is_src_pattern(m_annotation.swallow_apnea_status) && !has_apnea_range();
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
    return m_annotation.ear_click_status == EarClickAnnotationStatus::Ok;
}

std::optional<std::string_view> ActiveSwallowLabellingTaskView::earclick_label_error() const
{
    if (can_add_new_ear_click_range() && m_annotation.ear_click_labels.empty()) {
        return "Missing ear click label(s)";
    }

    return std::nullopt;
}

const models::TimeRange *ActiveSwallowLabellingTaskView::swallow_anpea_range() const
{
    if (can_edit_swallow_apnea_range()) {
        return &m_annotation.swallow_apnea_range;
    }

    return nullptr;
}

void ActiveSwallowLabellingTaskView::set_swallow_apnea_range(models::TimeRange range)
{
    if (can_edit_swallow_apnea_range()) {
        m_annotation.swallow_apnea_range = range;
        spdlog::debug("Set swallow apnea range to: [{}, {}]", range.start, range.end);
    } else {
        spdlog::error("Cannot set swallow apnea range");
    }
}

void ActiveSwallowLabellingTaskView::add_swallow_apnea_range(models::TimeRange range)
{
    if (can_add_new_swallow_apnea_range()) {
        m_annotation.swallow_apnea_range = range;
        spdlog::debug("Set swallow apnea range to: [{}, {}]", range.start, range.end);
    } else {
        spdlog::error("Cannot add swallow apnea range");
    }
}

void ActiveSwallowLabellingTaskView::delete_swallow_apnea_range()
{
    if (can_delete_swallow_apnea_range()) {
        spdlog::debug(
            "Deleted swallow apnea range: [{}, {}]",
            m_annotation.swallow_apnea_range.start,
            m_annotation.swallow_apnea_range.end
        );
        m_annotation.swallow_apnea_range = {NAN, NAN};
    } else {
        spdlog::error("Cannot delete swallow apnea range");
    }
}

bool ActiveSwallowLabellingTaskView::can_add_new_non_resp_flow_label() const
{
    return can_edit_swallow_apnea_range() && has_apnea_range();
}

const std::vector<NonRespFlowLabel> *ActiveSwallowLabellingTaskView::non_resp_flow_labels() const
{
    return can_add_new_non_resp_flow_label() ? &m_annotation.non_resp_flow_labels.items() : nullptr;
}

const models::TimeRange *ActiveSwallowLabellingTaskView::non_resp_flow_label(NonRespFlowLabel::ID id
) const
{
    if (!can_add_new_non_resp_flow_label()) {
        return nullptr;
    }

    const auto *label = m_annotation.non_resp_flow_labels.get_item(id);
    if (label == nullptr) {
        spdlog::error("No non-respiratory flow label with ID {}", id);
        return nullptr;
    }

    return label;
}

std::optional<NonRespFlowLabel::ID>
ActiveSwallowLabellingTaskView::add_non_resp_flow_label(double start, double end)
{
    if (!can_add_new_non_resp_flow_label()) {
        spdlog::error("Cannot add non-respiratory flow label");
        return std::nullopt;
    }

    const auto new_id = m_annotation.non_resp_flow_labels.add_item({start, end});
    spdlog::debug("Added non-respiratory flow label with ID {}: [{}, {}]", new_id, start, end);
    return new_id;
}

void ActiveSwallowLabellingTaskView::set_non_resp_flow_label(
    NonRespFlowLabel::ID id, double start, double end
)
{
    if (!can_add_new_non_resp_flow_label()) {
        spdlog::error("Cannot change non-respiratory flow labels");
        return;
    }

    auto *range = m_annotation.non_resp_flow_labels.get_item(id);
    if (range == nullptr) {
        spdlog::error("No non-respiratory flow label with ID {} - nothing to change!", id);
    } else {
        *range = {start, end};
        spdlog::debug("Changed non-respiratory flow label with ID {}: [{}, {}]", id, start, end);
    }
}

void ActiveSwallowLabellingTaskView::remove_non_resp_flow_label(NonRespFlowLabel::ID id)
{
    if (!can_add_new_non_resp_flow_label()) {
        spdlog::error("Cannot delete non-respiratory flow labels!");
        return;
    }

    const auto *range = m_annotation.non_resp_flow_labels.get_item(id);
    if (range) {
        spdlog::debug(
            "Removed non-respiratory flow label with ID {}: [{}, {}]", id, range->start, range->end
        );
        m_annotation.non_resp_flow_labels.remove_item(id);
    } else {
        spdlog::error("No non-respiratory flow label with ID {} - nothing to remove!", id);
    }
}

const std::vector<EarClickLabel> *ActiveSwallowLabellingTaskView::ear_click_labels() const
{
    return can_add_new_ear_click_range() ? &m_annotation.ear_click_labels.items() : nullptr;
}

const models::TimeRange *ActiveSwallowLabellingTaskView::ear_click_label(EarClickLabel::ID id) const
{
    if (!can_add_new_ear_click_range()) {
        return nullptr;
    }

    const auto *label = m_annotation.ear_click_labels.get_item(id);
    if (label == nullptr) {
        spdlog::error("No ear click label with ID {}", id);
        return nullptr;
    }

    return label;
}

void ActiveSwallowLabellingTaskView::remove_ear_click_label(EarClickLabel::ID id)
{
    if (!can_add_new_ear_click_range()) {
        spdlog::error("Cannot delete ear click labels!");
        return;
    }

    const auto *range = m_annotation.ear_click_labels.get_item(id);
    if (range) {
        spdlog::debug("Removed ear click label with ID {}: [{}, {}]", id, range->start, range->end);
        m_annotation.ear_click_labels.remove_item(id);
    } else {
        spdlog::error("No ear click label with ID {} - nothing to remove!", id);
    }
}

std::optional<EarClickLabel::ID>
ActiveSwallowLabellingTaskView::add_ear_click_label(double start, double end)
{
    if (!can_add_new_ear_click_range()) {
        spdlog::error("Cannot add ear click label");
        return std::nullopt;
    }

    const auto new_id = m_annotation.ear_click_labels.add_item({start, end});
    spdlog::debug("Added ear click with ID {}: [{}, {}]", new_id, start, end);
    return new_id;
}

void ActiveSwallowLabellingTaskView::set_ear_click_label(
    EarClickLabel::ID id, double start, double end
)
{
    if (!can_add_new_ear_click_range()) {
        spdlog::error("Cannot change ear click labels");
        return;
    }

    auto *range = m_annotation.ear_click_labels.get_item(id);
    if (range == nullptr) {
        spdlog::error("No ear click label with ID {} - nothing to change!", id);
    } else {
        *range = {start, end};
        spdlog::debug("Changed ear click label with ID {}: [{}, {}]", id, start, end);
    }
}

bool ActiveSwallowLabellingTaskView::valid_apnea_annotation() const
{
    return !apnea_status_is_src_pattern(m_annotation.swallow_apnea_status) || has_apnea_range();
}

bool ActiveSwallowLabellingTaskView::valid_earclick_annotation() const
{
    return m_annotation.ear_click_status != EarClickAnnotationStatus::Ok
        || !m_annotation.ear_click_labels.empty();
}

bool ActiveSwallowLabellingTaskView::can_save_annotation() const
{
    return valid_apnea_annotation() && valid_earclick_annotation();
}

bool ActiveSwallowLabellingTaskView::save_annotation()
{
    if (!can_save_annotation()) {
        spdlog::error("Cannot save annotation for task with ID={}", m_task.annotation_id());
        return false;
    }

    (void) m_annotation_store.add_annotation(m_task.annotation_id(), m_annotation.to_model());
    spdlog::info("Saved annotation for task with ID={}", m_task.annotation_id());
    return true;
}

void ActiveSwallowLabellingTaskView::delete_annotation()
{
    if (!can_delete_annotation()) {
        spdlog::error("Cannot delete annotation for task with ID={}", m_task.annotation_id());
        return;
    }

    m_annotation = Annotation();
    m_annotation_store.remove_annotation(m_task.annotation_id());
    spdlog::info("Deleted annotation for task with ID={}", m_task.annotation_id());
}

bool ActiveSwallowLabellingTaskView::annotation_unsaved() const
{
    // TODO: need a better way to check if annotation is unsaved...
    if (can_save_annotation()) {
        return !m_annotation_store.annotation_saved(
            m_task.annotation_id(), m_annotation.to_model()
        );
    }
    return false;
}

}; // namespace recap::labeller::app
