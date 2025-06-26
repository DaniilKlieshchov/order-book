cmake_minimum_required(VERSION 3.30)          # 3.30 doesn’t exist yet
project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── doctest via FetchContent ─────────────────────────────────────
include(FetchContent)

# 1.  Helper library that md_prsr depends on
FetchContent_Declare(
        transcoder
        GIT_REPOSITORY  https://github.com/serge-klim/transcoder.git
        GIT_TAG         v0.0.5      # or a later tag when it exists
)
FetchContent_MakeAvailable(transcoder)   # makes target transcoder::transcoder

# 2.  md_prsr itself – turn OFF everything except the ITCH part
set(BUILD_TESTS                  OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS                  OFF CACHE BOOL "" FORCE)
set(BUILD_CME_MDP3_V9            OFF CACHE BOOL "" FORCE)
set(BUILD_EOBI_V13_0             OFF CACHE BOOL "" FORCE)
set(BUILD_EOBI_V13_1             OFF CACHE BOOL "" FORCE)
set(BUILD_NASDAQ_MOLDUDP64_V1_0  OFF CACHE BOOL "" FORCE)
set(BUILD_NASDAQ_ITCH_V5_0        ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        md_prsr
        GIT_REPOSITORY  https://github.com/serge-klim/md_prsr.git
        GIT_TAG         main         # pin to a hash if you need reproducibility
)
FetchContent_MakeAvailable(md_prsr)      # makes target md_prsr::nasdaq_itch_v5_0

FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11        # update when a newer tag is out
)
FetchContent_MakeAvailable(doctest)           # creates doctest::doctest

# ─── core library (order book) ────────────────────────────────────
add_library(order_book STATIC
        src/order_book.cpp          # implementation
        src/order_book.h            # interface
)
target_include_directories(order_book PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(order_book
        PRIVATE
        md_prsr::nasdaq_itch_v5_0   # <- the parser + Boost + transcoder
)

# ─── unit-tests executable ───────────────────────────────────────
add_executable(order_book_tests
        tests/order_book_tests.cpp  # your doctest cases
)
target_link_libraries(order_book_tests
        PRIVATE order_book doctest::doctest
)