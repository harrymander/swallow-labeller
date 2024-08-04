#include "annotation-store.hpp"

#include "models/annotation-json.hpp"
#include "models/annotation.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace recap::labeller {

namespace {

struct SwallowAnnotationResult {
    models::SwallowAnnotation result;
};

[[maybe_unused]] void from_json(const nlohmann::json& json, SwallowAnnotationResult& result)

{
    result.result = json.template get<models::SwallowAnnotation>();
}

[[maybe_unused]] void to_json(nlohmann::json& json, const SwallowAnnotationResult& result)
{
    json = result.result;
}

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
        existing.result = std::move(annotation);
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
