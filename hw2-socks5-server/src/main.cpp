#include <server.hpp>
#include <socket.hpp>
#include <utils.hpp>

#include <tclap/CmdLine.h>

#include <optional>
#include <thread>

struct Params
{
    unsigned threads_count;
    in_port_t port;
};

static std::optional<Params> parse_cmd_line(int argc, char* argv[])
{
    try
    {
        TCLAP::CmdLine cmd("Help", ' ', "0.1");

        const unsigned default_thread_count = std::thread::hardware_concurrency();
        TCLAP::ValueArg<unsigned> threads_count_arg(
            /* short flag */    "t",
            /* long flag */     "threads",
            /* description */   "Thread count to use (0 stands for maximum hardware available threads)",
            /* required */      false,
            /* default */       default_thread_count,
            /* type info */     "int"
        );
        cmd.add(threads_count_arg);

        TCLAP::ValueArg<in_port_t> port_arg(
            /* short flag */    "p",
            /* long flag */     "port",
            /* description */   "Port number to use",
            /* required */      true,
            /* default */       0,
            /* type info */     "int"
        );
        cmd.add(port_arg);

        cmd.parse(argc, argv);
        unsigned threads_count = threads_count_arg.getValue();
        in_port_t port = port_arg.getValue();

        if (threads_count == 0)
            threads_count = default_thread_count;

        hw2::logger()->info("Using {0:d} threads", threads_count);
        hw2::logger()->info("Using port {0:d}", port);
     
        return Params{ .threads_count = threads_count, .port = port };
    }
    catch (TCLAP::ArgException& e)
    {
        hw2::logger()->error("Parsing command line arguments failed: '{0}' for arg {1}", e.error(), e.argId());
        return std::nullopt;
    }
}

static void signal_handler_sigint(int /* signum */)
{
    hw2::logger()->critical("Received SIGINT signal, exiting...");
    std::exit(EXIT_FAILURE);
}

static void signal_handler_sigquit(int /* signum */)
{
    hw2::logger()->critical("Received SIGQUIT signal, exiting...");
    std::exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) try
{    
    hw2::logger()->info("Welcome to io_uring-based SOCKS5 server!");

    try
    {
        std::optional<Params> params = parse_cmd_line(argc, argv);
        if (params == std::nullopt)
        {
            return EXIT_FAILURE;
        }

        if (::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        {
            hw2::logger()->critical("Cannot set signal handler for SIGPIPE");
            return EXIT_FAILURE;
        }
        if (::signal(SIGINT, signal_handler_sigint) == SIG_ERR)
        {
            hw2::logger()->critical("Cannot set signal handler for SIGINT");
            return EXIT_FAILURE;
        }
        if (::signal(SIGQUIT, signal_handler_sigquit) == SIG_ERR)
        {
            hw2::logger()->critical("Cannot set signal handler for SIGQUIT");
            return EXIT_FAILURE;
        }

        unsigned nconnections = 4096;
        hw2::MainSocket server_socket(params->port, static_cast<int>(nconnections * params->threads_count));

        auto thread_function = [&server_socket, nconnections]()
        {
            hw2::IoUring uring(server_socket, nconnections);
            uring.event_loop();
        };

        std::vector<std::thread> threads(params->threads_count - 1);
        for (std::thread& thread : threads)
        {
            thread = std::thread(thread_function);
        }
        thread_function();
    }
    catch (const std::exception& e)
    {
        hw2::logger()->critical("Unhandled exception: {}", e.what());
    }

    return EXIT_SUCCESS;
}
catch (...)
{
    std::cerr << "Unknown error" << std::endl;
}
