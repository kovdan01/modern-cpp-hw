#include "message.hpp"
#include "cbor_wrapper.hpp"

#include <msgpack.hpp>
#include <cbor.h>
#include <bson.h>
#include <benchmark/benchmark.h>

#include "test_input.inc"

namespace bm = benchmark;

static void pack_msgpack(bm::State& state)
{
    for (auto _ : state)
    {
        msgpack::sbuffer packed;
        msgpack::pack(packed, msg);
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(pack_msgpack);

static void pack_cbor(bm::State& state)
{
    for (auto _ : state)
    {
        hw1::CborBuffer packed = msg.cbor_pack();
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(pack_cbor);

static void unpack_msgpack(bm::State& state)
{
    msgpack::sbuffer packed;
    msgpack::pack(packed, msg);
    for (auto _ : state)
    {
        msgpack::object_handle oh = msgpack::unpack(packed.data(), packed.size());
        hw1::Message unpacked;
        oh.get().convert(unpacked);
        bm::DoNotOptimize(unpacked);
    }
}
BENCHMARK(unpack_msgpack);

static void unpack_cbor(bm::State& state)
{
    static const hw1::CborBuffer packed = msg.cbor_pack();
    for (auto _ : state)
    {
        hw1::Message unpacked = hw1::cbor_unpack_message(packed);
        bm::DoNotOptimize(unpacked);
    }
}
BENCHMARK(unpack_cbor);

BENCHMARK_MAIN();
