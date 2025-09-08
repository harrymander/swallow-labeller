#ifndef INCLUDE_RECAP_LABELLER_GUI_TASK_FILTER_HPP
#define INCLUDE_RECAP_LABELLER_GUI_TASK_FILTER_HPP

#include "models/annotation.hpp"
#include "models/task-info.hpp"

#include <list>
#include <memory>

namespace recap::labeller::gui {

class TaskFilter {
public:
    void draw(const char *id);

    [[nodiscard]] bool
    passes(const models::SwallowTaskInfo& task, const models::SwallowAnnotation *annotation) const;

    [[nodiscard]] bool enabled() const { return !m_filters.empty(); }

    class Filter {
    public:
        explicit Filter(const char *name) : m_name(name) {}

        virtual ~Filter() = default;

        virtual void draw() = 0;

        [[nodiscard]] virtual bool passes(
            const models::SwallowTaskInfo& task, const models::SwallowAnnotation *annotation
        ) const = 0;

        const char *name() const { return m_name; }

    private:
        const char *m_name;
    };

private:
    using FilterList = std::list<std::unique_ptr<Filter>>;

    void draw_filters();
    void draw_new_filter_control();

    void reset_changing_filter();
    void set_changing_filter(FilterList::const_iterator it);

    FilterList m_filters;
    std::optional<FilterList::const_iterator> m_changing_filter_it = std::nullopt;
    bool m_changing_filter_appearing = false;
    bool m_and = true;
};

}; // namespace recap::labeller::gui

#endif // INCLUDE_RECAP_LABELLER_GUI_TASK_FILTER_HPP
