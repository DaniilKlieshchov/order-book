#include <fstream>
#include <iostream>
#include <cstdint>
#include <unordered_set>

#include "order_book.h"
#include "transcoder/transcoder.hpp"
#include "md_prsr/nasdaq/itch_v5.0/transcoder.hpp"
#include "md_prsr/nasdaq/moldudp64_v1.0/describe.hpp"
#include "itch_router.h"

using Locate = std::uint16_t;
using BookMap = std::unordered_map<Locate, trading::OrderBook>;


int main() {
    BookMap books;
    std::unordered_map<Locate, std::string> symbols;
    std::unordered_set<std::string> watch = {"AAPL    ", "AMZN    " };


    std::ifstream file("/Users/danil/Downloads/12302019.NASDAQ_ITCH50",
                       std::ios::binary);

    std::vector<char> msg_buf;

    int counter = 0;
    while (counter++ < 1000000) {
        std::uint16_t len_be;

        if (!file.read(reinterpret_cast<char *>(&len_be), 2)) break;

        auto *cur = reinterpret_cast<tc::byte_t *>(&len_be);
        auto *end = cur + 2;

        auto packet_block = nasdaq::itch::v5_0::decode<nasdaq::moldudp64::v1_0::packet_block>(cur, end);

        msg_buf.resize(packet_block.message_size);
        if (!file.read(msg_buf.data(), packet_block.message_size)) {
            std::cerr << "No luck\n";
            break;
        }

        cur = reinterpret_cast<tc::byte_t *>(msg_buf.data());
        end = cur + packet_block.message_size;

        auto msg = nasdaq::itch::v5_0::decode<
            nasdaq::itch::v5_0::messages>(cur, end);


        std::visit([&](auto const &m) {
            using M = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<M, nasdaq::itch::v5_0::stock_directory>) {
                std::string stock(m.stock.begin(), m.stock.end());
                if (watch.contains(stock)) {
                    symbols[m.stock_locate] = stock;
                    // std::cout << m.stock_locate << " : " << stock << '\n';
                    books[m.stock_locate] = trading::OrderBook();
                }
            }
            if (books.contains(m.stock_locate)) {
                itch_router::handle(m, books[m.stock_locate]);
            }
        }, msg);

    }
    for (const auto& [key, value] : books) {
        std::cout << symbols[key] << '\n';
        trading::print_order_book(value);
    }
    std::cout << std::endl;
}
