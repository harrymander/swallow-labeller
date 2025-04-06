#ifndef RECAP_LABELLER_GUI_LABELLER_VIEW_HPP
#define RECAP_LABELLER_GUI_LABELLER_VIEW_HPP

#include "app/labeller.hpp"

#include <memory>

namespace recap::labeller::gui {

class LabellerView {
public:
    explicit LabellerView(recap::labeller::app::Labeller& labeller);
    ~LabellerView();

    LabellerView(const LabellerView&) = delete;
    LabellerView& operator=(const LabellerView&) = delete;
    LabellerView(LabellerView&&) = delete;
    LabellerView& operator=(LabellerView&&) = delete;

    void draw();
    void stop();
    static void set_scaling_factor(float scaling_factor);

    [[nodiscard]] bool ready_to_stop() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}; // namespace recap::labeller::gui

#endif //  RECAP_LABELLER_GUI_LABELLER_VIEW_HPP
