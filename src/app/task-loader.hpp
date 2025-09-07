#ifndef RECAP_LABELLER_APP_TASK_LOADER_HPP_INCLUDE
#define RECAP_LABELLER_APP_TASK_LOADER_HPP_INCLUDE

#include "models/data.hpp"

#include <filesystem>
#include <optional>
#include <unordered_map>

namespace recap::labeller::app {

class TaskLoader {
public:
    enum class Status : unsigned char {
        Ok,
        NotLoaded,
        FileNotFound,
        FileLoadError
    };

    Status get_task_data_status(const std::filesystem::path& path);
    std::optional<models::SwallowTaskData> load_task_data(const std::filesystem::path& path);

private:
    std::unordered_map<std::filesystem::path, Status> m_status;
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_TASK_LOADER_HPP_INCLUDE
