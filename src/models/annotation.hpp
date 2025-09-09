#ifndef RECAP_LABELLER_MODELS_ANNOTATION_HPP_INCLUDE
#define RECAP_LABELLER_MODELS_ANNOTATION_HPP_INCLUDE

#include "models/time-range.hpp"

#include <string>
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

    bool operator==(const SwallowApneaAnnotation&) const = default;
};

struct SwallowAnnotation {
    std::vector<SwallowApneaAnnotation> swallow_apneas;
    std::vector<TimeRange> ear_clicks;
    std::vector<TimeRange> non_respiratory_flow_events;
    std::vector<std::string> notes;

    bool operator==(const SwallowAnnotation&) const = default;
};

}; // namespace recap::labeller::models

#endif // RECAP_LABELLER_MODELS_ANNOTATION_HPP_INCLUDE
