#include <socket.hpp>

namespace hw2
{

sockaddr_in construct_address_ipv4(in_addr addr, in_port_t port)
{
    sockaddr_in address;
    std::memset(&address, 0, sizeof (address));
    address.sin_family = AF_INET;
    std::memcpy(&address.sin_addr, &addr, sizeof(in_addr));
    address.sin_port = port;
    return address;
}

sockaddr_in6 construct_address_ipv6(in6_addr addr, in_port_t port)
{
    sockaddr_in6 address;
    std::memset(&address, 0, sizeof (address));
    address.sin6_family = AF_INET6;
    std::memcpy(&address.sin6_addr, &addr, sizeof(in6_addr));
    address.sin6_port = port;
    return address;
}

Socket::Socket(int fd)
    : fd(fd)
{
}

Socket::~Socket()
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

SocketIPv4::SocketIPv4(in_addr addr, in_port_t port)
    : Socket(syscall_wrapper::socket_ipv4())
    , m_address(construct_address_ipv4(addr, port))
{
}

SocketIPv4::~SocketIPv4() = default;

const sockaddr* SocketIPv4::address() const
{
    return reinterpret_cast<const sockaddr*>(&m_address);
}

sockaddr* SocketIPv4::address()
{
    return reinterpret_cast<sockaddr*>(&m_address);
}

socklen_t SocketIPv4::address_length() const
{
    return sizeof(m_address);
}

SocketIPv6::SocketIPv6(in6_addr addr, in_port_t port)
    : Socket(syscall_wrapper::socket_ipv6())
    , m_address(construct_address_ipv6(addr, port))
{
}

SocketIPv6::~SocketIPv6() = default;

const sockaddr* SocketIPv6::address() const
{
    return reinterpret_cast<const sockaddr*>(&m_address);
}

sockaddr* SocketIPv6::address()
{
    return reinterpret_cast<sockaddr*>(&m_address);
}

socklen_t SocketIPv6::address_length() const
{
    return sizeof(m_address);
}

MainSocket::MainSocket(in_port_t port, std::size_t maxqueue)
    : SocketIPv4({INADDR_ANY}, ::htons(port))
{
    syscall_wrapper::setsockopt_reuseaddr(fd);
    syscall_wrapper::bind(fd, m_address);
    syscall_wrapper::listen(fd, maxqueue);
}

MainSocket::~MainSocket() = default;

}  // namespace hw2
