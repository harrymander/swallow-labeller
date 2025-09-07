#ifndef INCLUDE_RECAP_LABELLER_PLATFORM_HPP
#define INCLUDE_RECAP_LABELLER_PLATFORM_HPP

#include "app/annotation-store.hpp"
#include "models/task-info.hpp"

#include <filesystem>
#include <vector>

namespace recap::labeller::platform {

int run(
    const std::vector<models::SwallowTaskInfo>& tasks,
    SwallowAnnotationStore& annotation_store,
    const std::filesystem::path& data_dir
);

};

#endif // INCLUDE_RECAP_LABELLER_PLATFORM_HPP
