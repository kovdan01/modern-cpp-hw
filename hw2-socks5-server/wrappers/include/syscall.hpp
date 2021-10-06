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

HW2_WRAPPERS_EXPORT int socket(int address_family);
HW2_WRAPPERS_EXPORT int socket_ipv4();
HW2_WRAPPERS_EXPORT int socket_ipv6();
HW2_WRAPPERS_EXPORT void close(int fd);
HW2_WRAPPERS_EXPORT void bind(int fd, const sockaddr_in& address);
HW2_WRAPPERS_EXPORT void listen(int fd, int maxqueue);
HW2_WRAPPERS_EXPORT void setsockopt_reuseaddr(int fd);

}  // namespace hw2::syscall_wrapper

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_
