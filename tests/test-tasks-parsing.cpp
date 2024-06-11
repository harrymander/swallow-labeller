#include "labelling-task.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <vector>

using labelling_task::SwallowLabellingTask;
using Json = nlohmann::json;

TEST(TestSwallowLabellingTask, TestParseJson)
{
    Json json;
    {
        std::ifstream file(JSON_PATH);
        json = Json::parse(file);
    }

    const auto tasks = json.template get<std::vector<SwallowLabellingTask>>();
    ASSERT_EQ(tasks.size(), 728);
}
