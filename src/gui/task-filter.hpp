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
    void draw_filters();
    void draw_new_filter_control();

    std::list<std::pair<const char *, std::unique_ptr<Filter>>> m_filters;
    std::size_t m_new_filter_index = 0;
    bool m_and = true;
};

}; // namespace recap::labeller::gui

#endif // INCLUDE_RECAP_LABELLER_GUI_TASK_FILTER_HPP
