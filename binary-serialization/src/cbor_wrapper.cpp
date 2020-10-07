#include "cbor_wrapper.hpp"
#include "types.hpp"

#include <cstring>
#include <cstdlib>
#include <utility>

namespace hw1
{

CborItem::CborItem(cbor_item_t* item)
    : m_item(item)
{
}

CborItem::CborItem(const CborItem& t_other)
    : m_item(t_other.m_item)
{
    cbor_incref(m_item);
}

CborItem& CborItem::operator=(const CborItem& t_other)
{
    dtor_impl();

    m_item = t_other.m_item;
    cbor_incref(m_item);

    return *this;
}

CborItem::CborItem(CborItem&& t_other)
    : m_item(t_other.m_item)
{
    t_other.m_item = nullptr;
}

CborItem& CborItem::operator=(CborItem&& t_other)
{
    dtor_impl();

    m_item = t_other.m_item;
    t_other.m_item = nullptr;

    return *this;
}

CborItem::~CborItem()
{
    dtor_impl();
}

void CborItem::dtor_impl()
{
    if (m_item != nullptr)
        cbor_decref(&m_item);
}

CborItem::operator cbor_item_t*() const
{
    return m_item;
}

CborBuffer::CborBuffer(const CborBuffer& t_other)
    : m_size(t_other.m_size)
    , m_capacity(m_size)
    , m_buffer(reinterpret_cast<byte_t*>(std::malloc(m_size)))
{
    std::memcpy(m_buffer, t_other.m_buffer, m_size);
}

CborBuffer& CborBuffer::operator=(const CborBuffer& t_other)
{
    dtor_impl();

    m_size = t_other.m_size;
    m_capacity = m_size;
    m_buffer = reinterpret_cast<byte_t*>(std::malloc(m_size));
    std::memcpy(m_buffer, t_other.m_buffer, m_size);

    return *this;
}

CborBuffer::CborBuffer(CborBuffer&& t_other)
    : m_size(t_other.m_size)
    , m_capacity(t_other.m_capacity)
    , m_buffer(t_other.m_buffer)
{
    t_other.m_size = 0;
    t_other.m_capacity = 0;
    t_other.m_buffer = nullptr;
}

CborBuffer& CborBuffer::operator=(CborBuffer&& t_other)
{
    dtor_impl();

    m_size = t_other.m_size;
    m_capacity = t_other.m_capacity;
    m_buffer = t_other.m_buffer;

    t_other.m_size = 0;
    t_other.m_capacity = 0;
    t_other.m_buffer = nullptr;

    return *this;
}

std::size_t CborBuffer::size() const
{
    return m_size;
}

cbor_data CborBuffer::buffer() const
{
    return m_buffer;
}

CborBuffer::~CborBuffer()
{
    dtor_impl();
}

void CborBuffer::dtor_impl()
{
    if (m_buffer != nullptr)
        std::free(m_buffer);
}

CborBuffer::CborBuffer(const cbor_item_t* t_item)
{
    m_size = cbor_serialize_alloc(t_item, &m_buffer, &m_capacity);
}

Message cbor_unpack_message(const CborBuffer& t_buffer)
{
    cbor_load_result result;
    hw1::CborItem item = cbor_load(t_buffer.buffer(), t_buffer.size(), &result);

    hw1::CborItem from_handle = cbor_array_get(item, 0);
    hw1::CborItem to_handle   = cbor_array_get(item, 1);
    hw1::CborItem text_handle = cbor_array_get(item, 2);

    hw1::CborItem attachments_handle = cbor_array_get(item, 3);

    hw1::user_id_t from = cbor_get_uint64(from_handle);
    hw1::user_id_t to   = cbor_get_uint64(to_handle);
    std::string text(reinterpret_cast<char*>(cbor_string_handle(text_handle)), cbor_string_length(text_handle));

    std::vector<hw1::Attachment> attachments;
    std::size_t num_of_attachments = cbor_array_size(attachments_handle);
    attachments.reserve(num_of_attachments);
    for (std::size_t i = 0; i < num_of_attachments; ++i)
    {
        hw1::CborItem current = cbor_array_get(attachments_handle, i);
        const hw1::byte_t* begin = cbor_bytestring_handle(current);
        const hw1::byte_t* end = begin + cbor_bytestring_length(current);
        attachments.emplace_back(std::vector<hw1::byte_t>(begin, end));
    }

    return hw1::Message(from, to, std::move(text), std::move(attachments));
}

}  // namespace hw1
