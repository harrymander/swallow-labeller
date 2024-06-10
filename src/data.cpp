#include "data.hpp"

#include <cnpy.h>

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace plot {

template <class T> static Data<T> get_array(const cnpy::npz_t& data, const std::string& name)
{
    const auto found = data.find(name);
    if (found != data.end()) {
        return Data<T>::from_numpy(found->second);
    }
    throw std::invalid_argument("missing field: " + std::string(name));
}

SwallowTaskData SwallowTaskData::from_numpy(const cnpy::npz_t& data)
{
    const SwallowTaskData task_data{
        get_array<float>(data, "flow"),
        get_array<float>(data, "ear_audio"),
        get_array<uint8_t>(data, "event"),
    };

    if (task_data.event.data.size() != task_data.flow.data.size()) {
        throw std::invalid_argument("flow and event data length mismatch");
    }

    return task_data;
}

}; // namespace plot
