#ifndef INCLUDE_PLOT_DATA_HPP
#define INCLUDE_PLOT_DATA_HPP

#include <cnpy.h>

#include <stdexcept>
#include <vector>

namespace plot {

template <class T> struct Data {
    std::vector<T> data;
    std::vector<T> time;

    static Data from_numpy(const cnpy::NpyArray& array)
    {
        if (array.fortran_order) {
            throw std::invalid_argument("array in Fortran order");
        }

        if (array.shape.size() != 2) {
            throw std::invalid_argument("data must have 2 dimensions");
        }

        if (array.word_size != sizeof(T)) {
            throw std::invalid_argument("got invalid word size");
        }

        // Arrays are stored in row-major order, with time in first row and data in second
        if (array.shape[0] != 2) {
            throw std::invalid_argument("data must have 2 rows");
        }

        const size_t rowlen = array.shape[1];
        assert(rowlen == array.num_vals / 2);
        const T *data = array.data<T>();
        return {{data, data + rowlen}, {data + rowlen, data + rowlen * 2}};
    }
};

struct SwallowTaskData {
    Data<float> flow;
    Data<float> ear_audio;
    Data<uint8_t> event;

    static SwallowTaskData from_numpy(const cnpy::npz_t& data);
};

}; // namespace plot

#endif // INCLUDE_PLOT_DATA_HPP
