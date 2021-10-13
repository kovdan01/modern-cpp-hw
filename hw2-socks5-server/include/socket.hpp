#ifndef HW2_SOCKS5_SERVER_SOCKET_HPP_
#define HW2_SOCKS5_SERVER_SOCKET_HPP_

#include <syscall.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace hw2
{

class Socket
{
public:
    Socket() = delete;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(int fd);
    virtual ~Socket();

    [[nodiscard]] virtual const sockaddr* address() const = 0;
    [[nodiscard]] virtual sockaddr* address() = 0;
    [[nodiscard]] virtual socklen_t address_length() const = 0;

    [[nodiscard]] int fd() const;
    void close();

protected:
    int m_fd = -1;
};

class SocketIPv4 : public Socket
{
public:
    SocketIPv4(in_addr addr, in_port_t port);
    ~SocketIPv4() override;

    [[nodiscard]] const sockaddr* address() const override;
    [[nodiscard]] sockaddr* address() override;
    [[nodiscard]] socklen_t address_length() const override;

protected:
    sockaddr_in m_address;
};

class SocketIPv6 : public Socket
{
public:
    SocketIPv6(in6_addr addr, in_port_t port);
    ~SocketIPv6() override;

    [[nodiscard]] const sockaddr* address() const override;
    [[nodiscard]] sockaddr* address() override;
    [[nodiscard]] socklen_t address_length() const override;

protected:
    sockaddr_in6 m_address;
};

class MainSocket : public SocketIPv4
{
public:
    MainSocket(in_port_t port, int maxqueue);
    ~MainSocket() override;
};

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_SOCKET_HPP_
