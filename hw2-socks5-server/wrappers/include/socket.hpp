#ifndef HW2_SOCKS5_SERVER_WRAPPERS_SOCKET_HPP_
#define HW2_SOCKS5_SERVER_WRAPPERS_SOCKET_HPP_

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

class Socket
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

}  // namespace detail

class ReceiveSocket : public detail::Socket
{
public:
    ReceiveSocket(std::string_view host, in_port_t port)
        : detail::Socket()
    {
        std::memset(&m_address, 0, sizeof (m_address));
        m_address.sin_family = AF_INET;
        m_address.sin_port = ::htons(port);
        m_address.sin_addr.s_addr = ::inet_addr(host.data());

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

class SendSocket : public detail::Socket
{
public:
    SendSocket()
        : detail::Socket()
    {
    }
};

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_SOCKET_HPP_
