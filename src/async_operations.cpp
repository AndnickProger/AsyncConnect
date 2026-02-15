#include "async_operations.hpp"

EndpointIPv4::EndpointIPv4(int port)
{
    memset(&addr_struct, 0, sizeof(addr_struct));
    addr_struct.sin_family = AF_INET;
    addr_struct.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_struct.sin_port = htons(port);
}

EndpointIPv4::EndpointIPv4(const std::string &ip_address, int port)
{
    memset(&addr_struct, 0, sizeof(addr_struct));
    addr_struct.sin_family = AF_INET;
    addr_struct.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_address.c_str(), &addr_struct.sin_addr) <= 0)
    {
        throw std::runtime_error("Invalid ip-address or inet_pton error");
    }
}

EndpointIPv4::EndpointIPv4(EndpointIPv4 &&other) noexcept : addr_struct(std::move(other.addr_struct))
{
}

EndpointIPv4 &EndpointIPv4::operator=(EndpointIPv4 &&other) noexcept
{
    addr_struct = std::move(other.addr_struct);
    return *this;
}

sockaddr *EndpointIPv4::get_sockaddr()
{
    return reinterpret_cast<struct sockaddr *>(&addr_struct);
}

socklen_t EndpointIPv4::get_socklen() const
{
    return sizeof(addr_struct);
}

std::string EndpointIPv4::get_ip_str() const
{
    std::array<char, INET_ADDRSTRLEN> buffer;
    inet_ntop(AF_INET, &(addr_struct.sin_addr), buffer.data(), buffer.size());
    return buffer.data();
}

int EndpointIPv4::get_port() const
{
    return ntohs(addr_struct.sin_port);
}

TCPAsyncSocket::TCPAsyncSocket(TCPAsyncSocket::context_type &context_) : context(context_), socketField(-1)
{
    socketField = socket(AF_INET, SOCK_STREAM, 0);
    if (socketField == -1)
    {
        std::cout << "Socket not open" << std::endl;
    }
    set_nonblocking();
}

TCPAsyncSocket::TCPAsyncSocket(TCPAsyncSocket &&other) noexcept : context(other.context),
                                                                  socketField(other.socketField)
{
    other.socketField = -1;
}

TCPAsyncSocket &TCPAsyncSocket::operator=(TCPAsyncSocket &&other) noexcept
{
    // context = std::forward<context_reference>(std::move(other.context));
    socketField = std::move(other.socketField);
    other.socketField = -1;
    return *this;
}

TCPAsyncSocket::~TCPAsyncSocket()
{
    if (socketField != -1)
        close(socketField);
}

void TCPAsyncSocket::async_connect(EndpointIPv4 &endpoint, std::function<void(const std::error_code &)> handler)
{
    if (!is_open())
    {
        handler(std::error_code(EBADF, std::system_category()));
        return;
    }

    int result = ::connect(socketField, endpoint.get_sockaddr(), endpoint.get_socklen());

    if (result == 0)
    {
        handler(std::error_code());
    }
    else if (result == -1 && (errno == EINPROGRESS || errno == EWOULDBLOCK))
    {
        AsyncOperation operation;
        operation.socketPointer = this;
        operation.type = OperationType::CONNECT;

        operation.socket_handler = connection_handler_type(std::move(handler));

        context.register_operations(socketField, EPOLLOUT | EPOLLONESHOT, std::move(operation));
    }
    else
    {
        std::error_code errorCode(errno, std::system_category());
        close(socketField);
        socketField = -1;
        handler(errorCode);
    }
}

void TCPAsyncSocket::async_read(std::vector<char> &buffer, std::function<void(const std::error_code &, size_t)> handler)
{
    if (!is_open())
    {
        handler(std::make_error_code(std::errc::bad_file_descriptor), 0);
        return;
    }

    int64_t bytesRead = ::read(socketField, buffer.data(), buffer.size());

    if (bytesRead >= 0)
    {
        handler(std::error_code(), static_cast<size_t>(bytesRead));
    }
    else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            AsyncOperation operation;
            // operation.socketPointer = shared_from_this();
            operation.socketPointer = this;
            operation.type = OperationType::READ;
            operation.socket_handler = std::move(handler);

            operation.bufferArray = &buffer;
            operation.totalBytesRequested = buffer.size();

            context.register_operations(socketField, EPOLLIN | EPOLLONESHOT, std::move(operation));
        }
        else
        {
            handler(std::make_error_code(static_cast<std::errc>(errno)), 0);
        }
    }
}

void TCPAsyncSocket::async_write(std::vector<char> &buffer, std::function<void(const std::error_code &, size_t)> handler)
{
    if (!is_open())
    {
        handler(std::make_error_code(std::errc::bad_file_descriptor), 0);
        return;
    }

    int64_t bytesWritten = ::write(socketField, buffer.data(), buffer.size());

    if (bytesWritten >= 0)
    {
        handler(std::error_code(), static_cast<size_t>(bytesWritten));
    }
    else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            AsyncOperation operation;
            // operation.socketPointer = shared_from_this();
            operation.socketPointer = this;
            operation.type = OperationType::WRITE;
            operation.socket_handler = std::move(handler);
            operation.bufferArray = &buffer;
            operation.totalBytesRequested = buffer.size();

            context.register_operations(socketField, EPOLLOUT | EPOLLONESHOT, std::move(operation));
        }
    }
}

void TCPAsyncSocket::set_nonblocking()
{
    int flags = fcntl(socketField, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL");
    }
    if (fcntl(socketField, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL O_NONBLOCK");
    }
}

IOContext::IOContext() : epollManager()
{
    if (!epollManager.is_initialized())
        throw std::runtime_error("Failed to initialize Epoll instance");

    /*wakeupFD = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeupFD == -1)
    {
        throw std::runtime_error("Failed to create eventfd");
    }*/

    if (pipe(pipefd) == -1)
    {
        throw std::runtime_error("Failed to create pipe");
    }

    if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == -1 || fcntl(pipefd[1], F_SETFL, O_NONBLOCK) == -1) 
    {
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error("Failed to set pipe non-blocking");
    }

    if (epollManager.add(pipefd[0], EPOLLIN | EPOLLET) != EpollStatus::ES_SUCCESS)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error("Failed to add eventfd to Epoll");
    }
}

IOContext::~IOContext()
{
    /*if (wakeupFD != -1)
        close(wakeupFD);*/
    
    if (pipefd[0] != -1)
        close(pipefd[0]);
    if (pipefd[1] != -1)
        close(pipefd[1]);    
}

void IOContext::run()
{    
    std::stringstream stream;
    stream << "RUN START thread ip = " << std::this_thread::get_id() << "\n";
    std::cout << stream.str();
    stream.str(std::string());
    int event_count = 0;

    while (!stopRun.load(std::memory_order_relaxed) || workCount.load(std::memory_order_relaxed) > 0)
    {
        process_pending_tasks();

        if (stopRun.load(std::memory_order_relaxed) && workCount.load(std::memory_order_relaxed) == 0 && tasksQueue.empty())
            break;
       
        //int timeout = (stopRun.load(std::memory_order_relaxed) || workCount.load(std::memory_order_relaxed) == 0) ? 0 : -1;

        //int timeout = (stopRun.load(std::memory_order_relaxed) == true && workCount.load(std::memory_order_relaxed) == 0) ? 0 : -1;
        int timeout = (stopRun.load(std::memory_order_relaxed) || workCount.load(std::memory_order_relaxed) == 0) ? 0 : -1;
        //int timeout = -1;

        EpollStatus status = epollManager.wait(timeout, event_count);

        if (status == EpollStatus::ES_FAILED)
        {
            if (errno == EINTR)
                continue;
            if (stopRun.load(std::memory_order_relaxed))
                break;
            std::cerr << "Epoll wait failed." << std::endl;
            break;
        }

        for (int n = 0; n < event_count; ++n)
        {
            if (epollManager[n].data.fd == pipefd[0])
            {
                //handle_wakeup_event();
                handle_pipe_event();
                continue;
            }
            handle_event(epollManager[n]);
        }
    }
    process_pending_tasks();    
    stream << "RUN END thread ip = " << std::this_thread::get_id() << "\n";
    std::cout << stream.str();
}

void IOContext::stop()
{
    stopRun.store(true, std::memory_order_relaxed);
    /*uint64_t one = 1;
    write(wakeupFD, &one, sizeof(one));*/
    char byte = 'S';
    write(pipefd[1], &byte, sizeof(byte));
}

void IOContext::post(task_type task)
{
    inc_work();
    {
        std::lock_guard lock(tasksMutex);
        // tasksQueue.push(std::move(task));
        tasksQueue.push([t = std::move(task), this]()
                        {
            t();
            this->dec_work(); });
    }
    /*uint64_t one = 1;
    write(wakeupFD, &one, sizeof(one));*/
    char byte = 'P';
    write(pipefd[1], &byte, sizeof(byte));
}

void IOContext::register_operations(int sockId, uint32_t eventMask, AsyncOperation operation)
{
    inc_work();
    std::lock_guard lock(operationsMutex);

    EpollStatus status = epollManager.add(sockId, eventMask | EPOLLRDHUP | EPOLLERR);
    //EpollStatus status = epollManager.add(sockId, eventMask | EPOLLIN);

    if (status != EpollStatus::ES_SUCCESS)
    {
        throw std::runtime_error("Failed to add descriptor to Epoll");
    }

    pendingOperations[sockId] = std::move(operation);
}

void IOContext::deregister_operation(int sockId)
{
    {
        std::lock_guard lock(operationsMutex);

        EpollStatus status = epollManager.remove(sockId, 0);

        if (status != EpollStatus::ES_SUCCESS)
        {
            std::cerr << "epollManager.remove failed for sockId " << sockId << std::endl;
        }

        pendingOperations.erase(sockId);
    }
    dec_work();
}

void IOContext::inc_work()
{
    workCount.fetch_add(1, std::memory_order_relaxed);
    //size_t count = workCount.fetch_add(1, std::memory_order_relaxed) + 1;
    //std::cout << "inc_work: New count = " << count << std::endl;
}

void IOContext::dec_work()
{
    //workCount.fetch_sub(1, std::memory_order_relaxed);    
    size_t count = workCount.fetch_sub(1, std::memory_order_relaxed) - 1;
    std::cout << "dec_work: New count = " << count << std::endl;
    /*uint64_t one = 1;
    write(wakeupFD, &one, sizeof(one));  */ 
    char byte = 'D'; // Пишем 1 байт
    write(pipefd[1], &byte, sizeof(byte));
}

void IOContext::handle_event(const epoll_event &event)
{
    int sockId = event.data.fd;
    AsyncOperation operation;

    {
        std::unique_lock lock(operationsMutex);

        auto it = pendingOperations.find(sockId);
        if (it == pendingOperations.end())
            return;

        operation = std::move(it->second);

        pendingOperations.erase(it);

        lock.unlock();
    }

    std::error_code errorCode;
    size_t bytesTransfered = 0;

    if (event.events & (EPOLLERR | EPOLLHUP))
    {
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(sockId, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error != 0)
        {
            errorCode.assign(error, std::system_category());
        }
        else if (event.events & EPOLLHUP)
        {
            errorCode.assign(ECONNRESET, std::system_category());
        }
    }

    if (!errorCode)
    {
        if (operation.type == OperationType::READ)
        {
            ssize_t n = read(sockId,
                             operation.bufferArray->data(),
                             operation.totalBytesRequested);

            if (n > 0)
            {
                bytesTransfered = static_cast<size_t>(n);
            }
            else if (n == 0)
            {
                errorCode.assign(ECONNRESET, std::system_category());
            }
            else
            {
                if (errno != EWOULDBLOCK && errno != EAGAIN)
                {

                    errorCode.assign(errno, std::system_category());
                }
            }
        }
        else if (operation.type == OperationType::WRITE)
        {
            ssize_t n = write(sockId,
                              operation.bufferArray->data(),
                              operation.totalBytesRequested);

            if (n > 0)
            {

                bytesTransfered = static_cast<size_t>(n);
            }
            else if (n == -1)
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN)
                {
                    bytesTransfered = 0;
                }
                else
                {
                    errorCode.assign(errno, std::system_category());
                    bytesTransfered = 0;
                }
            }
        }
        else
        {
            int socketError = 0;
            socklen_t len = sizeof(socketError);

            if (getsockopt(sockId, SOL_SOCKET, SO_ERROR, &socketError, &len) == -1)
            {
                errorCode.assign(errno, std::system_category());
                bytesTransfered = 0;
            }
            else
            {
                if (socketError == 0)
                {
                }
                else
                {
                    errorCode.assign(socketError, std::system_category());
                }
            }
        }
    }

    std::visit(OperationInvoker{errorCode, bytesTransfered, operation.type},
               operation.socket_handler);

    dec_work();
}

/*void IOContext::handle_wakeup_event()
{
    uint64_t value;
    ssize_t bytesRead = read(wakeupFD, &value, sizeof(value));

    if (bytesRead != sizeof(value))
    {
        std::cerr << "Error handling eventfd wakeup event. Read " << bytesRead
                  << " bytes, expected " << sizeof(value) << ". Errno: " << errno << std::endl;
    }
}*/

void IOContext::handle_pipe_event()
{
    char buffer[128];   
    while (read(pipefd[0], buffer, sizeof(buffer)) > 0); 
}

void IOContext::process_pending_tasks()
{
    std::queue<task_type> localQueue;

    {
        std::lock_guard lock(tasksMutex);
        //std::swap(localQueue, tasksQueue);
        while(!tasksQueue.empty())
        {
            localQueue.push(tasksQueue.front());
            tasksQueue.pop();
        }
    }

    task_type task;

    while (!localQueue.empty())
    {
        task = std::move(localQueue.front());
        localQueue.pop();

        if (task)
            task();
    }
}
