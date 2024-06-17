#include "gui.hpp"
#include "labelling-task.hpp"
#include "platform.hpp"

#include <argparse/argparse.hpp>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

namespace platform = recap::labeller::platform;
using namespace recap::labeller::gui;
using labelling_task::SwallowLabellingTask;

static void setup_logging(std::optional<std::string>&& logfile)
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

static int parse_args(argparse::ArgumentParser& program, int argc, const char **argv)
{
    program.add_argument("--log").help("file to log to");
    program.add_argument("--tasks", "-t").required().help("path to labelling tasks JSON");
    program.add_argument("--data-dir", "-d").required().help("directory containing data files");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        std::cerr << program;
        return -1;
    }

    return 0;
}

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

    std::vector<SwallowLabellingTask> labelling_tasks;
    try {
        std::string tasks_path = program.get("--tasks");
        std::ifstream stream(tasks_path);
        if (!stream) {
            spdlog::critical("Could not open labelling tasks file: {}", tasks_path);
            return 1;
        }
        labelling_tasks = labelling_task::load_tasks_json(stream);
        if (labelling_tasks.empty()) {
            spdlog::critical("Labelling tasks list is empty!");
            return 1;
        }
    } catch (const std::invalid_argument& e) {
        spdlog::critical("Invalid labelling tasks file: {}", e.what());
        return 1;
    }

    Gui gui(labelling_tasks, data_dir);
    return platform::run(gui);
}
