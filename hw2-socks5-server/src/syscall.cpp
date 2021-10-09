#include <syscall.hpp>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace hw2::syscall_wrapper
{

Error::Error(const std::string& error_message, int error_code)
    : std::runtime_error(error_message)
    , error_code(error_code)
{
}

Error::~Error() = default;

int socket(int address_family)
{
    int sfd = ::socket(address_family, SOCK_STREAM, 0);
    if (sfd == -1)
    {
        std::perror("socket");
        throw Error("socket", errno);
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
        throw Error("close", errno);
    }
}

void bind(int fd, const sockaddr_in& address)
{
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof (address)) == -1)
    {
        std::perror("bind");
        throw Error("bind", errno);
    }
}

void listen(int fd, int maxqueue)
{
    if (::listen(fd, maxqueue) < 0)
    {
        std::perror("listen");
        throw Error("listen", errno);
    }
}

void setsockopt_reuseaddr(int fd)
{
    static constexpr int sockoptval = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof (sockoptval)) == -1)
    {
        std::perror("setsockopt");
        throw Error("setsockopt", errno);
    }
}

rlimit getrlimit_nofile()
{
    rlimit file_limit;
    if (::getrlimit(RLIMIT_NOFILE, &file_limit) != 0)
    {
        std::perror("getrlimit");
        throw Error("getrlimit", errno);
    }
    return file_limit;
}

void setrlimit_nofile(rlimit file_limit)
{
    if (::setrlimit(RLIMIT_NOFILE, &file_limit) != 0)
    {
        std::perror("setrlimit");
        throw Error("setrlimit", errno);
    }
}

}  // namespace hw2::syscall_wrapper
