#ifndef RECAP_LABELLER_MODELS_TIME_RANGE_HPP_INCLUDE
#define RECAP_LABELLER_MODELS_TIME_RANGE_HPP_INCLUDE

namespace recap::labeller::models {

struct TimeRange {
    double start;
    double end;

    bool operator==(const TimeRange&) const = default;
};

}; // namespace recap::labeller::models

#endif // RECAP_LABELLER_MODELS_TIME_RANGE_HPP_INCLUDE
