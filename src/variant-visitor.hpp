#ifndef RECAP_LABELLER_VARIANT_VISITOR_HPP_INCLUDE
#define RECAP_LABELLER_VARIANT_VISITOR_HPP_INCLUDE

#include <variant>

namespace recap::labeller {

template <class... Ts> struct VariantVisitor : Ts... {
    using Ts::operator()...;

    template <class... Vs> auto visit(const std::variant<Vs...>& v) const
    {
        return std::visit(*this, v);
    }

    template <class... Vs> auto visit(std::variant<Vs...>& v) { return std::visit(*this, v); }

    template <class... Vs> auto operator()(const std::variant<Vs...>& v) const { return visit(v); }

    template <class... Vs> auto operator()(std::variant<Vs...>& v) { return visit(v); }
};

}; // namespace recap::labeller

#endif // RECAP_LABELLER_VARIANT_VISITOR_HPP_INCLUDE
