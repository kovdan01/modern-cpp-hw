#ifndef HW1_BINARY_SERIALIZATION_WRAPPERS_CBOR_WRAPPER_HPP_
#define HW1_BINARY_SERIALIZATION_WRAPPERS_CBOR_WRAPPER_HPP_

#include <cbor.h>

#include <string>
#include <vector>

namespace hw1::cbor
{

class Buffer
{
public:
    Buffer() noexcept = default;
    inline Buffer(const Buffer&);
    inline Buffer& operator=(const Buffer&);
    inline Buffer(Buffer&&) noexcept;
    inline Buffer& operator=(Buffer&&) noexcept;
    inline ~Buffer();

    inline Buffer(const cbor_item_t* item);

    [[nodiscard]] inline std::size_t size() const noexcept;
    [[nodiscard]] inline cbor_data data() const noexcept;
    [[nodiscard]] inline cbor_mutable_data data() noexcept;

private:
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
    cbor_mutable_data m_buffer = nullptr;

    void dtor_impl() noexcept;
};

class Item
{
public:
    explicit inline Item(cbor_item_t* item) noexcept;
    inline Item(const Buffer& buffer);
    inline Item(const Item&) noexcept;
    inline Item& operator=(const Item&) noexcept;
    inline Item(Item&&) noexcept;
    inline Item& operator=(Item&&) noexcept;

    inline ~Item();

    inline operator const cbor_item_t*() const noexcept;
    inline operator cbor_item_t*() noexcept;

    inline void array_push(cbor_item_t* item);

private:
    cbor_item_t* m_item = nullptr;

    inline void dtor_impl();
};

inline Item build_uint8(std::uint8_t value);
inline Item build_uint16(std::uint16_t value);
inline Item build_uint32(std::uint32_t value);
inline Item build_uint64(std::uint64_t value);
inline Item build_negint8(std::uint8_t value);
inline Item build_negint16(std::uint16_t value);
inline Item build_negint32(std::uint32_t value);
inline Item build_negint64(std::uint64_t value);

inline Item new_definite_array(std::size_t size);
inline Item new_indefinite_array();

inline Item build_string(const std::string& str);
inline Item build_string(std::string_view str);
inline Item build_string(const char* val);
inline Item build_string(const char* val, std::size_t length);

inline Item build_bytestring(cbor_data handle, std::size_t length);

}  // namespace hw1::cbor

#include "cbor_wrapper.inc"

#endif  // HW1_BINARY_SERIALIZATION_WRAPPERS_CBOR_WRAPPER_HPP_
