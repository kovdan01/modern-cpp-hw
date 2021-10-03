#ifndef HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
#define HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_

#include <socket.hpp>
#include <syscall.hpp>

#include <utils.hpp>

#include <liburing.h>

#include <cassert>
#include <ctime>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <span>
#include <vector>
#include <unordered_set>

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
        for (std::size_t i = 0; i < nconnections; ++i)
        {
            m_free_buffers.push(i);
        }
    }

    std::size_t length(std::size_t buffer_index) const
    {
        return m_buffer_lengths[buffer_index];
    }

    std::size_t& length(std::size_t buffer_index)
    {
        return m_buffer_lengths[buffer_index];
    }

    std::size_t obtain_free_buffer()
    {
        if (UNLIKELY(m_free_buffers.empty()))
        {
            return std::size_t(-1);
        }
        std::size_t free_buffer_index = m_free_buffers.front();
        m_free_buffers.pop();
        return free_buffer_index;
    }

    void return_buffer(std::size_t buffer_index)
    {
        m_free_buffers.push(buffer_index);
    }

    std::span<const byte_t> buffer(std::size_t buffer_index) const
    {
        return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
    }

    std::span<byte_t> buffer(std::size_t buffer_index)
    {
        return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
    }

    std::size_t total_buffer_count() const
    {
        return m_buffer_lengths.size();
    }

private:
    std::vector<byte_t> m_buffers;
    std::vector<std::size_t> m_buffer_lengths;
    std::queue<std::size_t> m_free_buffers;
};

class Client
{
public:
    using Ptr = std::unique_ptr<Client>;
    using iterator_t = std::unordered_set<Ptr>::iterator;

    Client(int fd, std::size_t buffer_index, Buffers& buffers)
        : buffer_index(buffer_index)
        , fd(fd)
        , m_buffers(buffers)
    {
    }

    ~Client()
    {
        syscall_wrapper::close(fd);
        m_buffers.return_buffer(buffer_index);
    }

    EventType pending_event_type() const
    {
        return m_pending_event_type;
    }

    EventType& pending_event_type()
    {
        return m_pending_event_type;
    }

    iterator_t iterator() const
    {
        return m_iterator;
    }

    iterator_t& iterator()
    {
        return m_iterator;
    }

    const std::size_t buffer_index;
    const int fd;

private:
    EventType m_pending_event_type;
    iterator_t m_iterator;
    Buffers& m_buffers;
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
            if (::geteuid() != 0)
            {
                throw std::runtime_error("You need root privileges to do kernel polling.");
            }
        }

        struct io_uring_params params;
        std::memset(&params, 0, sizeof(io_uring_params));
        params.flags = poll ? IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF : 0;
        params.sq_thread_idle = 2'147'483'647;
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

        // TODO: handle client shutdown

        for (;;)
        {
            struct io_uring_cqe* cqe;
            if (UNLIKELY(io_uring_wait_cqe(&m_ring, &cqe) < 0))
                throw std::runtime_error("io_uring_wait_cqe");

            if (UNLIKELY(cqe->res < 0))
            {
                Client* client = reinterpret_cast<Client*>(cqe->user_data);
                std::cerr << std::strerror(-cqe->res);
            }
            else
            {
                if (cqe->user_data == 0) // accept
                {
                    this->add_accept_request(&client_addr, &client_addr_len);
                    std::size_t buffer_index = m_buffers.obtain_free_buffer();
                    if (LIKELY(buffer_index != std::size_t(-1)))
                    {
                        m_buffers.length(buffer_index) = 0;
                        int fd = cqe->res;
                        auto [it, was_emplaced] = m_clients.emplace(std::make_unique<Client>(fd, buffer_index, m_buffers));
                        assert(was_emplaced);
                        const Client::Ptr& client = *it;
                        client->iterator() = it;
                        this->add_read_request(client.get());
                    }
                    else
                    {
                        std::cerr << "Server capacity exceeded\n";
                        syscall_wrapper::close(cqe->res);
                    }
                }
                else
                {
                    Client* client = reinterpret_cast<Client*>(cqe->user_data);
                    switch (client->pending_event_type())
                    {
                    case EventType::ACCEPT:
                        assert(false);
                        break;
                    case EventType::READ:
                        if (LIKELY(cqe->res))  // non-empty request?
                        {
                            this->handle_request(client, cqe->res);
                        }
                        break;

                    case EventType::WRITE:
                        m_clients.erase(client->iterator());
                        break;
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
    std::unordered_set<Client::Ptr> m_clients;

    void handle_request(Client* client, std::size_t /* nread */)
    {
        char date[32];
        std::time_t t = std::time(nullptr);
        struct std::tm* tm = std::gmtime(&t);
        std::strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tm);

        int n = std::snprintf(m_buffers.buffer(client->buffer_index).data(), Buffers::BUFFER_SIZE, REPLY_200, date);

        this->add_write_request(client, n, true);
    }

    void add_accept_request(struct sockaddr_in* client_addr, socklen_t* client_addr_len)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_accept(sqe, m_socket.fd, reinterpret_cast<struct sockaddr*>(client_addr), client_addr_len, 0);
        io_uring_sqe_set_data(sqe, nullptr);
        io_uring_submit(&m_ring);
    }

    void add_read_request(Client* client)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        std::size_t current_length = m_buffers.length(client->buffer_index);
        io_uring_prep_recv(sqe, client->fd, m_buffers.buffer(client->buffer_index).data() + current_length,
                           Buffers::BUFFER_SIZE - current_length, 0);
        client->pending_event_type() = EventType::READ;
        io_uring_sqe_set_data(sqe, client);
        io_uring_submit(&m_ring);
    }

    void add_write_request(Client* client, std::size_t nbytes, bool more_data)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_send(sqe, client->fd, m_buffers.buffer(client->buffer_index).data(), nbytes,
                           MSG_DONTWAIT | (more_data ? MSG_MORE : 0));
        client->pending_event_type() = EventType::WRITE;
        io_uring_sqe_set_data(sqe, client);
        io_uring_submit(&m_ring);
    }
};

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
