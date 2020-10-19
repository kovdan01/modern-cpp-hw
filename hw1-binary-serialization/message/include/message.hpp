#ifndef HW1_BINARY_SERIALIZATION_MESSAGE_MESSAGE_HPP_
#define HW1_BINARY_SERIALIZATION_MESSAGE_MESSAGE_HPP_

#include "types.hpp"
#include <bson_wrapper.hpp>
#include <cbor_wrapper.hpp>
#include <hw1/message/export.h>

#include <msgpack.hpp>

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace hw1
{

class HW1_MESSAGE_EXPORT Attachment
{
public:
    Attachment() = default;
    Attachment(const Attachment&) = default;
    Attachment& operator=(const Attachment&) = default;
    Attachment(Attachment&&) = default;
    Attachment& operator=(Attachment&&) = default;

    explicit Attachment(std::vector<byte_t> buffer);

    bool operator==(const Attachment&) const = default;

    [[nodiscard]] const std::vector<byte_t>& buffer() const;

private:
    std::vector<byte_t> m_buffer;

public:
    MSGPACK_DEFINE(m_buffer)
};

std::ostream& operator<<(std::ostream& output, const Attachment& attachment);

class HW1_MESSAGE_EXPORT Message
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
    explicit Message(bson::Iter& iter);

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

    void to_bson_buffer(bson::Base& parent, std::string_view key) const;
};

std::ostream& operator<<(std::ostream& output, const Message& message);

class HW1_MESSAGE_EXPORT MessageVector
{
public:
    MessageVector() = default;
    MessageVector(const MessageVector&) = default;
    MessageVector& operator=(const MessageVector&) = default;
    MessageVector(MessageVector&&) noexcept = default;
    MessageVector& operator=(MessageVector&&) noexcept = default;

    explicit MessageVector(std::vector<Message> messages);

    explicit MessageVector(const cbor::Item& item);
    explicit MessageVector(const cbor::Buffer& buffer);
    explicit MessageVector(const msgpack::object_handle& oh);
    explicit MessageVector(const msgpack::sbuffer& sbuf);
    explicit MessageVector(bson::Iter& iter);

    bool operator==(const MessageVector&) const = default;

    [[nodiscard]] const std::vector<Message>& messages() const;

    [[nodiscard]] msgpack::object_handle to_msgpack_dom() const;
    [[nodiscard]] msgpack::sbuffer to_msgpack_buffer() const;

    [[nodiscard]] cbor::Item to_cbor_dom() const;
    [[nodiscard]] cbor::Buffer to_cbor_buffer() const;

    [[nodiscard]] bson::Ptr to_bson_buffer() const;

private:
    std::vector<Message> m_messages;
};

}  // namespace hw1

#endif  // HW1_BINARY_SERIALIZATION_MESSAGE_MESSAGE_HPP_
