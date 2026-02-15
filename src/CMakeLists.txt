


add_library(AsyncConnectLib 
            connection_manager.cpp
            async_operations.cpp
            epoll.cpp
            service_function.cpp
            )

target_include_directories(AsyncConnectLib PUBLIC ${CMAKE_SOURCE_DIR}/include)


