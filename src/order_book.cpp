#include "order_book.h"

namespace trading {
    static std::uint64_t now_us() {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    }

    std::vector<Trade> OrderBook::add_order(const Order &order) {
        std::vector<Trade> trades;

        if (index_.contains(order.id)) {
            return trades;
        }

        if (order.side == Side::Bid) {
            bool cross = !asks_.empty() && asks_.begin()->first <= order.price;
            uint32_t rest = order.quantity;
            if (cross) {
                for (auto it = asks_.begin(); it != asks_.end() && rest > 0;) {
                    auto &[price, fifo] = it->second;

                    while (!fifo.empty()) {
                        if (fifo.front().quantity < rest) {
                            rest -= fifo.front().quantity;
                            Trade tr{
                                next_trade_id_++,
                                fifo.front().id,
                                order.id,
                                order.side,
                                fifo.front().price,
                                fifo.front().quantity,
                                now_us()
                            };
                            trades.push_back(tr);
                            index_.erase(fifo.front().id);
                            fifo.pop_front();
                        } else {
                            fifo.front().quantity -= rest;
                            Trade tr{
                                next_trade_id_++,
                                fifo.front().id,
                                order.id,
                                order.side,
                                fifo.front().price,
                                rest,
                                now_us()
                            };
                            trades.push_back(tr);
                            rest = 0;
                            if (fifo.front().quantity == 0) {
                                index_.erase(fifo.front().id);
                                fifo.pop_front();
                            }
                            break;
                        }
                    }
                    if (fifo.empty()) {
                        it = asks_.erase(it);
                    } else {
                        ++it;
                    }
                    if (rest == 0) {
                        return trades;
                    }
                }
            }
            auto [lvlIt, _] = bids_.try_emplace(order.price, PriceLevel{order.price});
            lvlIt->second.fifo.emplace_back(order.id, Side::Bid, order.price, rest, order.timestamp);
            const auto &fifo = lvlIt->second.fifo;
            const std::size_t offset = fifo.size() - 1;
            index_[order.id] = {order.side, order.price, offset};
        }

        if (order.side == Side::Ask) {
            bool cross = !bids_.empty() && bids_.begin()->first >= order.price;
            uint32_t rest = order.quantity;
            if (cross) {
                for (auto it = bids_.begin(); it != bids_.end() && rest > 0;) {
                    auto &[price, fifo] = it->second;

                    while (!fifo.empty()) {
                        if (fifo.front().quantity < rest) {
                            rest -= fifo.front().quantity;
                            Trade tr{
                                next_trade_id_++,
                                fifo.front().id,
                                order.id,
                                order.side,
                                fifo.front().price,
                                fifo.front().quantity,
                                now_us()
                            };
                            trades.push_back(tr);
                            index_.erase(fifo.front().id);
                            fifo.pop_front();
                        } else {
                            fifo.front().quantity -= rest;
                            Trade tr{
                                next_trade_id_++,
                                fifo.front().id,
                                order.id,
                                order.side,
                                fifo.front().price,
                                rest,
                                now_us()
                            };
                            trades.push_back(tr);
                            rest = 0;
                            if (fifo.front().quantity == 0) {
                                index_.erase(fifo.front().id);
                                fifo.pop_front();
                            }
                            break;
                        }
                    }
                    if (fifo.empty()) {
                        it = bids_.erase(it);
                    } else {
                        ++it;
                    }
                    if (rest == 0) {
                        return trades;
                    }

                }
            }
            auto [lvlIt, _] = asks_.try_emplace(order.price, PriceLevel{order.price});
            lvlIt->second.fifo.emplace_back(order.id, Side::Ask, order.price, rest, order.timestamp);
            const auto &fifo = lvlIt->second.fifo;
            const std::size_t offset = fifo.size() - 1;
            index_[order.id] = {order.side, order.price, offset};

        }
        return trades;
    }


    template<class Tree>
    auto OrderBook::level(Tree &tree, double price)
        -> decltype(tree.find(price)) {
        return tree.find(price);
    }

    template<class Tree>
    void OrderBook::erase_from_level(Tree &tree,
                                     typename Tree::iterator level_it,
                                     std::size_t pos) {
        auto &fifo = level_it->second.fifo;
        fifo.erase(fifo.begin() + pos);

        for (std::size_t i = pos; i < fifo.size(); ++i)
            index_[fifo[i].id].index_in_level = i;

        if (fifo.empty())
            tree.erase(level_it);
    }


    bool OrderBook::cancel_order(std::uint64_t order_id) {
        auto idx = index_.find(order_id);
        if (idx == index_.end()) {
            return false;
        }

        const Side side = idx->second.side;
        const double price_key = idx->second.price;
        std::size_t pos = idx->second.index_in_level;


        if (side == Side::Bid) {
            erase_from_level(bids_, level(bids_, price_key), pos);
        } else {
            erase_from_level(asks_, level(asks_, price_key), pos);
        }

        index_.erase(idx);
        return true;
    }

    bool OrderBook::modify_order(std::uint64_t order_id,
                                 std::optional<double> new_price,
                                 std::optional<std::uint32_t> new_qty) {

        auto idx = index_.find(order_id);
        if (idx == index_.end())
            return false;

        const Side side = idx->second.side;
        const double price_key = idx->second.price;
        std::size_t pos = idx->second.index_in_level;

        auto modify_impl = [&](auto &tree) -> bool
        {
            auto levelIt = level(tree, price_key);
            if (levelIt == tree.end())
                return false;

            auto &fifo = levelIt->second.fifo;
            Order &ord = fifo[pos];

            double px = new_price ? *new_price : ord.price;
            std::uint32_t qty = new_qty ? *new_qty : ord.quantity;

            if (qty == 0)
                return cancel_order(order_id);

            if (px == ord.price) {
                ord.quantity = qty;
                return true;
            }

            Order moved = ord;
            erase_from_level(tree, levelIt, pos);
            index_.erase(idx);

            moved.price = px;
            moved.quantity = qty;
            moved.timestamp = now_us();

            add_order(moved);
            return true;
        };

        //dispatch to the correct tree (no type clash)
        return (side == Side::Bid)
                   ? modify_impl(bids_)
                   : modify_impl(asks_);
    }

    std::optional<Order> OrderBook::best_bid() const {
        if (bids_.empty())
            return std::nullopt;

        const auto &level = bids_.begin()->second;
        return level.fifo.front();
    }

    std::optional<Order> OrderBook::best_ask() const {
        if (asks_.empty())
            return std::nullopt;

        const auto &level = asks_.begin()->second;
        return level.fifo.front();
    }

    std::vector<Order> OrderBook::depth(Side side, std::size_t levels) const {

        auto fill_orders = [&levels](auto &tree) -> std::vector<Order> {
            std::vector<Order> orders;
            if (levels == 0) {
                return orders;
            }
            for (auto it = tree.cbegin(); it != tree.cend() && levels--; ++it) {
                const auto &fifo = it->second.fifo;
                for (const auto &order: fifo) {
                    orders.push_back(order);
                }
            }
            return orders;
        };

        return (side == Side::Bid)
                   ? fill_orders(bids_)
                   : fill_orders(asks_);
    }

    std::size_t OrderBook::total_orders() const {
        return index_.size();
    }

    void OrderBook::clear() {
        bids_.clear();
        asks_.clear();
        index_.clear();
        next_trade_id_ = 1;
        // index_.rehash(0);
    }
} // namespace trading
