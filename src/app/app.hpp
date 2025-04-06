#ifndef RECAP_LABELLER_APP_APP_INCLUDE_HPP
#define RECAP_LABELLER_APP_APP_INCLUDE_HPP

#include "app/annotation-store.hpp"
#include "app/labeller.hpp"
#include "util/observable.hpp"

#include <filesystem>
#include <memory>
#include <optional>

namespace recap::labeller::app {

class App {
public:
    using LabellerUpdateObservable = Observable<Labeller&>;

    explicit App(LabellingConfig config);

    LabellerUpdateObservable::Observer
    subscribe_labeller_update(LabellerUpdateObservable::Function&& function)
    {
        return m_labeller_update_observable.subscribe(function);
    }

    [[nodiscard]] const std::optional<std::string>& critical_error() const
    {
        return m_critical_error;
    }

    void set_critical_error(std::string s) { m_critical_error.emplace(std::move(s)); }

private:
    void create_new_labeller();

    LabellerUpdateObservable m_labeller_update_observable;
    std::optional<std::string> m_critical_error = std::nullopt;

    LabellingConfig m_config;
    std::optional<SwallowAnnotationStore::ErrorObservable::Observer>
        m_annotation_store_error_observer;
    std::filesystem::path m_annotations_path;
    std::filesystem::path m_labelling_tasks_path;
    std::filesystem::path m_data_dir;
    std::filesystem::path m_suggested_annotations_path;
    std::unique_ptr<Labeller> m_labeller;
    std::optional<std::string> m_labeller_load_error;
};

} // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_APP_INCLUDE_HPP
