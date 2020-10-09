#include "cbor_wrapper.hpp"
#include "types.hpp"

#include <string>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <stdexcept>
#include <cassert>

namespace hw1
{

namespace cbor
{

Item::Item(cbor_item_t* item)
    : m_item(item)
{
}

Item::Item(const Buffer& buffer)
{
    cbor_load_result result;
    m_item = cbor_load(buffer.data(), buffer.size(), &result);
    if (result.error.code != CBOR_ERR_NONE)
        throw std::runtime_error("Cbor deserialization error " + std::to_string(result.error.code));
}

Item::Item(const Item& t_other)
    : m_item(t_other.m_item)
{
    cbor_incref(m_item);
}

Item& Item::operator=(const Item& t_other)
{
    if (&t_other == this)
        return *this;

    dtor_impl();

    m_item = t_other.m_item;
    cbor_incref(m_item);

    return *this;
}

Item::Item(Item&& t_other) noexcept
    : m_item(t_other.m_item)
{
    t_other.m_item = nullptr;
}

Item& Item::operator=(Item&& t_other) noexcept
{
    if (&t_other == this)
        return *this;

    dtor_impl();

    m_item = t_other.m_item;
    t_other.m_item = nullptr;

    return *this;
}

Item::~Item()
{
    dtor_impl();
}

void Item::dtor_impl()
{
    if (m_item != nullptr)
        cbor_decref(&m_item);
}

Item::operator const cbor_item_t*() const
{
    return m_item;
}

Item::operator cbor_item_t*()
{
    return m_item;
}

Buffer::Buffer(const Buffer& t_other)
    : m_size(t_other.m_size)
    , m_capacity(m_size)
    , m_buffer(reinterpret_cast<byte_t*>(std::malloc(m_size)))
{
    std::memcpy(m_buffer, t_other.m_buffer, m_size);
}

Buffer& Buffer::operator=(const Buffer& t_other)
{
    if (&t_other == this)
        return *this;

    m_size = t_other.m_size;
    m_capacity = m_size;
    m_buffer = reinterpret_cast<byte_t*>(std::malloc(m_size));
    std::memcpy(m_buffer, t_other.m_buffer, m_size);

    return *this;
}

Buffer::Buffer(Buffer&& t_other) noexcept
    : m_size(t_other.m_size)
    , m_capacity(t_other.m_capacity)
    , m_buffer(t_other.m_buffer)
{
    t_other.m_size = 0;
    t_other.m_capacity = 0;
    t_other.m_buffer = nullptr;
}

Buffer& Buffer::operator=(Buffer&& t_other) noexcept
{
    if (&t_other == this)
        return *this;

    dtor_impl();

    m_size = t_other.m_size;
    m_capacity = t_other.m_capacity;
    m_buffer = t_other.m_buffer;

    t_other.m_size = 0;
    t_other.m_capacity = 0;
    t_other.m_buffer = nullptr;

    return *this;
}

std::size_t Buffer::size() const
{
    return m_size;
}

cbor_data Buffer::data() const
{
    return m_buffer;
}

cbor_mutable_data Buffer::data()
{
    return m_buffer;
}

Buffer::~Buffer()
{
    dtor_impl();
}

void Buffer::dtor_impl()
{
    if (m_buffer != nullptr)
        std::free(m_buffer);
}

Buffer::Buffer(const cbor_item_t* t_item)
{
    m_size = cbor_serialize_alloc(t_item, &m_buffer, &m_capacity);
}

Item build_uint8(std::uint8_t t_value)
{
    return Item(cbor_build_uint8(t_value));
}

Item build_uint16(std::uint16_t t_value)
{
    return Item(cbor_build_uint16(t_value));
}

Item build_uint32(std::uint32_t t_value)
{
    return Item(cbor_build_uint32(t_value));
}

Item build_uint64(std::uint64_t t_value)
{
    return Item(cbor_build_uint64(t_value));
}

Item build_negint8(std::uint8_t t_value)
{
    return Item(cbor_build_negint8(t_value));
}

Item build_negint16(std::uint16_t t_value)
{
    return Item(cbor_build_negint16(t_value));
}

Item build_negint32(std::uint32_t t_value)
{
    return Item(cbor_build_negint32(t_value));
}

Item build_negint64(std::uint64_t t_value)
{
    return Item(cbor_build_negint64(t_value));
}

Item new_definite_array(std::size_t t_size)
{
    return Item(cbor_new_definite_array(t_size));
}

Item new_indefinite_array()
{
    return Item(cbor_new_indefinite_array());
}

Item build_string(const std::string& t_str)
{
    return Item(cbor_build_stringn(t_str.c_str(), t_str.size()));
}

Item build_string(std::string_view t_str)
{
    return Item(cbor_build_stringn(t_str.data(), t_str.size()));
}

Item build_string(const char* t_val)
{
    return Item(cbor_build_string(t_val));
}

Item build_string(const char* t_val, std::size_t t_length)
{
    return Item(cbor_build_stringn(t_val, t_length));
}

Item build_bytestring(cbor_data t_handle, std::size_t t_length)
{
    return Item(cbor_build_bytestring(t_handle, t_length));
}

}  // namespace cbor

}  // namespace hw1
