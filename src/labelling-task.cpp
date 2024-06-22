#include "labelling-task.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace labelling_task {

using Json = nlohmann::json;

namespace {

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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SwallowAnnotation, id);

std::vector<SwallowAnnotation> load_swallow_annotation_json(std::istream& stream)
{
    return parse_json<std::vector<SwallowAnnotation>>(stream);
}

}; // namespace labelling_task
