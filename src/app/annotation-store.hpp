#ifndef INCLUDE_RECAP_LABELLER_ANNOTATION_STORE_HPP
#define INCLUDE_RECAP_LABELLER_ANNOTATION_STORE_HPP

#include "models/annotation.hpp"
#include "util/observable.hpp"

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <filesystem>
#include <istream>
#include <map>

namespace recap::labeller {

using UtcClock = std::chrono::system_clock;
using UtcTimePoint = UtcClock::time_point;

class SwallowAnnotationResult {
public:
    SwallowAnnotationResult() = default;
    explicit SwallowAnnotationResult(models::SwallowAnnotation result);

    [[nodiscard]] const models::SwallowAnnotation& result() const { return m_result; }

    [[nodiscard]] UtcTimePoint created_time() const { return m_created_time; }

    [[nodiscard]] const std::optional<UtcTimePoint>& last_modified_time() const
    {
        return m_last_modified_time;
    }

    void update_result(models::SwallowAnnotation result);

private:
    friend void from_json(const nlohmann::json&, SwallowAnnotationResult&);

    models::SwallowAnnotation m_result;
    UtcTimePoint m_created_time;
    std::optional<UtcTimePoint> m_last_modified_time = std::nullopt;
};

using SwallowAnnotationResultMap = std::map<std::string, SwallowAnnotationResult>;

// Throws std::runtime_error on parse error
SwallowAnnotationResultMap load_swallow_annotation_result_map_json(std::istream& stream);

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
    [[nodiscard]] const models::SwallowAnnotation *get_annotation(const std::string& id) const
    {
        const auto it = m_annotations.find(id);
        if (it == m_annotations.end()) {
            return nullptr;
        }
        return &it->second.result();
    }

    [[nodiscard]] bool has_annotation(const std::string& id) const
    {
        return m_annotations.contains(id);
    }

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

    ErrorObservable::Observer subscribe_sync_error(ErrorObservable::Function function)
    {
        return m_error_observable.subscribe(std::move(function));
    }

    /**
     * Writes annotations to path.
     *
     * Raises std::runtime_error if there is an error writing to file; **will not notify any
     * observers if there is an error**
     */
    void sync_to_file(const std::filesystem::path& path) const;

    /**
     * Writes annotations to file at path() (the path passed to constructor).
     *
     * Equivalent to sync_to_file(path())
     */
    void sync_to_file() const { sync_to_file(m_path); }

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
    ErrorObservable m_error_observable;
    SwallowAnnotationResultMap m_annotations;

    void sync_to_file_notify() const;
};

}; // namespace recap::labeller

#endif // INCLUDE_RECAP_LABELLER_ANNOTATION_STORE_HPP
