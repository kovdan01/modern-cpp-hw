#include <message.hpp>

#include <boost/program_options.hpp>
#include <msgpack.hpp>

#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static std::mt19937 g_prng(std::random_device{}());

static std::string generate_text(std::size_t t_size)
{
    static std::uniform_int_distribution<char> dist(std::numeric_limits<char>::min(),
                                                    std::numeric_limits<char>::max());
    std::string ans;
    ans.reserve(t_size);
    for (std::size_t i = 0; i < t_size; ++i)
    {
        char sym;
        do
        {
            sym = dist(g_prng);
        } while (!std::isprint(sym));
        ans.push_back(sym);
    }

    return ans;
}

static hw1::Attachment generate_attachment(std::size_t t_size)
{
    static std::uniform_int_distribution<hw1::byte_t> dist(std::numeric_limits<hw1::byte_t>::min(),
                                                           std::numeric_limits<hw1::byte_t>::max());
    std::vector<hw1::byte_t> ans;
    ans.reserve(t_size);
    for (std::size_t i = 0; i < t_size; ++i)
        ans.emplace_back(dist(g_prng));

    return hw1::Attachment(ans);
}

static hw1::Message generate_message(std::size_t t_text_from, std::size_t t_text_to,
                                     std::size_t t_attach_count_from, std::size_t t_attach_count_to,
                                     std::size_t t_attach_size_from, std::size_t t_attach_size_to)
{
    static std::uniform_int_distribution<hw1::user_id_t> user_id_dist(std::numeric_limits<hw1::user_id_t>::min(),
                                                                      std::numeric_limits<hw1::user_id_t>::max());
    hw1::user_id_t from = user_id_dist(g_prng);
    hw1::user_id_t to   = user_id_dist(g_prng);

    std::string text = generate_text(std::uniform_int_distribution<std::size_t>(t_text_from, t_text_to)(g_prng));

    // some magic constants here

    static std::gamma_distribution<double> attach_count_dist(1, 1);
    std::size_t attach_count;
    double magic_constant = static_cast<double>(t_attach_count_to) / 10;
    do
    {
        attach_count = static_cast<std::size_t>(magic_constant * attach_count_dist(g_prng));
    } while (attach_count < t_attach_count_from || attach_count > t_attach_count_to);

    std::vector<hw1::Attachment> attachments;
    attachments.reserve(attach_count);

    static std::gamma_distribution<double> attach_size_dist(1.5, 1.2);
    for (std::size_t i = 0; i < attach_count; ++i)
    {
        std::size_t attach_size;
        double magic_constant = static_cast<double>(t_attach_size_to) / 8;
        do
        {
            attach_size = static_cast<std::size_t>(magic_constant * attach_size_dist(g_prng));
        } while (attach_size < t_attach_size_from || attach_size > t_attach_size_to);
        attachments.emplace_back(generate_attachment(attach_size));
    }

    return hw1::Message(from, to, std::move(text), std::move(attachments));
}

static std::vector<hw1::Message> generate_vector_of_messages(std::size_t t_size, std::size_t t_text_from, std::size_t t_text_to,
                                                             std::size_t t_attach_count_from, std::size_t t_attach_count_to,
                                                             std::size_t t_attach_size_from, std::size_t t_attach_size_to)
{
    std::vector<hw1::Message> ans;
    ans.reserve(t_size);
    for (std::size_t i = 0; i < t_size; ++i)
        ans.emplace_back(generate_message(t_text_from, t_text_to, t_attach_count_from,
                                          t_attach_count_to, t_attach_size_from, t_attach_size_to));

    return ans;
}

int main(int argc, char* argv[]) try
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,H",                                                                  "Print this message")
        ("output,O",            po::value<std::string>()->required(),               "Filename to place generated data")
        ("count,C",             po::value<std::size_t>()->default_value(1000),      "Number of messages to generate")
        ("text_from",           po::value<std::size_t>()->default_value(0),         "Min message text length")
        ("text_to",             po::value<std::size_t>()->default_value(250),       "Max message text length")
        ("attach_count_from",   po::value<std::size_t>()->default_value(0),         "Min attachments count")
        ("attach_count_to",     po::value<std::size_t>()->default_value(10),        "Max attachments count")
        ("attach_size_from",    po::value<std::size_t>()->default_value(1 << 9),    "Min attachment size")  // 512 bytes
        ("attach_size_to",      po::value<std::size_t>()->default_value(1 << 22),   "Max attachment size")  // 4 megabytes
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

    std::string out_file_name     = vm["output"]           .as<std::string>();
    std::size_t msg_count         = vm["count"]            .as<std::size_t>();
    std::size_t text_from         = vm["text_from"]        .as<std::size_t>();
    std::size_t text_to           = vm["text_to"]          .as<std::size_t>();
    std::size_t attach_count_from = vm["attach_count_from"].as<std::size_t>();
    std::size_t attach_count_to   = vm["attach_count_to"]  .as<std::size_t>();
    std::size_t attach_size_from  = vm["attach_size_from"] .as<std::size_t>();
    std::size_t attach_size_to    = vm["attach_size_to"]   .as<std::size_t>();

    if (text_from > text_to)
        throw std::runtime_error("Error: text_from must be less or equal than text_to");
    if (attach_count_from > attach_count_to)
        throw std::runtime_error("Error: attach_count_from must be less or equal than attach_count_to");
    if (attach_size_from > attach_size_to)
        throw std::runtime_error("Error: attach_size_from must be less or equal than attach_size_to");

    std::vector<hw1::Message> messages = generate_vector_of_messages(msg_count, text_from, text_to, attach_count_from,
                                                                     attach_count_to, attach_size_from, attach_size_to);

    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, messages);

    std::ofstream out_file(out_file_name, std::ios_base::binary);
    if (!out_file.is_open())
        throw std::runtime_error("Error while opening output file");
    out_file.write(sbuf.data(), static_cast<std::streamsize>(sbuf.size()));

    return 0;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << '\n';
    return 1;
}
