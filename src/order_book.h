#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>

namespace trading {

// --- Basic types -----------------------------------------------------------------------------
/// Bid = buy, Ask = sell.
enum class Side { Bid, Ask };

/// Client order submitted to the book.
struct Order {
    std::uint64_t id;          ///< Unique client‑supplied id
    Side          side;        ///< Bid or Ask
    double        price;       ///< Limit price (floating‑point for simplicity)
    std::uint32_t quantity;    ///< Remaining quantity (shares/lots)
    std::uint64_t timestamp;   ///< Epoch microseconds — used for price‑time priority
};

/// Execution report produced by the matching engine.
struct Trade {
    std::uint64_t id;              ///< Engine‑generated trade id
    std::uint64_t maker_order_id;  ///< Resting order providing liquidity
    std::uint64_t taker_order_id;  ///< Incoming order taking liquidity
    Side          side;            ///< Side of the taker (Bid removes Ask, etc.)
    double        price;           ///< Execution price
    std::uint32_t quantity;        ///< Executed quantity
    std::uint64_t timestamp;       ///< Epoch microseconds
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
                      std::optional<double> new_price = std::nullopt,
                      std::optional<std::uint32_t> new_qty = std::nullopt);

    // --- Book queries ------------------------------------------------------------------------
    std::optional<Order> best_bid() const;               ///< Highest bid, if any
    std::optional<Order> best_ask() const;               ///< Lowest ask, if any

    /// Return up to `levels` price levels on the requested side (FIFO within a level).
    std::vector<Order> depth(Side side, std::size_t levels = 10) const;

    std::size_t total_orders() const;                    ///< #active resting orders
    void clear();                                        ///< Remove all orders

private:
    // Price bucket holding FIFO queue of resting orders.
    struct PriceLevel {
        double price;
        std::deque<Order> fifo;   ///< Resting orders, oldest first
    };

    template<class Tree>
    auto level(Tree& tree, double price) -> decltype(tree.find(price));

    template<class Tree>
    void erase_from_level(Tree& tree,
                          typename Tree::iterator level_it,
                          std::size_t pos);

    // bids sorted highest‑price‑first; asks lowest‑price‑first.
    std::map<double, PriceLevel, std::greater<double>> bids_;
    std::map<double, PriceLevel, std::less<double>>    asks_;

    // Fast lookup from order id → location for cancel/modify.
    struct OrderRef {
        Side side;
        double price;
        std::size_t index_in_level;
    };
    std::unordered_map<std::uint64_t, OrderRef> index_;

    // Simple monotonically increasing trade id generator.
    std::uint64_t next_trade_id_ = 1;
};

} // namespace trading
