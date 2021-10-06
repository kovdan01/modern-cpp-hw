#include <syscall.hpp>

namespace hw2::syscall_wrapper
{

Error::~Error() = default;

int socket(int address_family)
{
    int sfd = ::socket(address_family, SOCK_STREAM, 0);
    if (sfd == -1)
    {
        std::perror("socket");
        throw Error("socket");
    }
    return sfd;
}

int socket_ipv4()
{
    return socket(AF_INET);
}

int socket_ipv6()
{
    return socket(AF_INET6);
}

void close(int fd)
{
    if (::close(fd) == -1)
    {
        std::perror("close");
        throw Error("close");
    }
}

void bind(int fd, const sockaddr_in& address)
{
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof (address)) == -1)
    {
        std::perror("bind");
        throw Error("bind");
    }
}

void listen(int fd, int maxqueue)
{
    if (::listen(fd, maxqueue) < 0)
    {
        std::perror("listen");
        throw Error("listen");
    }
}

void setsockopt_reuseaddr(int fd)
{
    static constexpr int sockoptval = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof (sockoptval)) == -1)
    {
        std::perror("setsockopt");
        throw Error("setsockopt");
    }
}

}  // namespace hw2::syscall_wrapper
