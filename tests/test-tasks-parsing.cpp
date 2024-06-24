#include "labelling-task.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace recap::labeller::task;

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

// Just testing that the std::optional<T> parsing works...
TEST(TestSwallowAnnotations, TestParseJson)
{
    std::ifstream stream(ANNOTATIONS_JSON_PATH);
    auto annotations = load_swallow_annotation_json(stream);

    ASSERT_EQ(annotations.size(), 4);

    ASSERT_TRUE(annotations["all_fields_present"].swallow_apnea.has_value());
    ASSERT_EQ(*annotations["all_fields_present"].swallow_notes, "Test note");
    ASSERT_EQ(*annotations["all_fields_present"].ear_click_notes, "No ear clicks");

    ASSERT_FALSE(annotations["no_swallow_label"].swallow_apnea.has_value());
    ASSERT_EQ(*annotations["no_swallow_label"].swallow_notes, "Test note");
    ASSERT_EQ(*annotations["no_swallow_label"].ear_click_notes, "No ear clicks");

    ASSERT_TRUE(annotations["no_swallow_notes"].swallow_apnea.has_value());
    ASSERT_FALSE(annotations["no_swallow_notes"].swallow_notes.has_value());
    ASSERT_EQ(*annotations["no_swallow_notes"].ear_click_notes, "No ear clicks");

    ASSERT_TRUE(annotations["no_ear_click_notes"].swallow_apnea.has_value());
    ASSERT_EQ(*annotations["no_ear_click_notes"].swallow_notes, "Test note");
    ASSERT_FALSE(annotations["no_ear_click_notes"].ear_click_notes.has_value());
}
