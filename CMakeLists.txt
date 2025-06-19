cmake_minimum_required(VERSION 3.30)          # 3.30 doesn’t exist yet
project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── doctest via FetchContent ─────────────────────────────────────
include(FetchContent)

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

# ─── unit-tests executable ───────────────────────────────────────
add_executable(order_book_tests
        tests/order_book_tests.cpp  # your doctest cases
)
target_link_libraries(order_book_tests
        PRIVATE order_book doctest::doctest
)
