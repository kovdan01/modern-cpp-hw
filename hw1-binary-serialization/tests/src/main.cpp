#include <message.hpp>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>
#include <msgpack.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

hw1::MessageVector g_messages;

int main(int argc, char** argv) try
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Catch::Session session;

    std::string in_file_name;
    Catch::clara::Parser cli = session.cli()
        | Catch::clara::Opt(in_file_name, "input")["-I"]["--input"]("Name of file with test data").required();
    session.cli(cli);

    int ret_code = session.applyCommandLine(argc, argv);
    if (ret_code != 0)
    {
        std::cerr << "Error while parsing command line options" << std::endl;
        return ret_code;
    }

    Catch::Option<std::size_t> count = Catch::list(std::make_shared<Catch::Config>(session.configData()));
    if (count)
        return count.valueOr(0);

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

    std::cout << "Starting tests..." << std::endl;
    return session.run();
}
catch (const std::exception& e)
{
    std::cerr << e.what() << '\n';
    return 1;
}
