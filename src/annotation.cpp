#include "annotation.hpp"

#include "variant-visitor.hpp"

#include <nlohmann/json.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN

/**
 * JSON parser/serializer for std::variant<T, E>, where T and E can be serialized/deserialized
 * to/from nlohmann::json.
 *
 * E will be serialized as an object with a single field "error" mapping to the serialized object;
 * therefore, T cannot be an object with a single field that is "error".
 *
 * Adapted from:
 * https://json.nlohmann.me/features/arbitrary_types/#how-do-i-convert-third-party-types
 */
template <typename T, typename E> struct adl_serializer<std::variant<T, E>> {
    using Variant = std::variant<T, E>;

    static void to_json(json& j, const Variant& val)
    {
        recap::labeller::VariantVisitor{
            [&j](const T& val) { j = val; },
            [&j](const E& err) { j["error"] = err; },
        }(val);
    }

    static void from_json(const json& j, Variant& val)
    {
        if (j.is_object() && j.contains("error") && j.size() == 1) {
            val = j.at("error").template get<E>();
        } else {
            val = j.template get<T>();
        }
    }
};

NLOHMANN_JSON_NAMESPACE_END

namespace recap::labeller {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TimeRange, start, end);

NLOHMANN_JSON_SERIALIZE_ENUM(
    SRCPattern,
    {
        {SRCPattern::ExEx, "ex-ex"},
        {SRCPattern::ExIn, "ex-in"},
        {SRCPattern::InEx, "in-ex"},
        {SRCPattern::InIn, "in-in"},
    }
);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SwallowApneaAnnotation, is_ambiguous, pattern, time);

NLOHMANN_JSON_SERIALIZE_ENUM(
    ApneaError,
    {
        {ApneaError::FlowError, "flow-error"},
        {ApneaError::NoSwallow, "no-swallow"},
        {ApneaError::ApneaCutoff, "apnea-cutoff"},
    }
);

NLOHMANN_JSON_SERIALIZE_ENUM(
    EarClickError,
    {
        {EarClickError::NoEarClick, "no-ear-click"},
        {EarClickError::AudioError, "audio-error"},
    }
);

void to_json(nlohmann::json& j, const SwallowAnnotation& annotation)
{
    j = nlohmann::json{
        {"swallow_apnea", annotation.swallow_apnea},
        {"ear_clicks", annotation.ear_clicks},
    };
}

void from_json(const nlohmann::json& j, SwallowAnnotation& annotation)
{
    j.at("swallow_apnea").get_to(annotation.swallow_apnea);
    j.at("ear_clicks").get_to(annotation.ear_clicks);
}

}; // namespace recap::labeller
