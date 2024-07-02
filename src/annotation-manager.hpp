#ifndef INCLUDE_RECAP_LABELLER_ANNOTATION_MANAGER_HPP
#define INCLUDE_RECAP_LABELLER_ANNOTATION_MANAGER_HPP

#include "labelling-task.hpp"

#include <filesystem>
#include <ostream>

namespace recap::labeller::annotation_manager {

class AnnotationManager {
public:
    AnnotationManager(
        std::filesystem::path path, recap::labeller::task::AnnotationsMap annotations
    );

    /**
     * Retrieves annotation with annotation_id and returns pointer to it, or nullptr if no
     * annotation with that ID
     */
    [[nodiscard]] const recap::labeller::task::SwallowAnnotation *
    get_annotation(const std::string& annotation_id) const;

    /**
     * Returns true if there is an existing annotation with the same id and it is identical to
     * annotation, otherwise false.
     */
    [[nodiscard]] bool annotation_saved(
        const std::string& id, const recap::labeller::task::SwallowAnnotation& annotation
    ) const;

    /**
     * Adds or updates annotation with given annotation_id. Returns true if annotation added or
     * updated.
     */
    [[nodiscard]] bool add_annotation(
        const std::string& annotation_id, recap::labeller::task::SwallowAnnotation annotation
    );

    void remove_annotation(const std::string& annotation_id);

    /**
     * Writes annotations to file.
     *
     * Raises std::runtime_error if there is an error writing to file.
     */
    void sync_to_file() const;

private:
    std::filesystem::path path;
    recap::labeller::task::AnnotationsMap annotations;
    std::string path_str;
};

}; // namespace recap::labeller::annotation_manager

#endif // INCLUDE_RECAP_LABELLER_ANNOTATION_MANAGER_HPP
