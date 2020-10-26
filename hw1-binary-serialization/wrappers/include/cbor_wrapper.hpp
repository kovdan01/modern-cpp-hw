#ifndef HW1_BINARY_SERIALIZATION_WRAPPERS_CBOR_WRAPPER_HPP_
#define HW1_BINARY_SERIALIZATION_WRAPPERS_CBOR_WRAPPER_HPP_

#include <cbor.h>

#include <hw1/wrappers/export.h>

#include <string>
#include <vector>

namespace hw1::cbor
{

class HW1_WRAPPERS_EXPORT Buffer
{
public:
    Buffer() noexcept = default;
    Buffer(const Buffer&);
    Buffer& operator=(const Buffer&);
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;
    ~Buffer();

    Buffer(const cbor_item_t* item);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] cbor_data data() const noexcept;
    cbor_mutable_data data() noexcept;

private:
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
    cbor_mutable_data m_buffer = nullptr;

    void dtor_impl() noexcept;
};

class HW1_WRAPPERS_EXPORT Item
{
public:
    explicit Item(cbor_item_t* item) noexcept;
    Item(const Buffer& buffer);
    Item(const Item&) noexcept;
    Item& operator=(const Item&) noexcept;
    Item(Item&&) noexcept;
    Item& operator=(Item&&) noexcept;

    ~Item();

    operator const cbor_item_t*() const noexcept;
    operator cbor_item_t*() noexcept;

    void array_push(cbor_item_t* item);

private:
    cbor_item_t* m_item = nullptr;

    void dtor_impl();
};

HW1_WRAPPERS_EXPORT Item build_uint8(std::uint8_t value);
HW1_WRAPPERS_EXPORT Item build_uint16(std::uint16_t value);
HW1_WRAPPERS_EXPORT Item build_uint32(std::uint32_t value);
HW1_WRAPPERS_EXPORT Item build_uint64(std::uint64_t value);
HW1_WRAPPERS_EXPORT Item build_negint8(std::uint8_t value);
HW1_WRAPPERS_EXPORT Item build_negint16(std::uint16_t value);
HW1_WRAPPERS_EXPORT Item build_negint32(std::uint32_t value);
HW1_WRAPPERS_EXPORT Item build_negint64(std::uint64_t value);

HW1_WRAPPERS_EXPORT Item new_definite_array(std::size_t size);
HW1_WRAPPERS_EXPORT Item new_indefinite_array();

HW1_WRAPPERS_EXPORT Item build_string(const std::string& str);
HW1_WRAPPERS_EXPORT Item build_string(std::string_view str);
HW1_WRAPPERS_EXPORT Item build_string(const char* val);
HW1_WRAPPERS_EXPORT Item build_string(const char* val, std::size_t length);

HW1_WRAPPERS_EXPORT Item build_bytestring(cbor_data handle, std::size_t length);

}  // namespace hw1::cbor

#endif  // HW1_BINARY_SERIALIZATION_WRAPPERS_CBOR_WRAPPER_HPP_
