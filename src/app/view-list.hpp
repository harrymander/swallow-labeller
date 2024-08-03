#ifndef RECAP_LABELLER_APP_VIEW_LIST_HPP_INCLUDE
#define RECAP_LABELLER_APP_VIEW_LIST_HPP_INCLUDE

#include <algorithm>
#include <functional>
#include <iterator>
#include <numeric>
#include <random>
#include <ranges>
#include <stdexcept>
#include <vector>

namespace recap::labeller::app {

namespace view_list::internal {

template <typename T, typename Compare>
inline std::vector<T> sorted_vector(std::vector<T> v, Compare compare)
{
    std::sort(v.begin(), v.end(), compare);
    return v;
}

template <typename T> inline std::vector<T> shuffled_vector(std::vector<T> v)
{
    static std::mt19937 rng(std::random_device{}());
    std::shuffle(v.begin(), v.end(), rng);
    return v;
}

inline std::vector<std::size_t> range_vector(std::size_t n)
{
    std::vector<std::size_t> v;
    v.resize(n);
    std::iota(v.begin(), v.end(), 0);
    return v;
}

}; // namespace view_list::internal

template <typename T> class ViewList {
public:
    template <typename Compare = std::less<T>>
    ViewList(std::vector<T> items, bool shuffled, Compare compare = Compare()) :
        m_items(view_list::internal::sorted_vector(std::move(items), compare)),
        m_sorted_indices(view_list::internal::range_vector(m_items.size())),
        m_shuffled_indices(view_list::internal::shuffled_vector(m_sorted_indices))
    {
        if (shuffled) {
            shuffle();
            set_index(0);
        }
    }

    [[nodiscard]] std::size_t size() const { return m_items.size(); }

    [[nodiscard]] bool shuffled() const { return m_shuffled; }

    void shuffle()
    {
        if (m_shuffled) {
            return;
        }

        auto it = std::find(m_shuffled_indices.begin(), m_shuffled_indices.end(), m_index);
        if (it == m_shuffled_indices.end()) {
            throw std::logic_error("Index not found in shuffled indices");
        }
        m_index = static_cast<std::size_t>(std::distance(m_shuffled_indices.begin(), it));
        m_shuffled = true;
    }

    void unshuffle()
    {
        if (!m_shuffled) {
            return;
        }

        m_index = m_shuffled_indices[m_index];
        m_shuffled = false;
    }

    [[nodiscard]] std::size_t index() const { return m_index; }

    auto items() const
    {
        const auto& indices = m_shuffled ? m_shuffled_indices : m_sorted_indices;
        return indices
            | std::views::transform([this](std::size_t i) -> const T& { return m_items[i]; });
    }

    void set_index(std::size_t index)
    {
        if (index >= m_items.size()) {
            throw std::out_of_range("index out of range");
        }
        m_index = index;
    }

    [[nodiscard]] const T& at(std::size_t index) const { return at(*this, index); }

    [[nodiscard]] T& at(std::size_t index) { return at(*this, index); }

    [[nodiscard]] const T& index_item() const { return at(m_index); }

    [[nodiscard]] T& index_item() { return at(m_index); }

private:
    template <typename U> static auto& at(U&& self, std::size_t index)
    {
        if (self.m_shuffled) {
            return self.m_items[self.m_shuffled_indices.at(index)];
        }
        return self.m_items.at(index);
    }

    bool m_shuffled = false;
    std::size_t m_index = 0;

    std::vector<T> m_items;
    std::vector<std::size_t> m_sorted_indices;
    std::vector<std::size_t> m_shuffled_indices;
};

}; // namespace recap::labeller::app

#endif // RECAP_LABELLER_APP_VIEW_LIST_HPP_INCLUDE
