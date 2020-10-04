#include "message.hpp"

#include <msgpack.hpp>

#include <vector>
#include <iostream>
#include <cassert>

template <typename T>
void print_vector(const std::vector<T>& t_vec)
{
    for (const T& elem : t_vec)
        std::cout << elem << '\n';
}

std::vector<hw1::Message> get_messages()
{
    std::vector<hw1::Message> messages;
    messages.emplace_back(1, 2, std::string{"Hello"}, std::vector<hw1::Attachment>{});
    messages.emplace_back(1, 2, std::string{", world!"}, std::vector<hw1::Attachment>{std::vector<hw1::byte_t>{0x00, 0x01, 0x02}, std::vector<hw1::byte_t>{0x5f, 0x60, 0x61}});

    return messages;
}

msgpack::sbuffer pack_messages(const std::vector<hw1::Message>& t_messages)
{
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, t_messages);
    return sbuf;
}

std::vector<hw1::Message> unpack_messages(const msgpack::sbuffer& t_sbuf)
{
    msgpack::object_handle oh = msgpack::unpack(t_sbuf.data(), t_sbuf.size());
    msgpack::object obj = oh.get();

    std::vector<hw1::Message> messages;
    obj.convert(messages);

    return messages;
}

int main()
{
    std::vector<hw1::Message> msg_before = get_messages();
    msgpack::sbuffer packed = pack_messages(msg_before);
    std::vector<hw1::Message> msg_after = unpack_messages(packed);
    assert(msg_before == msg_after);
}
