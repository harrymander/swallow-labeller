#ifndef RECAP_LABELLER_MODELS_ANNOTATION_HPP_INCLUDE
#define RECAP_LABELLER_MODELS_ANNOTATION_HPP_INCLUDE

#include "models/time-range.hpp"

#include <istream>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace recap::labeller::models {

enum class SRCPattern {
    ExEx,
    ExIn,
    InEx,
    InIn,
};

struct SwallowApneaAnnotation {
    bool is_ambiguous;
    SRCPattern pattern;
    TimeRange time;

    bool operator==(const SwallowApneaAnnotation&) const = default;
};

enum class SwallowApneaError {
    FlowError,
    NoSwallow,
    ApneaCutoff,
};

enum class EarClickError {
    AudioError,
    NoEarClick,
};

struct SwallowAnnotation {
    std::variant<SwallowApneaAnnotation, SwallowApneaError> swallow_apnea;
    std::variant<std::vector<TimeRange>, EarClickError> ear_clicks;

    bool operator==(const SwallowAnnotation&) const = default;

    // Raises std::runtime_error on parse error
    static SwallowAnnotation from_json(std::string_view str);

    [[nodiscard]] std::string dump_json() const;
};

using SwallowAnnotationsMap = std::map<std::string, SwallowAnnotation>;

SwallowAnnotationsMap load_swallow_annotations_map_json(std::istream& stream);
void dump_swallow_annotations_map_json(
    std::ostream& os, const SwallowAnnotationsMap& map, int indent = -1
);

}; // namespace recap::labeller::models

#endif // RECAP_LABELLER_MODELS_ANNOTATION_HPP_INCLUDE
