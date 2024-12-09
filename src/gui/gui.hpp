#ifndef RECAP_LABELLER_GUI_HPP
#define RECAP_LABELLER_GUI_HPP

#include "app/app.hpp"

#include <memory>

namespace recap::labeller::gui {

class Gui {
public:
    Gui() = default;
    virtual ~Gui() = default;

    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;
    Gui(Gui&&) = delete;
    Gui& operator=(Gui&&) = delete;

    static void set_scaling_factor(float scaling_factor);

    virtual void draw() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool ready_to_stop() const = 0;
};

class LabellerGui final : public Gui {
public:
    explicit LabellerGui(recap::labeller::app::App& app);
    ~LabellerGui() override;

    LabellerGui(const LabellerGui&) = delete;
    LabellerGui& operator=(const LabellerGui&) = delete;
    LabellerGui(LabellerGui&&) = delete;
    LabellerGui& operator=(LabellerGui&&) = delete;

    void draw() override;
    void stop() override;
    [[nodiscard]] bool ready_to_stop() const override;

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}; // namespace recap::labeller::gui

#endif //  RECAP_LABELLER_GUI_HPP
