#include "annotation-json.hpp"

#include "models/annotation.hpp"
#include "models/time-range.hpp"
#include "nlohmann/detail/exceptions.hpp"

#include <nlohmann/json.hpp>

#include <array>

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
            constexpr int KeyNotFoundError = 403;
            throw nlohmann::detail::out_of_range::create(
                KeyNotFoundError, "Invalid enum value: " + std::string(str), nullptr
            );
        }
        e = it->first;
    }

private:
    std::array<std::pair<E, const char *>, N> items;
};

constexpr EnumStrConverter SRCPatternStrConverter{std::array{
    std::make_pair(SrcPattern::ExEx, "ex-ex"),
    std::make_pair(SrcPattern::ExIn, "ex-in"),
    std::make_pair(SrcPattern::InEx, "in-ex"),
    std::make_pair(SrcPattern::InIn, "in-in"),
}};

}; // namespace

void to_json(nlohmann::json& j, const SrcPattern& pattern)
{
    SRCPatternStrConverter.to_json(j, pattern);
}

void from_json(const nlohmann::json& j, SrcPattern& pattern)
{
    SRCPatternStrConverter.from_json(j, pattern);
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TimeRange, start, end);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SwallowApneaAnnotation, is_ambiguous, pattern, time);

void to_json(nlohmann::json& j, const SwallowAnnotation& annotation)
{
    j = nlohmann::json{
        {"swallow_apneas", annotation.swallow_apneas},
        {"ear_clicks", annotation.ear_clicks},
        {"non_respiratory_flow_events", annotation.non_respiratory_flow_events},
        {"notes", annotation.notes},
    };
}

void from_json(const nlohmann::json& j, SwallowAnnotation& annotation)
{
    j.at("swallow_apneas").get_to(annotation.swallow_apneas);
    j.at("ear_clicks").get_to(annotation.ear_clicks);
    j.at("non_respiratory_flow_events").get_to(annotation.non_respiratory_flow_events);
    j.at("notes").get_to(annotation.notes);
}

}; // namespace recap::labeller::models
