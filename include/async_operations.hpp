#pragma once

#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <stdexcept>
#include <array>
#include <cstring>
#include <iostream>
#include <system_error>
#include <variant>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <queue>
#include <sys/eventfd.h>
#include <thread>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

#include "epoll.hpp"

class EndpointIPv4
{
public:
    explicit EndpointIPv4(int port);

    EndpointIPv4(const std::string &ip_address, int port);

    EndpointIPv4(const EndpointIPv4 &other) = delete;
    EndpointIPv4 &operator=(const EndpointIPv4 &other) = delete;

    EndpointIPv4(EndpointIPv4 &&other) noexcept;
    EndpointIPv4 &operator=(EndpointIPv4 &&other) noexcept;

    virtual ~EndpointIPv4() = default;

    struct sockaddr *get_sockaddr();

    socklen_t get_socklen() const;

    std::string get_ip_str() const;

    int get_port() const;

private:
    struct sockaddr_in addr_struct;
};

class TCPAsyncSocket;

enum class OperationType
{
    CONNECT,
    READ,
    WRITE
};

using connection_handler_type = std::function<void(const std::error_code&)>;
using read_write_handler_type = std::function<void(const std::error_code&, size_t bytes_transfered)>;

struct AsyncOperation
{    
    TCPAsyncSocket* socketPointer;
    OperationType type;
    std::variant<connection_handler_type, read_write_handler_type> socket_handler;
    std::vector<char>* bufferArray;
    size_t totalBytesRequested = 0;
};

struct OperationInvoker
{
    const std::error_code& errorCode;
    size_t bytes;
    OperationType type;

    void operator () (const connection_handler_type& handler) const
    {
        if(type == OperationType::CONNECT) handler(errorCode);
    }

    void operator() (const read_write_handler_type handler) const
    {
        if(type == OperationType::READ || type == OperationType::WRITE)
        {
            handler(errorCode, bytes);
        }
    }    
};

class IOContext
{
    public:

    using task_type = std::function<void()>;

    IOContext();

    virtual ~IOContext();

    void run();

    void stop();

    void post(task_type task);

    void register_operations(int sockId, uint32_t eventMask, AsyncOperation operation);

    void deregister_operation(int sockId);

    void inc_work();

    void dec_work();

    private:
    
    Epoll epollManager;
    std::map<int, AsyncOperation> pendingOperations;
    std::atomic<bool> stopRun{false};
    std::atomic<size_t> workCount{0};

    std::mutex operationsMutex;    
    int pipefd[2];

    std::queue<task_type> tasksQueue;
    std::mutex tasksMutex;

    void handle_event(const epoll_event& event);    
    void handle_pipe_event();
    void process_pending_tasks();
};

class TCPAsyncSocket : public std::enable_shared_from_this<TCPAsyncSocket>
{
public:
    using context_type = IOContext;
    using context_reference = IOContext &;
    using socket_type = int;

    explicit TCPAsyncSocket(context_type &context_);

    TCPAsyncSocket(const TCPAsyncSocket &other) = delete;

    TCPAsyncSocket &operator=(const TCPAsyncSocket &other) = delete;

    TCPAsyncSocket(TCPAsyncSocket &&other) noexcept;

    TCPAsyncSocket &operator=(TCPAsyncSocket &&other) noexcept;

    virtual ~TCPAsyncSocket();

    [[nodiscard]] inline const int &get_socket() const noexcept
    {
        return const_cast<const int &>(socketField);
    }

    [[nodiscard]] inline bool is_open() const noexcept
    {
        return socketField >= 0;
    }

    void async_connect(EndpointIPv4 &endpoint, std::function<void(const std::error_code &)> handler);  
    
    void async_read(std::vector<char>& buffer, std::function<void(const std::error_code&, size_t)> handler);

    void async_write(std::vector<char>& buffer, std::function<void(const std::error_code&, size_t)> handler);

private:

    context_reference context;
    socket_type socketField;

    void set_nonblocking();
};


