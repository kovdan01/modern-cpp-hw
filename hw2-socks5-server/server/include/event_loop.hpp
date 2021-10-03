#ifndef HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
#define HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_

#include <socket.hpp>
#include <syscall.hpp>

#include <utils.hpp>

#include <liburing.h>

#include <algorithm>
#include <cassert>
#include <ctime>
#include <functional>
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

using byte_t = char;

enum class EventType
{
    ACCEPT = 0,
    READ,
    WRITE,
};

class Buffers
{
public:
    static constexpr std::size_t BUFFER_SIZE = 4096;
    Buffers(std::size_t nconnections)
        : total_buffer_count(nconnections)
        , m_buffers(nconnections * BUFFER_SIZE)
    {
        for (std::size_t i = 0; i < nconnections; ++i)
        {
            m_free_buffers.push(i);
        }
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

    const std::size_t total_buffer_count;

private:
    std::vector<byte_t> m_buffers;
    std::queue<std::size_t> m_free_buffers;
};

class IoUring;

class Client
{
public:
    using Ptr = std::unique_ptr<Client>;
    using iterator_t = std::unordered_set<Ptr>::iterator;

    Client(int fd, std::size_t buffer_index, Buffers& buffers, IoUring& server)
        : buffer_index(buffer_index)
        , fd(fd)
        , m_buffers(buffers)
        , m_server(server)
        , m_buffer(m_buffers.buffer(buffer_index))
    {
        m_read_check = [](){ return true; };
    }

    void write();
    void read_some(std::size_t n);

    ~Client()
    {
        syscall_wrapper::close(fd);
        m_buffers.return_buffer(buffer_index);
    }

    EventType pending_event_type() const { return m_pending_event_type; }
    EventType& pending_event_type() { return m_pending_event_type; }

    iterator_t iterator() const { return m_iterator; }
    iterator_t& iterator() { return m_iterator; }

    std::span<byte_t> buffer() { return m_buffer; }
    std::span<const byte_t> buffer() const { return m_buffer; }

    void handle_read(std::size_t nread);
    void handle_write(std::size_t nwrite);

    const std::size_t buffer_index;
    const int fd;

private:
    EventType m_pending_event_type;
    iterator_t m_iterator;
    Buffers& m_buffers;
    IoUring& m_server;
    std::span<byte_t> m_buffer;

    enum class State
    {
        READING_CLIENT_GREETING,
        READING_AUTH_METHODS,
        READING_CLIENT_CONNECTION_REQUEST,
        READING_ADDRESS,
        READING_USER_REQUESTS,
    };
    State m_state = State::READING_CLIENT_GREETING;

    std::function<bool()> m_read_check;
    // TODO: change to deque
    std::vector<byte_t> m_read_buffer;
    // TODO: change to deque
    std::vector<byte_t> m_write_buffer;

    std::size_t m_nauth;
    std::size_t m_auth_method;
    int m_cmd;
    int m_addr_type;
    in_addr_t m_ipv4_addr;
    in_port_t m_port;

    std::unique_ptr<ExternalConnection> m_connection;

    void consume_bytes_from_read_buffer(std::size_t nread)
    {
        m_read_buffer.erase(m_read_buffer.begin(), m_read_buffer.begin() + nread);
    }
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
                        int fd = cqe->res;
                        auto [it, was_emplaced] = m_clients.emplace(std::make_unique<Client>(fd, buffer_index, m_buffers, *this));
                        assert(was_emplaced);
                        const Client::Ptr& client = *it;
                        client->iterator() = it;
                        client->read_some(3);
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
                            client->handle_read(cqe->res);
                        }
                        break;

                    case EventType::WRITE:
                        client->handle_write(cqe->res);
                        break;
                    }
                }
            }
            io_uring_cqe_seen(&m_ring, cqe);
        }
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
        //std::size_t current_length = m_buffers.length(client->buffer_index);
        io_uring_prep_recv(sqe, client->fd, client->buffer().data(),
                           Buffers::BUFFER_SIZE, 0);
        client->pending_event_type() = EventType::READ;
        io_uring_sqe_set_data(sqe, client);
        io_uring_submit(&m_ring);
    }

    void add_write_request(Client* client, std::size_t nbytes, bool more_data)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_send(sqe, client->fd, client->buffer().data(), nbytes,
                           MSG_DONTWAIT | (more_data ? MSG_MORE : 0));
        client->pending_event_type() = EventType::WRITE;
        io_uring_sqe_set_data(sqe, client);
        io_uring_submit(&m_ring);
    }

private:
    const MainSocket& m_socket;
    Buffers m_buffers;
    struct io_uring m_ring;
    std::unordered_set<Client::Ptr> m_clients;
};

inline void Client::read_some(std::size_t n)
{
    m_read_check = [n, this](){ return m_read_buffer.size() >= n; };
    if (m_read_check())
    {
        handle_read(0);
    }
    else
    {
        m_server.add_read_request(this);
    }
}

inline void Client::write()
{
    std::size_t cnt = std::min(Buffers::BUFFER_SIZE, m_write_buffer.size());
    std::memcpy(m_buffer.data(), m_write_buffer.data(), cnt);
    m_server.add_write_request(this, cnt, false);
}

inline void Client::handle_read(std::size_t nread)
{
    if (nread != 0)
    {
        m_read_buffer.insert(m_read_buffer.end(), m_buffer.data(), m_buffer.data() + nread);
    }
    if (!m_read_check())
    {
        m_server.add_read_request(this);
        return;
    }

    switch (m_state)
    {
    case State::READING_CLIENT_GREETING:
        std::cerr << "Reading client greeting" << std::endl;
        // TODO: handle properly (version, must be 0x05)
        assert(m_read_buffer[0] == 0x05);
        m_nauth = m_read_buffer[1];
        this->consume_bytes_from_read_buffer(2);
        m_state = State::READING_AUTH_METHODS;
        this->read_some(m_nauth);
        break;
    case State::READING_AUTH_METHODS:
    {
        std::cerr << "Reading auth methods" << std::endl;
        char* from = m_read_buffer.data();
        char* to = m_read_buffer.data() + m_nauth;
        // TODO: handle properly (0x00 is no auth)
        assert(std::find(from, to, 0x00) != to);
        m_auth_method = 0x00;
        this->consume_bytes_from_read_buffer(m_nauth);
        m_write_buffer.resize(2);
        m_write_buffer[0] = 0x05;
        m_write_buffer[1] = 0x00;
        this->write();
        break;
    }
    case State::READING_CLIENT_CONNECTION_REQUEST:
        std::cerr << "Reading client connection request" << std::endl;
        // TODO: handle properly (version, must be 0x05)
        assert(m_read_buffer[0] == 0x05);
        m_cmd = m_read_buffer[1];
        // TODO: handle properly (reserved, must be 0x00);
        assert(m_read_buffer[2] == 0x00);
        m_addr_type = m_read_buffer[3];
        this->consume_bytes_from_read_buffer(4);
        m_state = State::READING_ADDRESS;
        switch (m_addr_type)
        {
        case 0x01:  // IPv4
            this->read_some(6);
            break;
        case 0x03:  // Domain name
            // TODO: handle properly
            assert(false);
            break;
        case 0x04:  // IPv6
            // TODO: handle properly
            assert(false);
            break;
        }
        break;
    case State::READING_ADDRESS:
        std::cerr << "Reading address" << std::endl;
        std::memcpy(&m_ipv4_addr, m_read_buffer.data(), 4);
        std::memcpy(&m_port, m_read_buffer.data() + 4, 2);
        std::cerr << "ADDR: " << m_ipv4_addr << ", PORT: " << m_port << std::endl;
        this->consume_bytes_from_read_buffer(6);
        //m_connection = std::make_unique<ExternalConnection>(m_ipv4_addr, m_port);
        m_write_buffer.resize(10);
        m_write_buffer[0] = 0x05;  // protocol version
        m_write_buffer[1] = 0x00;  // request granted
        m_write_buffer[2] = 0x00;  // reserved
        m_write_buffer[3] = 0x01;  // IPv4
        std::memcpy(m_write_buffer.data() + 4, &m_ipv4_addr, 4);
        std::memcpy(m_write_buffer.data() + 8, &m_port, 2);
        this->write();
        break;
    case State::READING_USER_REQUESTS:
    {
        std::cerr << "Reading user requests" << std::endl;
        char date[32];
        std::time_t t = std::time(nullptr);
        struct std::tm* tm = std::gmtime(&t);
        std::strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tm);

        int n = std::snprintf(m_buffer.data(), Buffers::BUFFER_SIZE, REPLY_200, date);

        m_write_buffer.resize(n);
        std::memcpy(m_write_buffer.data(), m_buffer.data(), n);
        this->write();
        break;
    }
    }
}

inline void Client::handle_write(std::size_t nwrite)
{
    std::cerr << "nwrite: " << nwrite << std::endl;
    m_write_buffer.erase(m_write_buffer.begin(), m_write_buffer.begin() + nwrite);
    if (!m_write_buffer.empty())
    {
        this->write();
        return;
    }

    switch (m_state)
    {
    case State::READING_AUTH_METHODS:
        std::cerr << "After reply auth methods" << std::endl;
        m_state = State::READING_CLIENT_CONNECTION_REQUEST;
        this->read_some(4);
        break;
    case State::READING_ADDRESS:
        std::cerr << "After reply address accepted" << std::endl;
        m_state = State::READING_USER_REQUESTS;
        this->read_some(1);
        break;
    case State::READING_CLIENT_GREETING:
        assert(false);
        break;
    case State::READING_CLIENT_CONNECTION_REQUEST:
        assert(false);
        break;
    case State::READING_USER_REQUESTS:
        std::cerr << "After reply user requests" << std::endl;
        break;
    }
}

//inline void Client::handle_read(std::size_t nread)
//{
//    char date[32];
//    std::time_t t = std::time(nullptr);
//    struct std::tm* tm = std::gmtime(&t);
//    std::strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tm);

//    int n = std::snprintf(m_buffers.buffer(buffer_index).data(), Buffers::BUFFER_SIZE, REPLY_200, date);

//    m_server.add_write_request(this, n, true);
//}

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
