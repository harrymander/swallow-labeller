#ifndef INCLUDE_RECAP_LABELLER_PLATFORM_HPP
#define INCLUDE_RECAP_LABELLER_PLATFORM_HPP

#include "app/annotation-store.hpp"
#include "models/task-info.hpp"

#include <vector>

namespace recap::labeller::platform {

int run(
    const std::vector<models::SwallowTaskInfo>& tasks, SwallowAnnotationStore& annotation_store
);

};

#endif // INCLUDE_RECAP_LABELLER_PLATFORM_HPP
