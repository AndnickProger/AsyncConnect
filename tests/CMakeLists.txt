find_package(Boost COMPONENTS unit_test_framework REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(UnitTests unit_tests.cpp)
target_link_libraries(UnitTests PRIVATE AsyncConnectLib ${Boost_LIBRARIES})
target_include_directories(UnitTests PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME UnitTests COMMAND UnitTests)

add_executable(IntegrationTests integration_tests.cpp)
target_link_libraries(IntegrationTests PRIVATE AsyncConnectLib ${Boost_LIBRARIES})
target_include_directories(IntegrationTests PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME IntegrationTests COMMAND IntegrationTests)
