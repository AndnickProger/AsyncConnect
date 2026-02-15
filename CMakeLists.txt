cmake_minimum_required(VERSION 3.10.0)
project(AsyncConnect VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDART 17)
set(CMAKE_CXX_STANDART_REQUIRED ON)

add_subdirectory(src)
add_subdirectory(include)

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(AsyncConnect main.cpp)

target_link_libraries(AsyncConnect PRIVATE AsyncConnectLib ${Boost_LIBRARIES})

enable_testing()
add_subdirectory(tests)


