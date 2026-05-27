cmake_minimum_required(VERSION 3.23)
project(PredictionMarketEngine)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include (FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(PredictionMarketEngine)

add_executable(arbiter_tests
    tests/constraint_graph_tests.cpp
    src/strategy.cpp
)

target_include_directories(arbiter_tests PRIVATE src)
target_link_libraries(arbiter_tests PRIVATE Catch2::Catch2WithMain)

target_sources(PredictionMarketEngine
    PRIVATE
        src/main.cpp
        src/strategy.cpp
)

