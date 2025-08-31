cmake_minimum_required(VERSION 3.30)

project(orderbook LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/extern/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "")
endif()

include(FetchContent)

FetchContent_Declare(
        md_prsr
        GIT_REPOSITORY https://github.com/serge-klim/md_prsr.git
        GIT_TAG        main
)
FetchContent_MakeAvailable(md_prsr)

add_library(order_book
        src/order_book.cpp
        src/order_book.h
        src/itch_router.h
)

add_executable(main src/main.cpp src/order_book.cpp)
target_link_libraries(main
        PRIVATE md_prsr::nasdaq_itch_v5_0)

target_link_libraries(order_book
        PRIVATE md_prsr::nasdaq_itch_v5_0)


target_include_directories(order_book PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

add_executable(order_book_tests tests/order_book_tests.cpp)
find_package(doctest CONFIG REQUIRED)
target_link_libraries(order_book_tests
        PRIVATE order_book doctest::doctest)
