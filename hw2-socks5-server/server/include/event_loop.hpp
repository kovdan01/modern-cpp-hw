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

#define REPLY_200 "HTTP/1.0 200 OK\r\nServer: otus-io-uring\r\nDate: %s\r\n\
Content-Type: text/plain\r\nContent-Length: 12\r\n\r\nHello world!\r\n"

namespace hw2
{

using byte_t = char;

enum class EventType
{
    CLIENT_ACCEPT = 0,
    CLIENT_READ,
    CLIENT_WRITE,
    DST_CONNECT,
    DST_READ,
    DST_WRITE,
};

class Client;

struct Event
{
    Client* client;
    EventType type;
};

class Buffers
{
public:
    static constexpr std::size_t HALF_BUFFER_SIZE = 4096;
    // 0: client greeting, receive from client, send to destination
    // 1: receive from destination, send to client
    static constexpr std::size_t BUFFER_SIZE = HALF_BUFFER_SIZE * 2;
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

    std::span<const byte_t> buffer_half0(std::size_t buffer_index) const
    {
        return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE };
    }

    std::span<byte_t> buffer_half0(std::size_t buffer_index)
    {
        return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE };
    }

    std::span<const byte_t> buffer_half1(std::size_t buffer_index) const
    {
        return { m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
    }

    std::span<byte_t> buffer_half1(std::size_t buffer_index)
    {
        return { m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
    }

//    std::span<const byte_t> buffer(std::size_t buffer_index) const
//    {
//        return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
//    }

//    std::span<byte_t> buffer(std::size_t buffer_index)
//    {
//        return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
//    }

    const std::size_t total_buffer_count;

private:
    std::vector<byte_t> m_buffers;
    std::queue<std::size_t> m_free_buffers;
};

class IoUring;

class Client
{
public:
    Client(int fd, std::size_t buffer_index, Buffers& buffers, IoUring& server)
        : buffer_index(buffer_index)
        , fd(fd)
        , m_buffers(buffers)
        , m_server(server)
        , m_buffer0(m_buffers.buffer_half0(buffer_index))
        , m_buffer1(m_buffers.buffer_half1(buffer_index))
    {
        m_read_check = [](){ return true; };
    }

    void write_client();
    //void write_destination(std::size_t n);
    void read_client();
    void read_client_some(std::size_t n);

    ~Client()
    {
        syscall_wrapper::close(fd);
        m_buffers.return_buffer(buffer_index);
    }

    std::span<byte_t> buffer0() { return m_buffer0; }
    std::span<const byte_t> buffer0() const { return m_buffer0; }

    std::span<byte_t> buffer1() { return m_buffer1; }
    std::span<const byte_t> buffer1() const { return m_buffer1; }

    void handle_client_read(std::size_t nread);
    void handle_client_write(std::size_t nwrite);
    void handle_dst_read(std::size_t nread);
    void handle_dst_write(std::size_t nwrite);

    Socket* destination_socket() { return m_destination_socket.get(); }

    const std::size_t buffer_index;
    const int fd;

    bool fail = false;

private:
    Buffers& m_buffers;
    IoUring& m_server;
    std::span<byte_t> m_buffer0;
    std::span<byte_t> m_buffer1;

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
    std::vector<byte_t> m_write_client_buffer;
    // TODO: change to deque
    //std::vector<byte_t> m_write_destination_buffer;

    std::size_t m_nauth;
    std::size_t m_auth_method;
    int m_cmd;
    int m_addr_type;
    in_addr_t m_ipv4_addr;
    in6_addr m_ipv6_addr;
    in_port_t m_port;

    std::unique_ptr<Socket> m_destination_socket;

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

        this->add_client_accept_request(&client_addr, &client_addr_len);

        // TODO: handle client shutdown

        for (;;)
        {
            struct io_uring_cqe* cqe;
            if (UNLIKELY(io_uring_wait_cqe(&m_ring, &cqe) < 0))
                throw std::runtime_error("io_uring_wait_cqe");

            if (UNLIKELY(cqe->res < 0))
            {
                std::cerr << "AAA: " << std::strerror(-cqe->res) << std::endl;
                if (cqe->user_data != 0)
                {
                    Event* event = reinterpret_cast<Event*>(cqe->user_data);
                    event->client->fail = true;
                    //delete event->client;
                    delete event;
                }
            }
            else
            {
                if (cqe->user_data == 0) // accept
                {
                    this->add_client_accept_request(&client_addr, &client_addr_len);
                    std::size_t buffer_index = m_buffers.obtain_free_buffer();
                    if (LIKELY(buffer_index != std::size_t(-1)))
                    {
                        int fd = cqe->res;
                        Client* client = new Client{fd, buffer_index, m_buffers, *this};
                        client->read_client_some(3);
                    }
                    else
                    {
                        std::cerr << "Server capacity exceeded\n";
                        syscall_wrapper::close(cqe->res);
                    }
                }
                else
                {
                    Event* event = reinterpret_cast<Event*>(cqe->user_data);
                    if (event->client->fail)
                    {
                        delete event->client;
                    }
                    else
                    {
                        switch (event->type)
                        {
                        case EventType::CLIENT_ACCEPT:
                            assert(false);
                            break;
                        case EventType::CLIENT_READ:
                            if (LIKELY(cqe->res))  // non-empty request?
                            {
                                event->client->handle_client_read(cqe->res);
                            }
                            else
                            {
                                // assume that empty read indicates client disconnected
                                std::cerr << "Empty read!" << std::endl;
                                //delete event->client;
                            }
                            break;

                        case EventType::CLIENT_WRITE:
                            event->client->handle_client_write(cqe->res);
                            break;
                        case EventType::DST_CONNECT:
                            assert(false);
                            break;
                        case EventType::DST_READ:
                            event->client->handle_dst_read(cqe->res);
                            break;
                        case EventType::DST_WRITE:
                            event->client->handle_dst_write(cqe->res);
                            break;
                        }
                    }
                    delete event;
                }
            }
            io_uring_cqe_seen(&m_ring, cqe);
        }
    }

    void add_client_accept_request(struct sockaddr_in* client_addr, socklen_t* client_addr_len)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_accept(sqe, m_socket.fd, reinterpret_cast<struct sockaddr*>(client_addr), client_addr_len, 0);
        io_uring_sqe_set_data(sqe, nullptr);
        io_uring_submit(&m_ring);
    }

    void add_client_read_request(Client* client)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_recv(sqe, client->fd, client->buffer0().data(),
                           Buffers::BUFFER_SIZE, 0);
        Event* event = new Event{client, EventType::CLIENT_READ};
        io_uring_sqe_set_data(sqe, event);
        io_uring_submit(&m_ring);
    }

    void add_client_write_request(Client* client, std::size_t nbytes, bool more_data)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_send(sqe, client->fd, client->buffer1().data(), nbytes,
                           MSG_WAITALL /* MSG_DONTWAIT | (more_data ? MSG_MORE : 0)*/);
        Event* event = new Event{client, EventType::CLIENT_WRITE};
        io_uring_sqe_set_data(sqe, event);
        io_uring_submit(&m_ring);
    }

//    void add_dst_connect_request(Client* client)
//    {
//        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
//        io_uring_prep_connect(sqe, client->destination_socket()->fd,
//                              reinterpret_cast<sockaddr*>(&client->destination_socket()->address()),
//                              sizeof(client->destination_socket()->address()));
//        Event* event = new Event{client, EventType::DST_CONNECT};
//        io_uring_sqe_set_data(sqe, event);
//        io_uring_submit(&m_ring);
//    }

    void add_dst_read_request(Client* client)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_recv(sqe, client->destination_socket()->fd, client->buffer1().data(),
                           Buffers::BUFFER_SIZE, 0);
        Event* event = new Event{client, EventType::DST_READ};
        io_uring_sqe_set_data(sqe, event);
        io_uring_submit(&m_ring);
    }

    void add_dst_write_request(Client* client, std::size_t nbytes, bool more_data)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_send(sqe, client->destination_socket()->fd, client->buffer0().data(), nbytes,
                            MSG_WAITALL /*MSG_DONTWAIT | (more_data ? MSG_MORE : 0)*/);
        Event* event = new Event{client, EventType::DST_WRITE};
        io_uring_sqe_set_data(sqe, event);
        io_uring_submit(&m_ring);
    }

private:
    const MainSocket& m_socket;
    Buffers m_buffers;
    struct io_uring m_ring;
};

inline void Client::read_client()
{
    m_read_check = [this](){ return !m_read_buffer.empty(); };
    if (m_read_check())
    {
        handle_client_read(0);
    }
    else
    {
        m_server.add_client_read_request(this);
    }
}

inline void Client::read_client_some(std::size_t n)
{
    m_read_check = [n, this](){ return m_read_buffer.size() >= n; };
    if (m_read_check())
    {
        handle_client_read(0);
    }
    else
    {
        m_server.add_client_read_request(this);
    }
}

inline void Client::write_client()
{
    std::size_t cnt = std::min(Buffers::BUFFER_SIZE, m_write_client_buffer.size());
    std::memcpy(m_buffer1.data(), m_write_client_buffer.data(), cnt);
    m_server.add_client_write_request(this, cnt, false);
}

//inline void Client::write_destination(std::size_t n)
//{
//    m_write_destination_buffer.resize(n);
//    std::memcpy(m_write_destination_buffer.data(), buffer0().data(), n);
//    m_server.add_dst_write_request(this, n, false);
//}

inline void Client::handle_client_read(std::size_t nread)
{
    if (m_state == State::READING_USER_REQUESTS)
    {
        m_server.add_dst_write_request(this, nread, false);
        return;
    }

    if (nread != 0)
    {
        m_read_buffer.insert(m_read_buffer.end(), m_buffer0.data(), m_buffer0.data() + nread);
    }
    if (!m_read_check())
    {
        m_server.add_client_read_request(this);
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
        this->read_client_some(m_nauth);
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
        m_write_client_buffer.resize(2);
        m_write_client_buffer[0] = 0x05;
        m_write_client_buffer[1] = 0x00;
        this->write_client();
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
            this->read_client_some(6);
            break;
        case 0x03:  // Domain name
            // TODO: handle properly
            assert(false);
            break;
        case 0x04:  // IPv6
            // TODO: handle properly
            this->read_client_some(18);
            break;
        default:
            // TODO: handle properly
            assert(false);
            break;
        }
        break;
    case State::READING_ADDRESS:
        std::cerr << "Reading address" << std::endl;
        switch (m_addr_type)
        {
        case 0x01:  // IPv4
        {
            std::memcpy(&m_ipv4_addr, m_read_buffer.data(), 4);
            std::memcpy(&m_port, m_read_buffer.data() + 4, 2);
            std::cerr << "ADDR: " << m_ipv4_addr << ", PORT: " << m_port << std::endl;
            this->consume_bytes_from_read_buffer(6);
            m_destination_socket = std::make_unique<ExternalConnectionIPv4>(m_ipv4_addr, m_port);
            m_write_client_buffer.resize(10);
            m_write_client_buffer[0] = 0x05;  // protocol version
            m_write_client_buffer[1] = 0x00;  // request granted
            m_write_client_buffer[2] = 0x00;  // reserved
            m_write_client_buffer[3] = 0x01;  // IPv4
            std::memcpy(m_write_client_buffer.data() + 4, &m_ipv4_addr, 4);
            std::memcpy(m_write_client_buffer.data() + 8, &m_port, 2);
            this->write_client();
            break;
        }
        case 0x03:  // Domain name
            // TODO: handle properly
            assert(false);
            break;
        case 0x04:  // IPv6
        {
            std::memcpy(&m_ipv6_addr, m_read_buffer.data(), 16);
            std::memcpy(&m_port, m_read_buffer.data() + 16, 2);
            std::cerr << "ADDR: IPv6, PORT: " << m_port << std::endl;
            this->consume_bytes_from_read_buffer(18);
            try
            {
                m_destination_socket = std::make_unique<ExternalConnectionIPv6>(m_ipv6_addr, m_port);
                m_write_client_buffer.resize(22);
                m_write_client_buffer[0] = 0x05;  // protocol version
                m_write_client_buffer[1] = 0x00;  // request granted
                m_write_client_buffer[2] = 0x00;  // reserved
                m_write_client_buffer[3] = 0x04;  // IPv6
                std::memcpy(m_write_client_buffer.data() + 4, &m_ipv6_addr, 16);
                std::memcpy(m_write_client_buffer.data() + 20, &m_port, 2);
            }
            catch (...)
            {
                m_write_client_buffer.resize(22);
                m_write_client_buffer[0] = 0x05;  // protocol version
                m_write_client_buffer[1] = 0x01;  // general failure
                m_write_client_buffer[2] = 0x00;  // reserved
                m_write_client_buffer[3] = 0x04;  // IPv6
                std::memcpy(m_write_client_buffer.data() + 4, &m_ipv6_addr, 16);
                std::memcpy(m_write_client_buffer.data() + 20, &m_port, 2);
                fail = true;
            }

            this->write_client();
            break;
        }
        default:
            // TODO: handle properly
            assert(false);
            break;
        }


        break;
    case State::READING_USER_REQUESTS:
        assert(false);
        break;
//    {

//        std::cerr << "Reading user requests" << std::endl;
//        char date[32];
//        std::time_t t = std::time(nullptr);
//        struct std::tm* tm = std::gmtime(&t);
//        std::strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tm);

//        int n = std::snprintf(m_buffer0.data(), Buffers::BUFFER_SIZE, REPLY_200, date);

//        m_write_buffer.resize(n);
//        std::memcpy(m_write_buffer.data(), m_buffer0.data(), n);
//        this->write_client();
//        break;
//    }
    }
}

inline void Client::handle_client_write(std::size_t nwrite)
{
    if (m_state == State::READING_USER_REQUESTS)
    {
        m_server.add_dst_read_request(this);
        return;
    }

    std::cerr << "client nwrite: " << nwrite << std::endl;
    m_write_client_buffer.erase(m_write_client_buffer.begin(), m_write_client_buffer.begin() + nwrite);
    if (!m_write_client_buffer.empty())
    {
        this->write_client();
        return;
    }

    switch (m_state)
    {
    case State::READING_AUTH_METHODS:
        std::cerr << "After reply auth methods" << std::endl;
        m_state = State::READING_CLIENT_CONNECTION_REQUEST;
        this->read_client_some(4);
        break;
    case State::READING_ADDRESS:
        std::cerr << "After reply address accepted" << std::endl;
        m_state = State::READING_USER_REQUESTS;
        m_server.add_dst_read_request(this);
        this->read_client();
        break;
    case State::READING_CLIENT_GREETING:
        assert(false);
        break;
    case State::READING_CLIENT_CONNECTION_REQUEST:
        assert(false);
        break;
    case State::READING_USER_REQUESTS:
        assert(false);
        break;
//        std::cerr << "After reply user requests" << std::endl;
//        this->read_client();
//        break;
    }
}

inline void Client::handle_dst_read(std::size_t nread)
{

    m_server.add_client_write_request(this, nread, false);
}

inline void Client::handle_dst_write(std::size_t /*nwrite*/)
{
    m_server.add_client_read_request(this);
}

}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
