#ifndef RECAP_LABELLER_UTIL_JSON_OPTIONAL_HPP
#define RECAP_LABELLER_UTIL_JSON_OPTIONAL_HPP

#include <nlohmann/json.hpp>

#include <optional>

NLOHMANN_JSON_NAMESPACE_BEGIN

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

#endif // RECAP_LABELLER_UTIL_JSON_OPTIONAL_HPP
