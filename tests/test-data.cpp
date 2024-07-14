#include "data.hpp"
#include "test-data-paths.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace recap::labeller;

static SwallowTaskData load_data(const std::string& path)
{
    return SwallowTaskData::from_numpy(cnpy::npz_load(path));
}

TEST(TestData, TestLoadNormal)
{
    const auto data = load_data(test_path::normal);

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

TEST(TestData, TestAudioAndFlowEqualLength)
{
    const auto data = load_data(test_path::audio_flow_equal_len);
    ASSERT_EQ(data.flow.size(), 10);
    ASSERT_EQ(data.flow_time.size(), 10);
    ASSERT_EQ(data.event.size(), 10);
    ASSERT_EQ(data.audio.size(), 10);
    ASSERT_EQ(data.audio_time.size(), 10);
}

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

TEST(TestData, TestNoSamplesFails)
{
    ASSERT_THROW_MSG(load_data(test_path::no_samples), std::invalid_argument, "missing samples");
}

TEST(TestData, TestAudioShorterFails)
{
    ASSERT_THROW_MSG(
        load_data(test_path::audio_field_shorter),
        std::invalid_argument,
        "audio field shorter than flow and event"
    );
}

class TestDataMissingField : public testing::TestWithParam<std::string> {};

TEST_P(TestDataMissingField, TestMissingFieldFails)
{
    const std::string field = GetParam();
    const std::string path = test_path::missing_field_prefix + field + ".npz";
    const std::string expected_msg = "missing field: " + field;
    ASSERT_THROW_MSG(load_data(path), std::invalid_argument, expected_msg);
}

const auto Fields = testing::Values("flow", "flow_time", "event", "audio", "audio_time");

INSTANTIATE_TEST_SUITE_P(MissingFields, TestDataMissingField, Fields);

TEST(TestData, TestInvalidFlowTypeFails)
{
    ASSERT_THROW_MSG(
        load_data(test_path::invalid_flow_type), std::invalid_argument, "got invalid word size"
    );
}

class TestDataShortField : public testing::TestWithParam<std::string> {};

TEST_P(TestDataShortField, TestFieldLenMismatchFails)
{
    const std::string path = test_path::len_mismatch_prefix + GetParam() + ".npz";
    ASSERT_THROW(load_data(path), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(ShortenedFields, TestDataShortField, Fields);

TEST(TestData, Test2DArrayFails)
{
    ASSERT_THROW_MSG(
        load_data(test_path::two_dimensional_array), std::invalid_argument, "expected 1D array"
    );
}

TEST(TestData, TestFortranOrderFails)
{
    ASSERT_THROW_MSG(
        load_data(test_path::fortran_order), std::invalid_argument, "array is in Fortran order"
    );
}
