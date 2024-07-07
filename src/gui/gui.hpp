#ifndef RECAP_LABELLER_GUI_HPP
#define RECAP_LABELLER_GUI_HPP

#include "../app.hpp"
#include "../old-gui.hpp"

#include <filesystem>
#include <optional>

namespace recap::labeller::gui {

class Gui {
public:
    explicit Gui(recap::labeller::app::App& app);
    ~Gui();

    Gui(const Gui&) = delete;
    Gui operator=(const Gui&) = delete;
    Gui(Gui&&) = delete;
    Gui operator=(Gui&&) = delete;

    void render();
    void stop();

    [[nodiscard]] bool ready_to_stop() const { return m_ready_to_stop; }

private:
    std::optional<std::filesystem::path> m_ini_path = std::nullopt;
    bool m_stop_requested = false;
    bool m_ready_to_stop = false;

    recap::labeller::app::App& m_app;

    recap::labeller::gui::OldGui old_gui;
};

}; // namespace recap::labeller::gui

#endif //  RECAP_LABELLER_GUI_HPP
