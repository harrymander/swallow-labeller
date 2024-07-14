#ifndef INCLUDE_LABELLING_TASK_HPP
#define INCLUDE_LABELLING_TASK_HPP

#include "time-range.hpp"

#include <istream>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace recap::labeller {

enum class SwallowTestType {
    TidalBreathing,
    Cued,
};

std::string swallow_test_type_string(SwallowTestType type);

struct LabellingDataFile {
    std::string path;
    std::string checksum;
};

struct SwallowTaskInfo {
    unsigned int subject;
    SwallowTestType test_type;
    unsigned int repeatnum;
    unsigned int swallownum;
    std::string recording_file;
    TimeRange csv_range_secs;
    TimeRange event_range_secs;
    LabellingDataFile npz_file;

    [[nodiscard]] const std::string& get_id() const { return npz_file.checksum; }
};

std::vector<SwallowTaskInfo> load_swallow_task_info_json(std::istream& stream);

}; // namespace recap::labeller

#endif // INCLUDE_LABELLING_TASK_HPP
