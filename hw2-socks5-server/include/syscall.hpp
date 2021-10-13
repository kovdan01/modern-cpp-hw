#ifndef HW2_SOCKS5_SERVER_SYSCALL_HPP_
#define HW2_SOCKS5_SERVER_SYSCALL_HPP_

#include <arpa/inet.h>
#include <sys/resource.h>

#include <stdexcept>
#include <string>

namespace hw2::syscall_wrapper
{

class Error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
    Error(const std::string& error_message, int error_code);
    ~Error() override;

    const int error_code;
};

int socket(int address_family);
int socket_ipv4();
int socket_ipv6();
void close(int fd);
void bind(int fd, const sockaddr_in& address);
void listen(int fd, int maxqueue);
void setsockopt_reuseaddr(int fd);
rlimit getrlimit_nofile();
void setrlimit_nofile(rlimit file_limit);
rlimit getrlimit_memlock();
void setrlimit_memlock(rlimit memory_limit);

}  // namespace hw2::syscall_wrapper

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_SYSCALL_HPP_
