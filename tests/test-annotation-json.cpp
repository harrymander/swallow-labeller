#include "models/annotation.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <variant>

// TODO: these tests are a mess

using namespace recap::labeller::models;

TEST(TestSwallowAnnotationJson, TestParsingWithApneaAndEarClick)
{
    const auto annotation = SwallowAnnotation::from_json(R"({
        "swallow_apnea": {
            "is_ambiguous": false,
            "pattern": "ex-ex",
            "time": {"start": 0.0, "end": 1.0}
        },
        "ear_clicks": [{"start": 12, "end": 100}, {"start": 200, "end": 300}],
        "note": null
    })");

    ASSERT_TRUE(std::holds_alternative<SwallowApneaAnnotation>(annotation.swallow_apnea));
    auto apnea = std::get<SwallowApneaAnnotation>(annotation.swallow_apnea);
    EXPECT_FALSE(apnea.is_ambiguous);
    EXPECT_EQ(apnea.pattern, SRCPattern::ExEx);
    TimeRange exp_time_range = {0.0, 1.0};
    EXPECT_EQ(apnea.time, exp_time_range);

    ASSERT_TRUE(std::holds_alternative<std::vector<TimeRange>>(annotation.ear_clicks));
    auto clicks = std::get<std::vector<TimeRange>>(annotation.ear_clicks);
    std::vector<TimeRange> exp_clicks = {{12, 100}, {200, 300}};
    EXPECT_EQ(clicks, exp_clicks);

    EXPECT_FALSE(annotation.note.has_value());
}

TEST(TestSwallowAnnotationJson, TestParsingWithApneaAndEarClickAndNote)
{
    const auto annotation = SwallowAnnotation::from_json(R"({
        "swallow_apnea": {
            "is_ambiguous": false,
            "pattern": "ex-ex",
            "time": {"start": 0.0, "end": 1.0}
        },
        "ear_clicks": [{"start": 12, "end": 100}, {"start": 200, "end": 300}],
        "note": "Hello, world! β"
    })");

    ASSERT_TRUE(std::holds_alternative<SwallowApneaAnnotation>(annotation.swallow_apnea));
    auto apnea = std::get<SwallowApneaAnnotation>(annotation.swallow_apnea);
    EXPECT_FALSE(apnea.is_ambiguous);
    EXPECT_EQ(apnea.pattern, SRCPattern::ExEx);
    TimeRange exp_time_range = {0.0, 1.0};
    EXPECT_EQ(apnea.time, exp_time_range);

    ASSERT_TRUE(std::holds_alternative<std::vector<TimeRange>>(annotation.ear_clicks));
    auto clicks = std::get<std::vector<TimeRange>>(annotation.ear_clicks);
    std::vector<TimeRange> exp_clicks = {{12, 100}, {200, 300}};
    EXPECT_EQ(clicks, exp_clicks);

    ASSERT_TRUE(annotation.note.has_value());
    EXPECT_EQ(annotation.note.value(), "Hello, world! β");
}

TEST(TestSwallowAnnotationJson, TestParsingInvalidSRCPatternFails)
{
    const auto json = R"({
        "swallow_apnea": {
            "is_ambiguous": false,
            "pattern": "ExEx",
            "time": {"start": 0.0, "end": 1.0}
        },
        "ear_clicks": [{"start": 12, "end": 100}, {"start": 200, "end": 300}],
        "note": null
    })";
    EXPECT_THROW(SwallowAnnotation::from_json(json), std::runtime_error);
}

TEST(TestSwallowAnnotationJson, TestParsingWithApneaAndNoEarClick)
{
    const auto annotation = SwallowAnnotation::from_json(R"({
        "swallow_apnea": {
            "is_ambiguous": false,
            "pattern": "ex-ex",
            "time": {"start": 0.0, "end": 1.0}
        },
        "ear_clicks": {"error": "audio-error"},
        "note": null
    })");

    ASSERT_TRUE(std::holds_alternative<SwallowApneaAnnotation>(annotation.swallow_apnea));
    auto apnea = std::get<SwallowApneaAnnotation>(annotation.swallow_apnea);
    EXPECT_FALSE(apnea.is_ambiguous);
    EXPECT_EQ(apnea.pattern, SRCPattern::ExEx);
    TimeRange exp_time_range = {0.0, 1.0};
    EXPECT_EQ(apnea.time, exp_time_range);

    ASSERT_TRUE(std::holds_alternative<EarClickError>(annotation.ear_clicks));
    EXPECT_EQ(std::get<EarClickError>(annotation.ear_clicks), EarClickError::AudioError);

    EXPECT_FALSE(annotation.note.has_value());
}

TEST(TestSwallowAnnotationJson, TestParsingWithEarClickAndNoApnea)
{
    const auto annotation = SwallowAnnotation::from_json(R"({
        "swallow_apnea": { "error": "apnea-cutoff" },
        "ear_clicks": [{"start": 12, "end": 100}, {"start": 200, "end": 300}],
        "note": null
    })");

    ASSERT_TRUE(std::holds_alternative<SwallowApneaError>(annotation.swallow_apnea));
    ASSERT_EQ(
        std::get<SwallowApneaError>(annotation.swallow_apnea), SwallowApneaError::ApneaCutoff
    );

    ASSERT_TRUE(std::holds_alternative<std::vector<TimeRange>>(annotation.ear_clicks));
    auto clicks = std::get<std::vector<TimeRange>>(annotation.ear_clicks);
    std::vector<TimeRange> exp_clicks = {{12, 100}, {200, 300}};
    EXPECT_EQ(clicks, exp_clicks);

    EXPECT_FALSE(annotation.note.has_value());
}

TEST(TestSwallowAnnotationJson, TestParsingWithoutApneaAndEarClick)
{
    const auto annotation = SwallowAnnotation::from_json(R"({
        "swallow_apnea": { "error": "apnea-cutoff" },
        "ear_clicks": { "error": "audio-error" },
        "note": null
    })");

    ASSERT_TRUE(std::holds_alternative<SwallowApneaError>(annotation.swallow_apnea));
    ASSERT_EQ(
        std::get<SwallowApneaError>(annotation.swallow_apnea), SwallowApneaError::ApneaCutoff
    );

    ASSERT_TRUE(std::holds_alternative<EarClickError>(annotation.ear_clicks));
    EXPECT_EQ(std::get<EarClickError>(annotation.ear_clicks), EarClickError::AudioError);

    EXPECT_FALSE(annotation.note.has_value());
}

TEST(TestSwallowAnnotationJson, TestParsingMissingApneaFails)
{
    const auto json = R"({
        "ear_clicks": { "error": "audio-error" },
        "note": null
    })";

    EXPECT_THROW(SwallowAnnotation::from_json(json), std::runtime_error);
}

TEST(TestSwallowAnnotationJson, TestParsingMissingEarClicksFails)
{
    const auto json = R"({
        "swallow_apnea": { "error": "apnea-cutoff" },
        "note": null
    })";

    EXPECT_THROW(SwallowAnnotation::from_json(json), std::runtime_error);
}

TEST(TestSwallowAnnotationJson, TestParsingMissingNoteFails)
{
    const auto json = R"({
        "swallow_apnea": { "error": "apnea-cutoff" },
        "ear_clicks": { "error": "audio-error" }
    })";

    EXPECT_THROW(SwallowAnnotation::from_json(json), std::runtime_error);
}

TEST(TestSwallowAnnotationJson, TestParsingInvalidErrorKeyFails)
{
    const auto json = R"({
        "swallow_apnea": { "errors": "apnea-cutoff" },
        "ear_clicks": { "error": "audio-error" },
        "note": null
    })";

    EXPECT_THROW(SwallowAnnotation::from_json(json), std::runtime_error);
}

TEST(TestSwallowAnnotationJson, TestParsingInvalidApneaErrorFails)
{
    const auto json = R"({
        "swallow_apnea": { "error": "apnea-is-cutoff" },
        "ear_clicks": { "error": "audio-error" },
        "note": null
    })";
    EXPECT_THROW(SwallowAnnotation::from_json(json), std::runtime_error);
}

TEST(TestSwallowAnnotationJson, TestParsingInvalidEarClickErrorFails)
{
    const auto json = R"({
        "swallow_apnea": { "error": "apnea-cutoff" },
        "ear_clicks": { "error": "audio-no-error" },
        "note": null
    })";
    EXPECT_THROW(SwallowAnnotation::from_json(json), std::runtime_error);
}

/**
 * Serialize an annotation to JSON object, then parse that object to another annotation object and
 * then assert that they are equal.
 */
#define ASSERT_SERIALIZE_DERIALIZE_EQ(annotation)                                                  \
    do {                                                                                           \
        std::string serialized = annotation.dump_json();                                           \
        auto annotation_deserialized = SwallowAnnotation::from_json(serialized);                   \
        ASSERT_EQ(annotation, annotation_deserialized);                                            \
    } while (0)

TEST(TestSwallowAnnotationJson, TestSerializingWithApneaAndEarClickAndNote)
{
    SwallowAnnotation annotation{
        .swallow_apnea =
            SwallowApneaAnnotation{
                .is_ambiguous = true,
                .pattern = SRCPattern::ExEx,
                .time = {0.0, 1.0},
            },
        .ear_clicks = std::vector<TimeRange>{{12, 100}, {200, 300}},
        .note = "Hello, world! β",
    };
    ASSERT_SERIALIZE_DERIALIZE_EQ(annotation);
}

TEST(TestSwallowAnnotationJson, TestSerializingWithApneaAndNoEarClickAndNote)
{
    SwallowAnnotation annotation{
        .swallow_apnea =
            SwallowApneaAnnotation{
                .is_ambiguous = true,
                .pattern = SRCPattern::ExEx,
                .time = {0.0, 1.0},
            },
        .ear_clicks = EarClickError::NoEarClick,
        .note = "Goodbye world ☺",
    };
    ASSERT_SERIALIZE_DERIALIZE_EQ(annotation);
}

TEST(TestSwallowAnnotationJson, TestSerializingWithEarClickAndNoApneaAndNoNote)
{
    SwallowAnnotation annotation{
        .swallow_apnea = SwallowApneaError::ApneaCutoff,
        .ear_clicks = std::vector<TimeRange>{{12, 100}, {200, 300}},
        .note = std::nullopt,
    };
    ASSERT_SERIALIZE_DERIALIZE_EQ(annotation);
}

TEST(TestSwallowAnnotationJson, TestSerializingWithoutApneaAndEarClickAndNote)
{
    SwallowAnnotation annotation{
        .swallow_apnea = SwallowApneaError::NoSwallow,
        .ear_clicks = EarClickError::NoEarClick,
        .note = std::nullopt,
    };
    ASSERT_SERIALIZE_DERIALIZE_EQ(annotation);
}
