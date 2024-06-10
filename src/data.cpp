#include "data.hpp"

#include <cnpy.h>

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace plot {

template <class T> static std::vector<T> get_array(const cnpy::npz_t& data, const std::string& name)
{
    const auto found = data.find(name);
    if (found == data.end()) {
        throw std::invalid_argument("missing field: " + std::string(name));
    }

    const cnpy::NpyArray array = found->second;

    if (array.fortran_order) {
        throw std::invalid_argument("array is in Fortran order");
    }

    if (array.shape.size() != 1) {
        throw std::invalid_argument("expected 1D array");
    }

    if (array.word_size != sizeof(T)) {
        throw std::invalid_argument("got invalid word size");
    }

    return array.as_vec<T>();
}

SwallowTaskData SwallowTaskData::from_numpy(const cnpy::npz_t& data)
{
    const SwallowTaskData task = {
        .flow = get_array<double>(data, "flow"),
        .event = get_array<uint8_t>(data, "event"),
        .flow_time = get_array<double>(data, "flow_time"),
        .audio = get_array<double>(data, "audio"),
        .audio_time = get_array<double>(data, "audio_time"),
    };

    const auto flow_size = task.flow.size();
    if (flow_size == 0) {
        throw std::invalid_argument("missing samples");
    }
    if (flow_size != task.flow_time.size()) {
        throw std::invalid_argument("flow and flow_time length mismatch");
    }
    if (flow_size != task.event.size()) {
        throw std::invalid_argument("flow and event length mismatch");
    }

    const auto audio_size = task.audio.size();
    if (audio_size != task.audio_time.size()) {
        throw std::invalid_argument("audio and audio_time length mismatch");
    }

    if (audio_size < flow_size) {
        throw std::invalid_argument("audio field shorter than flow and event");
    }

    return task;
}

}; // namespace plot
