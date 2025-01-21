#ifndef RECAP_LABELLER_MODELS_ANNOTATION_HPP_INCLUDE
#define RECAP_LABELLER_MODELS_ANNOTATION_HPP_INCLUDE

#include "models/time-range.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace recap::labeller::models {

enum class SrcPattern : unsigned char {
    ExEx,
    ExIn,
    InEx,
    InIn,
};

struct SwallowApneaAnnotation {
    bool is_ambiguous;
    SrcPattern pattern;
    TimeRange time;
    std::vector<TimeRange> non_respiratory_flow;

    bool operator==(const SwallowApneaAnnotation&) const = default;
};

enum class SwallowApneaError : unsigned char {
    FlowError,
    NoSwallow,
    ApneaCutoff,
};

enum class EarClickError : unsigned char {
    AudioError,
    NoEarClick,
};

struct SwallowAnnotation {
    std::variant<SwallowApneaAnnotation, SwallowApneaError> swallow_apnea;
    std::variant<std::vector<TimeRange>, EarClickError> ear_clicks;
    std::optional<std::string> note;

    bool operator==(const SwallowAnnotation&) const = default;
};

}; // namespace recap::labeller::models

#endif // RECAP_LABELLER_MODELS_ANNOTATION_HPP_INCLUDE
