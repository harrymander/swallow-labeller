#ifndef INCLUDE_RECAP_LABELLER_ANNOTATION_MANAGER_HPP
#define INCLUDE_RECAP_LABELLER_ANNOTATION_MANAGER_HPP

#include "labelling-task.hpp"

#include <filesystem>
#include <ostream>

namespace recap::labeller::annotation_manager {

class AnnotationManager {
public:
    AnnotationManager(std::filesystem::path path, labelling_task::AnnotationsMap annotations);

    /**
     * Retrieves annotation with id and returns pointer to it, or nullptr if no annotation with that
     * ID
     */
    [[nodiscard]] const labelling_task::SwallowAnnotation *get_annotation(const std::string& id
    ) const;

    /**
     * Adds or updates annotation with given id.
     */
    void add_annotation(const std::string& id, labelling_task::SwallowAnnotation annotation);

    /**
     * Writes annotations to file.
     *
     * Raises std::runtime_error if there is an error writing to file.
     */
    void sync_to_file() const;

private:
    std::filesystem::path path;
    labelling_task::AnnotationsMap annotations;
    std::string path_str;
};

}; // namespace recap::labeller::annotation_manager

#endif // INCLUDE_RECAP_LABELLER_ANNOTATION_MANAGER_HPP
