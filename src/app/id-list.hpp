#ifndef RECAP_LABELLER_APP_ID_LIST_HPP_INCLUDE
#define RECAP_LABELLER_APP_ID_LIST_HPP_INCLUDE

#include <algorithm>
#include <vector>

namespace recap::labeller::app {

template <typename T> class IDList {
public:
    struct Item {
        using ID = std::size_t;

        ID id;
        T item;
    };

    IDList() = default;

    explicit IDList(const std::vector<T>& items)
    {
        for (const auto& item : items) {
            m_items.push_back({m_next_id, item});
            m_next_id += 1;
        }
    }

    [[nodiscard]] std::size_t size() const { return m_items.size(); }

    [[nodiscard]] bool empty() const { return m_items.empty(); }

    [[nodiscard]] const std::vector<Item>& items() const { return m_items; }

    Item::ID add_item(const T& val)
    {
        typename Item::ID id = m_next_id;
        m_items.push_back({id, val});
        m_next_id += 1;
        return id;
    }

    [[nodiscard]] const T *get_item(Item::ID id) const { return get_item(*this, id); }

    [[nodiscard]] T *get_item(Item::ID id) { return get_item(*this, id); }

    void remove_item(Item::ID id)
    {
        const auto it = find_item(*this, id);
        if (it != m_items.end()) {
            m_items.erase(it);
        }
    }

private:
    Item::ID m_next_id = 1;
    std::vector<Item> m_items;

    template <typename Self> static auto find_item(Self&& self, Item::ID id)
    {
        auto it = std::lower_bound(
            self.m_items.begin(), self.m_items.end(), id, [](const Item& item, Item::ID id) {
                return item.id < id;
            }
        );
        if (it != self.m_items.end() && it->id == id) {
            return it;
        }
        return self.m_items.end();
    }

    template <typename Self> static auto *get_item(Self&& self, Item::ID id)
    {
        auto it = find_item(self, id);
        return it == self.m_items.end() ? nullptr : &(it->item);
    }
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_ID_LIST_HPP_INCLUDE
