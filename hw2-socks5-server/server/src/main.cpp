#include <io_uring.hpp>
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

    hw2::ReceiveSocket socket(params->port);

    // initialize io_uring

    hw2::io_uring::Ring ring(2048);

    // add first accept SQE to monitor for new incoming connections
    struct sockaddr_in client_address;
    ring.accept(socket.fd, reinterpret_cast<struct sockaddr*>(&client_address), sizeof (client_address));

    // start event loop
    for (;;)
    {
        io_uring_submit_and_wait(ring.handle(), 1);
        struct io_uring_cqe* cqe;
        unsigned head;
        unsigned count = 0;

        // go through all CQEs
        io_uring_for_each_cqe(ring.handle(), head, cqe)
        {
            ++count;
            struct hw2::io_uring::ConnectionInfo conn_i;
            std::memcpy(&conn_i, &cqe->user_data, sizeof(conn_i));

            if (cqe->res == -ENOBUFS)
            {
                fprintf(stdout, "bufs in automatic buffer selection empty, this should not happen...\n");
                fflush(stdout);
                exit(1);
            }

            switch (conn_i.type)
            {
            case hw2::io_uring::ConnectionInfo::Type::PROVIDE_BUF:
            {
                if (cqe->res < 0)
                {
                    printf("cqe->res = %d\n", cqe->res);
                    exit(1);
                }
                break;
            }
            case hw2::io_uring::ConnectionInfo::Type::ACCEPT:
            {
                int sock_conn_fd = cqe->res;
                // only read when there is no error, >= 0
                if (sock_conn_fd >= 0)
                    ring.read(sock_conn_fd);

                // new connected client; read data from socket and re-add accept to monitor for new connections
                ring.accept(socket.fd, reinterpret_cast<struct sockaddr*>(&client_address), sizeof (client_address));
                break;
            }
            case hw2::io_uring::ConnectionInfo::Type::READ:
            {
                int bytes_read = cqe->res;
                int bid = cqe->flags >> 16;
                if (cqe->res <= 0)
                {
                    // read failed, re-add the buffer
                    ring.provide_buf(bid);
                    // connection closed or error
                    shutdown(conn_i.fd, SHUT_RDWR);
                }
                else
                {
                    // bytes have been read into bufs, now add write to socket sqe
                    ring.write(conn_i.fd, bid, bytes_read);
                }
                break;
            }
            case hw2::io_uring::ConnectionInfo::Type::WRITE:
            {
                // write has been completed, first re-add the buffer
                ring.provide_buf(conn_i.bid);
                // add a new read for the existing connection
                ring.read(conn_i.fd);
                break;
            }
            }
        }

        io_uring_cq_advance(ring.handle(), count);
    }
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
}
catch (...)
{
    std::cerr << "Unknown error" << std::endl;
}
