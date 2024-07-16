#ifndef RECAP_LABELLER_OBSERVABLE_HPP_INCLUDE
#define RECAP_LABELLER_OBSERVABLE_HPP_INCLUDE

#include <functional>
#include <list>
#include <utility>

namespace recap::labeller {

template <typename... Ts> class Observable {
public:
    using Function = std::function<void(Ts...)>;
    using ObserverList = std::list<Function>;

    class Observer {
    private:
        using Handle = typename ObserverList::iterator;
        ObserverList& list;
        Handle handle;

        explicit Observer(ObserverList& list, Handle handle) : list(list), handle(handle) {}

        friend class Observable;

    public:
        ~Observer() { list.erase(handle); }

        Observer(const Observer&) = delete;
        Observer& operator=(const Observer&) = delete;
        Observer(Observer&&) = delete;
        Observer& operator=(Observer&&) = delete;
    };

    [[nodiscard]] typename ObserverList::size_type num_observers() const
    {
        return m_observers.size();
    }

    template <typename... Args> void notify(Args&&...args)
    {
        for (const auto& observer : m_observers) {
            observer(std::forward<Args>(args)...);
        }
    }

    [[nodiscard]] Observer subscribe(Function function)
    {
        m_observers.emplace_back(std::move(function));
        return Observer(m_observers, std::prev(m_observers.end()));
    }

private:
    ObserverList m_observers;
};

template <typename... Ts> using Observer = typename Observable<Ts...>::Observer;

}; // namespace recap::labeller

#endif // RECAP_LABELLER_OBSERVABLE_HPP_INCLUDE
