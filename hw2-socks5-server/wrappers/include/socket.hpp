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

namespace detail
{

class HW2_WRAPPERS_EXPORT Socket
{
public:
    Socket()
        : fd(syscall_wrapper::socket())
    {
    }

    ~Socket()
    {
        try
        {
            syscall_wrapper::close(fd);
        }
        catch (const std::exception& e)
        {
            // TODO
        }
        catch (...)
        {
            // TODO
        }
    }

    const int fd;
};

inline sockaddr_in construct_address(std::string_view host, in_port_t port)
{
    sockaddr_in address;
    std::memset(&address, 0, sizeof (address));
    address.sin_family = AF_INET;
    address.sin_port = ::htons(port);
    address.sin_addr.s_addr = ::inet_addr(host.data());
    return address;
}

}  // namespace detail

class HW2_WRAPPERS_EXPORT ReceiveSocket : public detail::Socket
{
public:
    ReceiveSocket(in_port_t port)
        : detail::Socket()
        , m_address(detail::construct_address("127.0.0.1", port))
    {
        syscall_wrapper::setsockopt(fd);
        syscall_wrapper::bind(fd, m_address);
        syscall_wrapper::listen(fd);
    }

    const sockaddr_in& address() const
    {
        return m_address;
    }

private:
    sockaddr_in m_address;
};

class HW2_WRAPPERS_EXPORT SendSocket : public detail::Socket
{
public:
    SendSocket(std::string_view host, in_port_t port)
        : detail::Socket()
        , m_address(detail::construct_address(host, port))
    {
        syscall_wrapper::connect(fd, m_address);
    }

private:
    sockaddr_in m_address;
};

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_SOCKET_HPP_
