#include "epoll.hpp"

Epoll::~Epoll()
{
    close(epollField);
}

EpollStatus Epoll::add(const epoll_event_type &event)
{
    const auto result = epoll_ctl(epollField, EPOLL_CTL_ADD, event.data.fd, const_cast<epoll_event_type *>(&event));
    if(result == 0) return EpollStatus::ES_SUCCESS;
    return EpollStatus::ES_FAILED;
}

EpollStatus Epoll::mod(const epoll_event_type &event)
{
    const auto result = epoll_ctl(epollField, EPOLL_CTL_MOD, event.data.fd, const_cast<epoll_event_type *>(&event));
    if(result == 0) return EpollStatus::ES_SUCCESS;
    return EpollStatus::ES_FAILED;
}

EpollStatus Epoll::remove(const epoll_event_type &event)
{
    const auto result = epoll_ctl(epollField, EPOLL_CTL_DEL, event.data.fd, const_cast<epoll_event_type*>(&event));
    if(result == 0) return EpollStatus::ES_SUCCESS;
    return EpollStatus::ES_FAILED;
}

EpollStatus Epoll::add(const descriptor_type &descriptor, const Epoll::event_type &event)
{
    struct epoll_event epollEvent;
    epollEvent.data.fd = descriptor;
    epollEvent.events = event;

   return add(epollEvent);
}

EpollStatus Epoll::mod(const descriptor_type &descriptor, const Epoll::event_type  &event)
{
    struct epoll_event epollEvent;
    epollEvent.data.fd = descriptor;
    epollEvent.events = event;

    return mod(epollEvent);
}

EpollStatus Epoll::remove(const descriptor_type &descriptor, const Epoll::event_type &event)
{
    struct epoll_event epollEvent;
    epollEvent.data.fd = descriptor;
    epollEvent.events = event;

    return remove(epollEvent);
}

EpollStatus Epoll::wait(const int &timeout, int &count)
{
    count = epoll_wait(epollField, eventsField.data(), maxEpollEvents, timeout);
    if(count > 0)
    {
        return EpollStatus::ES_SUCCESS;
    }else if(count < 0)
    {
        return EpollStatus::ES_FAILED;
    }else
    {
        return EpollStatus::ES_TIMEOUT_HAS_EXPIRED;
    }
}

std::ostream &operator << (std::ostream &stream, EpollStatus epollStatus)
{
    stream << epollStatusToString(epollStatus);
    return stream;
}

std::string epollStatusToString(const EpollStatus &epollStatus)
{
    switch (epollStatus)
    {
    case EpollStatus::ES_SUCCESS: return std::string("ES_SUCCESS");
    
    case EpollStatus::ES_FAILED: return std::string("ES_FAILED");

    case EpollStatus::ES_TIMEOUT_HAS_EXPIRED: return std::string("ES_TIMEOUT_HAS_EXPIRED");
    
    default: return std::string("UNKNOWN");
    }
}

