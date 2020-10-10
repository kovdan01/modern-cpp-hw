#ifndef BINARY_SERIALIZATION_CBOR_WRAPPER_HPP_
#define BINARY_SERIALIZATION_CBOR_WRAPPER_HPP_

#include "types.hpp"

#include <cbor.h>

#include <string>
#include <vector>

namespace hw1
{

namespace cbor
{

class Buffer
{
public:
    Buffer() = default;
    Buffer(const Buffer&);
    Buffer& operator=(const Buffer&);
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;
    ~Buffer();

    Buffer(const cbor_item_t* item);

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] cbor_data data() const;
    cbor_mutable_data data();

private:
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
    cbor_mutable_data m_buffer = nullptr;

    void dtor_impl();
};

class Item
{
public:
    explicit Item(cbor_item_t* item);
    Item(const Buffer& buffer);
    Item(const Item&);
    Item& operator=(const Item&);
    Item(Item&&) noexcept;
    Item& operator=(Item&&) noexcept;

    ~Item();

    operator const cbor_item_t*() const;
    operator cbor_item_t*();

private:
    cbor_item_t* m_item = nullptr;

    void dtor_impl();
};

Item build_uint8(std::uint8_t value);
Item build_uint16(std::uint16_t value);
Item build_uint32(std::uint32_t value);
Item build_uint64(std::uint64_t value);
Item build_negint8(std::uint8_t value);
Item build_negint16(std::uint16_t value);
Item build_negint32(std::uint32_t value);
Item build_negint64(std::uint64_t value);

Item new_definite_array(std::size_t size);
Item new_indefinite_array();

Item build_string(const std::string& str);
Item build_string(std::string_view str);
Item build_string(const char* val);
Item build_string(const char* val, std::size_t length);

Item build_bytestring(cbor_data handle, std::size_t length);

}  // namespace cbor

}  // namespace hw1

#endif // BINARY_SERIALIZATION_CBOR_WRAPPER_HPP_
