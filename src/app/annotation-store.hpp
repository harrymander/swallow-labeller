#ifndef INCLUDE_RECAP_LABELLER_ANNOTATION_STORE_HPP
#define INCLUDE_RECAP_LABELLER_ANNOTATION_STORE_HPP

#include "models/annotation.hpp"
#include "util/observable.hpp"

#include <filesystem>

namespace recap::labeller {

class SwallowAnnotationStoreMap;

class SwallowAnnotationStore {
public:
    /**
     * Raises std::runtime_error on parse error if existing is non-null
     */
    SwallowAnnotationStore(std::filesystem::path path, std::istream *existing);

    ~SwallowAnnotationStore();
    SwallowAnnotationStore(SwallowAnnotationStore&&) noexcept;
    SwallowAnnotationStore& operator=(SwallowAnnotationStore&&) noexcept;

    SwallowAnnotationStore(const SwallowAnnotationStore&) = delete;
    SwallowAnnotationStore& operator=(const SwallowAnnotationStore&) = delete;

    /**
     * Retrieves annotation with annotation_id and returns pointer to it, or nullptr if no
     * annotation with that ID
     */
    [[nodiscard]] const models::SwallowAnnotation *get_annotation(const std::string& annotation_id
    ) const;

    [[nodiscard]] bool has_annotation(const std::string& id) const;

    /**
     * Returns true if there is an existing annotation with the same id and it is identical to
     * annotation, otherwise false.
     */
    [[nodiscard]] bool
    annotation_saved(const std::string& id, const models::SwallowAnnotation& annotation) const;

    /**
     * The following two functions can write to file. If there is an error in writing, will notify
     * any subscribers with error message.
     */

    /**
     * Adds or updates annotation with given annotation_id. If annotation is added or updated,
     * syncs to file.
     */
    void add_annotation(const std::string& annotation_id, models::SwallowAnnotation annotation);

    /**
     * Removes annotation with given annotation_id, if it exists. Syncs to file if there was an
     * annotation removed.
     */
    void remove_annotation(const std::string& annotation_id);

    using ErrorObservable = Observable<const std::string&>;

    ErrorObservable::Observer subscribe_sync_error(ErrorObservable::Function&& function)
    {
        return m_error_observable.subscribe(function);
    }

    /**
     * Forces writing annotations to file.
     *
     * Raises std::runtime_error if there is an error writing to file; **will not notify any
     * observers if there is an error**
     */
    void sync_to_file() const;

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
    std::string path_str;
    ErrorObservable m_error_observable;
    std::unique_ptr<SwallowAnnotationStoreMap> m_annotations;

    void sync_to_file_notify() const;
};

}; // namespace recap::labeller

#endif // INCLUDE_RECAP_LABELLER_ANNOTATION_STORE_HPP
