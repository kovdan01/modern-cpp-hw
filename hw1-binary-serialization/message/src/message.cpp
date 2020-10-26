#include "message.hpp"

#include <iomanip>
#include <utility>

namespace hw1
{

Attachment::Attachment(std::vector<byte_t> buffer)
    : m_buffer(std::move(buffer))
{
}

const std::vector<byte_t>& Attachment::buffer() const noexcept
{
    return m_buffer;
}

std::ostream& operator<<(std::ostream& output, const Attachment& attachment)
{
    output << "{ ";
    output << std::hex;
    for (byte_t byte : attachment.buffer())
        output << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << ' ';
    output << std::dec;
    output << '}';

    return output;
}

Message::Message(user_id_t from, user_id_t to, std::string text,
                 std::vector<Attachment> attachments)
    : m_attachments(std::move(attachments))
    , m_text(std::move(text))
    , m_from(from)
    , m_to(to)
{
}

Message::Message(const cbor::Item& item)
{
    assert(cbor_isa_array(item));
    assert(cbor_array_size(item) == 4);

    cbor_item_t** item_handle = cbor_array_handle(item);

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

Message::Message(const cbor::Buffer& buffer)
    : Message(cbor::Item(buffer))
{
}

Message::Message(const msgpack::object_handle& oh)
{
    *this = oh.get().convert();
}

Message::Message(const msgpack::sbuffer& sbuf)
    : Message(msgpack::unpack(sbuf.data(), sbuf.size()))
{
}

Message::Message(bson::Iter& iter)
{
    bool result;

    result = iter.next();
    assert(result);
    m_from = iter.as_uint64();

    result = iter.next();
    assert(result);
    m_to   = iter.as_uint64();

    result = iter.next();
    assert(result);
    m_text = iter.as_utf8();

    result = iter.next();
    assert(result);
    bson::Iter attachments_iter = iter.as_array();
    while (attachments_iter.next())
    {
        std::span<const std::uint8_t> binary = attachments_iter.as_binary();
        m_attachments.emplace_back(std::vector<byte_t>(binary.begin(), binary.end()));
    }

    result = iter.next();
    assert(!result);
}

const std::vector<Attachment>& Message::attachments() const noexcept
{
    return m_attachments;
}

const std::string& Message::text() const noexcept
{
    return m_text;
}

user_id_t Message::from() const noexcept
{
    return m_from;
}

user_id_t Message::to() const noexcept
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
    for (const Attachment& attachment : m_attachments)
        attachments.array_push(cbor::build_bytestring(attachment.buffer().data(), attachment.buffer().size()));

    root.array_push(from);
    root.array_push(to);
    root.array_push(text);
    root.array_push(attachments);

    return root;
}

cbor::Buffer Message::to_cbor_buffer() const
{
    return cbor::Buffer(to_cbor_dom());
}

void Message::to_bson_buffer(bson::Base& parent, std::string_view key) const
{
    bson::SubArray message_bson(parent, key);

    message_bson.append_uint64(m_from);
    message_bson.append_uint64(m_to);
    message_bson.append_utf8(m_text);

    {
        HW1_BSON_DECLARE_STRING_INDEX(index, message_bson)
        bson::SubArray attachments_bson(message_bson, index);
        message_bson.increment();
        for (const Attachment& attachment : m_attachments)
            attachments_bson.append_binary(std::span(attachment.buffer().data(), attachment.buffer().size()));
    }
}

std::ostream& operator<<(std::ostream& output, const Message& message)
{
    output << message.from() << ", "
             << message.to() << ", \""
             << message.text() << "\",\n{\n";
    for (const Attachment& attachment : message.attachments())
        output << '\t' << attachment << '\n';
    output << "}\n";

    return output;
}

MessageVector::MessageVector(std::vector<Message> messages)
    : m_messages(std::move(messages))
{
}

MessageVector::MessageVector(const cbor::Item& item)
{
    assert(cbor_isa_array(item));

    cbor_item_t** item_handle = cbor_array_handle(item);
    std::size_t message_count = cbor_array_size(item);
    m_messages.reserve(message_count);
    for (std::size_t i = 0; i < message_count; ++i)
        m_messages.emplace_back(Message(item_handle[i]));
}

MessageVector::MessageVector(const cbor::Buffer& buffer)
    : MessageVector(cbor::Item(buffer))
{
}

MessageVector::MessageVector(const msgpack::object_handle& oh)
{
    oh.get().convert(m_messages);
}

MessageVector::MessageVector(const msgpack::sbuffer& sbuf)
    : MessageVector(msgpack::unpack(sbuf.data(), sbuf.size()))
{
}

MessageVector::MessageVector(bson::Iter& iter)
{
    bool result;

    result = iter.next();
    assert(result);
    bson::Iter array = iter.as_array();

    while (array.next())
    {
        bson::Iter message_iter = array.as_array();
        m_messages.emplace_back(Message(message_iter));
    }

    result = iter.next();
    assert(!result);
}

const std::vector<Message>& MessageVector::messages() const noexcept
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
            HW1_BSON_DECLARE_STRING_INDEX(index, array);
            message.to_bson_buffer(array, index);
            array.increment();
        }
    }
    return buffer;
}

}  // namespace hw1
