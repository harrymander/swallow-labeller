#ifndef INCLUDE_LABELLING_TASK_HPP
#define INCLUDE_LABELLING_TASK_HPP

#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace labelling_task {

enum class SwallowTestType {
    TidalBreathing,
    Cued,
};

std::string swallow_test_type_string(SwallowTestType type);

struct LabellingDataFile {
    std::string path;
    std::string checksum;
};

struct TimeRange {
    double start;
    double end;
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

struct SwallowAnnotation {
    std::string id;
};

std::vector<SwallowAnnotation> load_swallow_annotation_json(std::istream& stream);

}; // namespace labelling_task

#endif // INCLUDE_LABELLING_TASK_HPP
