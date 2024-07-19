#ifndef INCLUDE_RECAP_LABELLER_ANNOTATION_STORE_HPP
#define INCLUDE_RECAP_LABELLER_ANNOTATION_STORE_HPP

#include "models/annotation.hpp"

#include <filesystem>

namespace recap::labeller {

class SwallowAnnotationStore {
public:
    SwallowAnnotationStore(std::filesystem::path path, models::SwallowAnnotationsMap annotations);

    [[nodiscard]] std::size_t size() const { return annotations.size(); }

    /**
     * Retrieves annotation with annotation_id and returns pointer to it, or nullptr if no
     * annotation with that ID
     */
    [[nodiscard]] const models::SwallowAnnotation *get_annotation(const std::string& annotation_id
    ) const;

    [[nodiscard]] bool has_annotation(const std::string& id) const
    {
        return annotations.contains(id);
    }

    /**
     * Returns true if there is an existing annotation with the same id and it is identical to
     * annotation, otherwise false.
     */
    [[nodiscard]] bool
    annotation_saved(const std::string& id, const models::SwallowAnnotation& annotation) const;

    /**
     * Adds or updates annotation with given annotation_id. Returns true if annotation added or
     * updated.
     */
    [[nodiscard]] bool
    add_annotation(const std::string& annotation_id, models::SwallowAnnotation annotation);

    void remove_annotation(const std::string& annotation_id);

    /**
     * Writes annotations to file.
     *
     * Raises std::runtime_error if there is an error writing to file.
     */
    void sync_to_file() const;

private:
    std::filesystem::path path;
    models::SwallowAnnotationsMap annotations;
    std::string path_str;
};

}; // namespace recap::labeller

#endif // INCLUDE_RECAP_LABELLER_ANNOTATION_STORE_HPP
