#ifndef HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_
#define HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <stdexcept>

namespace hw2::syscall_wrapper
{

class Error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
    ~Error() override;
};

inline int socket()
{
    int sfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
    {
        std::perror("socket");
        throw Error("socket");
    }
    return sfd;
}

inline void close(int fd)
{
    if (::close(fd) == -1)
    {
        std::perror("close");
        throw Error("close");
    }
}

inline void bind(int fd, const sockaddr_in& address)
{
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof (address)) == -1)
    {
        std::perror("bind");
        throw Error("bind");
    }
}

inline void listen(int fd)
{
    if (::listen(fd, 1) < 0)
    {
        std::perror("listen");
        throw Error("listen");
    }
}

}  // namespace hw2::syscall_wrapper

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_
