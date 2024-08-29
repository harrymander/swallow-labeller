#include "os.hpp"

#include <pwd.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <future>
#include <optional>
#include <system_error>

namespace recap::labeller::os {

std::optional<std::filesystem::path> get_user_data_dir()
{
    auto xdg_data_home = ::recap::labeller::os::getenv("XDG_DATA_HOME");
    if (xdg_data_home) {
        return std::make_optional<std::filesystem::path>(*xdg_data_home);
    }

    spdlog::debug("XDG_DATA_HOME env var not defined, using $HOME");
    auto home_dir = ::recap::labeller::os::getenv("HOME");
    if (home_dir) {
        std::filesystem::path path(*home_dir);
        path.append(".local/share");
        return path;
    }

    spdlog::warn("HOME env var not defined, using getpwuid");
    ::passwd *pwuid = ::getpwuid(::getuid());
    if (pwuid) {
        std::filesystem::path pw_dir(pwuid->pw_dir);
        pw_dir.append(".local/share");
        return pw_dir;
    }

    spdlog::error("Error getting password file entry for user: {}", std::strerror(errno));
    return std::nullopt;
}

namespace {

std::future<OsOpenStatus> xdg_open(const std::filesystem::path& path)
{
    // strerror is actually thread-safe on glibc?
    // See https://man7.org/linux/man-pages/man3/strerror.3.html

    static constexpr const char *XdgOpenPath = "/usr/bin/xdg-open";

    spdlog::debug("Opening {} in explorer...", path);
    return std::async(std::launch::async, [path]() {
        ::pid_t pid = fork();
        if (pid < 0) {
            spdlog::error("fork(2) error: {}", std::strerror(errno));
            return OsOpenStatus::Error;
        }

        if (pid == 0) {
            (void) execl(XdgOpenPath, XdgOpenPath, path.c_str(), static_cast<char *>(nullptr));
            ::_exit(EXIT_FAILURE);
        } else {
            int retval;
            if (::waitpid(pid, &retval, 0) < 0) {
                spdlog::error("waitpid(2) error: {}", std::strerror(errno));
                return OsOpenStatus::Error;
            }

            if (retval) {
                spdlog::error(
                    "Error opening {} in explorer: {} exited with code {}",
                    path,
                    XdgOpenPath,
                    retval
                );
                return OsOpenStatus::Error;
            }
        }

        spdlog::debug("Opened {} in explorer", path);
        return OsOpenStatus::Success;
    });
}

}; // namespace

std::future<OsOpenStatus> open_path_in_file_explorer(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        spdlog::error("Cannot open {} in file explorer: path does not exist!", path);
        return {};
    }

    if (std::filesystem::is_directory(path)) {
        return xdg_open(path);
    }

    if (path.has_parent_path()) {
        return xdg_open(path.parent_path());
    }
    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (ec) {
        spdlog::error("Error getting current working directory: {}", ec.message());
        return {};
    }
    return xdg_open(cwd);
}

}; // namespace recap::labeller::os
