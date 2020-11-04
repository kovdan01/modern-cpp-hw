#include <utility>

namespace hw1
{

inline Attachment::Attachment(std::vector<byte_t> buffer)
    : m_buffer(std::move(buffer))
{
}

inline const std::vector<byte_t>& Attachment::buffer() const noexcept
{
    return m_buffer;
}

inline Message::Message(user_id_t from, user_id_t to, std::string text,
                 std::vector<Attachment> attachments)
    : m_attachments(std::move(attachments))
    , m_text(std::move(text))
    , m_from(from)
    , m_to(to)
{
}

inline Message::Message(const cbor::Buffer& buffer)
    : Message(cbor::Item(buffer))
{
}

inline Message::Message(const msgpack::object_handle& oh)
{
    *this = oh.get().convert();
}

inline Message::Message(const msgpack::sbuffer& sbuf)
    : Message(msgpack::unpack(sbuf.data(), sbuf.size()))
{
}

inline const std::vector<Attachment>& Message::attachments() const noexcept
{
    return m_attachments;
}

inline const std::string& Message::text() const noexcept
{
    return m_text;
}

inline user_id_t Message::from() const noexcept
{
    return m_from;
}

inline user_id_t Message::to() const noexcept
{
    return m_to;
}

inline msgpack::object_handle Message::to_msgpack_dom() const
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, *this);
    return msgpack::unpack(sbuf.data(), sbuf.size());
}

inline msgpack::sbuffer Message::to_msgpack_buffer() const
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, *this);
    return sbuf;
}

inline cbor::Buffer Message::to_cbor_buffer() const
{
    return cbor::Buffer(to_cbor_dom());
}

inline MessageVector::MessageVector(std::vector<Message> messages)
    : m_messages(std::move(messages))
{
}

inline MessageVector::MessageVector(const cbor::Buffer& buffer)
    : MessageVector(cbor::Item(buffer))
{
}

inline MessageVector::MessageVector(const msgpack::object_handle& oh)
{
    oh.get().convert(m_messages);
}

inline MessageVector::MessageVector(const msgpack::sbuffer& sbuf)
    : MessageVector(msgpack::unpack(sbuf.data(), sbuf.size()))
{
}

inline const std::vector<Message>& MessageVector::messages() const noexcept
{
    return m_messages;
}

inline msgpack::object_handle MessageVector::to_msgpack_dom() const
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, m_messages);
    return msgpack::unpack(sbuf.data(), sbuf.size());
}

inline msgpack::sbuffer MessageVector::to_msgpack_buffer() const
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, m_messages);
    return sbuf;
}

inline cbor::Buffer MessageVector::to_cbor_buffer() const
{
    return cbor::Buffer(to_cbor_dom());
}

}  // namespace hw1
