#ifndef RECAP_LABELLER_GUI_HPP
#define RECAP_LABELLER_GUI_HPP

#include "../app.hpp"
#include "../old-gui.hpp"

#include <optional>
#include <string>

namespace recap::labeller::gui {

class Gui {
public:
    explicit Gui(recap::labeller::app::App& app);
    ~Gui();

    Gui(const Gui&) = delete;
    Gui operator=(const Gui&) = delete;
    Gui(Gui&&) = delete;
    Gui operator=(Gui&&) = delete;

    void draw();
    void stop();
    static void set_scaling_factor(float scaling_factor);

    [[nodiscard]] bool ready_to_stop() const { return m_ready_to_stop; }

private:
    bool m_first_draw = true;
    std::optional<std::string> m_ini_path = std::nullopt;
    bool m_stop_requested = false;
    bool m_ready_to_stop = false;
    bool m_show_imgui_demo_window = false;
    bool m_show_implot_demo_window = false;
    bool m_show_imgui_metrics = false;

#if NDEBUG
    bool m_show_debug_info = false;
#else
    bool m_show_debug_info = true;
#endif

    recap::labeller::app::App& m_app;
    recap::labeller::gui::OldGui old_gui;

    void setup_dockspace() const;
    void draw_main_window();
    void draw_menu_bar();
    static void draw_debug_info();
};

}; // namespace recap::labeller::gui

#endif //  RECAP_LABELLER_GUI_HPP
