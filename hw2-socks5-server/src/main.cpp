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
    bool kerlen_polling;
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

        TCLAP::SwitchArg kernel_polling_arg(
            /* short flag */    "k",
            /* long flag */     "kernel_polling",
            /* description */   "Enable kernel polling (root required)",
            /* default */       false
        );
        cmd.add(kernel_polling_arg);

        cmd.parse(argc, argv);
        unsigned threads_count = threads_count_arg.getValue();
        in_port_t port = port_arg.getValue();
        bool kernel_polling = kernel_polling_arg.getValue();

        if (threads_count == 0)
            threads_count = default_thread_count;

        hw2::logger()->info("Using {0:d} threads", threads_count);
        hw2::logger()->info("Using port {0:d}", port);
        if (kernel_polling)
            hw2::logger()->info("Using kernel polling");
     
        return Params{ .threads_count = threads_count, .port = port, .kerlen_polling = kernel_polling };
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

        constexpr unsigned nconnections = 1 << 15;
        hw2::MainSocket server_socket(params->port, static_cast<int>(nconnections));

        rlimit file_limit = hw2::syscall_wrapper::getrlimit_nofile();
        file_limit.rlim_cur = nconnections * 2;
        hw2::syscall_wrapper::setrlimit_nofile(file_limit);

        rlimit memory_limit = hw2::syscall_wrapper::getrlimit_memlock();
        memory_limit.rlim_cur = (1 << 16);
        hw2::syscall_wrapper::setrlimit_memlock(memory_limit);

        auto thread_function = [&server_socket, &params]()
        {
            hw2::IoUring uring(server_socket, nconnections, params->kerlen_polling);
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
