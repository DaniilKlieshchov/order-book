#include <fstream>
#include <iostream>
#include <cstdint>
#include <unordered_set>
#include <chrono>
#include <iomanip>

#include "order_book.h"
#include "transcoder/transcoder.hpp"
#include "md_prsr/nasdaq/itch_v5.0/transcoder.hpp"
#include "md_prsr/nasdaq/moldudp64_v1.0/describe.hpp"
#include "itch_router.h"

using Locate = std::uint16_t;
using BookMap = std::unordered_map<Locate, trading::OrderBook>;
using Clock = std::chrono::steady_clock;

static inline std::uint64_t ns_between(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}

struct ScopeTimer {
    std::uint64_t &bucket;
    Clock::time_point t0;

    explicit ScopeTimer(std::uint64_t &b) : bucket(b), t0(Clock::now()) {
    }

    ~ScopeTimer() { bucket += ns_between(t0, Clock::now()); }
};

static inline std::string trim_stock(const std::array<char, 8> &raw) {
    std::string s(raw.data(), raw.data() + raw.size());
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

int main() {
    BookMap books;
    std::unordered_map<Locate, std::string> symbols;
    std::unordered_set<std::string> watch = {"AAPL", "AMZN"};

    std::ifstream file("/Users/danil/Downloads/12302019.NASDAQ_ITCH50",
                       std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    std::vector<char> msg_buf;

    std::size_t msg_count = 0;
    std::size_t bytes_read = 0;
    std::uint64_t io_ns = 0, decode_ns = 0, route_ns = 0, book_ns = 0;

    auto t_run0 = Clock::now();

    for (;;) {
        std::uint16_t len_be; {
            ScopeTimer t(io_ns);
            if (!file.read(reinterpret_cast<char *>(&len_be), 2)) break;
        }

        auto *cur = reinterpret_cast<tc::byte_t *>(&len_be);
        auto *end = cur + 2;

        nasdaq::moldudp64::v1_0::packet_block packet_block; {
            ScopeTimer t(decode_ns);
            packet_block = nasdaq::itch::v5_0::decode<
                nasdaq::moldudp64::v1_0::packet_block>(cur, end);
        }

        msg_buf.resize(packet_block.message_size); {
            ScopeTimer t(io_ns);
            if (!file.read(msg_buf.data(), packet_block.message_size)) {
                std::cerr << "Truncated read\n";
                break;
            }
            bytes_read += 2 + packet_block.message_size;
        }

        cur = reinterpret_cast<tc::byte_t *>(msg_buf.data());
        end = cur + packet_block.message_size;

        nasdaq::itch::v5_0::messages msg; {
            ScopeTimer t(decode_ns);
            msg = nasdaq::itch::v5_0::decode<
                nasdaq::itch::v5_0::messages>(cur, end);
        }

        bool stop = false;
        ++msg_count; {
            ScopeTimer t(route_ns);
            std::visit([&](auto const &m) {
                using M = std::decay_t<decltype(m)>;

                if constexpr (std::is_same_v<M, nasdaq::itch::v5_0::stock_directory>) {
                    std::string stock = trim_stock(m.stock);
                    if (watch.empty() || watch.contains(stock)) {
                        symbols[m.stock_locate] = stock;
                        books.try_emplace(m.stock_locate);
                    }
                    return;
                }
                // if constexpr (requires { m.timestamp; }) {
                //     constexpr std::uint64_t cutoff_ns = (15ull * 60 * 60 + 59ull * 60) * 1'000'000'000ull; //15:59:00
                //     if (static_cast<std::uint64_t>(m.timestamp) >= cutoff_ns) {
                //         stop = true;
                //         return;
                //     }
                // }

                if constexpr (requires { m.stock_locate; }) {
                    auto it = books.find(m.stock_locate);
                    if (it == books.end()) return; {
                        ScopeTimer tb(book_ns);
                        itch_router::handle(m, it->second);
                    }
                }
            }, msg);
        }
        if (stop) {break;}
    }

    auto t_run1 = Clock::now();
    double sec = ns_between(t_run0, t_run1) / 1e9;

    auto pct = [&](std::uint64_t x) { return 100.0 * x / std::max<std::uint64_t>(1, ns_between(t_run0, t_run1)); };

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Processed " << msg_count << " messages in " << sec << " s\n"
            << "Throughput: " << (msg_count / std::max(1e-9, sec)) << " msg/s, "
            << (bytes_read / (1024.0 * 1024.0) / std::max(1e-9, sec)) << " MB/s\n"
            << "Breakdown:  IO " << io_ns / 1e6 << " ms (" << pct(io_ns) << "%), "
            << "Decode " << decode_ns / 1e6 << " ms (" << pct(decode_ns) << "%), "
            << "Route " << route_ns / 1e6 << " ms (" << pct(route_ns) << "%), "
            << "Book " << book_ns / 1e6 << " ms (" << pct(book_ns) << "%)\n";

    for (const auto &[loc, book]: books) {
        std::cout << (symbols.count(loc) ? symbols[loc] : std::to_string(loc)) << '\n';
        trading::print_order_book(book);
    }
    std::cout << std::endl;

    return 0;
}
