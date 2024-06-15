#ifndef INCLUDE_RECAP_LABELLER_GUI_HPP
#define INCLUDE_RECAP_LABELLER_GUI_HPP

#include "data.hpp"
#include "labelling-task.hpp"

#include <filesystem>
#include <vector>

namespace recap::labeller::gui {

class Gui {
public:
    Gui(const std::vector<labelling_task::SwallowLabellingTask>& tasks,
        const std::filesystem::path& data_dir);
    ~Gui();

    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;
    Gui(Gui&&) = delete;
    Gui& operator=(Gui&&) = delete;

    // Returns False if should quit
    bool draw();
    void stop();

private:
    std::filesystem::path data_dir;
    std::vector<labelling_task::SwallowLabellingTask> tasks;

    plot::SwallowTaskData load_task(const labelling_task::SwallowLabellingTask& task) const;
    void draw_task_selector(plot::SwallowTaskData& task) const;
    void draw_plot();
    void draw_window_contents();

    bool to_close = false;
};

}; // namespace recap::labeller::gui

#endif // INCLUDE_RECAP_LABELLER_GUI_HPP
