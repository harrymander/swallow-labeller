#include "app/annotation-store.hpp"
#include "app/app.hpp"
#include "app/config.hpp"
#include "models/task-info.hpp"
#include "options.h"
#include "platform/platform.hpp"
#include "util/optutil.hpp"
#include "util/os.hpp"

#include <argparse/argparse.hpp>
#include <fmt/core.h>
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
    throw std::invalid_argument(fmt::format("Invalid path '{}': {}", path, reason));
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

void validate_regular_file_path(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        throw_invalid_path(path.string(), "does not exist");
    }

    // We won't check for all path types, just check for common ones to give better error messages
    if (std::filesystem::is_directory(path)) {
        throw_invalid_path(path.string(), "is a directory");
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw_invalid_path(path.string(), "not a regular file");
    }
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

        auto regular_file_action = [](const std::string& val) -> std::string {
            validate_regular_file_path(std::filesystem::path(val));
            return val;
        };

        argparse::ArgumentParser parser(program_name, program_version);
        parser.add_argument("--log").help(LogCliHelp).action(check_path_writable);
        parser.add_argument("--config", "-c").help("path to config file");
        parser.add_argument("--tasks", "-t")
            .help("path to labelling tasks JSON")
            .action(regular_file_action);
        parser.add_argument("--data-dir", "-d")
            .help("directory containing data files")
            .action(check_is_dir);
        parser.add_argument("--annotations", "-a")
            .help("path to write annotations to; if exists, "
                  "reads existing annotations from this file")
            .action(check_path_writable);
        parser.add_argument("--no-app-data-dir")
            .implicit_value(true)
            .default_value(false)
            .help("by default, if any of --config, --data-dir, --tasks, --annotations are not\n"
                  "provided, they will be set relative to the user app data dir. If this is\n"
                  "passed, then all paths must be explicitly provided.");
        parser.add_argument("--suggestions", "-s")
            .help("path to annotations file to use as suggestions")
            .action(regular_file_action);

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
                throw std::invalid_argument(fmt::format("{} required", argname));
            }

            return *app_data_dir / std::filesystem::path(default_filename);
        };

        auto optional_data_path = [&](const char *argname, const char *default_filename
                                  ) -> std::optional<std::filesystem::path> {
            // If option passed explicitly, check it exists, else get from data dir only if exists
            auto value = optutil::transform(parser.present(argname), [](const auto& v) {
                return std::filesystem::path(v);
            });
            if (value.has_value()) {
                validate_regular_file_path(*value);
            } else if (app_data_dir) {
                auto default_path = *app_data_dir / default_filename;
                if (std::filesystem::is_regular_file(default_path)) {
                    value = default_path;
                }
            }

            return value;
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
                .suggested_annotations_file = parser.present<std::string>("--suggestions"),
                .config_file = optional_data_path("--config", "config.json"),
            };
        } catch (const std::invalid_argument& error) {
            print_usage_error(error);
            return std::nullopt;
        }
    }

    std::optional<std::string> log_file;
    std::filesystem::path data_dir;
    std::filesystem::path tasks_file;
    std::filesystem::path annotations_file;
    std::optional<std::filesystem::path> suggested_annotations_file;
    std::optional<std::filesystem::path> config_file;
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

std::optional<SwallowAnnotationStore> make_annotations_store(const std::filesystem::path& path)
{
    std::unique_ptr<std::istream> stream;
    if (std::filesystem::exists(path)) {
        spdlog::info("Reading existing annotations from {}", path);
        stream = std::make_unique<std::ifstream>(path);
    } else {
        spdlog::info("No existing annotations, creating annotations file at {}", path);
    }

    if (stream && stream->fail()) {
        spdlog::critical("Error opening annotations file");
        return std::nullopt;
    }

    try {
        return SwallowAnnotationStore(path, stream.get());
    } catch (const std::runtime_error& e) {
        spdlog::critical("Error parsing annotations file: {}", e.what());
    }
    return std::nullopt;
}

std::optional<SwallowAnnotationResultMap>
load_suggested_annotations(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (stream.fail()) {
        spdlog::critical("Error opening suggested annotations file");
        return std::nullopt;
    }

    try {
        return load_swallow_annotation_result_map_json(stream);
    } catch (const std::runtime_error& e) {
        spdlog::critical("Error parsing suggested annotations file: {}", e.what());
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

    app::AppConfig config;
    if (options.config_file) {
        try {
            config = app::load_config(*options.config_file);
        } catch (const std::invalid_argument& err) {
            spdlog::critical(
                "Error parsing config file from {}: {}", *options.config_file, err.what()
            );
            return 1;
        }
        spdlog::info("Loaded config from {}:", *options.config_file);
    } else {
        spdlog::info("No config file, using default settings:");
    }
    app::log_config(config, spdlog::level::info);

    auto labelling_tasks = load_labelling_tasks(options.tasks_file);
    if (!labelling_tasks.has_value()) {
        return 1;
    }
    auto annotations_store = make_annotations_store(options.annotations_file);
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

    std::optional<SwallowAnnotationResultMap> suggested_annotations;
    if (options.suggested_annotations_file) {
        suggested_annotations = load_suggested_annotations(*options.suggested_annotations_file);
        if (!suggested_annotations.has_value()) {
            return 1;
        }
        spdlog::info("Loaded {} suggested annotation(s)", suggested_annotations->size());
    }

    app::App app(
        config,
        *labelling_tasks,
        std::move(*annotations_store),
        options.data_dir,
        std::move(suggested_annotations)
    );
    return platform::run(app);
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
