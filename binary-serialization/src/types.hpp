#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>

namespace hw1
{

using byte_t = std::uint8_t;

using user_id_t = std::uint64_t;
inline constexpr user_id_t INVALID_USER_ID = user_id_t(-1);

}  // namespace hw1

#endif // TYPES_HPP
