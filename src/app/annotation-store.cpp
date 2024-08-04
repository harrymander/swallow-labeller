#include "annotation-store.hpp"

#include "models/annotation-json.hpp"
#include "models/annotation.hpp"
#include "nlohmann/detail/abi_macros.hpp"
#include "util/json-optional.hpp"

#include <date/date.h>
#include <fmt/chrono.h>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#define ISO_UTC_DATETIME_FMT_STR "%Y-%m-%dT%H:%M:%SZ"

using UtcTimePoint = std::chrono::time_point<std::chrono::system_clock>;

NLOHMANN_JSON_NAMESPACE_BEGIN

template <> struct adl_serializer<UtcTimePoint> {
    static void from_json(const nlohmann::json& json, UtcTimePoint& time)
    {
        auto time_str = json.template get<std::string>();
        std::istringstream ss(time_str);
        ss >> date::parse(ISO_UTC_DATETIME_FMT_STR, time);
        if (ss.fail()) {
            throw std::runtime_error("Invalid datetime string: " + time_str);
        }
    }

    static void to_json(nlohmann::json& json, const UtcTimePoint& time)
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

struct SwallowAnnotationResult {
    models::SwallowAnnotation result;
    UtcTimePoint created_time;
    std::optional<UtcTimePoint> last_modified_time = std::nullopt;

    SwallowAnnotationResult() = default;

    explicit SwallowAnnotationResult(models::SwallowAnnotation result) :
        result(std::move(result)), created_time(utc_time_now())
    {}

    void update_result(models::SwallowAnnotation new_result)
    {
        result = std::move(new_result);
        last_modified_time = utc_time_now();
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    SwallowAnnotationResult, result, created_time, last_modified_time
);

}; // namespace

class SwallowAnnotationStoreMap : public std::map<std::string, SwallowAnnotationResult> {};

namespace {

std::unique_ptr<SwallowAnnotationStoreMap> load_swallow_annotations_map_json(std::istream& stream)
{
    try {
        return std::make_unique<SwallowAnnotationStoreMap>(
            nlohmann::json::parse(stream).template get<SwallowAnnotationStoreMap>()
        );
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }
}

void dump_swallow_annotations_map_json(std::ostream& os, const SwallowAnnotationStoreMap& map)
{
    constexpr int JsonIndent = 2;

    nlohmann::json json = map;
    os << json.dump(JsonIndent);
}

}; // namespace

SwallowAnnotationStore::SwallowAnnotationStore(std::filesystem::path path, std::istream *existing) :
    path(std::move(path)),
    m_annotations(
        existing ? load_swallow_annotations_map_json(*existing) :
                   std::make_unique<SwallowAnnotationStoreMap>()
    )
{
    if (existing) {
        spdlog::info("Loaded {} existing annotations", m_annotations->size());
    }
}

SwallowAnnotationStore::~SwallowAnnotationStore() = default;
SwallowAnnotationStore::SwallowAnnotationStore(SwallowAnnotationStore&&) noexcept = default;
SwallowAnnotationStore&
SwallowAnnotationStore::operator=(SwallowAnnotationStore&&) noexcept = default;

const models::SwallowAnnotation *SwallowAnnotationStore::get_annotation(const std::string& id) const
{
    const auto it = m_annotations->find(id);
    if (it == m_annotations->end()) {
        return nullptr;
    }
    return &it->second.result;
}

[[nodiscard]] bool SwallowAnnotationStore::has_annotation(const std::string& id) const
{
    return m_annotations->contains(id);
}

bool SwallowAnnotationStore::annotation_saved(
    const std::string& id, const models::SwallowAnnotation& annotation
) const
{
    const models::SwallowAnnotation *existing = get_annotation(id);
    if (existing) {
        return *existing == annotation;
    }
    return false;
}

void SwallowAnnotationStore::add_annotation(
    const std::string& id, models::SwallowAnnotation annotation
)
{
    const auto it = m_annotations->find(id);
    if (it != m_annotations->end()) {
        auto& existing = it->second;
        if (existing.result == annotation) {
            spdlog::debug("Annotation for id={} unchanged", id);
            return;
        }

        spdlog::debug("Annotation for id={} changed", id);
        existing.update_result(std::move(annotation));
    } else {
        spdlog::debug("New annotation for id={}", id);
        m_annotations->emplace(id, SwallowAnnotationResult(std::move(annotation)));
    }
    sync_to_file_notify();
}

void SwallowAnnotationStore::remove_annotation(const std::string& id)
{
    const auto it = m_annotations->find(id);
    if (it != m_annotations->end()) {
        spdlog::debug("Removing annotation for id={}", id);
        m_annotations->erase(it);
        sync_to_file_notify();
    } else {
        spdlog::warn("No annotation for id={} - nothing to remove!", id);
    }
}

void SwallowAnnotationStore::sync_to_file() const
{
    std::ofstream stream;
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.open(path);
    dump_swallow_annotations_map_json(stream, *m_annotations);
    stream << '\n';
    spdlog::debug("Wrote {} annotations to {}", m_annotations->size(), path);
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
