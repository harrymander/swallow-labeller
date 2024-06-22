#include "labelling-task.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace labelling_task;

static void load_from_path(const char *path, std::vector<SwallowTaskInfo>& tasks)
{
    std::ifstream file(path);
    if (file.fail()) {
        GTEST_FAIL() << "Cannot opening file '" << path << "'";
    }
    tasks = load_swallow_task_info_json(file);
}

TEST(TestSwallowLabellingTask, TestParseJson)
{
    std::vector<SwallowTaskInfo> tasks;
    load_from_path(JSON_PATH, tasks);
    ASSERT_EQ(tasks.size(), 728);
}

TEST(TestSwallowLabellingTask, TestParseInvalidJsonFails)
{
    std::vector<SwallowTaskInfo> tasks;
    ASSERT_THROW(load_from_path(INVALID_JSON_PATH, tasks), std::invalid_argument);
}

TEST(TestSwallowLabellingTask, TestParseJsonMissingFieldFails)
{
    std::vector<SwallowTaskInfo> tasks;
    ASSERT_THROW(load_from_path(MISSING_FIELD_JSON_PATH, tasks), std::invalid_argument);
}

TEST(TestSwallowLabellingTask, TestSwallowTestTypeToString)
{
    using enum SwallowTestType;
    EXPECT_EQ(swallow_test_type_string(TidalBreathing), "tidal-breathing");
    EXPECT_EQ(swallow_test_type_string(Cued), "cued");
}
