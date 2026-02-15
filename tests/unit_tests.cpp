#define BOOST_TEST_MODULE AsyncConnectUnitTests

#include <boost/test/included/unit_test.hpp>

#include <sys/epoll.h>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <numeric> 
#include <algorithm> 
#include <sys/socket.h>

#include "connection_manager.hpp"
#include "epoll.hpp"
#include "async_operations.hpp"

BOOST_AUTO_TEST_SUITE(IOContextTests)

BOOST_AUTO_TEST_CASE(test_multithreaded_execution_and_shutdown)
{
    IOContext context;
    std::atomic<int> counter(0);
    const int numTasks = 100;
    const int numThreads = 10;
  
    for (int i = 0; i < numTasks; ++i) {
        context.post([&]() {
            counter++;
        });
    }

    std::vector<std::thread> threads;
    for(int idx = 0; idx < numThreads; ++idx)
    {
        threads.emplace_back([&]{ context.run(); });
    }
    
    for(auto& thread : threads)
    {
        if (thread.joinable()) {
            thread.join();
        }
    }

    BOOST_CHECK_EQUAL(counter.load(), numTasks);    
}

BOOST_AUTO_TEST_CASE(test_post_task_and_auto_shutdown)
{
    IOContext context;
    std::atomic<bool> taskExecuted(false);
    std::atomic<int> sharedValue(0);
   
    context.post([&]() {
        sharedValue = 42;
        taskExecuted = true;
    });
       
    context.run();    
    
    BOOST_CHECK(taskExecuted.load());
    BOOST_CHECK_EQUAL(sharedValue.load(), 42);   
}

BOOST_AUTO_TEST_CASE(test_run_auto_shutdown_no_work)
{
    int a = 0;
    IOContext context;    
    
    context.run(); 
    a = 10;
    
    BOOST_CHECK_EQUAL(a, 10);       
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(endpoint_test)
{
    const std::string ipAddress("127.0.0.1");
    const int port = 8080;

    struct sockaddr_in addr_struct;
    memset(&addr_struct, 0, sizeof(addr_struct));
    addr_struct.sin_family = AF_INET;
    addr_struct.sin_port = htons(port);

    if(inet_pton(AF_INET, ipAddress.c_str(), &addr_struct.sin_addr) <= 0)
    {
        throw std::runtime_error("Invalid ip-address or inet_pton error");
    }

    const socklen_t socoLen = sizeof(addr_struct);

    EndpointIPv4 endpoint(ipAddress, port);

    BOOST_CHECK_EQUAL(endpoint.get_port(), 8080);
    BOOST_CHECK_EQUAL(endpoint.get_ip_str(), ipAddress);
    BOOST_CHECK_EQUAL(endpoint.get_socklen(), socoLen);        
}

BOOST_AUTO_TEST_CASE(Epoll_test)
{
    int socket_1 = socket(AF_INET, SOCK_STREAM, 0);
    BOOST_TEST(socket_1 > 0);

    int socket_2 = socket(AF_INET, SOCK_STREAM, 0);
    BOOST_TEST(socket_2 > 0);

    int socket_3 = socket(AF_INET, SOCK_STREAM, 0);
    BOOST_TEST(socket_3 > 0);

    static int MAX_EPOLL_EVENTS = 35'000;

    Epoll epoll(MAX_EPOLL_EVENTS);

    BOOST_TEST(epoll.epoll() > 0);
    BOOST_TEST(epoll.is_initialized() == true);
    BOOST_TEST(epoll.max_events() == MAX_EPOLL_EVENTS);

    BOOST_TEST(epoll.add(socket_1, EPOLLIN) == EpollStatus::ES_SUCCESS);
    BOOST_TEST(epoll.add(socket_2, EPOLLIN) == EpollStatus::ES_SUCCESS);
    BOOST_TEST(epoll.add(socket_3, EPOLLIN) == EpollStatus::ES_SUCCESS);

    int count{};
    auto result = epoll.wait(10, count);
    BOOST_TEST(result == EpollStatus::ES_SUCCESS);
    BOOST_TEST(count == 3);

    auto check_socket = [&socket_1, &socket_2, &socket_3](const int &socket) -> bool
    {
        if (socket == socket_1 || socket == socket_2 || socket == socket_3)
        {
            return true;
        }
        else
        {
            return false;
        }
    };

    for (int idx = 0; idx < count; ++idx)
    {
        BOOST_TEST(check_socket(epoll[idx].data.fd) == true);
    }

    BOOST_TEST(epoll.mod(socket_1, EPOLLOUT) == EpollStatus::ES_SUCCESS);
    BOOST_TEST(epoll.mod(socket_2, EPOLLOUT) == EpollStatus::ES_SUCCESS);
    BOOST_TEST(epoll.mod(socket_3, EPOLLOUT) == EpollStatus::ES_SUCCESS);

    result = epoll.wait(10, count);
    BOOST_TEST(result == EpollStatus::ES_SUCCESS);
    BOOST_TEST(count == 3);

    for (int idx = 0; idx < count; ++idx)
    {
        BOOST_TEST(check_socket(epoll[idx].data.fd) == true);
    }
}

