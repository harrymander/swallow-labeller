#ifndef INCLUDE_PLOT_DATA_HPP
#define INCLUDE_PLOT_DATA_HPP

#include <cnpy.h>

#include <vector>

namespace recap::labeller::models {

struct SwallowTaskData {
    std::vector<double> flow;
    std::vector<double> flow_time;

    std::vector<double> audio;
    std::vector<double> audio_time;

    // Throws std::invalid_argument on error
    static SwallowTaskData from_numpy(const cnpy::npz_t& data);
};

}; // namespace recap::labeller::models

#endif // INCLUDE_PLOT_DATA_HPP
