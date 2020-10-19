#include <bson_wrapper.hpp>
#include <cbor_wrapper.hpp>
#include <message.hpp>

#include <benchmark/benchmark.h>
#include <boost/program_options.hpp>
#include <msgpack.hpp>

#include <fstream>
#include <iostream>
#include <vector>

static hw1::MessageVector g_messages;

namespace bm = benchmark;

static void msgpack_dom_to_serialized(bm::State& state)
{
    msgpack::object_handle dom = g_messages.to_msgpack_dom();
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        msgpack::sbuffer packed;
        msgpack::pack(packed, dom.get());
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(msgpack_dom_to_serialized);  // NOLINT cert-err58-cpp

static void msgpack_serialized_to_dom(bm::State& state)
{
    msgpack::sbuffer packed = g_messages.to_msgpack_buffer();
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        msgpack::object_handle dom = msgpack::unpack(packed.data(), packed.size());
        bm::DoNotOptimize(dom);
    }
}
BENCHMARK(msgpack_serialized_to_dom);  // NOLINT cert-err58-cpp

static void msgpack_object_to_serialized(bm::State& state)
{
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        msgpack::sbuffer packed;
        msgpack::pack(packed, g_messages.messages());
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(msgpack_object_to_serialized);  // NOLINT cert-err58-cpp

static void msgpack_serialized_to_object(bm::State& state)
{
    msgpack::sbuffer packed = g_messages.to_msgpack_buffer();
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        hw1::MessageVector unpacked(packed);
        bm::DoNotOptimize(unpacked);
    }
}
BENCHMARK(msgpack_serialized_to_object);  // NOLINT cert-err58-cpp

static void cbor_dom_to_serialized(bm::State& state)
{
    hw1::cbor::Item dom = g_messages.to_cbor_dom();
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        hw1::cbor::Buffer packed(dom);
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(cbor_dom_to_serialized);  // NOLINT cert-err58-cpp

static void cbor_serialized_to_dom(bm::State& state)
{
    hw1::cbor::Buffer packed = g_messages.to_cbor_buffer();
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        hw1::cbor::Item dom(packed);
        bm::DoNotOptimize(dom);
    }
}
BENCHMARK(cbor_serialized_to_dom);  // NOLINT cert-err58-cpp

static void cbor_object_to_serialized(bm::State& state)
{
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        hw1::cbor::Buffer packed = g_messages.to_cbor_buffer();
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(cbor_object_to_serialized);  // NOLINT cert-err58-cpp

static void cbor_serialized_to_object(bm::State& state)
{
    hw1::cbor::Buffer packed = g_messages.to_cbor_buffer();
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        hw1::MessageVector unpacked(packed);
        bm::DoNotOptimize(unpacked);
    }
}
BENCHMARK(cbor_serialized_to_object);  // NOLINT cert-err58-cpp

static void bson_object_to_serialized(bm::State& state)
{
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        hw1::bson::Ptr packed = g_messages.to_bson_buffer();
        bm::DoNotOptimize(packed);
    }
}
BENCHMARK(bson_object_to_serialized);  // NOLINT cert-err58-cpp

static void bson_serialized_to_object(bm::State& state)
{
    hw1::bson::Ptr packed = g_messages.to_bson_buffer();
    for (auto _ : state)  // NOLINT clang-analyzer-deadcode.DeadStores
    {
        hw1::bson::Iter iter(packed->handle());
        hw1::MessageVector unpacked(iter);
        bm::DoNotOptimize(unpacked);
    }
}
BENCHMARK(bson_serialized_to_object);  // NOLINT cert-err58-cpp

int main(int argc, char** argv) try
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,H",                                                      "Print this message")
        ("input,I",             po::value<std::string>()->required(),   "Filename with test data")
        ("benchmark_filter",    po::value<std::string>(),               "Regex that specifies what benchmarks to run")
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
    if (!in_file.is_open())
        throw std::runtime_error("Error while opening input file");
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
    g_messages = hw1::MessageVector(oh);
    std::cout << "Convert complete!" << std::endl;

    std::cout << "Starting benchmarks..." << std::endl;
    bm::Initialize(&argc, argv);
    bm::RunSpecifiedBenchmarks();
}
catch (const std::exception& e)
{
    std::cerr << e.what() << '\n';
    return 1;
}
