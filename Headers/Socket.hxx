#pragma once

#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <format>
#include <cerrno>
#include <cstring>

/* RAII-structure for socket */

class Socket
{
public:
    Socket () {
        _fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
        if (_fd < 0)
            throw std::runtime_error(std::format("Failed to create socket: {}",
                                                 std::strerror(errno)));
    }

    ~Socket() {
        close(_fd);
    }

    // forbid copying
    Socket (const Socket &) = delete;
    Socket & operator= (const Socket &) = delete;

    // allow moving
    Socket (Socket &&other) noexcept {
        _fd = other._fd;
        other._fd = -1;
    }

    Socket & operator= (Socket &&other) noexcept {
        close(_fd);
        _fd = other._fd;
        other._fd = -1;
        return *this;
    }

    inline int fd () const { return _fd; }

private:
    int _fd;
};


