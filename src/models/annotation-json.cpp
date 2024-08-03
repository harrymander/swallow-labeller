#include "annotation-json.hpp"

#include "models/annotation.hpp"
#include "models/time-range.hpp"
#include "nlohmann/detail/exceptions.hpp"
#include "util/variant-visitor.hpp"

#include <nlohmann/json.hpp>

#include <array>

NLOHMANN_JSON_NAMESPACE_BEGIN

/**
 * These converters are adapted from:
 * https://json.nlohmann.me/features/arbitrary_types/#how-do-i-convert-third-party-types
 */

/**
 * JSON parser/serializer for std::variant<T, E>, where T and E can be serialized/deserialized
 * to/from nlohmann::json.
 *
 * E will be serialized as an object with a single field "error" mapping to the serialized object;
 * therefore, T cannot be an object with a single field that is "error".
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

/**
 * JSON parser/serializer for std::optional<T> where T can be serialized/deserialized to/from
 * nlohmann::json. Uses JSON null to represent std::nullopt.
 */
template <typename T> struct adl_serializer<std::optional<T>> {
    static void to_json(json& j, const std::optional<T>& val)
    {
        if (val.has_value()) {
            j = *val;
        } else {
            j = nullptr;
        }
    }

    static void from_json(const json& j, std::optional<T>& val)
    {
        if (j.is_null()) {
            val = std::nullopt;
        } else {
            val = j.template get<T>();
        }
    }
};

NLOHMANN_JSON_NAMESPACE_END

namespace recap::labeller::models {

namespace {

/**
 * Alternative to NLOHMANN_JSON_SERIALIZE_ENUM, which default-constructs the enum if it cannot
 * be converted to a matching value rather than giving an error.
 *
 * Assumes that all enum items are accounted for in the array, otherwise bad things will happen.
 *
 * Uses a linear search to match enum, which is obviously inefficient for large enums.
 */
template <typename E, std::size_t N> class EnumStrConverter {
public:
    explicit constexpr EnumStrConverter(std::array<std::pair<E, const char *>, N> items) noexcept :
        items{std::move(items)}
    {}

    void to_json(nlohmann::json& j, E e) const
    {
        const auto it = std::find_if(items.begin(), items.end(), [e](const auto& item) {
            return item.first == e;
        });

        // just set without checking iterator is valid, since we assume all enum items are included
        // in array
        j = it->second;
    }

    void from_json(const nlohmann::json& j, E& e) const
    {
        const auto str = j.get<std::string_view>();
        const auto it = std::find_if(items.begin(), items.end(), [str](const auto& item) {
            return item.second == str;
        });
        if (it == items.end()) {
            // FIXME: using internal details of JSON library to throw error... The 403 code is for
            // "key not found" errors:
            // https://json.nlohmann.me/home/exceptions/#jsonexceptionout_of_range403
            throw nlohmann::detail::out_of_range::create(
                403, "Invalid enum value: " + std::string(str), nullptr
            );
        }
        e = it->first;
    }

private:
    std::array<std::pair<E, const char *>, N> items;
};

constexpr EnumStrConverter SRCPatternStrConverter{std::array{
    std::make_pair(SRCPattern::ExEx, "ex-ex"),
    std::make_pair(SRCPattern::ExIn, "ex-in"),
    std::make_pair(SRCPattern::InEx, "in-ex"),
    std::make_pair(SRCPattern::InIn, "in-in"),
}};

constexpr EnumStrConverter ApneaErrorStrConverter{std::array{
    std::make_pair(SwallowApneaError::FlowError, "flow-error"),
    std::make_pair(SwallowApneaError::NoSwallow, "no-swallow"),
    std::make_pair(SwallowApneaError::ApneaCutoff, "apnea-cutoff"),
}};

constexpr EnumStrConverter EarClickErrorStrConverter{std::array{
    std::make_pair(EarClickError::NoEarClick, "no-ear-click"),
    std::make_pair(EarClickError::AudioError, "audio-error"),
}};

}; // namespace

// A macro could possibly be avoided here by patching into the nlohman:: namespace and defining an
// adl_serializer (see above)
#define DEFINE_JSON_ENUM_CONVERTERS(enum_type, converter)                                          \
    void to_json(nlohmann::json& j, const enum_type& e)                                            \
    {                                                                                              \
        (converter).to_json(j, e);                                                                 \
    }                                                                                              \
    void from_json(const nlohmann::json& j, enum_type& e)                                          \
    {                                                                                              \
        (converter).from_json(j, e);                                                               \
    }

DEFINE_JSON_ENUM_CONVERTERS(SRCPattern, SRCPatternStrConverter);
DEFINE_JSON_ENUM_CONVERTERS(SwallowApneaError, ApneaErrorStrConverter);
DEFINE_JSON_ENUM_CONVERTERS(EarClickError, EarClickErrorStrConverter);
#undef DEFINE_JSON_ENUM_CONVERTERS

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TimeRange, start, end);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    SwallowApneaAnnotation, is_ambiguous, pattern, time, non_respiratory_flow
);

void to_json(nlohmann::json& j, const SwallowAnnotation& annotation)
{
    j = nlohmann::json{
        {"swallow_apnea", annotation.swallow_apnea},
        {"ear_clicks", annotation.ear_clicks},
        {"note", annotation.note},
    };
}

void from_json(const nlohmann::json& j, SwallowAnnotation& annotation)
{
    j.at("swallow_apnea").get_to(annotation.swallow_apnea);
    j.at("ear_clicks").get_to(annotation.ear_clicks);
    j.at("note").get_to(annotation.note);
}

}; // namespace recap::labeller::models
