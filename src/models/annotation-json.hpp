#ifndef RECAP_LABELLER_ANNOTATION_JSON_HPP_INCLUDE
#define RECAP_LABELLER_ANNOTATION_JSON_HPP_INCLUDE

#include "models/annotation.hpp"

#include <nlohmann/json_fwd.hpp>

namespace recap::labeller::models {

void to_json(nlohmann::json& j, const SwallowAnnotation& annotation);
void from_json(const nlohmann::json& j, SwallowAnnotation& annotation);

}; // namespace recap::labeller::models

#endif // RECAP_LABELLER_ANNOTATION_JSON_HPP_INCLUDE
