#include <event_loop.hpp>
#include <socket.hpp>
#include <syscall.hpp>
#include <utils.hpp>

#include <liburing.h>
#include <netdb.h>

#include <algorithm>
#include <cassert>
#include <ctime>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <span>
#include <vector>

namespace hw2
{

BufferPool::InsufficientBuffersException::~InsufficientBuffersException() = default;

BufferPool::BufferPool(std::size_t nconnections)
    : total_buffer_count(nconnections)
    , m_buffers(nconnections * BUFFER_SIZE)
{
    for (std::size_t i = 0; i < nconnections; ++i)
    {
        m_free_buffers.push(i);
    }
}

std::size_t BufferPool::obtain_free_buffer()
{
    if (UNLIKELY(m_free_buffers.empty()))
    {
        throw InsufficientBuffersException("");
    }
    std::size_t free_buffer_index = m_free_buffers.front();
    m_free_buffers.pop();
    return free_buffer_index;
}

void BufferPool::return_buffer(std::size_t buffer_index)
{
    m_free_buffers.push(buffer_index);
}

std::span<const byte_t> BufferPool::buffer_half0(std::size_t buffer_index) const
{
    return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE };
}

std::span<byte_t> BufferPool::buffer_half0(std::size_t buffer_index)
{
    return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE };
}

std::span<const byte_t> BufferPool::buffer_half1(std::size_t buffer_index) const
{
    return { m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
}

std::span<byte_t> BufferPool::buffer_half1(std::size_t buffer_index)
{
    return { m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
}


Client::Client(int fd, IoUring& server, BufferPool& buffer_pool)
    : fd(fd)
    , m_server(server)
    , m_buffer_pool(buffer_pool)
    , buffer_index(m_buffer_pool.obtain_free_buffer())
    , m_buffer0(m_buffer_pool.buffer_half0(buffer_index))
    , m_buffer1(m_buffer_pool.buffer_half1(buffer_index))
{
    m_is_read_completed = [](){ return true; };
}


IoUring::IoUring(const MainSocket& socket, int nconnections)
    : m_socket(socket)
    , m_buffer_pool(nconnections)
{
    int poll = false;  // kernel polling, root required

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

IoUring::~IoUring()
{
    io_uring_queue_exit(&m_ring);
}

void IoUring::handle_accept(const io_uring_cqe* cqe)
{
    this->add_client_accept_request(&m_client_addr, &m_client_addr_len);
    int fd = cqe->res;
    Client* client;
    try
    {
        client = new Client{fd, *this, m_buffer_pool};
    }
    catch (const BufferPool::InsufficientBuffersException&)
    {
        std::cerr << "Server capacity exceeded\n";
        syscall_wrapper::close(fd);
        return;
    }
    client->awaiting_events_count = 1;
    client->read_some_from_client(3);
}

void IoUring::event_loop()
{
    this->add_client_accept_request(&m_client_addr, &m_client_addr_len);

    // TODO: handle client shutdown

    for (;;)
    {
        io_uring_cqe* cqe;
        if (UNLIKELY(io_uring_wait_cqe(&m_ring, &cqe) < 0))
            throw std::runtime_error("io_uring_wait_cqe");

        if (UNLIKELY(cqe->res < 0))
        {
            std::cerr << "cqe fail: " << std::strerror(-cqe->res) << std::endl;
            if (cqe->user_data != 0)
            {
                Event* event = reinterpret_cast<Event*>(cqe->user_data);
                event->client->fail = true;
                delete event;
            }
        }
        else
        {
            if (cqe->user_data == 0) // accept
            {
                this->handle_accept(cqe);
            }
            else
            {
                Event* event = reinterpret_cast<Event*>(cqe->user_data);
                --event->client->awaiting_events_count;
                if (event->client->fail)
                {
                    if (event->client->awaiting_events_count == 0)
                    {
                        delete event->client;
                    }
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
                        else  // assume that empty read indicates that client disconnected
                        {
                            event->client->fail = true;
                        }
                        break;
                    case EventType::CLIENT_WRITE:
                        event->client->handle_client_write(cqe->res);
                        break;
                    case EventType::DESTINATION_CONNECT:
                        assert(false);
                        break;
                    case EventType::DESTINATION_READ:
                        event->client->handle_dst_read(cqe->res);
                        break;
                    case EventType::DESTINATION_WRITE:
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

void IoUring::add_client_accept_request(struct sockaddr_in* client_addr, socklen_t* client_addr_len)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    io_uring_prep_accept(sqe, m_socket.fd, reinterpret_cast<struct sockaddr*>(client_addr), client_addr_len, 0);
    io_uring_sqe_set_data(sqe, nullptr);
    io_uring_submit(&m_ring);
}

void IoUring::add_client_read_request(Client* client)
{
    ++client->awaiting_events_count;
    struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    io_uring_prep_recv(sqe, client->fd, client->buffer0().data(),
                       BufferPool::BUFFER_SIZE, 0);
    Event* event = new Event{client, EventType::CLIENT_READ};
    io_uring_sqe_set_data(sqe, event);
    io_uring_submit(&m_ring);
}

void IoUring::add_client_write_request(Client* client, std::size_t nbytes)
{
    ++client->awaiting_events_count;
    struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    io_uring_prep_send(sqe, client->fd, client->buffer1().data(), nbytes, MSG_WAITALL);
    Event* event = new Event{client, EventType::CLIENT_WRITE};
    io_uring_sqe_set_data(sqe, event);
    io_uring_submit(&m_ring);
}

//void IoUring::add_dst_connect_request(Client* client)
//{
//    struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
//    io_uring_prep_connect(sqe, client->destination_socket()->fd,
//                          reinterpret_cast<sockaddr*>(&client->destination_socket()->address()),
//                          sizeof(client->destination_socket()->address()));
//    Event* event = new Event{client, EventType::DST_CONNECT};
//    io_uring_sqe_set_data(sqe, event);
//    io_uring_submit(&m_ring);
//}

void IoUring::add_dst_read_request(Client* client)
{
    ++client->awaiting_events_count;
    struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    io_uring_prep_recv(sqe, client->destination_socket()->fd, client->buffer1().data(),
                       BufferPool::BUFFER_SIZE, 0);
    Event* event = new Event{client, EventType::DESTINATION_READ};
    io_uring_sqe_set_data(sqe, event);
    io_uring_submit(&m_ring);
}

void IoUring::add_dst_write_request(Client* client, std::size_t nbytes)
{
    ++client->awaiting_events_count;
    struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    io_uring_prep_send(sqe, client->destination_socket()->fd, client->buffer0().data(), nbytes, MSG_WAITALL);
    Event* event = new Event{client, EventType::DESTINATION_WRITE};
    io_uring_sqe_set_data(sqe, event);
    io_uring_submit(&m_ring);
}

void Client::read_from_client()
{
    m_is_read_completed = [this](){ return !m_read_buffer.empty(); };
    if (m_is_read_completed())
    {
        handle_client_read(0);
    }
    else
    {
        m_server.add_client_read_request(this);
    }
}

void Client::read_some_from_client(std::size_t n)
{
    m_is_read_completed = [n, this](){ return m_read_buffer.size() >= n; };
    if (m_is_read_completed())
    {
        handle_client_read(0);
    }
    else
    {
        m_server.add_client_read_request(this);
    }
}

void Client::write_to_client()
{
    std::size_t cnt = std::min(BufferPool::BUFFER_SIZE, m_write_client_buffer.size());
    std::memcpy(m_buffer1.data(), m_write_client_buffer.data(), cnt);
    m_server.add_client_write_request(this, cnt);
}

void Client::read_client_greeting()
{
    std::cerr << "Reading client greeting" << std::endl;
    byte_t version = m_read_buffer[0];
    if (version != 0x05)
    {
        this->send_fail_message();
        return;
    }
    m_auth_methods_count = m_read_buffer[1];
    this->consume_bytes_from_read_buffer(2);
    m_state = State::READING_AUTH_METHODS;
    this->read_some_from_client(m_auth_methods_count);
}

void Client::read_auth_methods()
{
    std::cerr << "Reading auth methods" << std::endl;
    char* from = m_read_buffer.data();
    char* to = m_read_buffer.data() + m_auth_methods_count;
    // only 0x00 (no auth) is supported
    if (std::find(from, to, 0x00) == to)
    {
        m_auth_method = 0xFF;  // no acceptable methods were offered
        fail = true;
    }
    else
    {
        m_auth_method = 0x00;
    }
    this->consume_bytes_from_read_buffer(m_auth_methods_count);
    m_write_client_buffer.resize(2);
    m_write_client_buffer[0] = 0x05;
    m_write_client_buffer[1] = 0x00;
    this->write_to_client();
}

void Client::read_client_connection_request()
{
    std::cerr << "Reading client connection request" << std::endl;
    byte_t version = m_read_buffer[0];
    if (version != 0x05)
    {
        this->send_fail_message();
        return;
    }

    byte_t command = m_read_buffer[1];
    // TODO: handle properly
    assert(command == COMMAND_CONNECT);
    m_command = COMMAND_CONNECT;

    byte_t reserved = m_read_buffer[2];
    if (reserved != 0x00)
    {
        this->send_fail_message();
        return;
    }

    byte_t address_type = m_read_buffer[3];

    this->consume_bytes_from_read_buffer(4);

    switch (address_type)
    {
    case ADDRESS_TYPE_IPV4:  // IPv4
        m_address_type = ADDRESS_TYPE_IPV4;
        m_state = State::READING_ADDRESS;
        this->read_some_from_client(6);
        break;
    case ADDRESS_TYPE_DOMAIN_NAME:  // Domain name
        m_address_type = ADDRESS_TYPE_DOMAIN_NAME;
        m_state = State::READING_DOMAIN_NAME_LENGTH;
        this->read_some_from_client(1);
        break;
    case ADDRESS_TYPE_IPV6:  // IPv6
        m_address_type = ADDRESS_TYPE_IPV6;
        m_state = State::READING_ADDRESS;
        this->read_some_from_client(18);
        break;
    default:
        fail = true;
        m_write_client_buffer.resize(10);
        m_write_client_buffer[0] = 0x05;  // protocol version
        m_write_client_buffer[1] = 0x05;  // general failure
        m_write_client_buffer[2] = 0x00;  // reserved
        m_write_client_buffer[3] = 0x01;  // IPv4
        std::memset(m_write_client_buffer.data() + 4, 0, 6);
        this->write_to_client();
        break;
    }
}

void Client::read_domain_name_length()
{
    m_domain_name_length = m_read_buffer[0];
    this->consume_bytes_from_read_buffer(1);
    m_state = State::READING_ADDRESS;
    this->read_some_from_client(m_domain_name_length + 2);
}

void Client::send_fail_message()
{
    fail = true;
    m_write_client_buffer.resize(10);
    m_write_client_buffer[0] = 0x05;  // protocol version
    m_write_client_buffer[1] = 0x05;  // general failure
    m_write_client_buffer[2] = 0x00;  // reserved
    m_write_client_buffer[3] = 0x01;  // IPv4
    std::memset(m_write_client_buffer.data() + 4, 0, 6);
    this->write_to_client();
}

void Client::connect_ipv4_destination()
{
    try
    {
        m_destination_socket = std::make_unique<ExternalConnectionIPv4>(m_ipv4_address, m_port);
    }
    catch (...)
    {
        this->send_fail_message();
        return;
    }
    m_write_client_buffer.resize(10);
    m_write_client_buffer[0] = 0x05;  // protocol version
    m_write_client_buffer[1] = 0x00;  // request granted
    m_write_client_buffer[2] = 0x00;  // reserved
    m_write_client_buffer[3] = 0x01;  // IPv4
    std::memcpy(m_write_client_buffer.data() + 4, &m_ipv4_address, 4);
    std::memcpy(m_write_client_buffer.data() + 8, &m_port, 2);
    this->write_to_client();
}

void Client::connect_ipv6_destination()
{
    try
    {
        m_destination_socket = std::make_unique<ExternalConnectionIPv6>(m_ipv6_address, m_port);
    }
    catch (...)
    {
        this->send_fail_message();
        return;
    }
    m_write_client_buffer.resize(22);
    m_write_client_buffer[0] = 0x05;  // protocol version
    m_write_client_buffer[1] = 0x00;  // request granted
    m_write_client_buffer[2] = 0x00;  // reserved
    m_write_client_buffer[3] = 0x04;  // IPv6
    std::memcpy(m_write_client_buffer.data() + 4, &m_ipv6_address, 16);
    std::memcpy(m_write_client_buffer.data() + 20, &m_port, 2);
    this->write_to_client();
}

void Client::read_address()
{
    std::cerr << "Reading address" << std::endl;
    switch (m_address_type)
    {
    case ADDRESS_TYPE_IPV4:
        std::memcpy(&m_ipv4_address, m_read_buffer.data(), 4);
        std::memcpy(&m_port, m_read_buffer.data() + 4, 2);
        std::cerr << "ADDR: " << m_ipv4_address.s_addr << ", PORT: " << m_port << std::endl;
        this->consume_bytes_from_read_buffer(6);
        this->connect_ipv4_destination();
        break;

    case ADDRESS_TYPE_DOMAIN_NAME:
    {
        m_domain_name = std::string(m_read_buffer.data(), m_read_buffer.data() + m_domain_name_length);
        std::memcpy(&m_port, m_read_buffer.data() + m_domain_name_length, 2);
        this->consume_bytes_from_read_buffer(m_domain_name_length + 2);

        hostent* he;
        he = ::gethostbyname(m_domain_name.c_str());
        if (he == nullptr)
        {
            this->send_fail_message();
            ::herror("gethostbyname");
            return;
        }
        else
        {
            if (he->h_addr_list[0] == nullptr)
            {
                this->send_fail_message();
                ::herror("gethostbyname");
            }
            switch (he->h_addrtype)
            {
            case AF_INET:
            {
                in_addr* addr = reinterpret_cast<in_addr*>(he->h_addr_list[0]);
                std::memcpy(&m_ipv4_address, &addr, sizeof(in_addr));
                this->connect_ipv4_destination();
                break;
            }
            case AF_INET6:
            {
                in6_addr* addr = reinterpret_cast<in6_addr*>(he->h_addr_list[0]);
                std::memcpy(&m_ipv6_address, &addr, sizeof(in6_addr));
                this->connect_ipv6_destination();
                break;
            }
#ifndef DNDEBUG
            default:
                assert(false);
                break;
#endif
            }
        }
        break;
    }

    case ADDRESS_TYPE_IPV6:
        std::memcpy(&m_ipv6_address, m_read_buffer.data(), 16);
        std::memcpy(&m_port, m_read_buffer.data() + 16, 2);
        std::cerr << "ADDR: IPv6, PORT: " << m_port << std::endl;
        this->consume_bytes_from_read_buffer(18);
        this->connect_ipv6_destination();
        break;

#ifndef DNDEBUG
    default:
        assert(false);
        break;
#endif
    }
}

void Client::handle_client_read(std::size_t nread)
{
    if (m_state == State::READING_USER_REQUESTS)
    {
        m_server.add_dst_write_request(this, nread);
        return;
    }

    if (nread != 0)
    {
        m_read_buffer.insert(m_read_buffer.end(), m_buffer0.data(), m_buffer0.data() + nread);
    }
    if (!m_is_read_completed())
    {
        m_server.add_client_read_request(this);
        return;
    }

    switch (m_state)
    {
    case State::READING_CLIENT_GREETING:
        this->read_client_greeting();
        break;
    case State::READING_AUTH_METHODS:
        this->read_auth_methods();
        break;
    case State::READING_CLIENT_CONNECTION_REQUEST:
        this->read_client_connection_request();
        break;
    case State::READING_DOMAIN_NAME_LENGTH:
        this->read_domain_name_length();
        break;
    case State::READING_ADDRESS:
        this->read_address();
        break;
#ifndef DNDEBUG
    case State::READING_USER_REQUESTS:
        assert(false);
        break;
#endif
    }
}

void Client::handle_client_write(std::size_t nwrite)
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
        this->write_to_client();
        return;
    }

    switch (m_state)
    {
    case State::READING_AUTH_METHODS:
        std::cerr << "After reply auth methods" << std::endl;
        m_state = State::READING_CLIENT_CONNECTION_REQUEST;
        this->read_some_from_client(4);
        break;
    case State::READING_DOMAIN_NAME_LENGTH:
        assert(false);
        break;
    case State::READING_ADDRESS:
        std::cerr << "After reply address accepted" << std::endl;
        m_state = State::READING_USER_REQUESTS;
        m_server.add_dst_read_request(this);
        this->read_from_client();
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
    }
}

void Client::handle_dst_read(std::size_t nread)
{
    std::cerr << "DST READ " << nread << std::endl;
    m_server.add_client_write_request(this, nread);
}

void Client::handle_dst_write(std::size_t nwrite)
{
    std::cerr << "DST WRITE " << nwrite << std::endl;
    m_server.add_client_read_request(this);
}

void Client::consume_bytes_from_read_buffer(std::size_t nread)
{
    m_read_buffer.erase(m_read_buffer.begin(), m_read_buffer.begin() + nread);
}

Client::~Client()
{
    try
    {
        syscall_wrapper::close(fd);
        m_buffer_pool.return_buffer(buffer_index);
    }
    catch (...)
    {
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace hw2
