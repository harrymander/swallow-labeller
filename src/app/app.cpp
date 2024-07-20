#include "app.hpp"

#include "annotation-store.hpp"
#include "models/annotation.hpp"
#include "models/data.hpp"
#include "models/task-info.hpp"
#include "models/time-range.hpp"
#include "spdlog/spdlog.h"
#include "util/optutil.hpp"
#include "util/strutil.hpp"
#include "util/variant-visitor.hpp"

#include <fmt/core.h>
#include <magic_enum.hpp>

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
    const std::vector<models::SwallowTaskInfo>& swallow_tasks,
    SwallowAnnotationStore annotation_store,
    const fs::path& data_dir
) :
    m_swallow_tasks(labelling_tasks(swallow_tasks, data_dir)),
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

class UnsavedTaskSwitcher : public App::UnsavedTaskHandler {
public:
    UnsavedTaskSwitcher(App& app, std::size_t next_task_index) :
        m_app(app), m_next_task_index(next_task_index)
    {}

    void cancel() override
    {
        spdlog::debug(
            "Cancelling task index switch to {}, staying on {}",
            m_next_task_index,
            m_app.m_active_task_index
        );
    }

    void submit() override
    {
        spdlog::debug(
            "Switching task index {} -> {}", m_app.m_active_task_index, m_next_task_index
        );
        m_app.m_active_task_index = m_next_task_index;
        m_app.load_active_task();
    }

private:
    App& m_app;
    std::size_t m_next_task_index;
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

void App::set_active_task_index(std::size_t index)
{
    if (m_unsaved_task_handler) {
        spdlog::error("There is already an unsaved task action pending, not switching");
        return;
    }
    if (index >= m_swallow_tasks.size()) {
        spdlog::error("Invalid task index {}; not changing!", index);
        return;
    }
    if (index == m_active_task_index) {
        spdlog::warn("Task index is already {}; not changing!", index);
        return;
    }

    if (active_task_unsaved()) {
        spdlog::debug(
            "Task switch {} -> {} requested but task is unsaved; blocking task switch",
            m_active_task_index,
            index
        );
        m_unsaved_task_handler = std::make_unique<UnsavedTaskSwitcher>(*this, index);
    } else {
        spdlog::debug("Setting task index to {}", index);
        m_active_task_index = index;
        load_active_task();
    }
}

void App::cancel_unsaved_task_switch()
{
    if (m_unsaved_task_handler) {
        spdlog::debug("Cancelling unsaved task change");
        m_unsaved_task_handler->cancel();
        m_unsaved_task_handler.reset();
    } else {
        spdlog::warn("No task switch to cancel, staying on index {}", m_active_task_index);
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
        spdlog::warn("No unsaved task to save, staying on index {}", m_active_task_index);
    }
}

void App::discard_unsaved_task_and_switch()
{
    if (m_unsaved_task_handler) {
        spdlog::debug("Discarding unsaved task");
        m_unsaved_task_handler->submit();
        m_unsaved_task_handler.reset();
    } else {
        spdlog::warn("No unsaved task to discard, staying on index {}", m_active_task_index);
    }
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

bool App::active_task_unsaved() const
{
    const auto *task_view = std::get_if<ActiveSwallowLabellingTaskView>(m_active_task.get());
    if (task_view) {
        return task_view->annotation_unsaved();
    }
    return false;
}

void App::save_active_task()
{
    auto *task_view = std::get_if<ActiveSwallowLabellingTaskView>(m_active_task.get());
    if (task_view) {
        spdlog::debug("Saving annotation for active task");
        task_view->save_annotation();
    } else {
        spdlog::error("Cannot save annotation for active task: in error state");
    }
}

void App::on_active_task_saved()
{
    if (!m_auto_advance_on_save) {
        return;
    }

    std::size_t index = m_active_task_index + 1;
    while (index != m_active_task_index) {
        if (index == m_swallow_tasks.size()) {
            // If the active index is 0 and we have reached end of list, then we have completed a
            // full loop
            if (m_active_task_index == 0) {
                break;
            }
            index = 0;
        }

        const auto& task = m_swallow_tasks[index];
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
            m_active_task = make_unique_active_task<ActiveSwallowLabellingTaskView>(
                task, data, m_annotation_store, [this]() { on_active_task_saved(); }
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
    SwallowLabellingTask& task,
    SwallowTaskData data,
    SwallowAnnotationStore& annotation_store,
    std::function<void()> on_task_save
) :
    m_task(task),
    m_data(std::move(data)),
    m_annotation_store(annotation_store),
    m_on_task_save(std::move(on_task_save))
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

    ear_click_status(ear_click_status_from_ear_click_model(annotation.ear_clicks)),

    ear_click_labels(VariantVisitor{
        [](const std::vector<models::TimeRange>& ranges) {
            EarClickLabel::ID id = 1;
            std::vector<EarClickLabel> result;
            result.reserve(ranges.size());
            for (const auto& range : ranges) {
                result.emplace_back(id++, range);
            }
            return result;
        },
        [](auto) { return std::vector<EarClickLabel>{}; },
    }(annotation.ear_clicks)),

    next_ear_click_label_id(ear_click_labels.empty() ? 1 : ear_click_labels.back().id + 1),

    note(optutil::value_or_default(annotation.note))
{}

namespace {

[[nodiscard]] inline bool apnea_status_is_src_pattern(SwallowApneaAnnotationStatus status)
{
    using enum SwallowApneaAnnotationStatus;
    return status == ExEx || status == InEx || status == ExIn || status == InIn;
}

[[nodiscard]] std::variant<models::SwallowApneaAnnotation, models::SwallowApneaError>
swallow_apnea_annotation_model(
    bool is_ambiguous, SwallowApneaAnnotationStatus status, models::TimeRange range
)
{
    auto pattern = magic_enum::enum_cast<models::SRCPattern>(magic_enum::enum_name(status)).value();
    return models::SwallowApneaAnnotation{
        .is_ambiguous = is_ambiguous,
        .pattern = pattern,
        .time = range,
    };
}

[[nodiscard]] std::variant<models::SwallowApneaAnnotation, models::SwallowApneaError>
swallow_apnea_error_model(SwallowApneaAnnotationStatus status)
{
    return magic_enum::enum_cast<models::SwallowApneaError>(magic_enum::enum_name(status)).value();
}

[[nodiscard]] std::variant<std::vector<models::TimeRange>, models::EarClickError>
ear_clicks_annotation_model(
    EarClickAnnotationStatus status, const std::vector<EarClickLabel>& labels
)
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
    }

    std::vector<models::TimeRange> ranges;
    ranges.reserve(labels.size());
    for (const auto& label : labels) {
        ranges.push_back(label.range);
    }
    return ranges;
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
    using namespace models;

    return {
        .swallow_apnea = apnea_status_is_src_pattern(swallow_apnea_status) ?
            swallow_apnea_annotation_model(
                swallow_is_ambiguous, swallow_apnea_status, swallow_apnea_range
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
    return can_add_new_ear_click_range() ? &m_annotation.ear_click_labels : nullptr;
}

const EarClickLabel *ActiveSwallowLabellingTaskView::ear_click_label(EarClickLabel::ID id) const
{
    if (!can_add_new_ear_click_range()) {
        return nullptr;
    }

    const auto it = find_ear_click(m_annotation.ear_click_labels, id);
    if (it == m_annotation.ear_click_labels.end()) {
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

    const auto it = find_ear_click(m_annotation.ear_click_labels, id);
    if (it == m_annotation.ear_click_labels.end()) {
        spdlog::error("No ear click label with ID {} - nothing to remove!", id);
    } else {
        const auto& range = it->range;
        m_annotation.ear_click_labels.erase(it);
        spdlog::debug("Removed ear click label with ID {}: [{}, {}]", id, range.start, range.end);
    }
}

std::optional<EarClickLabel::ID>
ActiveSwallowLabellingTaskView::add_ear_click_label(double start, double end)
{
    if (!can_add_new_ear_click_range()) {
        spdlog::error("Cannot add ear click label");
        return std::nullopt;
    }

    const EarClickLabel::ID new_id = m_annotation.next_ear_click_label_id;
    m_annotation.ear_click_labels.emplace_back(new_id, models::TimeRange{start, end});
    spdlog::debug("Added ear click with ID {}: [{}, {}]", new_id, start, end);
    m_annotation.next_ear_click_label_id++;
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

    const auto const_it = find_ear_click(m_annotation.ear_click_labels, id);
    if (const_it == m_annotation.ear_click_labels.end()) {
        spdlog::error("No ear click label with ID {} - nothing to change!", id);
    } else {
        auto it = m_annotation.ear_click_labels.begin()
            + (const_it - m_annotation.ear_click_labels.cbegin());
        it->range = {start, end};
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

void ActiveSwallowLabellingTaskView::save_annotation()
{
    if (!can_save_annotation()) {
        spdlog::error("Cannot save annotation for task with ID={}", m_task.annotation_id());
        return;
    }

    (void) m_annotation_store.add_annotation(m_task.annotation_id(), m_annotation.to_model());
    spdlog::info("Saved annotation for task with ID={}", m_task.annotation_id());
    m_on_task_save();
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
