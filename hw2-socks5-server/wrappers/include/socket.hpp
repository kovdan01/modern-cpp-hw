#ifndef HW2_SOCKS5_SERVER_WRAPPERS_SOCKET_HPP_
#define HW2_SOCKS5_SERVER_WRAPPERS_SOCKET_HPP_

#include <hw2-wrappers/export.h>
#include <syscall.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace hw2
{

class HW2_WRAPPERS_EXPORT Socket
{
public:
    Socket() = delete;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(int fd)
        : fd(fd)
    {
    }

    virtual const sockaddr* address() const = 0;
    virtual sockaddr* address() = 0;
    virtual socklen_t address_length() const = 0;

    virtual ~Socket()
    {
        try
        {
            syscall_wrapper::close(fd);
        }
        catch (...)
        {
            std::exit(EXIT_FAILURE);
        }
    }

    const int fd;
};

namespace detail
{

inline sockaddr_in construct_address_ipv4(in_addr addr, in_port_t port)
{
    sockaddr_in address;
    std::memset(&address, 0, sizeof (address));
    address.sin_family = AF_INET;
    std::memcpy(&address.sin_addr, &addr, sizeof(in_addr));
    address.sin_port = port;
    return address;
}

inline sockaddr_in6 construct_address_ipv6(in6_addr addr, in_port_t port)
{
    sockaddr_in6 address;
    std::memset(&address, 0, sizeof (address));
    address.sin6_family = AF_INET6;
    std::memcpy(&address.sin6_addr, &addr, sizeof(in6_addr));
    address.sin6_port = port;
    return address;
}

inline sockaddr_in construct_address_local(in_port_t port)
{
    return construct_address_ipv4({INADDR_ANY}, ::htons(port));
}

}  // namespace detail

class HW2_WRAPPERS_EXPORT SocketIPv4 : public Socket
{
public:
    SocketIPv4(in_addr addr, in_port_t port)
        : Socket(syscall_wrapper::socket4())
        , m_address(detail::construct_address_ipv4(addr, port))
    {
    }

    ~SocketIPv4() override = default;

    const sockaddr* address() const override
    {
        return reinterpret_cast<const sockaddr*>(&m_address);
    }

    sockaddr* address() override
    {
        return reinterpret_cast<sockaddr*>(&m_address);
    }

    socklen_t address_length() const override
    {
        return sizeof(m_address);
    }

protected:
    sockaddr_in m_address;
};

class HW2_WRAPPERS_EXPORT SocketIPv6 : public Socket
{
public:
    SocketIPv6(in6_addr addr, in_port_t port)
        : Socket(syscall_wrapper::socket6())
        , m_address(detail::construct_address_ipv6(addr, port))
    {
    }

    ~SocketIPv6() override = default;

    const sockaddr* address() const override
    {
        return reinterpret_cast<const sockaddr*>(&m_address);
    }

    sockaddr* address() override
    {
        return reinterpret_cast<sockaddr*>(&m_address);
    }

    socklen_t address_length() const override
    {
        return sizeof(m_address);
    }

protected:
    sockaddr_in6 m_address;
};

class HW2_WRAPPERS_EXPORT MainSocket : public SocketIPv4
{
public:
    MainSocket(in_port_t port, int maxqueue)
        : SocketIPv4({INADDR_ANY}, ::htons(port))
    {
        syscall_wrapper::setsockopt(fd);
        syscall_wrapper::bind(fd, m_address);
        syscall_wrapper::listen(fd, maxqueue);
    }
};

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_SOCKET_HPP_
