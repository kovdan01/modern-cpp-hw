#include "message.hpp"

#include <iomanip>
#include <utility>

namespace hw1
{

Attachment::Attachment(std::vector<byte_t> t_buffer)
    : m_buffer(std::move(t_buffer))
{
}

const std::vector<byte_t>& Attachment::buffer() const
{
    return m_buffer;
}

std::ostream& operator<<(std::ostream& t_output, const Attachment& t_attachment)
{
    t_output << "{ ";
    t_output << std::hex;
    for (byte_t byte : t_attachment.buffer())
        t_output << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << ' ';
    t_output << std::dec;
    t_output << '}';

    return t_output;
}

Message::Message(user_id_t t_from, user_id_t t_to, std::string t_text,
                 std::vector<Attachment> t_attachments)
    : m_attachments(std::move(t_attachments))
    , m_text(std::move(t_text))
    , m_from(t_from)
    , m_to(t_to)
{
}

Message::Message(const cbor::Item& t_item)
{
    assert(cbor_isa_array(t_item));
    assert(cbor_array_size(t_item) == 4);

    cbor_item_t** item_handle = cbor_array_handle(t_item);

    assert(cbor_isa_uint(item_handle[0]));
    assert(cbor_isa_uint(item_handle[1]));
    assert(cbor_isa_string(item_handle[2]));
    assert(cbor_isa_array(item_handle[3]));

    m_from = cbor_get_uint64(item_handle[0]);
    m_to   = cbor_get_uint64(item_handle[1]);

    cbor_item_t* text_handle = item_handle[2];
    m_text = std::string(reinterpret_cast<char*>(cbor_string_handle(text_handle)), cbor_string_length(text_handle));

    cbor_item_t* attachments_array = item_handle[3];
    std::size_t num_of_attachments = cbor_array_size(attachments_array);
    cbor_item_t** attachments_handle = cbor_array_handle(attachments_array);

    m_attachments.reserve(num_of_attachments);

    for (std::size_t i = 0; i < num_of_attachments; ++i)
    {
        cbor_item_t* current = attachments_handle[i];
        assert(cbor_isa_bytestring(current));
        const hw1::byte_t* begin = cbor_bytestring_handle(current);
        const hw1::byte_t* end = begin + cbor_bytestring_length(current);
        m_attachments.emplace_back(std::vector<hw1::byte_t>(begin, end));
    }
}

Message::Message(const cbor::Buffer& t_buffer)
    : Message(cbor::Item(t_buffer))
{
}

Message::Message(const msgpack::object_handle& t_oh)
{
    *this = t_oh.get().convert();
}

Message::Message(const msgpack::sbuffer& t_sbuf)
    : Message(msgpack::unpack(t_sbuf.data(), t_sbuf.size()))
{
}

Message::Message(bson::Iter& t_iter)
{
    bool result;

    result = t_iter.next();
    assert(result);
    m_from = t_iter.as_uint64();

    result = t_iter.next();
    assert(result);
    m_to   = t_iter.as_uint64();

    result = t_iter.next();
    assert(result);
    m_text = t_iter.as_utf8();

    result = t_iter.next();
    assert(result);
    bson::Iter attachments_iter = t_iter.as_array();
    while (attachments_iter.next())
    {
        std::span<const std::uint8_t> binary = attachments_iter.as_binary();
        m_attachments.emplace_back(std::vector<byte_t>(binary.begin(), binary.end()));
    }

    result = t_iter.next();
    assert(!result);
}

const std::vector<Attachment>& Message::attachments() const
{
    return m_attachments;
}

const std::string& Message::text() const
{
    return m_text;
}

user_id_t Message::from() const
{
    return m_from;
}

user_id_t Message::to() const
{
    return m_to;
}

msgpack::object_handle Message::to_msgpack_dom() const
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, *this);
    return msgpack::unpack(sbuf.data(), sbuf.size());
}

msgpack::sbuffer Message::to_msgpack_buffer() const
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, *this);
    return sbuf;
}

cbor::Item Message::to_cbor_dom() const
{
    cbor::Item root = cbor::new_definite_array(4);

    cbor::Item from = cbor::build_uint64(m_from);
    cbor::Item to   = cbor::build_uint64(m_to);
    cbor::Item text = cbor::build_string(m_text);

    cbor::Item attachments = cbor::new_definite_array(m_attachments.size());
    std::vector<cbor::Item> attachments_wrapper;
    for (const Attachment& attachment : m_attachments)
        cbor_array_push(attachments, cbor::build_bytestring(attachment.buffer().data(), attachment.buffer().size()));

    cbor_array_push(root, from);
    cbor_array_push(root, to);
    cbor_array_push(root, text);
    cbor_array_push(root, attachments);

    return root;
}

cbor::Buffer Message::to_cbor_buffer() const
{
    return cbor::Buffer(to_cbor_dom());
}

void Message::to_bson_buffer(bson::Base& t_parent, std::string_view t_key) const
{
    bson::SubArray message_bson(t_parent, t_key);

    message_bson.append_uint64(m_from);
    message_bson.append_uint64(m_to);
    message_bson.append_utf8(m_text);

    {
        bson::SubArray attachments_bson(message_bson, message_bson.index());
        message_bson.increment();
        for (const Attachment& attachment : m_attachments)
            attachments_bson.append_binary(std::span(attachment.buffer().data(), attachment.buffer().size()));
    }
}

std::ostream& operator<<(std::ostream& t_output, const Message& t_message)
{
    t_output << t_message.from() << ", "
             << t_message.to() << ", \""
             << t_message.text() << "\",\n{\n";
    for (const Attachment& attachment : t_message.attachments())
        t_output << '\t' << attachment << '\n';
    t_output << "}\n";

    return t_output;
}

MessageVector::MessageVector(std::vector<Message> t_messages)
    : m_messages(std::move(t_messages))
{
}

MessageVector::MessageVector(const cbor::Item& t_item)
{
    assert(cbor_isa_array(t_item));

    cbor_item_t** item_handle = cbor_array_handle(t_item);
    std::size_t message_count = cbor_array_size(t_item);
    m_messages.reserve(message_count);
    for (std::size_t i = 0; i < message_count; ++i)
        m_messages.emplace_back(Message(item_handle[i]));
}

MessageVector::MessageVector(const cbor::Buffer& t_buffer)
    : MessageVector(cbor::Item(t_buffer))
{
}

MessageVector::MessageVector(const msgpack::object_handle& t_oh)
{
    t_oh.get().convert(m_messages);
}

MessageVector::MessageVector(const msgpack::sbuffer& t_sbuf)
    : MessageVector(msgpack::unpack(t_sbuf.data(), t_sbuf.size()))
{
}

MessageVector::MessageVector(bson::Iter& t_iter)
{
    bool result;

    result = t_iter.next();
    assert(result);
    bson::Iter array = t_iter.as_array();

    while (array.next())
    {
        bson::Iter message_iter = array.as_array();
        m_messages.emplace_back(Message(message_iter));
    }

    result = t_iter.next();
    assert(!result);
}

const std::vector<Message>& MessageVector::messages() const
{
    return m_messages;
}

msgpack::object_handle MessageVector::to_msgpack_dom() const
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, m_messages);
    return msgpack::unpack(sbuf.data(), sbuf.size());
}

msgpack::sbuffer MessageVector::to_msgpack_buffer() const
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, m_messages);
    return sbuf;
}

cbor::Item MessageVector::to_cbor_dom() const
{
    cbor::Item root = cbor::new_definite_array(m_messages.size());

    for (const Message& message : m_messages)
        cbor_array_push(root, message.to_cbor_dom());

    return root;
}

cbor::Buffer MessageVector::to_cbor_buffer() const
{
    return cbor::Buffer(to_cbor_dom());
}

bson::Ptr MessageVector::to_bson_buffer() const
{
    auto buffer = std::make_unique<bson::Bson>();
    {
        bson::SubArray array(*buffer, "0");
        for (const Message& message : m_messages)
        {
            message.to_bson_buffer(array, array.index());
            array.increment();
        }
    }
    return buffer;
}

}  // namespace hw1
