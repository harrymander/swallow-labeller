#ifndef INCLUDE_LABELLING_TASK_HPP
#define INCLUDE_LABELLING_TASK_HPP

#include <nlohmann/json.hpp>

#include <string>

namespace labelling_task {

enum class SwallowTestType {
    TidalBreathing,
    Cued,
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    SwallowTestType,
    {
        {SwallowTestType::TidalBreathing, "tidal-breathing"},
        {SwallowTestType::Cued, "cued"},
    }
)

struct LabellingDataFile {
    std::string path;
    std::string checksum;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LabellingDataFile, path, checksum)

struct TimeRange {
    double start;
    double end;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TimeRange, start, end)

struct SwallowLabellingTask {
    SwallowTestType test_type;
    unsigned int repeatnum;
    unsigned int swallownum;
    std::string recording_file;
    TimeRange csv_range_secs;
    TimeRange event_range_secs;
    LabellingDataFile npz_file;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    SwallowLabellingTask,
    test_type,
    repeatnum,
    swallownum,
    recording_file,
    csv_range_secs,
    event_range_secs,
    npz_file
)

}; // namespace labelling_task

#endif // INCLUDE_LABELLING_TASK_HPP
