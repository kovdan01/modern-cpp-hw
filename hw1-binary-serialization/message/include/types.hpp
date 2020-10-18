#ifndef BINARY_SERIALIZATION_TYPES_HPP_
#define BINARY_SERIALIZATION_TYPES_HPP_

#include <cstdint>

namespace hw1
{

using byte_t = std::uint8_t;

using user_id_t = std::uint64_t;
inline constexpr user_id_t INVALID_USER_ID = user_id_t(-1);

}  // namespace hw1

#endif // BINARY_SERIALIZATION_TYPES_HPP_
