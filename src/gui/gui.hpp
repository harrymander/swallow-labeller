#ifndef RECAP_LABELLER_GUI_HPP
#define RECAP_LABELLER_GUI_HPP

#include <memory>

namespace recap::labeller::gui {

class Gui {
public:
    Gui();
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
