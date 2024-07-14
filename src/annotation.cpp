#include "annotation.hpp"

#include "variant-visitor.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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
    explicit EnumStrConverter(std::array<std::pair<E, std::string_view>, N> items) noexcept :
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
            throw std::runtime_error("Invalid enum value: " + std::string(str));
        }
        e = it->first;
    }

private:
    std::array<std::pair<E, std::string_view>, N> items;
};

static const EnumStrConverter<SRCPattern, 4> SRCPatternStrConverter({{
    {SRCPattern::ExEx, "ex-ex"},
    {SRCPattern::ExIn, "ex-in"},
    {SRCPattern::InEx, "in-ex"},
    {SRCPattern::InIn, "in-in"},
}});

static const EnumStrConverter<ApneaError, 3> ApneaErrorStrConverter({{
    {ApneaError::FlowError, "flow-error"},
    {ApneaError::NoSwallow, "no-swallow"},
    {ApneaError::ApneaCutoff, "apnea-cutoff"},
}});

static const EnumStrConverter<EarClickError, 2> EarClickErrorStrConverter({{
    {EarClickError::NoEarClick, "no-ear-click"},
    {EarClickError::AudioError, "audio-error"},
}});

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
DEFINE_JSON_ENUM_CONVERTERS(ApneaError, ApneaErrorStrConverter);
DEFINE_JSON_ENUM_CONVERTERS(EarClickError, EarClickErrorStrConverter);
#undef DEFINE_JSON_ENUM_CONVERTERS

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TimeRange, start, end);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SwallowApneaAnnotation, is_ambiguous, pattern, time);

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

SwallowAnnotation SwallowAnnotation::from_json(std::string_view str)
{
    try {
        const auto json = nlohmann::json::parse(str);
        return json.template get<SwallowAnnotation>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }
}

std::string SwallowAnnotation::dump_json() const
{
    return nlohmann::json(*this).dump();
}

SwallowAnnotationsMap load_swallow_annotations_map_json(std::istream& stream)
{
    try {
        return nlohmann::json::parse(stream).template get<SwallowAnnotationsMap>();
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }
}

void dump_swallow_annotations_map_json(
    std::ostream& os, const SwallowAnnotationsMap& map, int indent
)
{
    nlohmann::json json = map;
    os << json.dump(indent);
}

}; // namespace recap::labeller
