#include "annotation-manager.hpp"
#include "gui.hpp"
#include "labelling-task.hpp"
#include "platform.hpp"

#include <argparse/argparse.hpp>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

namespace platform = recap::labeller::platform;
using namespace recap::labeller::gui;
using namespace labelling_task;
using recap::labeller::annotation_manager::AnnotationManager;

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
    program.add_argument("--annotations", "-a")
        .required()
        .help("path to write annotations to; if exists and --existing-annotations not passed, "
              "reads existing annotations from this file");
    program.add_argument("--existing-annotations", "-e")
        .help("reads existing annotations from this file rather than file passed to --annotations");
    program.add_argument("--data-dir", "-d").required().help("directory containing data files");
    program.add_argument("--no-shuffle").flag().help("do not display tasks in random order");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        std::cerr << program;
        return -1;
    }

    return 0;
}

std::optional<std::vector<SwallowTaskInfo>>
load_labelling_tasks(const argparse::ArgumentParser& program)
{
    std::string tasks_path = program.get("--tasks");
    std::ifstream stream(tasks_path);
    if (!stream) {
        spdlog::critical("Could not open labelling tasks file: {}", tasks_path);
        return std::nullopt;
    }

    try {
        auto tasks = load_swallow_task_info_json(stream);
        if (tasks.empty()) {
            spdlog::critical("Labelling tasks list is empty!");
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

std::optional<AnnotationsMap> load_annotations(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream) {
        spdlog::critical("Could not open annotations file: {}", path);
        return std::nullopt;
    }
    try {
        auto annotations = load_swallow_annotation_json(stream);
        spdlog::debug("Loaded {} annotation(s)", annotations.size());
        return annotations;
    } catch (const std::invalid_argument& e) {
        spdlog::critical("Invalid annotations file: {}", e.what());
    } catch (const std::runtime_error& e) {
        spdlog::critical("Error reading from file: {}", e.what());
    }
    return std::nullopt;
}

std::optional<AnnotationManager> make_annotations_mgr(const argparse::ArgumentParser& parser)
{
    auto existing_path = parser.present("--existing-annotations");
    std::filesystem::path annotations_path(parser.get("--annotations"));
    AnnotationsMap annotations;
    if (existing_path.has_value()) {
        spdlog::info(
            "Loading existing annotations from {} rather than {}",
            *existing_path,
            annotations_path.string()
        );
        auto opt = load_annotations(*existing_path);
        if (!opt.has_value()) {
            return std::nullopt;
        }
        annotations = std::move(*opt);
    } else if (std::filesystem::exists(annotations_path)) {
        spdlog::info("Path {} exists, trying to load annotations...", annotations_path.string());
        auto opt = load_annotations(annotations_path);
        if (!opt.has_value()) {
            return std::nullopt;
        }
        annotations = std::move(*opt);
    } else {
        spdlog::info(
            "No existing annotations, creating annotations file at {}", annotations_path.string()
        );
    }

    AnnotationManager manager(annotations_path, annotations);
    try {
        // Sync to file to check that writing works
        manager.sync_to_file();
        return manager;
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
        spdlog::critical("Data directory does not exist: {}", data_dir.string());
        return 1;
    }

    auto labelling_tasks = load_labelling_tasks(program);
    if (!labelling_tasks.has_value()) {
        return 1;
    }
    auto annotations_mgr = make_annotations_mgr(program);
    if (!annotations_mgr.has_value()) {
        return 1;
    }

    Gui gui(*labelling_tasks, *annotations_mgr, data_dir, program["--no-shuffle"] == false);
    return platform::run(gui);
}
