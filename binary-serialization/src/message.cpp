#include "message.hpp"

#include <utility>
#include <iomanip>

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
    : m_attachments(t_attachments)
    , m_text(t_text)
    , m_from(t_from)
    , m_to(t_to)
{
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

CborBuffer Message::cbor_pack() const
{
    CborItem root = cbor_new_definite_array(4);

    CborItem from = cbor_build_uint64(m_from);
    CborItem to   = cbor_build_uint64(m_to);
    CborItem text = cbor_build_stringn(m_text.c_str(), m_text.size());

    CborItem attachments = cbor_new_definite_array(m_attachments.size());
    std::vector<CborItem> attachments_wrapper;
    attachments_wrapper.reserve(m_attachments.size());
    for (const Attachment& attachment : m_attachments)
    {
        attachments_wrapper.emplace_back(cbor_build_bytestring(attachment.buffer().data(), attachment.buffer().size()));
        cbor_array_push(attachments, attachments_wrapper.back());
    }

    cbor_array_push(root, from);
    cbor_array_push(root, to);
    cbor_array_push(root, text);
    cbor_array_push(root, attachments);

    CborBuffer cbor_buf(root);
    return cbor_buf;
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

}  // namespace hw1
