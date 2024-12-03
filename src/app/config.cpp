#include "config.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string_view>

namespace recap::labeller::app {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AppConfig, max_snrf_time);

namespace {

AppConfig load_raw_config(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(stream);
    } catch (const nlohmann::json::exception& err) {
        throw std::invalid_argument(fmt::format("invalid JSON: {}", err.what()));
    }

    try {
        return json.template get<AppConfig>();
    } catch (const nlohmann::json::exception& err) {
        throw std::invalid_argument(fmt::format("invalid config: {}", err.what()));
    }
}

[[noreturn]] void invalid_field(std::string_view key, std::string_view msg)
{
    throw std::invalid_argument(fmt::format("invalid value for '{}': {}", key, msg));
}

void validate_config(const AppConfig& config)
{
    if (config.max_snrf_time <= 0) {
        invalid_field("max_snrf_time", "must be greater than 0");
    }
}

}; // namespace

AppConfig load_config(const std::filesystem::path& path)
{
    auto config = load_raw_config(path);
    validate_config(config);
    return config;
}

}; // namespace recap::labeller::app
