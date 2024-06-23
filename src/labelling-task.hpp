#ifndef INCLUDE_LABELLING_TASK_HPP
#define INCLUDE_LABELLING_TASK_HPP

#include <istream>
#include <map>
#include <optional>
#include <ostream>
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

    bool operator==(const TimeRange&) const = default;
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

enum class SRCPattern {
    ExEx,
    ExIn,
    InEx,
    InIn,
};

struct SwallowApneaLabel {
    TimeRange time;
    SRCPattern pattern;
    bool is_ambiguous;

    bool operator==(const SwallowApneaLabel&) const = default;
};

enum class SwallowLabelInfo {
    Ok,
    FlowError,
    NoSwallow,
    ApneaCutOff,
};

enum class EarClickLabelInfo {
    Ok,
    NoEarClick,
    AudioError,
};

struct SwallowAnnotation {
    std::optional<SwallowApneaLabel> swallow_apnea;
    SwallowLabelInfo swallow_info;
    std::optional<std::string> swallow_notes;

    std::vector<TimeRange> ear_clicks;
    EarClickLabelInfo ear_click_info;
    std::optional<std::string> ear_click_notes;

    bool operator==(const SwallowAnnotation&) const = default;
};

using AnnotationsMap = std::map<std::string, SwallowAnnotation>;

AnnotationsMap load_swallow_annotation_json(std::istream& stream);

/**
 * Writes the map JSON to os
 *
 * Raises std::runtime_error or subclass thereof on write error.
 */
void dump_swallow_annotations_json(std::ostream& os, const AnnotationsMap& map);

}; // namespace labelling_task

#endif // INCLUDE_LABELLING_TASK_HPP
