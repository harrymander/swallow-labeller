#include "app/app.hpp"

#include "app/annotation-store.hpp"

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace recap::labeller::app {

namespace {

template <typename... Args>
void throw_runtime_error [[noreturn]] (fmt::format_string<Args...> fmt_str, Args&&...args)
{
    const auto msg = fmt::format(fmt_str, std::forward<Args>(args)...);
    spdlog::error(msg);
    throw std::runtime_error(msg);
}

// TODO: this whole structure is a mess, need to encapsulate task management in a class...
bool all_task_ids_unique(const std::vector<models::SwallowTaskInfo>& tasks)
{
    std::unordered_set<std::string> ids;
    for (const auto& task : tasks) {
        const auto& id = task.get_id();
        if (ids.contains(id)) {
            spdlog::error("Duplicate task ID: {}", id);
            return false;
        }
        ids.insert(id);
    }
    return true;
}

std::vector<models::SwallowTaskInfo> load_labelling_tasks(const std::filesystem::path& tasks_path)
{
    std::ifstream stream(tasks_path);
    if (!stream) {
        throw_runtime_error("Could not open labelling tasks file {}", tasks_path);
    }

    try {
        auto tasks = models::load_swallow_task_info_json(stream);
        if (tasks.empty()) {
            throw_runtime_error("Labelling tasks list is empty!");
        }
        if (!all_task_ids_unique(tasks)) {
            throw_runtime_error("Got duplicate task IDs");
        }
        spdlog::debug("Loaded {} task info(s)", tasks.size());
        return tasks;
    } catch (const std::invalid_argument& e) {
        throw_runtime_error("Invalid labelling tasks file: {}", e.what());
    } catch (const std::runtime_error& e) {
        throw_runtime_error("Error reading from file: {}", e.what());
    }
}

SwallowAnnotationStore make_annotations_store(const std::filesystem::path& path)
{
    std::unique_ptr<std::istream> stream;
    if (std::filesystem::exists(path)) {
        spdlog::info("Reading existing annotations from {}", path);
        stream = std::make_unique<std::ifstream>(path);
    } else {
        spdlog::info("No existing annotations, creating annotations file at {}", path);
    }

    if (stream && stream->fail()) {
        throw_runtime_error("Error opening annotations file");
    }

    std::optional<SwallowAnnotationStore> store;
    try {
        store = {path, stream.get()};
    } catch (const std::runtime_error& e) {
        throw_runtime_error("Error parsing annotations file: {}", e.what());
    }

    try {
        // Sync to file to check that writing works
        store->sync_to_file();
    } catch (const std::runtime_error& e) {
        throw_runtime_error("Error writing to annotations file: {}", e.what());
    }

    return std::move(*store);
}

SwallowAnnotationResultMap load_suggested_annotations(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (stream.fail()) {
        throw_runtime_error("Error opening suggested annotations file");
    }

    try {
        auto suggestions = load_swallow_annotation_result_map_json(stream);
        spdlog::info("Loaded {} suggested annotation(s)", suggestions.size());
        return suggestions;
    } catch (const std::runtime_error& e) {
        throw_runtime_error("Error parsing suggested annotations file: {}", e.what());
    }
}

}; // namespace

App::App(LabellingConfig config) : m_config(config) {}

void App::create_new_labeller()
{
    try {
        auto labelling_tasks = load_labelling_tasks(m_labelling_tasks_path);

        auto annotations_store = make_annotations_store(m_annotations_path);
        m_annotation_store_error_observer =
            annotations_store.subscribe_sync_error([this](const auto& err) {
                auto msg = fmt::format("Error syncing annotations: {}", err);
                spdlog::critical(msg);
                set_critical_error(msg);
            });

        std::optional<SwallowAnnotationResultMap> suggested_annotations = std::nullopt;
        if (!m_suggested_annotations_path.empty()) { // TODO: use optional
            suggested_annotations = load_suggested_annotations(m_suggested_annotations_path);
        }

        m_labeller = std::make_unique<Labeller>(
            m_config,
            labelling_tasks,
            std::move(annotations_store),
            m_data_dir,
            suggested_annotations
        );
        m_labeller_load_error.reset();
        spdlog::debug("Loaded new labeller");
        m_labeller_update_observable.notify(*m_labeller);
    } catch (const std::runtime_error& error) {
        m_labeller_load_error = error.what();
    }
}

}; // namespace recap::labeller::app
