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

inline sockaddr_in construct_address(in_port_t port)
{
    sockaddr_in address;
    std::memset(&address, 0, sizeof (address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = ::htons(port);
    return address;
}

}  // namespace detail

class HW2_WRAPPERS_EXPORT MainSocket : public detail::Socket
{
public:
    MainSocket(in_port_t port, int maxqueue)
        : detail::Socket()
        , m_address(detail::construct_address(port))
    {
        syscall_wrapper::setsockopt(fd);
        syscall_wrapper::bind(fd, m_address);
        syscall_wrapper::listen(fd, maxqueue);
    }

    const sockaddr_in& address() const
    {
        return m_address;
    }

private:
    sockaddr_in m_address;
};

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_SOCKET_HPP_
