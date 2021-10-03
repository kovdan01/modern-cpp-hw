#ifndef HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
#define HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_

#include <socket.hpp>
#include <syscall.hpp>

#include <utils.hpp>

#include <liburing.h>

#include <ctime>
#include <limits>
#include <span>
#include <vector>

#define REPLY_200 "HTTP/1.0 200 OK\r\nServer: otus-io-uring\r\nDate: %s\r\n\
Content-Type: text/plain\r\nContent-Length: 12\r\n\r\nHello world!\r\n"

namespace hw2
{

enum class EventType
{
    ACCEPT = 0,
    READ,
    WRITE,
};

class Buffers
{
public:
    using byte_t = char;
    static constexpr std::size_t BUFFER_SIZE = 4096;
    Buffers(std::size_t nconnections)
        : m_buffers(nconnections * BUFFER_SIZE)
        , m_buffer_lengths(nconnections, 0)
    {
    }

    std::size_t length(std::size_t buffer_index) const
    {
        return m_buffer_lengths[buffer_index];
    }

    std::size_t& length(std::size_t buffer_index)
    {
        return m_buffer_lengths[buffer_index];
    }

    std::span<const byte_t> buffer(std::size_t buffer_index) const
    {
        return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
    }

    std::span<byte_t> buffer(std::size_t buffer_index)
    {
        return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
    }

    std::size_t size() const
    {
        return m_buffer_lengths.size();
    }

private:
    std::vector<byte_t> m_buffers;
    std::vector<std::size_t> m_buffer_lengths;
};

// HACK: fit client-related data into one 64-bit word
inline uint64_t make_request_data(int client_fd, EventType event_type)
{
    return (uint64_t)(event_type) << 32 | client_fd;
}

inline int request_data_client_fd(uint64_t request_data)
{
    // UNIT_MAX = 0x00000000FFFFFFFF
    return request_data & std::numeric_limits<unsigned>::max();
}

inline EventType request_data_event_type(uint64_t request_data)
{
    // ULLONG_MAX - UINT_MAX = 0xFFFFFFFF00000000
    return EventType((request_data & (std::numeric_limits<unsigned long long>::max() - std::numeric_limits<unsigned>::max())) >> 32);
}

void add_accept_request(struct io_uring* ring, int server_fd,
                               struct sockaddr_in* client_addr,
                               socklen_t* client_addr_len);
void add_read_request(struct io_uring* ring, int client_fd, Buffers& buffers);
void add_write_request(struct io_uring* ring, int client_fd,
                              size_t nbytes, bool more_data, Buffers& buffers);

void handle_request(struct io_uring* ring, int client_fd, size_t nread, Buffers& buffers);

class Client
{
public:

private:

};

class IoUring
{
public:
    IoUring(const MainSocket& socket, int nconnections)
        : m_socket(socket)
        , m_buffers(nconnections)
    {
        int poll = false; // kernel polling, root required

        if (poll)
        {
            if (geteuid() != 0)
            {
                fputs("You need root privileges to do kernel polling.", stderr);
                exit(EXIT_FAILURE);
            }
        }

        struct io_uring_params params;
        memset(&params, 0, sizeof(io_uring_params));
        params.flags = poll ? IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF : 0;
        params.sq_thread_idle = 2147483647;
        if (io_uring_queue_init_params(nconnections, &m_ring, &params) != 0)
            throw std::runtime_error("io_uring_queue_init_params");
    }

    ~IoUring()
    {
        io_uring_queue_exit(&m_ring);
    }

    void event_loop()
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        this->add_accept_request(&client_addr, &client_addr_len);

        for (;;)
        {
            struct io_uring_cqe* cqe;
            if (UNLIKELY(io_uring_wait_cqe(&m_ring, &cqe) < 0))
                throw std::runtime_error("io_uring_wait_cqe");

            if (UNLIKELY(cqe->res < 0))
            {
                fprintf(stderr, "async request (fd %d, et %d): %s\n",
                        request_data_client_fd(cqe->user_data),
                        request_data_event_type(cqe->user_data),
                        strerror(-cqe->res));
            }
            else
            {
                switch (static_cast<EventType>(request_data_event_type(cqe->user_data)))
                {
                case EventType::ACCEPT:
                    this->add_accept_request(&client_addr, &client_addr_len);
                    if (LIKELY(cqe->res < m_buffers.size()))
                    {
                        m_buffers.length(cqe->res) = 0;
                        this->add_read_request(cqe->res);
                    }
                    else
                    {
                        fprintf(stderr, "server capacity exceeded: %d / %d\n",
                                cqe->res, m_buffers.size());
                        close(cqe->res);
                    }
                    break;

                case EventType::READ:
                    if (LIKELY(cqe->res)) // non-empty request?
                    {
                        this->handle_request(
                                       request_data_client_fd(cqe->user_data),
                                       cqe->res);
                    }
                    break;

                case EventType::WRITE:
                {
                    int client_fd = request_data_client_fd(cqe->user_data);
                    close(client_fd);
                }
                }
            }
            io_uring_cqe_seen(&m_ring, cqe);
        }
    }

private:
    const MainSocket& m_socket;
    Buffers m_buffers;
    struct io_uring m_ring;

    void handle_request(int client_fd, size_t /* nread */)
    {
        char date[32];
        std::time_t t = std::time(nullptr);
        struct std::tm* tm = std::gmtime(&t);
        std::strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tm);

        int n = std::snprintf(m_buffers.buffer(client_fd).data(), Buffers::BUFFER_SIZE, REPLY_200, date);

        this->add_write_request(client_fd, n, true);
    }

    void add_accept_request(struct sockaddr_in* client_addr,
                            socklen_t* client_addr_len)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_accept(sqe, m_socket.fd, (struct sockaddr*)client_addr,
                             client_addr_len, 0);
        io_uring_sqe_set_data(
            sqe, (void*)make_request_data(0, EventType::ACCEPT));
        io_uring_submit(&m_ring);
    }

    void add_read_request(int client_fd)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        size_t current_length = m_buffers.length(client_fd);
        io_uring_prep_recv(sqe, client_fd,
                           m_buffers.buffer(client_fd).data() + current_length,
                           Buffers::BUFFER_SIZE - current_length, 0);
        io_uring_sqe_set_data(
            sqe, (void*)make_request_data(client_fd, EventType::READ));
        io_uring_submit(&m_ring);
    }

    void add_write_request(int client_fd, size_t nbytes, bool more_data)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_send(sqe, client_fd, m_buffers.buffer(client_fd).data(), nbytes,
                           MSG_DONTWAIT | (more_data ? MSG_MORE : 0));
        io_uring_sqe_set_data(
            sqe, (void*)make_request_data(client_fd, EventType::WRITE));
        io_uring_submit(&m_ring);
    }
};

void event_loop(int server_fd, struct io_uring* ring, int nconnections, Buffers& buffers);

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
