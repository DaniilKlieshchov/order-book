#pragma once
#include "order_book.h"
#include "md_prsr/nasdaq/itch_v5.0/messages.hpp"

namespace itch_router {

template<typename Msg>
std::vector<trading::Trade> handle(const Msg& m, trading::OrderBook& book) {
    using namespace nasdaq::itch::v5_0;

    if constexpr (std::is_same_v<Msg, add_order> || std::is_same_v<Msg, add_order_mpid>) {
        trading::Order o{
            m.order_reference_number,
            (m.buy_sell_indicator=='B') ? trading::Side::Bid : trading::Side::Ask,
            static_cast<trading::price4_t>(m.price),
            static_cast<trading::qty_t>(m.shares),
            static_cast<trading::ts_ns_t>(m.timestamp)  // tagged<uint64_t> -> uint64_t
        };
        return book.add_order(o);
    }

    else if constexpr (std::is_same_v<Msg, order_executed> || std::is_same_v<Msg, order_executed_with_price>) {
        book.decrease_qty(m.order_reference_number, static_cast<trading::qty_t>(m.executed_shares));
        return {};
    }

    else if constexpr (std::is_same_v<Msg, order_cancel>) {
        book.decrease_qty(m.order_reference_number, static_cast<trading::qty_t>(m.canceled_shares));
        return {};
    }
    else if constexpr (std::is_same_v<Msg, order_delete>) {
        book.cancel_order(m.order_reference_number);
        return {};
    }
    else if constexpr (std::is_same_v<Msg, order_replace>) {
        auto side = book.side_of(m.original_order_reference_number);
        if (!side) return {};
        book.cancel_order(m.original_order_reference_number);

        trading::Order o{
            m.new_order_reference_number,
            *side,
            static_cast<trading::price4_t>(m.price),
            static_cast<trading::qty_t>(m.shares),
            static_cast<trading::ts_ns_t>(m.timestamp)
        };
        return book.add_order(o);
    }
    else {
        return {};
    }
}


} // namespace itch_router
