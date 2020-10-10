#ifndef BINARY_SERIALIZATION_MESSAGE_HPP_
#define BINARY_SERIALIZATION_MESSAGE_HPP_

#include "types.hpp"
#include "cbor_wrapper.hpp"

#include <msgpack.hpp>

#include <cstdint>
#include <vector>
#include <string>
#include <ostream>

namespace hw1
{

class Attachment
{
public:
    Attachment() = default;
    Attachment(const Attachment&) = default;
    Attachment& operator=(const Attachment&) = default;
    Attachment(Attachment&&) = default;
    Attachment& operator=(Attachment&&) = default;

    Attachment(std::vector<byte_t> buffer);

    bool operator==(const Attachment&) const = default;

    [[nodiscard]] const std::vector<byte_t>& buffer() const;

private:
    std::vector<byte_t> m_buffer;

public:
    MSGPACK_DEFINE(m_buffer)
};

std::ostream& operator<<(std::ostream& output, const Attachment& attachment);

class Message
{
public:
    Message() = default;
    Message(const Message&) = default;
    Message& operator=(const Message&) = default;
    Message(Message&&) = default;
    Message& operator=(Message&&) = default;

    Message(user_id_t from, user_id_t to, std::string text,
            std::vector<Attachment> attachments);

    explicit Message(const cbor::Item& item);
    explicit Message(const cbor::Buffer& buffer);
    explicit Message(const msgpack::object_handle& oh);
    explicit Message(const msgpack::sbuffer& sbuf);

    bool operator==(const Message&) const = default;

    [[nodiscard]] const std::vector<Attachment>& attachments() const;
    [[nodiscard]] const std::string& text() const;
    [[nodiscard]] user_id_t from() const;
    [[nodiscard]] user_id_t to() const;

private:
    std::vector<Attachment> m_attachments;
    std::string m_text;
    user_id_t m_from = INVALID_USER_ID;
    user_id_t m_to = INVALID_USER_ID;

public:
    MSGPACK_DEFINE(m_from, m_to, m_text, m_attachments)

    [[nodiscard]] msgpack::object_handle to_msgpack_dom() const;
    [[nodiscard]] msgpack::sbuffer to_msgpack_buffer() const;

    [[nodiscard]] cbor::Item to_cbor_dom() const;
    [[nodiscard]] cbor::Buffer to_cbor_buffer() const;
};

std::ostream& operator<<(std::ostream& output, const Message& message);

}  // namespace hw1

#endif // BINARY_SERIALIZATION_MESSAGE_HPP_
