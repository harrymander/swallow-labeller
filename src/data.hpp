#ifndef INCLUDE_PLOT_DATA_HPP
#define INCLUDE_PLOT_DATA_HPP

#include <cnpy.h>

#include <vector>

namespace recap::labeller {

struct SwallowTaskData {
    std::vector<double> flow;
    std::vector<uint8_t> event;
    std::vector<double> flow_time;

    std::vector<double> audio;
    std::vector<double> audio_time;

    // Throws std::invalid_argument on error
    static SwallowTaskData from_numpy(const cnpy::npz_t& data);
};

}; // namespace recap::labeller

#endif // INCLUDE_PLOT_DATA_HPP
