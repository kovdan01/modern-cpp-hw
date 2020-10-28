#include <bson_wrapper.hpp>
#include <cbor_wrapper.hpp>
#include <message.hpp>

#include <catch2/catch.hpp>
#include <msgpack.hpp>

extern hw1::MessageVector g_messages;

TEST_CASE("C++ obj serialized with MsgPack to bin             and than deserialized             is equal to itself",
          "[msgpack]")
{
    msgpack::sbuffer sbuf = g_messages.to_msgpack_buffer();
    hw1::MessageVector got(sbuf);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with MsgPack to bin             and than deserialized through DOM is equal to itself",
          "[msgpack][.intermediate]")
{
    msgpack::sbuffer sbuf = g_messages.to_msgpack_buffer();
    msgpack::object_handle oh = msgpack::unpack(sbuf.data(), sbuf.size());
    hw1::MessageVector got(oh);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with MsgPack to bin through DOM and than deserialized             is equal to itself",
          "[msgpack][.intermediate]")
{
    msgpack::object_handle oh = g_messages.to_msgpack_dom();
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, oh.get());
    hw1::MessageVector got(sbuf);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with MsgPack to bin through DOM and than deserialized through DOM is equal to itself",
          "[msgpack][.intermediate]")
{
    msgpack::object_handle oh1 = g_messages.to_msgpack_dom();
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, oh1.get());
    msgpack::object_handle oh2 = msgpack::unpack(sbuf.data(), sbuf.size());
    hw1::MessageVector got(oh2);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with MsgPack to DOM             and than deserialized             is equal to itself",
          "[msgpack]")
{
    msgpack::object_handle oh = g_messages.to_msgpack_dom();
    hw1::MessageVector got(oh);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with Cbor    to bin             and than deserialized             is equal to itself",
          "[cbor]")
{
    hw1::cbor::Buffer buffer = g_messages.to_cbor_buffer();
    hw1::MessageVector got(buffer);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with Cbor    to bin             and than deserialized through DOM is equal to itself",
          "[cbor][.intermediate]")
{
    hw1::cbor::Buffer buffer = g_messages.to_cbor_buffer();
    hw1::cbor::Item item(buffer);
    hw1::MessageVector got(item);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with Cbor    to bin through DOM and than deserialized             is equal to itself",
          "[cbor][.intermediate]")
{
    hw1::cbor::Item item = g_messages.to_cbor_dom();
    hw1::cbor::Buffer buffer(item);
    hw1::MessageVector got(buffer);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with Cbor    to bin through DOM and than deserialized through DOM is equal to itself",
          "[cbor][.intermediate]")
{
    hw1::cbor::Item item1 = g_messages.to_cbor_dom();
    hw1::cbor::Buffer buffer(item1);
    hw1::cbor::Item item2(buffer);
    hw1::MessageVector got(item2);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with Cbor    to DOM             and than deserialized             is equal to itself",
          "[cbor]")
{
    hw1::cbor::Item item = g_messages.to_cbor_dom();
    hw1::MessageVector got(item);
    REQUIRE(g_messages == got);
}

TEST_CASE("C++ obj serialized with Bson    to bin             and than deserialized             is equal to itself",
          "[bson]")
{
    hw1::bson::Ptr buffer = g_messages.to_bson_buffer();
    hw1::bson::Iter iter(buffer->handle());
    hw1::MessageVector got(iter);
    REQUIRE(g_messages == got);
}
