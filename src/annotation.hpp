#ifndef RECAP_LABELLER_ANNOTATION_HPP_INCLUDE
#define RECAP_LABELLER_ANNOTATION_HPP_INCLUDE

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace recap::labeller {

struct TimeRange {
    double start;
    double end;

    bool operator==(const TimeRange&) const = default;
};

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

enum class ApneaError {
    FlowError,
    NoSwallow,
    ApneaCutoff,
};

enum class EarClickError {
    AudioError,
    NoEarClick,
};

struct SwallowAnnotation {
    std::variant<SwallowApneaAnnotation, ApneaError> swallow_apnea;
    std::variant<std::vector<TimeRange>, EarClickError> ear_clicks;

    bool operator==(const SwallowAnnotation&) const = default;

    // Raises std::runtime_error on parse error
    static SwallowAnnotation from_json(std::string_view str);

    [[nodiscard]] std::string dump_json() const;
};

}; // namespace recap::labeller

#endif // RECAP_LABELLER_ANNOTATION_HPP_INCLUDE
