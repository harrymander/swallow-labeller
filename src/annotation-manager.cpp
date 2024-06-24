#include "annotation-manager.hpp"

#include "labelling-task.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>

namespace recap::labeller::annotation_manager {

using namespace recap::labeller::task;

namespace {

void normalise_annotation(SwallowAnnotation& annotation)
{
    std::sort(
        annotation.ear_clicks.begin(),
        annotation.ear_clicks.end(),
        [](const TimeRange& a, const TimeRange& b) {
            return a.start < b.start ? true : a.end < b.end;
        }
    );
}

}; // namespace

AnnotationManager::AnnotationManager(std::filesystem::path path, AnnotationsMap annotations) :
    path(std::move(path)), annotations(std::move(annotations)), path_str(this->path.string())
{
    for (auto& item : this->annotations) {
        normalise_annotation(item.second);
    }
}

const SwallowAnnotation *AnnotationManager::get_annotation(const std::string& id) const
{
    const auto it = annotations.find(id);
    if (it == annotations.end()) {
        return nullptr;
    }
    return &it->second;
}

bool AnnotationManager::add_annotation(const std::string& id, SwallowAnnotation annotation)
{
    normalise_annotation(annotation);
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

void AnnotationManager::remove_annotation(const std::string& id)
{
    const auto it = annotations.find(id);
    if (it != annotations.end()) {
        spdlog::debug("Removing annotation for id={}", id);
        annotations.erase(it);
    }
}

void AnnotationManager::sync_to_file() const
{
    std::ofstream stream;
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.open(path);
    dump_swallow_annotations_json(stream, annotations);
    spdlog::debug("Wrote {} annotations to {}", annotations.size(), path_str);
}

}; // namespace recap::labeller::annotation_manager
