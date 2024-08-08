#include "app/annotation-store.hpp"
#include "app/app.hpp"
#include "fmt/core.h"
#include "gui/gui.hpp"
#include "models/task-info.hpp"
#include "options.h"
#include "platform/platform.hpp"
#include "util/optutil.hpp"
#include "util/os.hpp"

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
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {

using namespace recap::labeller;

void setup_console_logging()
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::default_logger()->sinks() = {std::make_shared<spdlog::sinks::stderr_color_sink_mt>()};
}

void setup_file_logging(const std::filesystem::path& path)
{
    spdlog::debug("Logging to {}", path);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string());
    spdlog::default_logger()->sinks().push_back(sink);

    // Create a separate temporary logger to write start message just to file:
    spdlog::set_automatic_registration(false);
    spdlog::logger("", {sink})
        .info("*************************** NEW LOG START ***************************");
    spdlog::set_automatic_registration(true);
}

[[noreturn]] void throw_invalid_path(const std::string& path, const std::string& reason)
{
    throw std::invalid_argument("Invalid path '" + path + "': " + reason);
}

std::string check_is_dir(const std::string& path_str)
{
    std::filesystem::path path(path_str);
    if (!std::filesystem::exists(path)) {
        throw_invalid_path(path_str, "does not exist");
    }
    if (!std::filesystem::is_directory(path)) {
        throw_invalid_path(path_str, "not a directory");
    }

    return path_str;
}

std::string check_is_file(const std::string& path_str)
{
    std::filesystem::path path(path_str);
    if (!std::filesystem::exists(path)) {
        throw_invalid_path(path_str, "does not exist");
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw_invalid_path(path_str, "not a regular file");
    }

    return path_str;
}

std::string check_path_writable(const std::string& path_str)
{
    // Doesn't really check if path is writable, just checks that is not a directory and parent
    // directory exists.

    std::filesystem::path path(path_str);
    if (std::filesystem::is_directory(path)) {
        throw_invalid_path(path_str, "is a directory");
    }

    if (path.has_parent_path()) {
        auto parent = path.parent_path();
        if (!std::filesystem::exists(parent)) {
            throw_invalid_path(path_str, "parent path does not exist");
        }
        if (!std::filesystem::is_directory(parent)) {
            throw_invalid_path(path_str, "parent path is not a directory");
        }
    }

    return path_str;
}

bool create_directories_if_not_exist(const std::filesystem::path& dir)
{
    if (std::filesystem::exists(dir)) {
        if (std::filesystem::is_directory(dir)) {
            spdlog::debug("Directory {} already exists", dir);
            return true;
        }

        spdlog::error("Path {} exists but is not a directory", dir);
        return false;
    }

    std::error_code ec;
    const bool created = std::filesystem::create_directories(dir, ec);
    if (ec) {
        spdlog::error("Error creating directory {}: {}", dir, ec.message());
        return false;
    }

    if (!created) {
        spdlog::error("Directory {} not created!", dir);
        return false;
    }

    spdlog::debug("Created directory {}", dir);
    return true;
}

std::optional<std::filesystem::path> get_app_data_dir()
{
    auto dir = os::get_user_data_dir();
    if (dir) {
        dir->append(PROGRAM_NAME);
        spdlog::debug("App data dir: {}", *dir);
        return dir;
    }
    return std::nullopt;
}

struct ProgramOptions {
    static std::optional<ProgramOptions> from_cli_arguments(
        const char *program_name, const char *program_version, int argc, const char **argv
    )
    {
        constexpr const char *LogCliHelp =
#ifdef RECAP_LABELLER_LOG_TO_APP_DATA_DIR
            "file to log to; if not provided logs to user app data dir";
#else
            "file to log to";
#endif

        argparse::ArgumentParser parser(program_name, program_version);
        parser.add_argument("--log").help(LogCliHelp).action(check_path_writable);
        parser.add_argument("--tasks", "-t")
            .help("path to labelling tasks JSON")
            .action(check_path_writable);
        parser.add_argument("--data-dir", "-d")
            .help("directory containing data files")
            .action(check_is_dir);
        parser.add_argument("--annotations", "-a")
            .help("path to write annotations to; if exists and --existing-annotations\n"
                  "not passed, reads existing annotations from this file")
            .action(check_path_writable);
        parser.add_argument("--existing-annotations", "-e")
            .help("reads existing annotations from this file rather than file passed to\n"
                  "--annotations; WARNING: this will cause any existing annotations in\n"
                  "file passed to --annotations to be overwritten!")
            .action(check_is_file);
        parser.add_argument("--not-shuffled")
            .implicit_value(true)
            .default_value(false)
            .help("do not shuffle labelling tasks by default");
        parser.add_argument("--no-app-data-dir")
            .implicit_value(true)
            .default_value(false)
            .help("by default, if any of --data-dir, --tasks, --annotations are not provided,\n"
                  "they will be set relative to the user app data dir. If this is passed, then\n"
                  "all paths must be explicitly provided.");

        auto print_usage_error = [&](const std::exception& exc) {
            std::cerr << "Error: " << exc.what() << '\n';
            std::cerr << parser;
        };

        try {
            parser.parse_args(argc, argv);
        } catch (const std::exception& error) {
            print_usage_error(error);
            return std::nullopt;
        }

        std::optional<std::filesystem::path> app_data_dir = std::nullopt;
        if (!parser.is_used("--no-app-data-dir")) {
            app_data_dir = get_app_data_dir();
            if (app_data_dir && !create_directories_if_not_exist(*app_data_dir)) {
                app_data_dir.reset();
            }
        }

        auto data_path = [&](const char *argname,
                             const char *default_filename) -> std::filesystem::path {
            auto path_str = parser.present(argname);
            if (path_str) {
                return {*path_str};
            }

            if (!app_data_dir) {
                throw std::runtime_error(fmt::format("{} required", argname));
            }

            return *app_data_dir / std::filesystem::path(default_filename);
        };

#ifdef RECAP_LABELLER_LOG_TO_APP_DATA_DIR
        auto log_file = data_path("--log", "logs.txt").string();
#else
        auto log_file = parser.present("--log");
#endif // RECAP_LABELLER_LOG_TO_APP_DATA_DIR

        try {
            return ProgramOptions{
                .log_file = log_file,
                .data_dir = data_path("--data-dir", "swallow-data"),
                .tasks_file = data_path("--tasks", "tasks.json"),
                .annotations_file = data_path("--annotations", "annotations.json").make_preferred(),
                .existing_annotations = optutil::transform(
                    parser.present("--existing-annotations"),
                    [](const std::string& path) { return std::filesystem::path(path); }
                ),
                .shuffled = !parser.is_used("--not-shuffled"),
            };
        } catch (const std::runtime_error& error) {
            print_usage_error(error);
            return std::nullopt;
        }
    }

    std::optional<std::string> log_file;
    std::filesystem::path data_dir;
    std::filesystem::path tasks_file;
    std::filesystem::path annotations_file;
    std::optional<std::filesystem::path> existing_annotations;
    bool shuffled;
};

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
load_labelling_tasks(const std::filesystem::path& tasks_path)
{
    std::ifstream stream(tasks_path);
    if (!stream) {
        spdlog::critical("Could not open labelling tasks file {}", tasks_path);
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

std::optional<SwallowAnnotationStore> make_annotations_store(
    const std::filesystem::path& annotations_path,
    const std::optional<std::filesystem::path>& existing_path
)
{
    std::unique_ptr<std::istream> stream;
    if (existing_path.has_value()) {
        spdlog::info(
            "Loading existing annotations from {} rather than {}", *existing_path, annotations_path
        );
        stream = std::make_unique<std::ifstream>(*existing_path);
    } else if (std::filesystem::exists(annotations_path)) {
        spdlog::info("Reading existing annotations from {}", annotations_path);
        stream = std::make_unique<std::ifstream>(annotations_path);
    } else {
        spdlog::info("No existing annotations, creating annotations file at {}", annotations_path);
    }

    if (stream && stream->fail()) {
        spdlog::critical("Error opening annotations file");
        return std::nullopt;
    }

    try {
        return SwallowAnnotationStore(annotations_path, stream.get());
    } catch (const std::runtime_error& e) {
        spdlog::critical("Error parsing annotations file: {}", e.what());
    }
    return std::nullopt;
}

int run_main(int argc, const char *argv[])
{
    setup_console_logging();
    auto parse_options = ProgramOptions::from_cli_arguments(PROGRAM_NAME, VERSION_STR, argc, argv);
    if (!parse_options) {
        return 2;
    }
    const auto& options = *parse_options;
    if (options.log_file) {
        setup_file_logging(*options.log_file);
    }

    auto labelling_tasks = load_labelling_tasks(options.tasks_file);
    if (!labelling_tasks.has_value()) {
        return 1;
    }
    auto annotations_store =
        make_annotations_store(options.annotations_file, options.existing_annotations);
    if (!annotations_store.has_value()) {
        return 1;
    }
    try {
        // Sync to file to check that writing works
        annotations_store->sync_to_file();
    } catch (const std::runtime_error& e) {
        spdlog::critical("Error writing to annotations file: {}", e.what());
        return 1;
    }

    app::App app(
        *labelling_tasks, std::move(*annotations_store), options.data_dir, options.shuffled
    );
    gui::Gui gui(app);
    return platform::run(gui);
}

}; // namespace

int main(int argc, const char *argv[])
{
#if NDEBUG
    try {
        return run_main(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "An unknown error occurred!\n";
        return 1;
    }
#else
    return run_main(argc, argv);
#endif
}
