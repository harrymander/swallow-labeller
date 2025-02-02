#ifndef INCLUDE_RECAP_LABELLER_GUI_TASK_FILTER_HPP
#define INCLUDE_RECAP_LABELLER_GUI_TASK_FILTER_HPP

#include "app/app.hpp"
#include "models/annotation.hpp"

#include <cstddef>
#include <list>
#include <memory>
#include <utility>

namespace recap::labeller::gui {

class TaskFilter {
public:
    void draw(const char *id);

    [[nodiscard]] bool passes(
        const app::SwallowLabellingTask& task, const models::SwallowAnnotation *annotation
    ) const;

    class Filter {
    public:
        virtual ~Filter() = default;

        virtual void draw() = 0;

        [[nodiscard]] virtual bool passes(
            const app::SwallowLabellingTask& task, const models::SwallowAnnotation *annotation
        ) const = 0;
    };

private:
    using FilterList = std::list<std::pair<const char *, std::unique_ptr<Filter>>>;

    void draw_filters();
    void draw_filter(const TaskFilter::FilterList::iterator& it);
    void draw_new_filter_control();

    FilterList m_filters;
    std::optional<FilterList::const_iterator> m_changing_filter = std::nullopt;
    std::size_t m_new_filter_index = 0;
    bool m_and = true;
};

}; // namespace recap::labeller::gui

#endif // INCLUDE_RECAP_LABELLER_GUI_TASK_FILTER_HPP
