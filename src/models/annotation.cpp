#include "annotation.hpp"

#include "models/annotation-json.hpp"

#include <nlohmann/json.hpp>

namespace recap::labeller::models {

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

}; // namespace recap::labeller::models
