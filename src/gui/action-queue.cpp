#include "action-queue.hpp"

#include <spdlog/spdlog.h>

namespace recap::labeller::gui {

void ActionQueue::add_action(ActionFunction action)
{
    m_actions.push_back(std::move(action));
    spdlog::debug("Added action to queue: {} action(s) now in queue", m_actions.size());
}

void ActionQueue::process_actions()
{
    for (auto action = m_actions.begin(); action != m_actions.end();) {
        if ((*action)()) {
            action = m_actions.erase(action);
            spdlog::debug("Action removed from queue: {} action(s) remaining", m_actions.size());
        } else {
            action++;
        }
    }
}

void ActionQueue::clear()
{
    m_actions.clear();
}

}; // namespace recap::labeller::gui
