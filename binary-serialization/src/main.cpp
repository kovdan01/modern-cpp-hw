#include "message.hpp"
#include "cbor_wrapper.hpp"

#include <msgpack.hpp>
#include <cbor.h>
#include <bson/bson.h>
#include <benchmark/benchmark.h>

#include "test_input.inc"

namespace bm = benchmark;

static void msgpack_dom_to_serialized(bm::State& state)
{
    msgpack::object_handle dom = msg.to_msgpack_dom();
    for (auto _ : state)
    {
        msgpack::sbuffer packed;
        msgpack::pack(packed, dom.get());
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(msgpack_dom_to_serialized);

static void cbor_dom_to_serialized(bm::State& state)
{
    hw1::cbor::Item dom = msg.to_cbor_dom();
    for (auto _ : state)
    {
        hw1::cbor::Buffer packed(dom);
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(cbor_dom_to_serialized);

static void msgpack_serialized_to_dom(bm::State& state)
{
    msgpack::sbuffer packed = msg.to_msgpack_buffer();
    for (auto _ : state)
    {
        msgpack::object_handle dom = msgpack::unpack(packed.data(), packed.size());
        bm::DoNotOptimize(dom);
    }
}
BENCHMARK(msgpack_serialized_to_dom);

static void cbor_serialized_to_dom(bm::State& state)
{
    hw1::cbor::Buffer packed = msg.to_cbor_buffer();
    for (auto _ : state)
    {
        hw1::cbor::Item dom(packed);
        bm::DoNotOptimize(dom);
    }
}
BENCHMARK(cbor_serialized_to_dom);

BENCHMARK_MAIN();

static void test()
{
    {
        msgpack::sbuffer sbuf = msg.to_msgpack_buffer();
        hw1::Message m(sbuf);
        assert(msg == m);
    }
    {
        msgpack::object_handle oh = msg.to_msgpack_dom();
        hw1::Message m(oh);
        assert(msg == m);
    }
    {
        hw1::cbor::Buffer buffer = msg.to_cbor_buffer();
        hw1::Message m(buffer);
        assert(msg == m);
    }
    {
        hw1::cbor::Item item = msg.to_cbor_dom();
        hw1::Message m(item);
        assert(msg == m);
    }
}
