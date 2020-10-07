#ifndef CBOR_WRAPPER_HPP
#define CBOR_WRAPPER_HPP

#include "types.hpp"
#include "message.hpp"

#include <cbor.h>

#include <string>
#include <vector>

namespace hw1
{

class CborItem
{
public:
    CborItem(cbor_item_t* item);
    CborItem(const CborItem&);
    CborItem& operator=(const CborItem&);
    CborItem(CborItem&&);
    CborItem& operator=(CborItem&&);

    ~CborItem();

    operator cbor_item_t*() const;

private:
    void dtor_impl();

    cbor_item_t* m_item = nullptr;
};

class CborBuffer
{
public:
    CborBuffer() = default;
    CborBuffer(const CborBuffer&);
    CborBuffer& operator=(const CborBuffer&);
    CborBuffer(CborBuffer&&);
    CborBuffer& operator=(CborBuffer&&);
    ~CborBuffer();

    CborBuffer(const cbor_item_t* item);

    std::size_t size() const;
    cbor_data buffer() const;

protected:
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
    cbor_mutable_data m_buffer = nullptr;

private:
    void dtor_impl();
};

Message cbor_unpack_message(const CborBuffer& t_buffer);

}  // namespace hw1

#endif // CBOR_WRAPPER_HPP
