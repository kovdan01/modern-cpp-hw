#include "cbor_wrapper.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace hw1::cbor
{

Item::Item(cbor_item_t* item) noexcept
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

Item::Item(const Item& other) noexcept
    : m_item(other.m_item)
{
    cbor_incref(m_item);
}

Item& Item::operator=(const Item& other) noexcept
{
    if (&other == this)
        return *this;

    dtor_impl();

    m_item = other.m_item;
    cbor_incref(m_item);

    return *this;
}

Item::Item(Item&& other) noexcept
    : m_item(other.m_item)
{
    other.m_item = nullptr;
}

Item& Item::operator=(Item&& other) noexcept
{
    if (&other == this)
        return *this;

    dtor_impl();

    m_item = other.m_item;
    other.m_item = nullptr;

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

Item::operator const cbor_item_t*() const noexcept
{
    return m_item;
}

Item::operator cbor_item_t*() noexcept
{
    return m_item;
}

void Item::array_push(cbor_item_t* item)
{
    if (!cbor_array_push(m_item, item))
        throw std::runtime_error("Cbor array push error");
}

Buffer::Buffer(const Buffer& other)
    : m_size(other.m_size)
    , m_capacity(m_size)
    , m_buffer(reinterpret_cast<cbor_mutable_data>(std::malloc(m_size)))  // NOLINT cppcoreguidelines-no-malloc
{
    if (m_buffer == nullptr)
        throw std::bad_alloc();
    std::memcpy(m_buffer, other.m_buffer, m_size);
}

Buffer& Buffer::operator=(const Buffer& other)
{
    if (&other == this)
        return *this;

    dtor_impl();

    m_size = other.m_size;
    m_capacity = m_size;
    m_buffer = reinterpret_cast<cbor_mutable_data>(std::malloc(m_size));  // NOLINT cppcoreguidelines-no-malloc
    if (m_buffer == nullptr)
        throw std::bad_alloc();
    std::memcpy(m_buffer, other.m_buffer, m_size);

    return *this;
}

Buffer::Buffer(Buffer&& other) noexcept
    : m_size(other.m_size)
    , m_capacity(other.m_capacity)
    , m_buffer(other.m_buffer)
{
    other.m_size = 0;
    other.m_capacity = 0;
    other.m_buffer = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    if (&other == this)
        return *this;

    dtor_impl();

    m_size = other.m_size;
    m_capacity = other.m_capacity;
    m_buffer = other.m_buffer;

    other.m_size = 0;
    other.m_capacity = 0;
    other.m_buffer = nullptr;

    return *this;
}

std::size_t Buffer::size() const noexcept
{
    return m_size;
}

cbor_data Buffer::data() const noexcept
{
    return m_buffer;
}

cbor_mutable_data Buffer::data() noexcept
{
    return m_buffer;
}

Buffer::~Buffer()
{
    dtor_impl();
}

void Buffer::dtor_impl() noexcept
{
    std::free(m_buffer);  // NOLINT cppcoreguidelines-no-malloc
}

static void check_ptr(void* ptr)
{
    if (ptr == nullptr)
        throw std::bad_alloc();
}

Buffer::Buffer(const cbor_item_t* item)
{
    m_size = cbor_serialize_alloc(item, &m_buffer, &m_capacity);
    check_ptr(m_buffer);
}

Item build_uint8(std::uint8_t value)
{
    cbor_item_t* item = cbor_build_uint8(value);
    check_ptr(item);
    return Item(item);
}

Item build_uint16(std::uint16_t value)
{
    cbor_item_t* item = cbor_build_uint16(value);
    check_ptr(item);
    return Item(item);
}

Item build_uint32(std::uint32_t value)
{
    cbor_item_t* item = cbor_build_uint32(value);
    check_ptr(item);
    return Item(item);
}

Item build_uint64(std::uint64_t value)
{
    cbor_item_t* item = cbor_build_uint64(value);
    check_ptr(item);
    return Item(item);
}

Item build_negint8(std::uint8_t value)
{
    cbor_item_t* item = cbor_build_negint8(value);
    check_ptr(item);
    return Item(item);
}

Item build_negint16(std::uint16_t value)
{
    cbor_item_t* item = cbor_build_negint16(value);
    check_ptr(item);
    return Item(item);
}

Item build_negint32(std::uint32_t value)
{
    cbor_item_t* item = cbor_build_negint32(value);
    check_ptr(item);
    return Item(item);
}

Item build_negint64(std::uint64_t value)
{
    cbor_item_t* item = cbor_build_negint64(value);
    check_ptr(item);
    return Item(item);
}

Item new_definite_array(std::size_t size)
{
    cbor_item_t* item = cbor_new_definite_array(size);
    check_ptr(item);
    return Item(item);
}

Item new_indefinite_array()
{
    cbor_item_t* item = cbor_new_indefinite_array();
    check_ptr(item);
    return Item(item);
}

Item build_string(const std::string& str)
{
    cbor_item_t* item = cbor_build_stringn(str.c_str(), str.size());
    check_ptr(item);
    return Item(item);
}

Item build_string(std::string_view str)
{
    cbor_item_t* item = cbor_build_stringn(str.data(), str.size());
    check_ptr(item);
    return Item(item);
}

Item build_string(const char* val)
{
    cbor_item_t* item = cbor_build_string(val);
    check_ptr(item);
    return Item(item);
}

Item build_string(const char* val, std::size_t length)
{
    cbor_item_t* item = cbor_build_stringn(val, length);
    check_ptr(item);
    return Item(item);
}

Item build_bytestring(cbor_data handle, std::size_t length)
{
    cbor_item_t* item = cbor_build_bytestring(handle, length);
    check_ptr(item);
    return Item(item);
}

}  // namespace hw1::cbor
