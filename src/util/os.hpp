#ifndef RECAP_LABELLER_UTIL_OS_HPP_INCLUDE
#define RECAP_LABELLER_UTIL_OS_HPP_INCLUDE

#include <filesystem>
#include <future>
#include <optional>
#include <string>

namespace recap::labeller::os {

// None of these functions are necessarily thread-safe.

/**
 * Gets value of environment variable with name. Returns nullopt if no variable with that name
 * exists.
 */
std::optional<std::string> getenv(const char *name);

/**
 * Returns path to the user data directory or nullopt if cannot be determined.
 */
std::optional<std::filesystem::path> get_user_data_dir();

enum class OsOpenStatus {
    Success,
    Error,
};

/**
 * Opens a path in the system file explorer. If possible, will open the folder with file at path
 * selected.
 */
[[nodiscard]] std::future<OsOpenStatus>
open_path_in_file_explorer(const std::filesystem::path& path);

}; // namespace recap::labeller::os

#endif // RECAP_LABELLER_UTIL_OS_HPP_INCLUDE
