#ifndef INCLUDE_RECAP_LABELLER_GUI_HPP
#define INCLUDE_RECAP_LABELLER_GUI_HPP

#include "data.hpp"
#include "labelling-task.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace recap::labeller::gui {

class Gui {
public:
    Gui(const std::vector<labelling_task::SwallowLabellingTask>& tasks,
        const std::filesystem::path& data_dir);
    ~Gui();

    // Returns False when ready to quit
    bool draw();
    void stop();

    void set_scaling_factor(float scaling_factor);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}; // namespace recap::labeller::gui

#endif // INCLUDE_RECAP_LABELLER_GUI_HPP
