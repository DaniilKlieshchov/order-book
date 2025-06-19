#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "../src/order_book.h"

using trading::Side;
using trading::OrderBook;
using trading::Order;


static std::uint64_t ts = 0;

auto make = [](std::uint64_t id, Side s, double px, std::uint32_t qty) {
    return Order{id, s, px, qty, ts++};
};


TEST_CASE("single cross – full fill") {
    OrderBook book;

    book.add_order(make(1, Side::Ask, 101.0, 50));         // rests
    auto trades = book.add_order(make(2, Side::Bid, 105.0, 50)); // taker

    REQUIRE(trades.size() == 1);
    CHECK(trades[0].quantity == 50);
    CHECK(trades[0].price == doctest::Approx(101.0));
    CHECK(book.best_ask() == std::nullopt);                // level consumed
    CHECK(book.total_orders() == 0);
}

TEST_CASE("walk book – partial fills across two levels") {
    OrderBook book;

    book.add_order(make(1, Side::Ask, 100.0, 30));  // best ask
    book.add_order(make(2, Side::Ask, 101.0, 50));  // next level

    auto trades = book.add_order(make(3, Side::Bid, 102.0, 60));

    REQUIRE(trades.size() == 2);
    CHECK(trades[0].quantity == 30);
    CHECK(trades[1].quantity == 30);
    CHECK(book.best_ask()->price == doctest::Approx(101.0));
    CHECK(book.best_ask()->quantity == 20);
    CHECK(book.total_orders() == 1);                // only leftover ask
}

TEST_CASE("duplicate order id is rejected") {
    OrderBook book;
    book.add_order(make(10, Side::Bid, 99.0, 5));
    CHECK(book.add_order(make(10, Side::Ask, 90.0, 5)).empty());
    CHECK(book.total_orders() == 1);
}

TEST_CASE("cancel removes resting order") {
    OrderBook book;
    book.add_order(make(1, Side::Bid, 101.0, 40));
    REQUIRE(book.cancel_order(1) == true);
    CHECK(book.best_bid() == std::nullopt);
    CHECK(book.total_orders() == 0);
    CHECK(book.cancel_order(1) == false);           // already gone
}

TEST_CASE("modify size only") {
    OrderBook book;
    book.add_order(make(1, Side::Ask, 105.0, 20));
    REQUIRE(book.modify_order(1, std::nullopt, 50) == true);
    CHECK(book.best_ask()->quantity == 50);
}

TEST_CASE("modify price – order crosses and trades") {
    OrderBook book;
    book.add_order(make(1, Side::Ask, 103.0, 30));      // maker
    book.add_order(make(2, Side::Bid, 100.0, 30));      // resting bid

    REQUIRE(book.modify_order(2, 104.0, std::nullopt) == true); // now crosses

    CHECK(book.total_orders() == 0);        // both fully filled
}

TEST_CASE("modify to zero qty acts like cancel") {
    OrderBook book;
    book.add_order(make(1, Side::Ask, 110.0, 10));
    REQUIRE(book.modify_order(1, std::nullopt, 0) == true);
    CHECK(book.total_orders() == 0);
}

TEST_CASE("depth snapshot") {
    OrderBook book;
    book.add_order(make(1, Side::Bid, 100.0, 10));
    book.add_order(make(2, Side::Bid,  99.0, 20));
    book.add_order(make(3, Side::Bid,  99.0, 20));
    book.add_order(make(4, Side::Bid,  98.0, 30));

    auto d = book.depth(Side::Bid, 2);   // best two price levels

    REQUIRE(d.size() == 3);              // one order per level
    CHECK(d.front().price == doctest::Approx(100.0));
    CHECK(d.back().price  == doctest::Approx( 99.0));
}

TEST_CASE("clear resets book state") {
    OrderBook book;
    book.add_order(make(1, Side::Ask, 120.0, 5));
    book.clear();
    CHECK(book.total_orders() == 0);
    CHECK(book.best_ask() == std::nullopt);
    CHECK(book.add_order(make(1, Side::Bid, 80.0, 5)).size() == 0); // id reused ok
}
