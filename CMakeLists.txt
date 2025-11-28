cmake_minimum_required(VERSION 4.0)
project(holdem_analysis)

set(CMAKE_CXX_STANDARD 20)

add_executable(holdem_analysis main.cpp)

# Headers
target_include_directories(holdem_analysis PRIVATE
        ${CMAKE_SOURCE_DIR}/PokerHandEvaluator/cpp/include
)

# Library
target_link_libraries(holdem_analysis PRIVATE
        ${CMAKE_SOURCE_DIR}/PokerHandEvaluator/cpp/build/libpheval.a
)
