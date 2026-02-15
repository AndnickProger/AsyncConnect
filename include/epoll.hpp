#pragma once

#include <sys/epoll.h>
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <unistd.h>

enum class EpollStatus
{
    ES_SUCCESS,
    ES_FAILED,
    ES_TIMEOUT_HAS_EXPIRED
};

std::ostream &operator<<(std::ostream &stream, EpollStatus epollStatus);

std::string epollStatusToString(const EpollStatus &epollStatus);

class Epoll
{
public:
    using epoll_event_type = struct epoll_event;
    using event_type = uint32_t;
    using descriptor_type = int;
    using epoll_type = int;
    using max_type = int;

    Epoll() : maxEpollEvents(35'000), epollField(epoll_create(maxEpollEvents)), eventsField(maxEpollEvents)
    {
    }

    Epoll(const max_type &maxEpollEvents_) : maxEpollEvents(maxEpollEvents_), epollField(epoll_create(maxEpollEvents)), eventsField(maxEpollEvents)
    {
    }

    Epoll(const Epoll &other) = delete;
    Epoll &operator=(const Epoll &other) = delete;

    Epoll(Epoll &&other) noexcept : maxEpollEvents(std::move(other.maxEpollEvents)),
                                    epollField(std::move(other.epollField)),
                                    eventsField(std::move(other.eventsField))
    {
    }

    Epoll &operator=(Epoll &&other) noexcept
    {
        maxEpollEvents = std::move(other.maxEpollEvents);
        epollField = std::move(other.epollField);
        eventsField = std::move(other.eventsField);

        return *this;
    }

    virtual ~Epoll();

    [[nodiscard]] inline max_type max_events() const noexcept
    {
        return maxEpollEvents;
    }

    [[nodiscard]] inline bool is_initialized() const noexcept
    {
        return epollField != -1;
    }

    [[nodiscard]] inline epoll_type epoll() const noexcept
    {
        return epollField;
    }

    [[nodiscard]] inline std::vector<epoll_event_type> &events() const noexcept
    {
        return const_cast<std::vector<epoll_event_type> &>(eventsField);
    }

    [[nodiscard]] inline std::vector<epoll_event_type> &events() noexcept
    {
        return eventsField;
    }

    EpollStatus add(const epoll_event_type& event);

    EpollStatus mod(const epoll_event_type& event);

    EpollStatus remove(const epoll_event_type& event);

    EpollStatus add(const descriptor_type &descriptor, const event_type &event);

    EpollStatus mod(const descriptor_type &descriptor, const event_type &event);

    EpollStatus remove(const descriptor_type &descriptor, const event_type &event);

    EpollStatus wait(const int &timeout, int &count);

    [[nodiscard]] inline epoll_event_type &at(const std::size_t &index)
    {
        if (index >= 0 && index < maxEpollEvents)
        {
            return eventsField[index];
        }
        else
        {
            std::stringstream stream;
            stream << "Index out of range (0, ";
            stream << std::to_string(maxEpollEvents);
            stream << ")";
            throw std::invalid_argument(stream.str());
        }
    }

    [[nodiscard]] inline const epoll_event_type &at(const std::size_t &index) const
    {
        if (index >= 0 && index < maxEpollEvents)
        {
            return const_cast<const epoll_event_type &>(eventsField[index]);
        }
        else
        {
            std::stringstream stream;
            stream << "Index out of range (0, ";
            stream << std::to_string(maxEpollEvents);
            stream << ")";
            throw std::invalid_argument(stream.str());
        }
    }

    [[nodiscard]] inline epoll_event_type &operator[](const std::size_t &index) noexcept
    {
        return eventsField[index];
    }

private:
    max_type maxEpollEvents;
    epoll_type epollField;
    std::vector<epoll_event_type> eventsField;
};