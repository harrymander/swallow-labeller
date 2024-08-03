#include "app/annotation-store.hpp"
#include "app/app.hpp"
#include "gui/gui.hpp"
#include "models/annotation.hpp"
#include "models/task-info.hpp"
#include "platform/platform.hpp"

#include <argparse/argparse.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {

using namespace recap::labeller;

void setup_logging(std::optional<std::string>&& logfile)
{
    spdlog::set_level(spdlog::level::debug);

    auto& sinks = spdlog::default_logger()->sinks();
    sinks = {std::make_shared<spdlog::sinks::stderr_color_sink_mt>()};
    if (logfile) {
        spdlog::debug("Logging to {}", *logfile);
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(*logfile);
        sinks.push_back(sink);

        // Create a separate temporary logger to write start message just to file:
        spdlog::set_automatic_registration(false);
        spdlog::logger("", {sink})
            .info("*************************** NEW LOG START ***************************");
        spdlog::set_automatic_registration(true);
    }
}

int parse_args(argparse::ArgumentParser& program, int argc, const char **argv)
{
    program.add_argument("--log").help("file to log to");
    program.add_argument("--tasks", "-t").required().help("path to labelling tasks JSON");
    program.add_argument("--data-dir", "-d").required().help("directory containing data files");
    program.add_argument("--annotations", "-a")
        .required()
        .help("path to write annotations to; if exists and --existing-annotations\n"
              "not passed, reads existing annotations from this file");
    program.add_argument("--existing-annotations", "-e")
        .help("reads existing annotations from this file rather than file passed to\n"
              "--annotations; WARNING: this will cause any existing annotations in\n"
              "file passed to --annotations to be overwritten!");
    program.add_argument("--not-shuffled")
        .implicit_value(true)
        .default_value(false)
        .help("do not shuffle labelling tasks by default");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        std::cerr << program;
        return -1;
    }

    return 0;
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

std::optional<std::vector<models::SwallowTaskInfo>>
load_labelling_tasks(const argparse::ArgumentParser& program)
{
    std::string tasks_path = program.get("--tasks");
    std::ifstream stream(tasks_path);
    if (!stream) {
        spdlog::critical("Could not open labelling tasks file: {}", tasks_path);
        return std::nullopt;
    }

    try {
        auto tasks = models::load_swallow_task_info_json(stream);
        if (tasks.empty()) {
            spdlog::critical("Labelling tasks list is empty!");
            return std::nullopt;
        }
        if (!all_task_ids_unique(tasks)) {
            spdlog::critical("Got duplicate task IDs");
            return std::nullopt;
        }
        spdlog::debug("Loaded {} task info(s)", tasks.size());
        return tasks;
    } catch (const std::invalid_argument& e) {
        spdlog::critical("Invalid labelling tasks file: {}", e.what());
    } catch (const std::runtime_error& e) {
        spdlog::critical("Error reading from file: {}", e.what());
    }

    return std::nullopt;
}

std::optional<models::SwallowAnnotationsMap> load_annotations(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream) {
        spdlog::critical("Could not open annotations file: {}", path);
        return std::nullopt;
    }
    try {
        auto annotations = models::load_swallow_annotations_map_json(stream);
        spdlog::debug("Loaded {} annotation(s)", annotations.size());
        return annotations;
    } catch (const std::invalid_argument& e) {
        spdlog::critical("Invalid annotations file: {}", e.what());
    } catch (const std::runtime_error& e) {
        spdlog::critical("Error reading from file: {}", e.what());
    }
    return std::nullopt;
}

std::optional<SwallowAnnotationStore> make_annotations_store(const argparse::ArgumentParser& parser)
{
    auto existing_path = parser.present("--existing-annotations");
    auto annotations_path = std::filesystem::path(parser.get("--annotations")).make_preferred();
    models::SwallowAnnotationsMap annotations;
    if (existing_path.has_value()) {
        spdlog::info(
            "Loading existing annotations from {} rather than {}", *existing_path, annotations_path
        );
        auto path = std::filesystem::path(*existing_path).make_preferred();
        auto opt = load_annotations(path);
        if (!opt.has_value()) {
            return std::nullopt;
        }
        annotations = std::move(*opt);
    } else if (std::filesystem::exists(annotations_path)) {
        spdlog::info("Path {} exists, trying to load annotations...", annotations_path);
        auto opt = load_annotations(annotations_path);
        if (!opt.has_value()) {
            return std::nullopt;
        }
        annotations = std::move(*opt);
    } else {
        spdlog::info("No existing annotations, creating annotations file at {}", annotations_path);
    }

    SwallowAnnotationStore store(annotations_path, annotations);
    try {
        // Sync to file to check that writing works
        store.sync_to_file();
        return store;
    } catch (const std::runtime_error& e) {
        spdlog::critical("Error writing to annotations file: {}", e.what());
    }

    return std::nullopt;
}

}; // namespace

int main(int argc, const char *argv[])
{
    argparse::ArgumentParser program(PROGRAM_NAME, VERSION_STR);
    if (parse_args(program, argc, argv) < 0) {
        return 2;
    }
    setup_logging(program.present("--log"));

    const std::filesystem::path data_dir = program.get("--data-dir");
    if (!std::filesystem::is_directory(data_dir)) {
        spdlog::critical("Data directory does not exist: {}", data_dir);
        return 1;
    }

    auto labelling_tasks = load_labelling_tasks(program);
    if (!labelling_tasks.has_value()) {
        return 1;
    }
    auto annotations_store = make_annotations_store(program);
    if (!annotations_store.has_value()) {
        return 1;
    }

    app::App app(
        *labelling_tasks, *annotations_store, data_dir, !program.is_used("--not-shuffled")
    );
    gui::Gui gui(app);
    return platform::run(gui);
}
