#include "bson_wrapper.hpp"

#include <cassert>

#define HW1_BSON_DECLARE_STRING_INDEX_THIS(string_name) \
    HW1_BSON_DECLARE_STRING_INDEX(string_name, (*this))

namespace hw1::bson
{

static std::int64_t uint64_to_int64(std::uint64_t value) noexcept
{
    return *reinterpret_cast<std::int64_t*>(&value);
}

static std::uint64_t int64_to_uint64(std::int64_t value) noexcept
{
    return *reinterpret_cast<std::uint64_t*>(&value);
}

Base::~Base()
{
    bson_destroy(&m_bson);
}

Base::Base(const bson_t& value) noexcept
    : m_bson(value)
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

void Base::append_uint64(std::string_view key, std::uint64_t value)
{
    append_int64(key, uint64_to_int64(value));
}

void Base::append_int64(std::string_view key, std::int64_t value)
{
    if (!bson_append_int64(handle(), key.data(), static_cast<int>(key.size()), value))
        throw std::runtime_error("Error while appending int64 to bson");
}

void Base::append_utf8(std::string_view key, std::string_view value)
{
    if (!bson_append_utf8(handle(), key.data(), static_cast<int>(key.size()),
                          value.data(), static_cast<int>(value.size())))
        throw std::runtime_error("Error while appending utf8 to bson");
}

void Base::append_binary(std::string_view key, std::span<const std::uint8_t> value)
{
    if (!bson_append_binary(handle(), key.data(), static_cast<int>(key.size()), BSON_SUBTYPE_BINARY,
                            value.data(), static_cast<std::uint32_t>(value.size())))
        throw std::runtime_error("Error while appending binary to bson");
}

Bson::Bson() noexcept
    : Base(BSON_INITIALIZER)
{
}

SubArray::SubArray(Base& parent, std::string_view key)
    : m_parent(parent)
{
    if (!bson_append_array_begin(m_parent.handle(), key.data(), static_cast<int>(key.size()), handle()))
        throw std::runtime_error("Can't append bson array");
}

SubArray::~SubArray()
{
    [[maybe_unused]] bool res = bson_append_array_end(m_parent.handle(), handle());
    assert(res);
}

void SubArray::append_uint64(std::uint64_t value)
{
    append_int64(uint64_to_int64(value));
}

void SubArray::append_int64(std::int64_t value)
{
    HW1_BSON_DECLARE_STRING_INDEX_THIS(i);
    Base::append_int64(i, value);
}

void SubArray::append_utf8(std::string_view value)
{
    HW1_BSON_DECLARE_STRING_INDEX_THIS(i);
    Base::append_utf8(i, value);
}

void SubArray::append_binary(std::span<const std::uint8_t> value)
{
    HW1_BSON_DECLARE_STRING_INDEX_THIS(i);
    Base::append_binary(i, value);
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
    if (!bson_iter_init(&m_iter, document))
        throw std::runtime_error("Can't init bson_iter_t from bson_t");
}

bool Iter::next() noexcept
{
    return bson_iter_next(&m_iter);
}

bool Iter::is_uint64() const noexcept
{
    return is_int64();
}

bool Iter::is_int64() const noexcept
{
    return BSON_ITER_HOLDS_INT64(&m_iter);
}

bool Iter::is_binary() const noexcept
{
    return BSON_ITER_HOLDS_BINARY(&m_iter);
}

bool Iter::is_utf8() const noexcept
{
    return BSON_ITER_HOLDS_UTF8(&m_iter);
}

bool Iter::is_array() const noexcept
{
    return BSON_ITER_HOLDS_ARRAY(&m_iter);
}

std::uint64_t Iter::as_uint64() const noexcept
{
    assert(is_uint64());
    return int64_to_uint64(as_int64());
}

std::int64_t Iter::as_int64() const noexcept
{
    assert(is_int64());
    return bson_iter_as_int64(&m_iter);
}

std::span<const std::uint8_t> Iter::as_binary() const
{
    assert(is_binary());

    const std::uint8_t* binary;
    std::uint32_t binary_len;
    bson_iter_binary(&m_iter, nullptr, &binary_len, &binary);

    return std::span(binary, binary_len);
}

std::string_view Iter::as_utf8() const
{
    assert(is_utf8());

    std::uint32_t length;
    const char* utf8 = bson_iter_utf8(&m_iter, &length);

    return std::string_view(utf8, length);
}

Iter Iter::as_array() const noexcept
{
    assert(is_array());

    Iter ans;
    bson_iter_recurse(&m_iter, &ans.m_iter);

    return ans;
}

}  // namespace hw1::bson
