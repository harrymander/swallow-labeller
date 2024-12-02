#ifndef INCLUDE_RECAP_LABELLER_CONFIG_HPP
#define INCLUDE_RECAP_LABELLER_CONFIG_HPP

#include <spdlog/spdlog.h>

#include <filesystem>

namespace recap::labeller::app {

struct AppConfig {
    double max_snrf_time = 0.249;
};

inline void log_config(const AppConfig& c, spdlog::level::level_enum level)
{
    spdlog::log(level, "  max_snrf_time = {}", c.max_snrf_time);
}

// Throws std::invalid_argument on error
AppConfig load_config(const std::filesystem::path& path);

}; // namespace recap::labeller::app

#endif // INCLUDE_RECAP_LABELLER_CONFIG_HPP
