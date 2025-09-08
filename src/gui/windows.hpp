#ifndef RECAP_LABELLER_GUI_WINDOWS_HPP_INCLUDE
#define RECAP_LABELLER_GUI_WINDOWS_HPP_INCLUDE

// This is a bit hacky. Put all window names here so they can be arranged in the initial dockspace
// configuration in Gui::draw.

namespace recap::labeller::gui {

inline const char *const TaskListWindowId = "Labelling tasks##window";
inline const char *const TaskViewDataPlotsWindowId = "Plots##task_view_data_plots_window";
inline const char *const TaskViewLabelControlsWindowId =
    "Annotations##task_view_labels_control_window";

}; // namespace recap::labeller::gui

#endif // RECAP_LABELLER_GUI_WINDOWS_HPP_INCLUDE
