#ifndef INCLUDE_PLOT_DATA_HPP
#define INCLUDE_PLOT_DATA_HPP

#include <cnpy.h>

#include <stdexcept>
#include <vector>

namespace plot {

struct SwallowTaskData {
    std::vector<double> flow;
    std::vector<uint8_t> event;
    std::vector<double> flow_time;

    std::vector<double> audio;
    std::vector<double> audio_time;

    static SwallowTaskData from_numpy(const cnpy::npz_t& data);
};

}; // namespace plot

#endif // INCLUDE_PLOT_DATA_HPP
