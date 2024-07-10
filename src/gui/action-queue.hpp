#ifndef RECAP_LABELLER_GUI_ACTION_QUEUE_HPP_INCLUDE
#define RECAP_LABELLER_GUI_ACTION_QUEUE_HPP_INCLUDE

#include <functional>
#include <list>

namespace recap::labeller::gui {

/**
 * A function for performing an action. Should return true when the action is resolved.
 */
using ActionFunction = std::function<bool()>;

/**
 * Container for managing a queue of oneshot actions.
 */
class ActionQueue {
public:
    void add_action(ActionFunction action);

    /**
     * Runs all action functions once, starting with the oldest. If an action function returns true,
     * it is removed from the queue.
     */
    void process_actions();

    void clear();

private:
    std::list<ActionFunction> m_actions;
};

}; // namespace recap::labeller::gui

#endif // RECAP_LABELLER_GUI_ACTION_QUEUE_HPP_INCLUDE
