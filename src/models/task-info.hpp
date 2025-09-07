#ifndef RECAP_LABELLER_MODELS_TASK_INFO_HPP_INCLUDE
#define RECAP_LABELLER_MODELS_TASK_INFO_HPP_INCLUDE

#include "models/time-range.hpp"

#include <compare>
#include <istream>
#include <string>
#include <vector>

namespace recap::labeller::models {

enum class SwallowTestType : unsigned char {
    TidalBreathing,
    Cued,
};

std::string swallow_test_type_string(SwallowTestType type);

struct LabellingDataFile {
    std::string path;
    std::string checksum;

    auto operator<=>(const LabellingDataFile&) const = default;
};

struct SwallowTaskInfo {
    unsigned int subject;
    SwallowTestType test_type;
    unsigned int repeatnum;
    std::string recording_file;
    LabellingDataFile npz_file;
    std::vector<TimeRange> event_times;

    [[nodiscard]] const std::string& get_id() const { return npz_file.path; }

    auto operator<=>(const SwallowTaskInfo&) const = default;
};

std::vector<SwallowTaskInfo> load_swallow_task_info_json(std::istream& stream);

}; // namespace recap::labeller::models

#endif // RECAP_LABELLER_MODELS_TASK_INFO_HPP_INCLUDE
