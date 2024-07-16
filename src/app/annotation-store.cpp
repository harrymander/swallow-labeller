#include "annotation-store.hpp"

#include "models/annotation.hpp"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace recap::labeller {

SwallowAnnotationStore::SwallowAnnotationStore(
    std::filesystem::path path, models::SwallowAnnotationsMap annotations
) :
    path(std::move(path)), annotations(std::move(annotations))
{}

const models::SwallowAnnotation *SwallowAnnotationStore::get_annotation(const std::string& id) const
{
    const auto it = annotations.find(id);
    if (it == annotations.end()) {
        return nullptr;
    }
    return &it->second;
}

bool SwallowAnnotationStore::annotation_saved(
    const std::string& id, const models::SwallowAnnotation& annotation
) const
{
    const models::SwallowAnnotation *existing = get_annotation(id);
    if (existing) {
        models::SwallowAnnotation normed = annotation;
        return *existing == normed;
    }
    return false;
}

bool SwallowAnnotationStore::add_annotation(
    const std::string& id, models::SwallowAnnotation annotation
)
{
    const auto it = annotations.find(id);
    if (it != annotations.end()) {
        const auto& existing = it->second;
        if (existing == annotation) {
            spdlog::debug("Annotation for id={} unchanged", id);
            return false;
        }

        spdlog::debug("Annotation for id={} changed", id);
    } else {
        spdlog::debug("New annotation for id={}", id);
    }
    annotations[id] = annotation;
    return true;
}

void SwallowAnnotationStore::remove_annotation(const std::string& id)
{
    const auto it = annotations.find(id);
    if (it != annotations.end()) {
        spdlog::debug("Removing annotation for id={}", id);
        annotations.erase(it);
    } else {
        spdlog::warn("No annotation for id={} - nothing to remove!", id);
    }
}

void SwallowAnnotationStore::sync_to_file() const
{
    std::ofstream stream;
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.open(path);
    dump_swallow_annotations_map_json(stream, annotations);
    stream << '\n';
    spdlog::debug("Wrote {} annotations to {}", annotations.size(), path);
}

}; // namespace recap::labeller
