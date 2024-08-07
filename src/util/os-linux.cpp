#include "os.hpp"

#include <pwd.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <optional>

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

}; // namespace recap::labeller::os
