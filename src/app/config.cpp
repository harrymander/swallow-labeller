#include "config.hpp"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string_view>

namespace recap::labeller::app {

namespace {

AppConfig GlobalAppConfig{};

template <typename T>
void get_json_field(const nlohmann::json& json, std::string_view key, T& value)
{
    if (json.contains(key)) {
        try {
            json.at(key).get_to(value);
        } catch (const nlohmann::json::exception& err) {
            throw std::invalid_argument(fmt::format("invalid value for '{}': {}", key, err.what()));
        }
    }
}

}; // namespace

void from_json(const nlohmann::json& json, AppConfig& config) // cppcheck-suppress unusedFunction
{
#define APP_CONFIG_PARSE_FIELD(type, name, default_value) get_json_field(json, #name, config.name);
    RECAP_LABELLER_APP_CONFIG_FIELDS(APP_CONFIG_PARSE_FIELD);
#undef APP_CONFIG_PARSE_FIELD
}

namespace {

[[noreturn]] void throw_invalid_config(const std::exception& err)
{
    throw std::invalid_argument(fmt::format("invalid config: {}", err.what()));
}

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
        throw_invalid_config(err);
    } catch (const std::invalid_argument& err) {
        throw_invalid_config(err);
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

void load_config(const std::filesystem::path& path)
{
    auto config = load_raw_config(path);
    validate_config(config);
    GlobalAppConfig = config;
}

const AppConfig& get_global_app_config()
{
    return GlobalAppConfig;
}

}; // namespace recap::labeller::app
