#ifndef INCLUDE_RECAP_LABELLER_GUI_HPP
#define INCLUDE_RECAP_LABELLER_GUI_HPP

#include "annotation-manager.hpp"
#include "labelling-task.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace recap::labeller::gui {

class OldGui {
public:
    OldGui(
        std::vector<recap::labeller::SwallowTaskInfo> tasks,
        annotation_manager::AnnotationManager& annotation_mgr,
        std::filesystem::path data_dir,
        bool shuffle = true
    );
    ~OldGui();

    OldGui(const OldGui&) = delete;
    OldGui(OldGui&&) = delete;
    OldGui operator=(OldGui&&) = delete;
    OldGui operator=(const OldGui&) = delete;

    // Returns False when ready to quit
    bool draw();
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}; // namespace recap::labeller::gui

#endif // INCLUDE_RECAP_LABELLER_GUI_HPP
