#ifndef RECAP_LABELLER_TIME_RANGE_HPP_INCLUDE
#define RECAP_LABELLER_TIME_RANGE_HPP_INCLUDE

namespace recap::labeller {

struct TimeRange {
    double start;
    double end;

    bool operator==(const TimeRange&) const = default;
};

}; // namespace recap::labeller

#endif // RECAP_LABELLER_TIME_RANGE_HPP_INCLUDE
