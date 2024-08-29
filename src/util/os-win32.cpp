#include "os.hpp"

#include <Objbase.h>
#include <Shlobj.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <type_traits>

namespace recap::labeller::os {

std::optional<std::string> getenv(const char *name)
{
    char *buf = nullptr;
    size_t buflen = 0;
    if (::_dupenv_s(&buf, &buflen, name)) {
        return std::nullopt;
    }

    if (buf) {
        std::string var(buf, buflen);
        std::free(buf);
        return var;
    }

    return std::nullopt;
}

namespace {

std::optional<std::filesystem::path> get_known_folder_path(REFKNOWNFOLDERID rfid)
{
    PWSTR path_str;
    HRESULT res = SHGetKnownFolderPath(rfid, 0, nullptr, &path_str);
    std::optional<std::filesystem::path> folder_path = std::nullopt;
    if (res == S_OK) {
        folder_path = std::filesystem::path(path_str);
    } else {
        const char *error = "unknown error";
        switch (res) {
        case E_FAIL:
            error = "known folder does not have a path";
            break;
        case E_INVALIDARG:
            error = "invalid argument";
            break;
        default:
            break;
        }
        spdlog::error("Error getting known folder path: {}", error);
    }
    CoTaskMemFree(path_str);
    return folder_path;
}

}; // namespace

std::optional<std::filesystem::path> get_user_data_dir()
{
    auto local_app_data_dir = get_known_folder_path(FOLDERID_LocalAppData);
    if (local_app_data_dir) {
        return local_app_data_dir;
    }

    spdlog::warn("Error getting known path for local app data, using %LOCALAPPDATA%");
    auto env_var = ::recap::labeller::os::getenv("LOCALAPPDATA");
    if (env_var) {
        return std::filesystem::path(*env_var);
    }

    return std::nullopt;
}

namespace {

bool init_com_lib()
{
    HRESULT res = CoInitialize(nullptr);
    if (res == S_OK) {
        return true;
    }
    if (res == S_FALSE) {
        spdlog::warn("COM library already initialised");
        return true;
    }

    spdlog::error("Error initialising COM library: {}", std::system_category().message(res));
    return false;
}

struct ItemIdListDeleter {
    void operator()(PIDLIST_ABSOLUTE pidlist) const { ILFree(pidlist); }
};

using UniqueItemIdList =
    std::unique_ptr<std::remove_pointer_t<PIDLIST_ABSOLUTE>, ItemIdListDeleter>;

OsOpenStatus open_path_in_file_explorer_sync(const std::filesystem::path& path)
{
    std::error_code ec;
    auto abs_path = std::filesystem::absolute(path, ec);
    if (ec) {
        spdlog::error("Error getting absolute path for {}: {}", path, ec.message());
        return OsOpenStatus::Error;
    }

    UniqueItemIdList pidlist(ILCreateFromPath(abs_path.c_str()));
    if (!pidlist) {
        spdlog::error("Error getting ITEMIDLIST from path {}", abs_path);
        return OsOpenStatus::Error;
    }

    if (!init_com_lib()) {
        return OsOpenStatus::Error;
    }

    HRESULT res = SHOpenFolderAndSelectItems(pidlist.get(), 0, nullptr, 0);
    if (res != S_OK) {
        spdlog::error(
            "Error opening {} in explorer: {}", path, std::system_category().message(res)
        );
        return OsOpenStatus::Error;
    }

    spdlog::debug("Opened {} in explorer", path);
    return OsOpenStatus::Success;
}

}; // namespace

std::future<OsOpenStatus> open_path_in_file_explorer(const std::filesystem::path& path)
{
    spdlog::debug("Opening {} in explorer...", path);
    return std::async(std::launch::async, [path]() {
        return open_path_in_file_explorer_sync(path);
    });
}

}; // namespace recap::labeller::os
