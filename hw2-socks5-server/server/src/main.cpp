#include <event_loop.hpp>
#include <socket.hpp>

#include <tclap/CmdLine.h>

#include <optional>
#include <thread>

struct Params
{
    std::size_t threads_count;
    in_port_t port;
};

static std::optional<Params> parse_cmd_line(int argc, char* argv[])
{
    try
    {
        TCLAP::CmdLine cmd("Help", ' ', "0.1");

        const std::size_t default_thread_count = std::thread::hardware_concurrency();
        TCLAP::ValueArg<std::size_t> threads_count_arg(
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
        std::size_t threads_count = threads_count_arg.getValue();
        in_port_t port = port_arg.getValue();

        if (threads_count == 0)
            threads_count = default_thread_count;

        std::cout << "Using " << threads_count << " threads, port " << port << std::endl;

        return Params{ .threads_count = threads_count, .port = port };
    }
    catch (TCLAP::ArgException& e)
    {
        std::cerr << "Error: " << e.error() << " for arg " << e.argId() << std::endl;
        return std::nullopt;
    }
}

int main(int argc, char* argv[]) try
{
    std::optional<Params> params = parse_cmd_line(argc, argv);
    if (params == std::nullopt)
        return 1;

    int nconnections = 2;
    hw2::Buffers buffers(nconnections);

    hw2::MainSocket server_socket(params->port, nconnections);
    hw2::IoUring uring(server_socket, nconnections);
    ::signal(SIGPIPE, SIG_IGN);
    uring.event_loop();

    return 0;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
}
catch (...)
{
    std::cerr << "Unknown error" << std::endl;
}
