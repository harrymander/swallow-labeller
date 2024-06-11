#include "labelling-task.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace labelling_task;

static void load_from_path(const char *path, std::vector<SwallowLabellingTask>& tasks)
{
    std::ifstream file(path);
    if (file.fail()) {
        GTEST_FAIL() << "Cannot opening file '" << path << "'";
    }
    tasks = load_tasks_json(file);
}

TEST(TestSwallowLabellingTask, TestParseJson)
{
    std::vector<SwallowLabellingTask> tasks;
    load_from_path(JSON_PATH, tasks);
    ASSERT_EQ(tasks.size(), 728);
}

TEST(TestSwallowLabellingTask, TestParseInvalidJsonFails)
{
    std::vector<SwallowLabellingTask> tasks;
    ASSERT_THROW(load_from_path(INVALID_JSON_PATH, tasks), std::invalid_argument);
}

TEST(TestSwallowLabellingTask, TestParseJsonMissingFieldFails)
{
    std::vector<SwallowLabellingTask> tasks;
    ASSERT_THROW(load_from_path(MISSING_FIELD_JSON_PATH, tasks), std::invalid_argument);
}
