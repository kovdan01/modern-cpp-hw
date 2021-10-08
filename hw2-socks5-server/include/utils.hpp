#ifndef HW2_SOCKS5_SERVER_SERVER_UTILS_HPP_
#define HW2_SOCKS5_SERVER_SERVER_UTILS_HPP_

#define LIKELY(x)      __builtin_expect(!!(x), 1)
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)

#include <spdlog/spdlog.h>

namespace hw2
{

std::shared_ptr<spdlog::logger> logger();

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_SERVER_UTILS_HPP_
