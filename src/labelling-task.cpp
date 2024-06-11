#include "labelling-task.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace labelling_task {

using Json = nlohmann::json;

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
    SwallowLabellingTask,
    test_type,
    repeatnum,
    swallownum,
    recording_file,
    csv_range_secs,
    event_range_secs,
    npz_file
);

std::string dump_tasks_json(const std::vector<SwallowLabellingTask>& tasks)
{
    return Json(tasks).dump();
}

std::vector<SwallowLabellingTask> load_tasks_json(std::istream& stream)
{
    try {
        return Json::parse(stream).template get<std::vector<SwallowLabellingTask>>();
    } catch (const nlohmann::json::parse_error&) {
        throw std::invalid_argument("JSON parse error");
    } catch (const nlohmann::json::exception&) {
        throw std::invalid_argument("Invalid JSON");
    }
}

}; // namespace labelling_task
