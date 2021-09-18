#ifndef HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_
#define HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_

#include <hw2-wrappers/export.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <iostream>
#include <span>
#include <stdexcept>
#include <thread>

namespace hw2
{

using byte_t = unsigned char;

}  // namespace hw2

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
    while (::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof (address)) == -1)
    {
        std::perror("connect error");
        std::cerr << "connect retry..." << std::endl;
        //throw Error("connect");
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

inline void send(int fd, std::span<const byte_t> data)
{
    // TODO: send in while (data might not be fully transported)
    std::size_t total_sent = 0;
    for (;;)
    {
        ssize_t len = ::send(fd, data.data(), data.size(), 0);
        if (len == -1)
        {
            std::perror("send");
            throw Error("send");
        }
        total_sent += len;
        if (total_sent == data.size())
            break;
    }
    ssize_t len = ::send(fd, data.data(), data.size(), 0);
    if (len != data.size())
    {
        std::cerr << "tried to send " << data.size() << " bytes, got " << len << std::endl;
        //std::perror("send");
        //throw Error("send");
    }
}

inline ssize_t recv(int fd, std::span<byte_t> data)
{
    // TODO: recv in while (data might not be fully transported)
    ssize_t len = 0;
    while (len == 0)
    {
        len = ::recv(fd, data.data(), data.size(), 0/*MSG_WAITALL*/);
        if (len == -1)
        {
            std::perror("recv");
            throw Error("recv");
        }
        if (len != data.size())
        {
            std::cerr << "tried to recv " << data.size() << " bytes, got " << len << std::endl;
            //std::perror("recv");
            //throw Error("recv");
        }
        //break;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    return len;
}

}  // namespace hw2::syscall_wrapper

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_
