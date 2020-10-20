#include "bson_wrapper.hpp"

#include <cassert>

#define HW1_BSON_DECLARE_STRING_INDEX_THIS(string_name) \
    HW1_BSON_DECLARE_STRING_INDEX(string_name, (*this))

namespace hw1
{

namespace bson
{

static std::int64_t uint64_to_int64(std::uint64_t t_value) noexcept
{
    return *reinterpret_cast<std::int64_t*>(&t_value);
}

static std::uint64_t int64_to_uint64(std::int64_t t_value) noexcept
{
    return *reinterpret_cast<std::uint64_t*>(&t_value);
}

Base::~Base()
{
    bson_destroy(&m_bson);
}

Base::Base(const bson_t& t_value) noexcept
    : m_bson(t_value)
{
}

const bson_t* Base::handle() const noexcept
{
    return &m_bson;
}

bson_t* Base::handle() noexcept
{
    return &m_bson;
}

std::uint32_t Base::size() const noexcept
{
    return m_bson.len;
}

void Base::append_uint64(std::string_view t_key, std::uint64_t t_value)
{
    append_int64(t_key, uint64_to_int64(t_value));
}

void Base::append_int64(std::string_view t_key, std::int64_t t_value)
{
    if (!bson_append_int64(handle(), t_key.data(), static_cast<int>(t_key.size()), t_value))
        throw std::runtime_error("Error while appending int64 to bson");
}

void Base::append_utf8(std::string_view t_key, std::string_view t_value)
{
    if (!bson_append_utf8(handle(), t_key.data(), static_cast<int>(t_key.size()),
                          t_value.data(), static_cast<int>(t_value.size())))
        throw std::runtime_error("Error while appending utf8 to bson");
}

void Base::append_binary(std::string_view t_key, std::span<const std::uint8_t> t_value)
{
    if (!bson_append_binary(handle(), t_key.data(), static_cast<int>(t_key.size()), BSON_SUBTYPE_BINARY,
                            t_value.data(), static_cast<std::uint32_t>(t_value.size())))
        throw std::runtime_error("Error while appending binary to bson");
}

Bson::Bson()
    : Base(BSON_INITIALIZER)
{
}

SubArray::SubArray(Base& t_parent, std::string_view t_key) noexcept
    : m_parent(t_parent)
{
    bson_append_array_begin(m_parent.handle(), t_key.data(), static_cast<int>(t_key.size()), handle());
}

SubArray::~SubArray()
{
    bson_append_array_end(m_parent.handle(), handle());
}

void SubArray::append_uint64(std::uint64_t t_value)
{
    append_int64(uint64_to_int64(t_value));
}

void SubArray::append_int64(std::int64_t t_value)
{
    HW1_BSON_DECLARE_STRING_INDEX_THIS(i);
    Base::append_int64(i, t_value);
}

void SubArray::append_utf8(std::string_view t_value)
{
    HW1_BSON_DECLARE_STRING_INDEX_THIS(i);
    Base::append_utf8(i, t_value);
}

void SubArray::append_binary(std::span<const std::uint8_t> t_value)
{
    HW1_BSON_DECLARE_STRING_INDEX_THIS(i);
    Base::append_binary(i, t_value);
}

void SubArray::increment() noexcept
{
    ++m_index;
}

std::uint32_t SubArray::index() const noexcept
{
    return m_index;
}

Iter::Iter(const bson_t* document)
{
    bson_iter_init(&m_iter, document);
}

bool Iter::next()
{
    return bson_iter_next(&m_iter);
}

bool Iter::is_uint64() const
{
    return is_int64();
}

bool Iter::is_int64() const
{
    return BSON_ITER_HOLDS_INT64(&m_iter);
}

bool Iter::is_binary() const
{
    return BSON_ITER_HOLDS_BINARY(&m_iter);
}

bool Iter::is_utf8() const
{
    return BSON_ITER_HOLDS_UTF8(&m_iter);
}

bool Iter::is_array() const
{
    return BSON_ITER_HOLDS_ARRAY(&m_iter);
}

std::uint64_t Iter::as_uint64() const
{
    assert(is_uint64());  // NOLINT cppcoreguidelines-pro-bounds-array-to-pointer-decay
    return int64_to_uint64(as_int64());
}

std::int64_t Iter::as_int64() const
{
    assert(is_int64());  // NOLINT cppcoreguidelines-pro-bounds-array-to-pointer-decay
    return bson_iter_as_int64(&m_iter);
}

std::span<const std::uint8_t> Iter::as_binary() const
{
    assert(is_binary());  // NOLINT cppcoreguidelines-pro-bounds-array-to-pointer-decay

    const std::uint8_t* binary;
    std::uint32_t binary_len;
    bson_iter_binary(&m_iter, nullptr, &binary_len, &binary);

    return std::span(binary, binary_len);
}

std::string_view Iter::as_utf8() const
{
    assert(is_utf8());  // NOLINT cppcoreguidelines-pro-bounds-array-to-pointer-decay

    std::uint32_t length;
    const char* utf8 = bson_iter_utf8(&m_iter, &length);

    return std::string_view(utf8, length);
}

Iter Iter::as_array() const
{
    assert(is_array());  // NOLINT cppcoreguidelines-pro-bounds-array-to-pointer-decay

    Iter ans;
    bson_iter_recurse(&m_iter, &ans.m_iter);

    return ans;
}

}  // namespace bson

}  // namespace hw1
