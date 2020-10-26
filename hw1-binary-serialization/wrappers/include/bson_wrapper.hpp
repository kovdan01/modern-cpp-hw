#ifndef HW1_BINARY_SERIALIZATION_WRAPPERS_BSON_WRAPPER_HPP_
#define HW1_BINARY_SERIALIZATION_WRAPPERS_BSON_WRAPPER_HPP_

#include <bson/bson.h>

#include <hw1/wrappers/export.h>

#include <array>
#include <charconv>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace hw1::bson
{

class HW1_WRAPPERS_EXPORT Base
{
public:
    Base(const Base&) noexcept = default;
    Base& operator=(const Base&) noexcept = default;
    Base(Base&&) noexcept = default;
    Base& operator=(Base&&) noexcept = default;

    ~Base();

    [[nodiscard]] const bson_t* handle() const noexcept;
    bson_t* handle() noexcept;

    [[nodiscard]] const std::uint8_t* data() const noexcept;
    std::uint8_t* data() noexcept;

    [[nodiscard]] std::uint32_t size() const noexcept;

    void append_uint64(std::string_view key, std::uint64_t value);
    void append_int64(std::string_view key, std::int64_t value);
    void append_utf8(std::string_view key, std::string_view value);
    void append_binary(std::string_view key, std::span<const std::uint8_t> value);

protected:
    Base() noexcept = default;
    Base(const bson_t&) noexcept;

private:
    bson_t m_bson;
};

using Ptr = std::unique_ptr<Base>;

class HW1_WRAPPERS_EXPORT Bson : public Base
{
public:
    using Base::Base;

    Bson() noexcept;
};

class HW1_WRAPPERS_EXPORT SubArray : public Base
{
public:
    using Base::Base;

    SubArray(Base& parent, std::string_view key);
    ~SubArray();

    void append_uint64(std::uint64_t value);
    void append_int64(std::int64_t value);
    void append_utf8(std::string_view value);
    void append_binary(std::span<const std::uint8_t> value);

    void increment() noexcept;
    [[nodiscard]] std::uint32_t index() const noexcept;

private:
    Base& m_parent;
    std::uint32_t m_index = 0;
};

class HW1_WRAPPERS_EXPORT Iter
{
public:
    Iter(const bson_t* document);

    Iter(const Iter&) noexcept = default;
    Iter& operator=(const Iter&) noexcept = default;
    Iter(Iter&&) noexcept = default;
    Iter& operator=(Iter&&) noexcept = default;

    ~Iter() = default;

    bool next() noexcept;

    [[nodiscard]] bool is_uint64() const noexcept;
    [[nodiscard]] bool is_int64() const noexcept;
    [[nodiscard]] bool is_binary() const noexcept;
    [[nodiscard]] bool is_utf8() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;

    [[nodiscard]] std::uint64_t as_uint64() const noexcept;
    [[nodiscard]] std::int64_t as_int64() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> as_binary() const;
    [[nodiscard]] std::string_view as_utf8() const;
    [[nodiscard]] Iter as_array() const noexcept;

private:
    Iter() noexcept = default;

    bson_iter_t m_iter;
};

}  // namespace hw1::bson

#define HW1_BSON_DETAIL_UNIQUE_ID_IMPL(varname, lineno) varname##lineno
#define HW1_BSON_DETAIL_UNIQUE_ID(varname, lineno) HW1_BSON_DETAIL_UNIQUE_ID_IMPL(varname, lineno)

#define HW1_BSON_DECLARE_STRING_INDEX(string_name, sub_array)                               \
    std::array<char, 11> HW1_BSON_DETAIL_UNIQUE_ID(str, __LINE__);                          \
    auto [HW1_BSON_DETAIL_UNIQUE_ID(ptr, __LINE__),                                         \
          HW1_BSON_DETAIL_UNIQUE_ID(ec, __LINE__)] =                                        \
                  std::to_chars(HW1_BSON_DETAIL_UNIQUE_ID(str, __LINE__).data(),            \
                                HW1_BSON_DETAIL_UNIQUE_ID(str, __LINE__).data() +           \
                                HW1_BSON_DETAIL_UNIQUE_ID(str, __LINE__).size(),            \
                                (sub_array).index());                                       \
    assert(HW1_BSON_DETAIL_UNIQUE_ID(ec, __LINE__) == std::errc());                         \
    std::string_view string_name(HW1_BSON_DETAIL_UNIQUE_ID(str, __LINE__).data(),           \
                                 HW1_BSON_DETAIL_UNIQUE_ID(ptr, __LINE__));

#endif  // HW1_BINARY_SERIALIZATION_WRAPPERS_BSON_WRAPPER_HPP_
