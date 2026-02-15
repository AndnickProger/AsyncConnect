# AsyncConnect
An asynchronous C++ client for obtaining currency exchange rates from the website of the Central Bank of the Russian Federation (cbr.ru), built on its own implementations of key components (network layer, event loop, and parsing).
Key self-written components

 Network layer

 EndpointIPv4 is an abstraction of a network endpoint (IP + port) with secure initialization and conversion to system sockaddr_in structures.

 TCPAsyncSocket is an asynchronous TCP socket with non-blocking operations:

 async_connect() — asynchronous connection;

 async_write() — sending data without blocking;

 async_read() — reads the response to the buffer.

 Handle management via system calls (socket(), connect(), read()/write()).

 The event cycle

 IOContext is a proprietary multiplexing mechanism based on epoll:

 Registration/deletion of observed descriptors;

 Event handling (read, write, connect);

 Multithreaded execution (std::thread
