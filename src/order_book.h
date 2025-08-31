#pragma once

#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>

namespace trading {

// ---- ITCH-aligned scalar types -------------------------------------------
using order_id_t = std::uint64_t;   // ITCH order_reference_number
using qty_t      = std::uint32_t;   // ITCH shares
using price4_t   = std::uint32_t;   // ITCH price (USD * 10^4)
using ts_ns_t    = std::uint64_t;   // ITCH timestamp (ns since midnight; widened from 48-bit)

// --- Basic types -----------------------------------------------------------------------------
/// Bid = buy, Ask = sell.
enum class Side { Bid, Ask };

/// Client order submitted to the book.
struct Order {
    std::uint64_t id;          ///< Unique client-supplied id
    Side          side;        ///< Bid or Ask
    price4_t      price;       ///< Limit price (floating-point for simplicity) [NOTE: ITCH fixed-point int: USD * 1e4]
    qty_t         quantity;    ///< Remaining quantity (shares/lots)
    ts_ns_t       timestamp;   ///< Epoch microseconds — used for price-time priority [NOTE: ITCH provides ns since midnight]
};

/// Execution report produced by the matching engine.
struct Trade {
    std::uint64_t id;              ///< Engine-generated trade id
    std::uint64_t maker_order_id;  ///< Resting order providing liquidity
    std::uint64_t taker_order_id;  ///< Incoming order taking liquidity
    Side          side;            ///< Side of the taker (Bid removes Ask, etc.)
    price4_t      price;           ///< Execution price [NOTE: ITCH fixed-point int: USD * 1e4]
    qty_t         quantity;        ///< Executed quantity
    ts_ns_t       timestamp;       ///< Epoch microseconds [NOTE: using ns-compatible integer]
};

// --- OrderBook interface ----------------------------------------------------------------------
class OrderBook {
public:
    OrderBook() = default;

    /// Submit a limit order. Returns all trades generated while executing the order.
    /// If the order is fully filled, it does not enter the book; else the residual size becomes
    /// a new resting order.
    std::vector<Trade> add_order(const Order& order);

    /// Cancel a resting order by id. Returns true if the order was found and removed.
    bool cancel_order(std::uint64_t order_id);

    /// Modify price and/or remaining quantity of a resting order. Returns true on success.
    bool modify_order(std::uint64_t order_id,
                      std::optional<price4_t> new_price = std::nullopt,
                      std::optional<qty_t>    new_qty   = std::nullopt);

    bool decrease_qty(order_id_t order_id, qty_t delta);

    std::optional<Side> side_of(order_id_t order_id) const;

    // --- Book queries ------------------------------------------------------------------------
    std::optional<Order> best_bid() const;               ///< Highest bid, if any
    std::optional<Order> best_ask() const;               ///< Lowest ask, if any

    /// Return up to `levels` price levels on the requested side (FIFO within a level).
    std::vector<std::pair<price4_t, qty_t>> depth(Side side, std::size_t levels = 10) const;

    std::size_t total_orders() const;                    ///< #active resting orders
    void clear();                                        ///< Remove all orders

private:
    // Price bucket holding FIFO queue of resting orders.
    struct PriceLevel {
        price4_t      price;
        std::deque<Order> fifo;   ///< Resting orders, oldest first
    };

    template<class Tree>
    auto level(Tree& tree, price4_t price) -> decltype(tree.find(price));

    template<class Tree>
    void erase_from_level(Tree& tree,
                          typename Tree::iterator level_it,
                          std::size_t pos);

    // bids sorted highest-price-first; asks lowest-price-first.
    std::map<price4_t, PriceLevel, std::greater<price4_t>> bids_;
    std::map<price4_t, PriceLevel, std::less<price4_t>>    asks_;

    // Fast lookup from order id → location for cancel/modify.
    struct OrderRef {
        Side     side;
        qty_t quantity;
        price4_t price;
        std::size_t index_in_level;
    };
    std::unordered_map<std::uint64_t, OrderRef> index_;

    // Simple monotonically increasing trade id generator.
    std::uint64_t next_trade_id_ = 1;
};

inline void print_order_book(const OrderBook& book, std::size_t levels = 10) {
        // Get top N bids (highest first) and asks (lowest first)
        auto bids = book.depth(Side::Bid, levels);
        auto asks = book.depth(Side::Ask, levels);

        std::cout << "--------- ORDER BOOK ---------\n";
        std::cout << "  Ask (Sell)\t|\tBid (Buy)\n";
        std::cout << "-------------------------------\n";

        for (std::size_t i = 0; i < levels; ++i) {
            std::string ask_str = (i < asks.size())
                ? (std::to_string(asks[i].second) + " @ " +
                   std::to_string(asks[i].first / 10000.0)) // convert price4_t to float
                : "";

            std::string bid_str = (i < bids.size())
                ? (std::to_string(bids[i].second) + " @ " +
                   std::to_string(bids[i].first / 10000.0))
                : "";

            std::cout << std::setw(15) << ask_str << " | "
                      << std::setw(15) << bid_str << "\n";
        }

        std::cout << "-------------------------------\n";
    }

} // namespace trading
