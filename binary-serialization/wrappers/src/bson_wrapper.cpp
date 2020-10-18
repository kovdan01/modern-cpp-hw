#include "bson_wrapper.hpp"

#include <cassert>

namespace hw1
{

namespace bson
{

namespace detail
{

std::int64_t uint64_to_int64(std::uint64_t t_value)
{
    return *reinterpret_cast<std::int64_t*>(&t_value);
}

std::uint64_t int64_to_uint64(std::int64_t t_value)
{
    return *reinterpret_cast<std::uint64_t*>(&t_value);
}

}  // namespace detail

Base::~Base()
{
    bson_destroy(&m_bson);
}

Base::Base(const bson_t& t_value)
    : m_bson(t_value)
{
}

const bson_t* Base::handle() const
{
    return &m_bson;
}

bson_t* Base::handle()
{
    return &m_bson;
}

std::uint32_t Base::size() const
{
    return m_bson.len;
}

bool Base::append_uint64(std::string_view t_key, std::uint64_t t_value)
{
    return append_int64(t_key, detail::uint64_to_int64(t_value));
}

bool Base::append_int64(std::string_view t_key, std::int64_t t_value)
{
    return bson_append_int64(handle(), t_key.data(), static_cast<int>(t_key.size()), t_value);
}

bool Base::append_utf8(std::string_view t_key, std::string_view t_value)
{
    return bson_append_utf8(handle(), t_key.data(), static_cast<int>(t_key.size()),
                            t_value.data(), static_cast<int>(t_value.size()));
}

bool Base::append_binary(std::string_view t_key, std::span<const std::uint8_t> t_value)
{
    return bson_append_binary(handle(), t_key.data(), static_cast<int>(t_key.size()), BSON_SUBTYPE_BINARY,
                              t_value.data(), static_cast<std::uint32_t>(t_value.size()));
}

Bson::Bson()
    : Base(BSON_INITIALIZER)
{
}

SubArray::SubArray(Base& t_parent, std::string_view t_key)
    : m_parent(t_parent)
{
    bson_append_array_begin(m_parent.handle(), t_key.data(), static_cast<int>(t_key.size()), handle());
}

SubArray::~SubArray()
{
    bson_append_array_end(m_parent.handle(), handle());
}

bool SubArray::append_uint64(std::uint64_t t_value)
{
    return append_int64(detail::uint64_to_int64(t_value));
}

bool SubArray::append_int64(std::int64_t t_value)
{
    return Base::append_int64(index(), t_value);
}

bool SubArray::append_utf8(std::string_view t_value)
{
    return Base::append_utf8(index(), t_value);
}

bool SubArray::append_binary(std::span<const std::uint8_t> t_value)
{
    return Base::append_binary(index(), t_value);
}

void SubArray::increment()
{
    ++m_index;
}

std::string SubArray::index() const
{
    return std::to_string(m_index);
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
    assert(is_uint64());
    return detail::int64_to_uint64(as_int64());
}

std::int64_t Iter::as_int64() const
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

Iter Iter::as_array() const
{
    assert(is_array());

    Iter ans;
    bson_iter_recurse(&m_iter, &ans.m_iter);

    return ans;
}

}  // namespace bson

}  // namespace hw1