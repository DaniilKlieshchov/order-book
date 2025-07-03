#include <fstream>
#include <iostream>
#include <cstdint>
#include "transcoder/transcoder.hpp"
#include "md_prsr/nasdaq/itch_v5.0/transcoder.hpp"
#include "md_prsr/nasdaq/itch_v5.0/io.hpp"
#include "md_prsr/nasdaq/moldudp64_v1.0/describe.hpp"

// int main() {
//     std::ifstream file("/Users/danil/Downloads/12302019.NASDAQ_ITCH50",
//                        std::ios::binary);
//     std::vector<char> msg_buf;
//
//     while (true) {
//         std::uint16_t len_be;
//         if (!file.read(reinterpret_cast<char*>(&len_be), 2)) break;
//         std::uint16_t len = (len_be >> 8) | (len_be << 8);
//
//         msg_buf.resize(len);
//         if (!file.read(msg_buf.data(), len)) {
//             std::cerr << "No luck\n";
//             break;
//         }
//
//         auto* cur = reinterpret_cast<tc::byte_t*>(msg_buf.data());
//         auto* end = cur + len;
//
//         auto msg = nasdaq::itch::v5_0::decode<
//                        nasdaq::itch::v5_0::messages>(cur, end);
//
//         std::visit([](auto const& m) { std::cout << m << '\n'; }, msg);
//     }
// }

int main() {
    std::ifstream file("/Users/danil/Downloads/12302019.NASDAQ_ITCH50",
                       std::ios::binary);

    std::vector<char> msg_buf;

    while (true) {

        std::uint16_t len_be;

        if (!file.read(reinterpret_cast<char*>(&len_be), 2)) break;

        auto* cur = reinterpret_cast<tc::byte_t*>(&len_be);
        auto* end = cur + 2;

        auto packet_block = nasdaq::itch::v5_0::decode<nasdaq::moldudp64::v1_0::packet_block>(cur, end);

        msg_buf.resize(packet_block.message_size);
        if (!file.read(msg_buf.data(), packet_block.message_size)) {
            std::cerr << "No luck\n";
            break;
        }

        cur = reinterpret_cast<tc::byte_t*>(msg_buf.data());
        end = cur + packet_block.message_size;

        auto msg = nasdaq::itch::v5_0::decode<
                               nasdaq::itch::v5_0::messages>(cur, end);

        std::visit([](auto const& m) { std::cout << m << '\n'; }, msg);
    }
}
