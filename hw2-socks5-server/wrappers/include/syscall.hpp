#ifndef HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_
#define HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_

#include <hw2-wrappers/export.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <stdexcept>

namespace hw2::syscall_wrapper
{

class HW2_WRAPPERS_EXPORT Error : public std::runtime_error
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

inline void connect(int fd, const sockaddr_in& address)
{
    if (::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof (address)) == -1)
    {
        std::perror("connect");
        throw Error("connect");
    }
}

inline void setsockopt(int fd)
{
    static constexpr int sockoptval = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof (sockoptval)) == -1)
    {
        std::perror("setsockopt");
        throw Error("setsockopt");
    }
}

}  // namespace hw2::syscall_wrapper

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_
