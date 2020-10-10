#include <cbor_wrapper.hpp>
#include <message.hpp>

#include <benchmark/benchmark.h>
#include <boost/program_options.hpp>
#include <msgpack.hpp>

#include <fstream>
#include <iostream>
#include <vector>

//#include "test_input.inc"

static hw1::MessageVector messages;

namespace bm = benchmark;

static void msgpack_dom_to_serialized(bm::State& state)
{
    msgpack::object_handle dom = messages.to_msgpack_dom();
    for (auto _ : state)
    {
        msgpack::sbuffer packed;
        msgpack::pack(packed, dom.get());
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(msgpack_dom_to_serialized);

static void msgpack_serialized_to_dom(bm::State& state)
{
    msgpack::sbuffer packed = messages.to_msgpack_buffer();
    for (auto _ : state)
    {
        msgpack::object_handle dom = msgpack::unpack(packed.data(), packed.size());
        bm::DoNotOptimize(dom);
    }
}
BENCHMARK(msgpack_serialized_to_dom);

static void msgpack_object_to_serialized(bm::State& state)
{
    for (auto _ : state)
    {
        msgpack::sbuffer packed;
        msgpack::pack(packed, messages.messages());
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(msgpack_object_to_serialized);

static void msgpack_serialized_to_object(bm::State& state)
{
    msgpack::sbuffer packed = messages.to_msgpack_buffer();
    for (auto _ : state)
    {
        hw1::MessageVector unpacked(packed);
        bm::DoNotOptimize(unpacked);
    }
}
BENCHMARK(msgpack_serialized_to_object);

static void cbor_dom_to_serialized(bm::State& state)
{
    hw1::cbor::Item dom = messages.to_cbor_dom();
    for (auto _ : state)
    {
        hw1::cbor::Buffer packed(dom);
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(cbor_dom_to_serialized);

static void cbor_serialized_to_dom(bm::State& state)
{
    hw1::cbor::Buffer packed = messages.to_cbor_buffer();
    for (auto _ : state)
    {
        hw1::cbor::Item dom(packed);
        bm::DoNotOptimize(dom);
    }
}
BENCHMARK(cbor_serialized_to_dom);

static void cbor_object_to_serialized(bm::State& state)
{
    for (auto _ : state)
    {
        hw1::cbor::Buffer packed = messages.to_cbor_buffer();
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(cbor_object_to_serialized);

static void cbor_serialized_to_object(bm::State& state)
{
    hw1::cbor::Buffer packed = messages.to_cbor_buffer();
    for (auto _ : state)
    {
        hw1::MessageVector unpacked(packed);
        bm::DoNotOptimize(unpacked);
    }
}
BENCHMARK(cbor_serialized_to_object);

static void test()
{
    std::cout << "Performing tests...\n";
    const hw1::MessageVector& expected = messages;
    {
        msgpack::sbuffer sbuf = expected.to_msgpack_buffer();
        hw1::MessageVector got(sbuf);
        assert(expected == got);
    }
    {
        msgpack::sbuffer sbuf = expected.to_msgpack_buffer();
        msgpack::object_handle oh = msgpack::unpack(sbuf.data(), sbuf.size());
        hw1::MessageVector got(oh);
        assert(expected == got);
    }
    {
        msgpack::object_handle oh = expected.to_msgpack_dom();
        hw1::MessageVector got(oh);
        assert(expected == got);
    }
    {
        hw1::cbor::Buffer buffer = expected.to_cbor_buffer();
        hw1::MessageVector got(buffer);
        assert(expected == got);
    }
    {
        hw1::cbor::Buffer buffer = expected.to_cbor_buffer();
        hw1::cbor::Item item(buffer);
        hw1::MessageVector got(item);
        assert(expected == got);
    }
    {
        hw1::cbor::Item item = expected.to_cbor_dom();
        hw1::MessageVector got(item);
        assert(expected == got);
    }
    std::cout << "Passed!\n";
}

int main(int argc, char** argv) try
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,H",                                              "Print this message")
        ("input,I",     po::value<std::string>()->required(),   "Filename with test data")
        ;

    po::variables_map vm;
    try
    {
        po::store(parse_command_line(argc, argv, desc), vm);
        if (vm.contains("help"))
        {
            std::cout << desc << "\n";
            return 0;
        }
        po::notify(vm);
    }
    catch (const po::error& error)
    {
        std::cerr << "Error while parsing command-line arguments: "
                  << error.what() << "\nPlease use --help to see help message\n";
        return 1;
    }

    std::string in_file_name = vm["input"].as<std::string>();
    std::ifstream in_file(in_file_name, std::ios_base::binary | std::ios_base::ate);
    std::streamsize size = in_file.tellg();
    in_file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<std::size_t>(size));
    std::cout << "Reading data from file \"" << in_file_name << "\"..." << std::endl;
    if (!in_file.read(buffer.data(), size))
        throw std::runtime_error("Error while reading from file");
    std::cout << "Read complete!\n"
                 "Deserializing test data from binary to DOM..." << std::endl;

    msgpack::object_handle oh = msgpack::unpack(buffer.data(), buffer.size());
    std::cout << "Deserialize complete!\n"
              << "Converting from DOM to std::vector<hw1::Message>..." << std::endl;
    messages = hw1::MessageVector(oh);
    std::cout << "Convert complete!" << std::endl;

#ifndef NDEBUG
    test();
#endif

    std::cout << "Starting benchmarks..." << std::endl;
    bm::Initialize(&argc, argv);
    bm::RunSpecifiedBenchmarks();
}
catch (const std::exception& e)
{
    std::cerr << e.what() << '\n';
    return 1;
}
