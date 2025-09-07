#include "models/annotation-json.hpp"
#include "models/annotation.hpp"
#include "models/time-range.hpp"
#include "nlohmann/json_fwd.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <string_view>

// TODO: these tests are a mess

using namespace recap::labeller::models;

void parse_json_str(std::string_view json_str, nlohmann::json& json)
{
    try {
        json = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::exception& e) {
        FAIL() << "Failed to parse JSON: " << e.what();
    }
}

void load_annotation_json(SwallowAnnotation& annotation, std::string_view json_str)
{
    nlohmann::json json;
    parse_json_str(json_str, json);
    annotation = json.template get<SwallowAnnotation>();
}

TEST(TestSwallowAnnotationJson, TestParsingSwallowAnnotationsWithoutNote)
{
    SwallowAnnotation annotation;
    load_annotation_json(annotation, R"({
      "swallow_apneas": [
        {
          "is_ambiguous": false,
          "pattern": "in-in",
          "time": {
            "start": 119.08481475903011,
            "end": 120.48098328244551
          }
        },
        {
          "is_ambiguous": true,
          "pattern": "in-ex",
          "time": {
            "start": 245.05845865157465,
            "end": 245.60912588908397
          }
        },
        {
          "is_ambiguous": false,
          "pattern": "in-in",
          "time": {
            "start": 374.12441952867147,
            "end": 375.0705088668944
          }
        }
      ],
      "ear_clicks": [
        {
          "start": 121.3987885795338,
          "end": 121.42115575741595
        }
      ],
      "non_respiratory_flow_events": [
        {
          "start": 118.95642548594623,
          "end": 119.08919092071459
        },
        {
          "start": 120.48326282214042,
          "end": 120.64156868974659
        }
      ],
      "note": null
    })");

    std::vector<SwallowApneaAnnotation> expected_apneas = {
        SwallowApneaAnnotation{
            .is_ambiguous = false,
            .pattern = SrcPattern::InIn,
            .time = TimeRange{119.08481475903011, 120.48098328244551},
        },
        SwallowApneaAnnotation{
            .is_ambiguous = true,
            .pattern = SrcPattern::InEx,
            .time = TimeRange{245.05845865157465, 245.60912588908397},
        },
        SwallowApneaAnnotation{
            .is_ambiguous = false,
            .pattern = SrcPattern::InIn,
            .time = TimeRange{374.12441952867147, 375.0705088668944},
        },
    };
    EXPECT_EQ(annotation.swallow_apneas, expected_apneas);

    std::vector<TimeRange> expected_ear_clicks = {
        TimeRange{121.3987885795338, 121.42115575741595},
    };
    EXPECT_EQ(annotation.ear_clicks, expected_ear_clicks);

    std::vector<TimeRange> expected_snrf_events = {
        TimeRange{118.95642548594623, 119.08919092071459},
        TimeRange{120.48326282214042, 120.64156868974659},
    };
    EXPECT_EQ(annotation.non_respiratory_flow_events, expected_snrf_events);

    EXPECT_EQ(annotation.note, std::nullopt);
}

TEST(TestSwallowAnnotationJson, TestParsingSwallowAnnotationsWithNote)
{
    SwallowAnnotation annotation;
    load_annotation_json(annotation, R"({
      "swallow_apneas": [],
      "ear_clicks": [],
      "non_respiratory_flow_events": [],
      "note": "hello, world!!!"
    })");

    EXPECT_TRUE(annotation.swallow_apneas.empty());
    EXPECT_TRUE(annotation.ear_clicks.empty());
    EXPECT_TRUE(annotation.non_respiratory_flow_events.empty());
    EXPECT_TRUE(annotation.note.has_value());
    EXPECT_EQ(*annotation.note, "hello, world!!!");
}

TEST(TestSwallowAnnotationJson, TestParsingMissingSwallowApneasFails)
{
    const auto json = R"({
        "ear_clicks": [],
        "non_respiratory_flow_events": [],
        "note": null
    })";

    SwallowAnnotation annotation;
    EXPECT_THROW(load_annotation_json(annotation, json), nlohmann::json::exception);
}

TEST(TestSwallowAnnotationJson, TestParsingMissingEarClicksFails)
{
    const auto json = R"({
        "swallow_apneas": [],
        "non_respiratory_flow_events": [],
        "note": null
    })";

    SwallowAnnotation annotation;
    EXPECT_THROW(load_annotation_json(annotation, json), nlohmann::json::exception);
}

TEST(TestSwallowAnnotationJson, TestParsingMissingSNRFsFails)
{
    const auto json = R"({
        "swallow_apneas": [],
        "ear_clicks": [],
        "note": null
    })";

    SwallowAnnotation annotation;
    EXPECT_THROW(load_annotation_json(annotation, json), nlohmann::json::exception);
}

TEST(TestSwallowAnnotationJson, TestParsingMissingNoteFails)
{
    const auto json = R"({
        "swallow_apneas": [],
        "ear_clicks": [],
        "non_respiratory_flow_events": []
    })";

    SwallowAnnotation annotation;
    EXPECT_THROW(load_annotation_json(annotation, json), nlohmann::json::exception);
}

/**
 * Serialize an annotation to JSON object, then parse that object to another annotation object and
 * then assert that they are equal.
 */
#define ASSERT_SERIALIZE_DERIALIZE_EQ(annotation)                                                  \
    do {                                                                                           \
        std::string serialized = nlohmann::json(annotation).dump();                                \
        auto json = nlohmann::json::parse(serialized);                                             \
        auto annotation_deserialized = json.template get<SwallowAnnotation>();                     \
        ASSERT_EQ(annotation, annotation_deserialized);                                            \
    } while (0)

TEST(TestSwallowAnnotationJson, TestSerializing)
{
    SwallowAnnotation annotation{
        .swallow_apneas =
            {
                SwallowApneaAnnotation{
                    .is_ambiguous = true,
                    .pattern = SrcPattern::ExEx,
                    .time = {0.0, 1.0},
                },
            },
        .ear_clicks = std::vector<TimeRange>{{12, 100}, {200, 300}},
        .non_respiratory_flow_events = std::vector<TimeRange>{{150, 200}},
        .note = "Hello, world! β",
    };
    ASSERT_SERIALIZE_DERIALIZE_EQ(annotation);
}
