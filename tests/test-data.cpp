#include "data.hpp"
#include "test-data-paths.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace plot;

SwallowTaskData load_data(const std::string& path)
{
    return SwallowTaskData::from_numpy(cnpy::npz_load(path));
}

TEST(TestData, TestLoadNormal)
{
    const auto data = load_data(path::normal);

    ASSERT_EQ(data.flow.size(), 10);
    for (int i = 0; i < data.flow.size(); i++) {
        ASSERT_EQ(data.flow[i], data.flow.size() - i);
    }

    ASSERT_EQ(data.flow_time.size(), 10);
    for (int i = 0; i < data.flow_time.size(); i++) {
        ASSERT_EQ(data.flow_time[i], i);
    }

    ASSERT_EQ(data.event.size(), 10);
    for (int i = 0; i < data.event.size(); i++) {
        ASSERT_EQ(bool(data.event[i]), i % 2 == 0);
    }

    ASSERT_EQ(data.audio.size(), 100);
    for (int i = 0; i < data.audio.size(); i++) {
        ASSERT_EQ(data.audio[i], (double) i / 2);
    }

    ASSERT_EQ(data.audio_time.size(), 100);
    for (int i = 0; i < data.audio_time.size(); i++) {
        ASSERT_EQ(data.audio_time[i], i);
    }
}

class TestDataMissingField : public testing::TestWithParam<std::string> {};

#define ASSERT_THROW_MSG(statement, exc, msg)                                                      \
    ASSERT_THROW(                                                                                  \
        {                                                                                          \
            try {                                                                                  \
                statement;                                                                         \
            } catch (const exc& e) {                                                               \
                ASSERT_EQ(e.what(), std::string(msg));                                             \
                throw;                                                                             \
            }                                                                                      \
        },                                                                                         \
        exc                                                                                        \
    )

TEST_P(TestDataMissingField, TestMissingFieldFails)
{
    const std::string field = GetParam();
    const std::string path = path::missing_field_prefix + field + ".npz";
    ASSERT_THROW_MSG(
        load_data(path), std::invalid_argument, std::string("missing field: ") + field
    );
}

const auto Fields = testing::Values("flow", "flow_time", "event", "audio", "audio_time");

INSTANTIATE_TEST_SUITE_P(MissingFields, TestDataMissingField, Fields);

TEST(TestData, TestInvalidFlowTypeFails)
{
    ASSERT_THROW_MSG(
        load_data(path::invalid_flow_type), std::invalid_argument, "got invalid word size"
    );
}

class TestDataShortField : public testing::TestWithParam<std::string> {};

TEST_P(TestDataShortField, TestAudioTimeLengthMismatchFails)
{
    ASSERT_THROW(load_data(path::len_mismatch_prefix + GetParam() + ".npz"), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(ShortenedFields, TestDataShortField, Fields);

TEST(TestData, Test2DArrayFails)
{
    ASSERT_THROW_MSG(
        load_data(path::two_dimensional_array), std::invalid_argument, "expected 1D array"
    );
}

TEST(TestData, TestFortranOrderFails)
{
    ASSERT_THROW_MSG(
        load_data(path::fortran_order), std::invalid_argument, "array is in Fortran order"
    );
}
