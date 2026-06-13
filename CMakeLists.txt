cmake_minimum_required(VERSION 3.23)
project(PredictionMarketEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(PredictionMarketEngine
    src/main.cpp
    src/order_book.cpp
    src/book_builder.cpp
    src/polymarket_adapter.cpp
    src/paper_oms.cpp
)

target_include_directories(PredictionMarketEngine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

add_executable(engine_tests
    tests/test_event_pipeline.cpp
    tests/test_spsc_queue.cpp
    tests/test_order_book.cpp
    tests/strategy_tests.cpp
    tests/test_order_book_apply.cpp
    tests/test_book_builder.cpp
    tests/test_polymarket_adapter.cpp
    tests/test_paper_oms.cpp

    src/order_book.cpp
    src/book_builder.cpp
    src/polymarket_adapter.cpp
    src/paper_oms.cpp
)

target_include_directories(engine_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(engine_tests PRIVATE Catch2::Catch2WithMain)