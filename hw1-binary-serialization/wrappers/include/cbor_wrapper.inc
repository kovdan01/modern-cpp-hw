#include <cassert>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace hw1::cbor
{

inline Item::Item(cbor_item_t* item) noexcept
    : m_item(item)
{
}

inline Item::Item(const Buffer& buffer)
{
    cbor_load_result result;
    m_item = cbor_load(buffer.data(), buffer.size(), &result);
    if (result.error.code != CBOR_ERR_NONE)
        throw std::runtime_error("Cbor deserialization error " + std::to_string(result.error.code));
}

inline Item::Item(const Item& other) noexcept
    : m_item(other.m_item)
{
    cbor_incref(m_item);
}

inline Item& Item::operator=(const Item& other) noexcept
{
    if (&other == this)
        return *this;

    dtor_impl();

    m_item = other.m_item;
    cbor_incref(m_item);

    return *this;
}

inline Item::Item(Item&& other) noexcept
    : m_item(other.m_item)
{
    other.m_item = nullptr;
}

inline Item& Item::operator=(Item&& other) noexcept
{
    if (&other == this)
        return *this;

    dtor_impl();

    m_item = other.m_item;
    other.m_item = nullptr;

    return *this;
}

inline Item::~Item()
{
    dtor_impl();
}

inline void Item::dtor_impl()
{
    if (m_item != nullptr)
        cbor_decref(&m_item);
}

inline Item::operator const cbor_item_t*() const noexcept
{
    return m_item;
}

inline Item::operator cbor_item_t*() noexcept
{
    return m_item;
}

inline void Item::array_push(cbor_item_t* item)
{
    if (!cbor_array_push(m_item, item))
        throw std::runtime_error("Cbor array push error");
}

inline Buffer::Buffer(const Buffer& other)
    : m_size(other.m_size)
    , m_capacity(m_size)
    , m_buffer(reinterpret_cast<cbor_mutable_data>(std::malloc(m_size)))  // NOLINT cppcoreguidelines-no-malloc
{
    if (m_buffer == nullptr)
        throw std::bad_alloc();
    std::memcpy(m_buffer, other.m_buffer, m_size);
}

inline Buffer& Buffer::operator=(const Buffer& other)
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

inline Buffer::Buffer(Buffer&& other) noexcept
    : m_size(other.m_size)
    , m_capacity(other.m_capacity)
    , m_buffer(other.m_buffer)
{
    other.m_size = 0;
    other.m_capacity = 0;
    other.m_buffer = nullptr;
}

inline Buffer& Buffer::operator=(Buffer&& other) noexcept
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

inline std::size_t Buffer::size() const noexcept
{
    return m_size;
}

inline cbor_data Buffer::data() const noexcept
{
    return m_buffer;
}

inline cbor_mutable_data Buffer::data() noexcept
{
    return m_buffer;
}

inline Buffer::~Buffer()
{
    dtor_impl();
}

inline void Buffer::dtor_impl() noexcept
{
    std::free(m_buffer);  // NOLINT cppcoreguidelines-no-malloc
}

namespace detail
{

inline static void check_ptr(void* ptr)
{
    if (ptr == nullptr)
        throw std::bad_alloc();
}

}  // namespace detail

inline Buffer::Buffer(const cbor_item_t* item)
{
    m_size = cbor_serialize_alloc(item, &m_buffer, &m_capacity);
    detail::check_ptr(m_buffer);
}

inline Item build_uint8(std::uint8_t value)
{
    cbor_item_t* item = cbor_build_uint8(value);
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_uint16(std::uint16_t value)
{
    cbor_item_t* item = cbor_build_uint16(value);
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_uint32(std::uint32_t value)
{
    cbor_item_t* item = cbor_build_uint32(value);
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_uint64(std::uint64_t value)
{
    cbor_item_t* item = cbor_build_uint64(value);
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_negint8(std::uint8_t value)
{
    cbor_item_t* item = cbor_build_negint8(value);
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_negint16(std::uint16_t value)
{
    cbor_item_t* item = cbor_build_negint16(value);
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_negint32(std::uint32_t value)
{
    cbor_item_t* item = cbor_build_negint32(value);
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_negint64(std::uint64_t value)
{
    cbor_item_t* item = cbor_build_negint64(value);
    detail::check_ptr(item);
    return Item(item);
}

inline Item new_definite_array(std::size_t size)
{
    cbor_item_t* item = cbor_new_definite_array(size);
    detail::check_ptr(item);
    return Item(item);
}

inline Item new_indefinite_array()
{
    cbor_item_t* item = cbor_new_indefinite_array();
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_string(const std::string& str)
{
    cbor_item_t* item = cbor_build_stringn(str.c_str(), str.size());
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_string(std::string_view str)
{
    cbor_item_t* item = cbor_build_stringn(str.data(), str.size());
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_string(const char* val)
{
    cbor_item_t* item = cbor_build_string(val);
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_string(const char* val, std::size_t length)
{
    cbor_item_t* item = cbor_build_stringn(val, length);
    detail::check_ptr(item);
    return Item(item);
}

inline Item build_bytestring(cbor_data handle, std::size_t length)
{
    cbor_item_t* item = cbor_build_bytestring(handle, length);
    detail::check_ptr(item);
    return Item(item);
}

}  // namespace hw1::cbor
