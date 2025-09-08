#include "app/task-loader.hpp"

#include "models/data.hpp"

#include <cnpy.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>

namespace recap::labeller::app {

namespace {

models::SwallowTaskData load_task_data_from_path(const std::filesystem::path& path)
{
    // cnpz doesn't support loading from std::filesystem::path directly
    std::ifstream stream(path, std::ios::binary | std::ios::in);
    if (!stream) {
        throw std::runtime_error("error opening file");
    }
    return models::SwallowTaskData::from_numpy(cnpy::npz_load(stream));
}

}; // namespace

std::optional<models::SwallowTaskData> TaskLoader::load_task_data(const std::filesystem::path& path)
{
    auto it = m_status.find(path);
    Status status = it == m_status.end() ? Status::NotLoaded : it->second;
    if (status == Status::FileLoadError || status == Status::FileNotFound) {
        return std::nullopt;
    }

    if (!std::filesystem::is_regular_file(path)) {
        m_status[path] = Status::FileNotFound;
        spdlog::error("File not found: {}", path);
        return std::nullopt;
    }

    try {
        auto data = load_task_data_from_path(path);
        m_status[path] = Status::Ok;
        return data;
    } catch (const std::exception& error) {
        spdlog::error("Error loading task data from {}: {}", path, error.what());
        m_status[path] = Status::FileLoadError;
    }

    return std::nullopt;
}

TaskLoader::Status TaskLoader::get_task_data_status(const std::filesystem::path& path)
{
    auto it = m_status.find(path);
    if (it != m_status.end()) {
        return it->second;
    }

    const bool exists = std::filesystem::is_regular_file(path);
    Status status = exists ? Status::NotLoaded : Status::FileNotFound;
    if (!exists) {
        spdlog::error("File not found: {}", path);
    }
    m_status[path] = status;
    return status;
}

}; // namespace recap::labeller::app
