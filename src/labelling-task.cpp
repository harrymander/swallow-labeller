#include "labelling-task.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

NLOHMANN_JSON_NAMESPACE_BEGIN

// Adapted from
// https://json.nlohmann.me/features/arbitrary_types/#how-do-i-convert-third-party-types
template <typename T> struct adl_serializer<std::optional<T>> {
    static void to_json(json& j, const std::optional<T>& val)
    {
        if (val.has_value()) {
            j = *val;
        } else {
            j = nullptr;
        }
    }

    static void from_json(const json& j, std::optional<T>& val)
    {
        if (j.is_null()) {
            val = std::nullopt;
        } else {
            val = j.template get<T>();
        }
    }
};

NLOHMANN_JSON_NAMESPACE_END

namespace recap::labeller::task {

using Json = nlohmann::json;

namespace {

constexpr int JsonIndentSize = 2;

template <class T> T parse_json(std::istream& stream)
{
    try {
        return Json::parse(stream).template get<T>();
    } catch (const nlohmann::json::parse_error&) {
        throw std::invalid_argument("JSON parse error");
    } catch (const nlohmann::json::exception&) {
        throw std::invalid_argument("Invalid JSON");
    }
}

}; // namespace

NLOHMANN_JSON_SERIALIZE_ENUM(
    SwallowTestType,
    {
        {SwallowTestType::TidalBreathing, "tidal-breathing"},
        {SwallowTestType::Cued, "cued"},
    }
);

std::string swallow_test_type_string(SwallowTestType type)
{
    return Json(type).get<std::string>();
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LabellingDataFile, path, checksum);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TimeRange, start, end);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    SwallowTaskInfo,
    subject,
    test_type,
    repeatnum,
    swallownum,
    recording_file,
    csv_range_secs,
    event_range_secs,
    npz_file
);

std::vector<SwallowTaskInfo> load_swallow_task_info_json(std::istream& stream)
{
    return parse_json<std::vector<SwallowTaskInfo>>(stream);
}

NLOHMANN_JSON_SERIALIZE_ENUM(
    SRCPattern,
    {
        {SRCPattern::ExEx, "ex-ex"},
        {SRCPattern::ExIn, "ex-in"},
        {SRCPattern::InEx, "in-ex"},
        {SRCPattern::InIn, "in-in"},
    }
);

NLOHMANN_JSON_SERIALIZE_ENUM(
    SwallowLabelInfo,
    {
        {SwallowLabelInfo::Ok, "ok"},
        {SwallowLabelInfo::FlowError, "flow-error"},
        {SwallowLabelInfo::NoSwallow, "no-swallow"},
        {SwallowLabelInfo::ApneaCutOff, "apnea-cutoff"},
    }
);

NLOHMANN_JSON_SERIALIZE_ENUM(
    EarClickLabelInfo,
    {
        {EarClickLabelInfo::Ok, "ok"},
        {EarClickLabelInfo::NoEarClick, "no-ear-click"},
        {EarClickLabelInfo::AudioError, "audio-error"},
    }
);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SwallowApneaLabel, time, pattern, is_ambiguous);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    SwallowAnnotation,
    swallow_apnea,
    swallow_info,
    swallow_notes,
    ear_clicks,
    ear_click_info,
    ear_click_notes
);

AnnotationsMap load_swallow_annotation_json(std::istream& stream)
{
    return parse_json<AnnotationsMap>(stream);
}

void dump_swallow_annotations_json(std::ostream& os, const AnnotationsMap& map)
{
    os << Json(map).dump(JsonIndentSize) << '\n';
    if (!os.good()) {
        throw std::runtime_error("Error writing JSON to stream");
    }
}

class AnnotationsMapJsonWriter {
public:
    explicit AnnotationsMapJsonWriter(const AnnotationsMap& map) : map(map) {}

private:
    const AnnotationsMap& map;
};

std::unique_ptr<AnnotationsMapJsonWriter> annotations_map_to_json(const AnnotationsMap& map)
{
    return std::make_unique<AnnotationsMapJsonWriter>(map);
}

}; // namespace recap::labeller::task
