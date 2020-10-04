#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <msgpack.hpp>

#include <cstdint>
#include <vector>
#include <string>
#include <ostream>

namespace hw1
{

using byte_t = std::uint8_t;

using user_id_t = std::uint64_t;
inline constexpr user_id_t INVALID_USER_ID = user_id_t(-1);

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

    const std::vector<byte_t>& buffer() const;

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

    bool operator==(const Message&) const = default;

    const std::vector<Attachment>& attachments() const;
    const std::string& text() const;
    user_id_t from() const;
    user_id_t to() const;

private:
    std::vector<Attachment> m_attachments;
    std::string m_text;
    user_id_t m_from = INVALID_USER_ID;
    user_id_t m_to = INVALID_USER_ID;

public:
    MSGPACK_DEFINE(m_from, m_to, m_text, m_attachments)
};

std::ostream& operator<<(std::ostream& output, const Message& message);

}  // namespace hw1

#endif // MESSAGE_HPP
