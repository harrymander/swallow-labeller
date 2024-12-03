#include "annotation-store.hpp"

#include "models/annotation-json.hpp"
#include "models/annotation.hpp"
#include "nlohmann/detail/abi_macros.hpp"
#include "util/chrono.hpp"
#include "util/json-optional.hpp"

#include <fmt/chrono.h>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>

#define ISO_UTC_DATETIME_FMT_STR "%Y-%m-%dT%H:%M:%SZ"

NLOHMANN_JSON_NAMESPACE_BEGIN

template <> struct adl_serializer<recap::labeller::UtcTimePoint> {
    static void from_json(const nlohmann::json& json, recap::labeller::UtcTimePoint& time)
    {
        auto time_str = json.template get<std::string>();
        std::istringstream ss(time_str);
        ss >> recap::labeller::chrono::parse(ISO_UTC_DATETIME_FMT_STR, time);
        if (ss.fail()) {
            throw std::runtime_error("Invalid datetime string: " + time_str);
        }
    }

    static void to_json(nlohmann::json& json, const recap::labeller::UtcTimePoint& time)
    {
        json = fmt::format(
            "{:" ISO_UTC_DATETIME_FMT_STR "}", std::chrono::floor<std::chrono::seconds>(time)
        );
    }
};

NLOHMANN_JSON_NAMESPACE_END

namespace recap::labeller {

namespace {

UtcTimePoint utc_time_now()
{
    return std::chrono::system_clock::now();
}

nlohmann::json::const_reference json_at(const nlohmann::json& json, const char *key)
{
    try {
        return json.at(key);
    } catch (const nlohmann::json::type_error& error) {
        throw std::runtime_error(fmt::format("invalid type for key '{}': {}", key, error.what()));
    }
}

}; // namespace

void to_json(nlohmann::json& json, const SwallowAnnotationResult& result)
{
    json = {
        {"result", result.result()},
        {"created_time", result.created_time()},
        {"last_modified_time", result.last_modified_time()},
    };
}

void from_json(const nlohmann::json& json, SwallowAnnotationResult& result)
{
    json_at(json, "result").get_to(result.m_result);
    json_at(json, "created_time").get_to(result.m_created_time);
    json_at(json, "last_modified_time").get_to(result.m_last_modified_time);
}

SwallowAnnotationResult::SwallowAnnotationResult(models::SwallowAnnotation result) :
    m_result(std::move(result)), m_created_time(utc_time_now()), m_last_modified_time(std::nullopt)
{}

void SwallowAnnotationResult::update_result(models::SwallowAnnotation result)
{
    m_result = std::move(result);
    m_last_modified_time = utc_time_now();
}

SwallowAnnotationResultMap load_swallow_annotation_result_map_json(std::istream& stream)
{
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(stream);
    } catch (const nlohmann::json::exception& parse_error) {
        throw std::runtime_error(std::string("invalid JSON: ") + parse_error.what());
    }

    try {
        return json.template get<SwallowAnnotationResultMap>();
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error(std::string("invalid JSON model: ") + error.what());
    }
}

namespace {

void dump_swallow_annotations_map_json(std::ostream& os, const SwallowAnnotationResultMap& map)
{
    constexpr int JsonIndent = 2;

    nlohmann::json json = map;
    os << json.dump(JsonIndent);
}

}; // namespace

SwallowAnnotationStore::SwallowAnnotationStore(std::filesystem::path path, std::istream *existing) :
    m_path(std::move(path))
{
    if (existing) {
        m_annotations = load_swallow_annotation_result_map_json(*existing);
        spdlog::info("Loaded {} existing annotations", m_annotations.size());
    }
}

SwallowAnnotationStore::~SwallowAnnotationStore() = default;
SwallowAnnotationStore::SwallowAnnotationStore(SwallowAnnotationStore&&) noexcept = default;
SwallowAnnotationStore&
SwallowAnnotationStore::operator=(SwallowAnnotationStore&&) noexcept = default;

void SwallowAnnotationStore::add_annotation(
    const std::string& id, models::SwallowAnnotation annotation
)
{
    const auto it = m_annotations.find(id);
    if (it != m_annotations.end()) {
        auto& existing = it->second;
        if (existing.result() == annotation) {
            spdlog::debug("Annotation for id={} unchanged", id);
            return;
        }

        spdlog::debug("Annotation for id={} changed", id);
        existing.update_result(std::move(annotation));
    } else {
        spdlog::debug("New annotation for id={}", id);
        m_annotations.emplace(id, SwallowAnnotationResult(std::move(annotation)));
    }
    sync_to_file_notify();
}

void SwallowAnnotationStore::remove_annotation(const std::string& id)
{
    const auto it = m_annotations.find(id);
    if (it != m_annotations.end()) {
        spdlog::debug("Removing annotation for id={}", id);
        m_annotations.erase(it);
        sync_to_file_notify();
    } else {
        spdlog::warn("No annotation for id={} - nothing to remove!", id);
    }
}

void SwallowAnnotationStore::sync_to_file(const std::filesystem::path& path) const
{
    std::ofstream stream;
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.open(path, std::ios::out | std::ios::binary);
    dump_swallow_annotations_map_json(stream, m_annotations);
    stream << '\n';
    spdlog::debug("Wrote {} annotations to {}", m_annotations.size(), path);
}

void SwallowAnnotationStore::sync_to_file_notify() const
{
    try {
        sync_to_file();
    } catch (const std::exception& e) {
        std::string err = e.what();
        spdlog::error("Error writing annotations to file: {}", err);
        m_error_observable.notify(err);
    }
}

}; // namespace recap::labeller
