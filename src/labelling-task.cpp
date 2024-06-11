#include "labelling-task.hpp"

#include "data.hpp"

#include <cnpy.h>

#include <filesystem>

using namespace labelling_task;

plot::SwallowTaskData SwallowLabellingTask::load_data(const std::filesystem::path& data_dir) const
{
    const auto path = data_dir / std::filesystem::path(recording_file);
    return plot::SwallowTaskData::from_numpy(cnpy::npz_load(path));
}
