#ifndef RECAP_LABELLER_GUI_HPP
#define RECAP_LABELLER_GUI_HPP

#include "app/annotation-store.hpp"
#include "models/task-info.hpp"

#include <memory>
#include <vector>

namespace recap::labeller::gui {

class Gui {
public:
    Gui(const std::vector<models::SwallowTaskInfo>& tasks,
        SwallowAnnotationStore& annotation_store,
        const std::filesystem::path& data_dir);
    ~Gui();

    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;
    Gui(Gui&&) = delete;
    Gui& operator=(Gui&&) = delete;

    void draw();
    void stop();
    static void set_scaling_factor(float scaling_factor);

    [[nodiscard]] bool ready_to_stop() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}; // namespace recap::labeller::gui

#endif //  RECAP_LABELLER_GUI_HPP
