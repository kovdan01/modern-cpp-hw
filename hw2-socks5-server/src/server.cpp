#include <server.hpp>
#include <socket.hpp>
#include <syscall.hpp>
#include <utils.hpp>

#include <spdlog/fmt/bin_to_hex.h>

#include <arpa/inet.h>
#include <liburing.h>
#include <netdb.h>

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <queue>
#include <span>
#include <vector>

namespace hw2
{

EventPool::EventPool(std::size_t nconnections)
    : total_events_count(4 * nconnections)  // read & write for client & destination
    , m_events(total_events_count)
{
    for (std::size_t i = 0; i < total_events_count; ++i)
    {
        m_events[i].id = i;
        m_free_events.push(i);
    }
}

Event& EventPool::obtain_event()
{
    assert(!m_free_events.empty());
    std::size_t free_event_index = m_free_events.front();
    m_free_events.pop();
    return m_events[free_event_index];
}

void EventPool::return_event(const Event &event)
{
    m_free_events.push(event.id);
}

BufferPool::InsufficientBuffersException::~InsufficientBuffersException() = default;

BufferPool::BufferPool(unsigned nconnections)
    : total_buffer_count(nconnections)
    , m_buffers(total_buffer_count * BUFFER_SIZE)
    , m_iovecs(2 * total_buffer_count)
{
    for (unsigned i = 0; i < total_buffer_count; ++i)
    {
        m_free_buffers.push(i);
        m_iovecs[2 * i].iov_base = m_buffers.data() + 2 * i * HALF_BUFFER_SIZE;
        m_iovecs[2 * i].iov_len = HALF_BUFFER_SIZE;
        m_iovecs[2 * i + 1].iov_base = m_buffers.data() + (2 * i + 1) * HALF_BUFFER_SIZE;
        m_iovecs[2 * i + 1].iov_len = HALF_BUFFER_SIZE;
    }
}

std::span<const iovec> BufferPool::get_iovecs() const
{
    return { m_iovecs.data(), m_iovecs.size() };
}

std::span<iovec> BufferPool::get_iovecs()
{
    return { m_iovecs.data(), m_iovecs.size() };
}

unsigned BufferPool::obtain_free_buffer()
{
    if (UNLIKELY(m_free_buffers.empty()))
    {
        throw InsufficientBuffersException("");
    }
    unsigned free_buffer_index = m_free_buffers.front();
    m_free_buffers.pop();
    return free_buffer_index;
}

void BufferPool::return_buffer(unsigned buffer_index)
{
    m_free_buffers.push(buffer_index);
}

std::span<const byte_t> BufferPool::buffer_half0(unsigned buffer_index) const
{
    return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE };
}

std::span<byte_t> BufferPool::buffer_half0(unsigned buffer_index)
{
    return { m_buffers.data() + buffer_index * BUFFER_SIZE, m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE };
}

std::span<const byte_t> BufferPool::buffer_half1(unsigned buffer_index) const
{
    return { m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
}

std::span<byte_t> BufferPool::buffer_half1(unsigned buffer_index)
{
    return { m_buffers.data() + buffer_index * BUFFER_SIZE + HALF_BUFFER_SIZE, m_buffers.data() + (buffer_index + 1) * BUFFER_SIZE };
}


Client::Client(int fd, IoUring& server, BufferPool& buffer_pool)
    : fd(fd)
    , m_server(server)
    , m_buffer_pool(buffer_pool)
    , m_buffer_index(m_buffer_pool.obtain_free_buffer())
    , buffer0_index(static_cast<unsigned>(2 * m_buffer_index + 0))
    , buffer1_index(static_cast<unsigned>(2 * m_buffer_index + 1))
    , m_buffer0(m_buffer_pool.buffer_half0(m_buffer_index))
    , m_buffer1(m_buffer_pool.buffer_half1(m_buffer_index))
{
    m_is_read_completed = [](){ return true; };
}


IoUring::IoUring(const MainSocket& socket, unsigned nconnections, bool kernel_polling)
    : m_socket(socket)
    , m_event_pool(nconnections)
    , m_buffer_pool(nconnections)
    , m_is_root(::geteuid() == 0 ? true : false)
{
    if (kernel_polling)
    {
        if (!m_is_root)
        {
            throw std::runtime_error("You need root privileges to do kernel polling.");
        }
    }

    struct io_uring_params params;
    std::memset(&params, 0, sizeof(io_uring_params));
    params.flags = kernel_polling ? IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF : 0;
    params.sq_thread_idle = 5'000;
    if (io_uring_queue_init_params(nconnections, &m_ring, &params) != 0)
        throw std::runtime_error("io_uring_queue_init_params");

    if (m_is_root)
    {
        std::span<const iovec> iovecs = m_buffer_pool.get_iovecs();
        int res = io_uring_register_buffers(&m_ring, iovecs.data(), static_cast<unsigned>(iovecs.size()));
        if (res != 0)
        {
            std::perror("io_uring_register_buffers");
            throw std::runtime_error("io_uring_register_buffers");
        }
    }
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
        logger()->error("Server capacity exceeded");
        syscall_wrapper::close(fd);
        return;
    }
    client->awaiting_events_count = 1;
    client->read_some_from_client(3);
}

void IoUring::event_loop()
{
    this->add_client_accept_request(&m_client_addr, &m_client_addr_len);

    for (;;)
    {
        io_uring_cqe* cqe;
        if (UNLIKELY(io_uring_wait_cqe(&m_ring, &cqe) < 0))
        {
            int error_code = errno;
            logger()->error("io_uring_wait_cqe failed: {0}", std::strerror(error_code));
            throw syscall_wrapper::Error("io_uring_wait_cqe", error_code);
        }

        if (UNLIKELY(cqe->res < 0))
        {
            logger()->debug("cqe fail: {0}", std::strerror(-cqe->res));
            if (cqe->user_data != 0)
            {
                Event* event = reinterpret_cast<Event*>(cqe->user_data);
                event->client->fail = true;
                m_event_pool.return_event(*event);
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
#ifndef NDEBUG
                    case EventType::CLIENT_ACCEPT:
                        assert(false);
                        break;
#endif
                    case EventType::CLIENT_READ:
                        if (LIKELY(cqe->res != 0))
                        {
                            event->client->handle_client_read(static_cast<unsigned>(cqe->res));
                        }
                        else  // empty read indicates that client disconnected
                        {
                            event->client->fail = true;
                        }
                        break;
                    case EventType::CLIENT_WRITE:
                        event->client->handle_client_write(static_cast<unsigned>(cqe->res));
                        break;
                    case EventType::DESTINATION_CONNECT:
                        event->client->handle_destination_connect();
                        break;
                    case EventType::DESTINATION_READ:
                        if (LIKELY(cqe->res != 0))
                        {
                            event->client->handle_destination_read(static_cast<unsigned>(cqe->res));
                        }
                        else  // empty read indicates that destination disconnected
                        {
                            event->client->fail = true;
                        }
                        break;
                    case EventType::DESTINATION_WRITE:
                        event->client->handle_destination_write(static_cast<unsigned>(cqe->res));
                        break;
                    }
                    if (event->client->fail && event->client->awaiting_events_count == 0)
                    {
                        delete event->client;
                    }
                }
                m_event_pool.return_event(*event);
            }
        }
        io_uring_cqe_seen(&m_ring, cqe);
    }
}

void IoUring::add_client_accept_request(struct sockaddr_in* client_addr, socklen_t* client_addr_len)
{
    io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    io_uring_prep_accept(sqe, m_socket.fd, reinterpret_cast<struct sockaddr*>(client_addr), client_addr_len, 0);
    io_uring_sqe_set_data(sqe, nullptr);
    io_uring_submit(&m_ring);
}

void IoUring::add_client_read_request(Client* client)
{
    ++client->awaiting_events_count;
    io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    if (m_is_root)
    {
        io_uring_prep_read_fixed(sqe, client->fd, client->buffer0().data(), BufferPool::HALF_BUFFER_SIZE,
                                 0, static_cast<int>(client->buffer0_index));
    }
    else
    {
        io_uring_prep_read(sqe, client->fd, client->buffer0().data(),
                           BufferPool::BUFFER_SIZE, 0);
    }
    Event& event = m_event_pool.obtain_event();
    event.client = client;
    event.type = EventType::CLIENT_READ;
    io_uring_sqe_set_data(sqe, &event);
    io_uring_submit(&m_ring);
}

void IoUring::add_client_write_request(Client* client, unsigned nbytes, unsigned offset)
{
    ++client->awaiting_events_count;
    io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    if (m_is_root)
    {
        io_uring_prep_write_fixed(sqe, client->fd, client->buffer1().data(),
                                  nbytes, offset, static_cast<int>(client->buffer1_index));
    }
    else
    {
        io_uring_prep_write(sqe, client->fd, client->buffer1().data(), nbytes, offset);
    }
    Event& event = m_event_pool.obtain_event();
    event.client = client;
    event.type = EventType::CLIENT_WRITE;
    io_uring_sqe_set_data(sqe, &event);
    io_uring_submit(&m_ring);
}

void IoUring::add_destination_connect_request(Client* client)
{
    io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    io_uring_prep_connect(sqe, client->destination_socket()->fd,
                          client->destination_socket()->address(),
                          client->destination_socket()->address_length());
    Event& event = m_event_pool.obtain_event();
    event.client = client;
    event.type = EventType::DESTINATION_CONNECT;
    io_uring_sqe_set_data(sqe, &event);
    io_uring_submit(&m_ring);
}

void IoUring::add_destination_read_request(Client* client)
{
    ++client->awaiting_events_count;
    io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    if (m_is_root)
    {
        io_uring_prep_read_fixed(sqe, client->destination_socket()->fd, client->buffer1().data(),
                                 BufferPool::HALF_BUFFER_SIZE, 0, static_cast<int>(client->buffer1_index));
    }
    else
    {
        io_uring_prep_read(sqe, client->destination_socket()->fd,client->buffer1().data(),
                           BufferPool::HALF_BUFFER_SIZE, 0);
    }
    Event& event = m_event_pool.obtain_event();
    event.client = client;
    event.type = EventType::DESTINATION_READ;
    io_uring_sqe_set_data(sqe, &event);
    io_uring_submit(&m_ring);
}

void IoUring::add_destination_write_request(Client* client, unsigned nbytes, unsigned offset)
{
    ++client->awaiting_events_count;
    io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
    if (m_is_root)
    {
        io_uring_prep_write_fixed(sqe, client->destination_socket()->fd, client->buffer0().data(),
                                  nbytes, offset, static_cast<int>(client->buffer0_index));
    }
    else
    {
        io_uring_prep_write(sqe, client->destination_socket()->fd,
                            client->buffer0().data(), nbytes, offset);
    }
    Event& event = m_event_pool.obtain_event();
    event.client = client;
    event.type = EventType::DESTINATION_WRITE;
    io_uring_sqe_set_data(sqe, &event);
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

void Client::read_some_from_client(unsigned n)
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

// write function for common case, when client may
// intend to write more data than one buffer can contain
void Client::write_to_client()
{
    unsigned cnt = std::min(BufferPool::HALF_BUFFER_SIZE, static_cast<unsigned>(m_write_client_buffer.size()));
    std::memcpy(m_buffer1.data(), m_write_client_buffer.data(), cnt);
    m_server.add_client_write_request(this, cnt);
}

void Client::read_client_greeting()
{
    logger()->debug("Reading client greeting");
    byte_t version = m_read_buffer[0];
    if (version != 0x05)
    {
        fail = true;
        return;
    }
    m_auth_methods_count = m_read_buffer[1];
    if (m_auth_methods_count == 0)
    {
        fail = true;
        return;
    }
    this->consume_bytes_from_read_buffer(2);
    m_state = State::READING_AUTH_METHODS;
    this->read_some_from_client(m_auth_methods_count);
}

void Client::read_auth_methods()
{
    logger()->debug("Reading auth methods");
    byte_t* from = m_read_buffer.data();
    byte_t* to = m_read_buffer.data() + m_auth_methods_count;
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
    logger()->debug("Reading client connection request");
    byte_t version = m_read_buffer[0];
    if (version != 0x05)
    {
        this->send_fail_message(0x07);  // Command not supported / protocol error
        return;
    }

    byte_t command = m_read_buffer[1];
    if (command != COMMAND_CONNECT)
    {
        this->send_fail_message(0x07);  // Command not supported / protocol error
        return;
    }
    m_command = COMMAND_CONNECT;

    byte_t reserved = m_read_buffer[2];
    if (reserved != 0x00)
    {
        this->send_fail_message(0x07);  // Command not supported / protocol error
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

void Client::send_fail_message(byte_t error_code)
{
    fail = true;
    m_write_client_buffer.resize(10);
    m_write_client_buffer[0] = 0x05;  // protocol version
    m_write_client_buffer[1] = error_code;
    m_write_client_buffer[2] = 0x00;  // reserved
    m_write_client_buffer[3] = 0x01;  // IPv4
    std::memset(m_write_client_buffer.data() + 4, 0, 6);
    this->write_to_client();
}

void Client::translate_errno(int error_code)
{
    switch (error_code)
    {
    case ENETUNREACH:  // Network unreachable
        this->send_fail_message(0x03);
        break;
    case EHOSTUNREACH:  // Host is unreachable
        this->send_fail_message(0x04);
        break;
    case ECONNREFUSED:  // Connection refused
        this->send_fail_message(0x05);
        break;
    default:
        this->send_fail_message();
        break;
    }
}

void Client::connect_ipv4_destination()
{
    try
    {
        m_destination_socket = std::make_unique<SocketIPv4>(m_ipv4_address, m_port);
    }
    catch (syscall_wrapper::Error& e)
    {
        this->translate_errno(e.error_code);
    }
    catch (...)
    {
        this->send_fail_message();
        return;
    }
    m_state = State::CONNECTING_TO_DESTINATION;
    m_server.add_destination_connect_request(this);
}

void Client::connect_ipv6_destination()
{
    try
    {
        m_destination_socket = std::make_unique<SocketIPv6>(m_ipv6_address, m_port);
    }
    catch (syscall_wrapper::Error& e)
    {
        this->translate_errno(e.error_code);
    }
    catch (...)
    {
        this->send_fail_message();
        return;
    }
    m_state = State::CONNECTING_TO_DESTINATION;
    m_server.add_destination_connect_request(this);
}

void Client::read_address()
{
    logger()->debug("Reading address");
    switch (m_address_type)
    {
    case ADDRESS_TYPE_IPV4:
        std::memcpy(&m_ipv4_address, m_read_buffer.data(), 4);
        std::memcpy(&m_port, m_read_buffer.data() + 4, 2);
        logger()->debug("Got IPv4 address {0}, port {1}", ::inet_ntoa(m_ipv4_address), m_port);
        this->consume_bytes_from_read_buffer(6);
        this->connect_ipv4_destination();
        break;

    case ADDRESS_TYPE_DOMAIN_NAME:
    {
        m_domain_name = std::string(m_read_buffer.data(), m_read_buffer.data() + m_domain_name_length);
        std::memcpy(&m_port, m_read_buffer.data() + m_domain_name_length, 2);
        logger()->debug("Got domain name {0}, port {1}", m_domain_name, m_port);
        this->consume_bytes_from_read_buffer(m_domain_name_length + 2);

        hostent* he;
        he = ::gethostbyname(m_domain_name.c_str());
        if (he == nullptr || he->h_addr_list[0] == nullptr)
        {
            logger()->debug("gethostbyname from '{0}' fail: {1}", m_domain_name, hstrerror(h_errno));
            this->send_fail_message(0x04);  // Host unreachable
            return;
        }

        switch (he->h_addrtype)
        {
        case AF_INET:
        {
            m_address_type = ADDRESS_TYPE_IPV4;
            in_addr* addr = reinterpret_cast<in_addr*>(he->h_addr_list[0]);
            std::memcpy(&m_ipv4_address, addr, sizeof(in_addr));
            this->connect_ipv4_destination();
            break;
        }
        case AF_INET6:
        {
            m_address_type = ADDRESS_TYPE_IPV6;
            in6_addr* addr = reinterpret_cast<in6_addr*>(he->h_addr_list[0]);
            std::memcpy(&m_ipv6_address, addr, sizeof(in6_addr));
            this->connect_ipv6_destination();
            break;
        }
#ifndef NDEBUG
        default:
            assert(false);
            break;
#endif
        }
        break;
    }

    case ADDRESS_TYPE_IPV6:
    {
        std::memcpy(&m_ipv6_address, m_read_buffer.data(), 16);
        std::memcpy(&m_port, m_read_buffer.data() + 16, 2);
#ifndef NDEBUG
        char address_string[INET6_ADDRSTRLEN];
        const char* res = ::inet_ntop(AF_INET6, &m_ipv6_address, address_string, INET6_ADDRSTRLEN);
        assert(res == address_string);
        logger()->debug("Got IPv6 address {0}, port {1}", address_string, m_port);
#endif
        this->consume_bytes_from_read_buffer(18);
        this->connect_ipv6_destination();
        break;
    }

#ifndef NDEBUG
    default:
        assert(false);
        break;
#endif
    }
}

void Client::handle_client_read(unsigned nread)
{
    logger()->debug("CQE: read from client, nread = {}", nread);
    if (m_state == State::READING_USER_REQUESTS)
    {
        m_destination_write_offset = 0;
        m_destination_write_size = nread;
        m_server.add_destination_write_request(this, nread);
        return;
    }

    if (nread != 0)
    {
        m_read_buffer.insert(m_read_buffer.end(), m_buffer0.data(), m_buffer0.data() + nread);
    }
    if (!m_is_read_completed())
    {
        logger()->debug("Partial read occurred, re-add read request");
        m_server.add_client_read_request(this);
        return;
    }
    logger()->debug("Whole read completed");

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
#ifndef NDEBUG
    case State::CONNECTING_TO_DESTINATION:
        assert(false);
        break;
    case State::READING_USER_REQUESTS:
        assert(false);
        break;
#endif
    }
}

void Client::handle_client_write(unsigned nwrite)
{
    logger()->debug("CQE: write to client, nwrite = {}", nwrite);
    if (m_state == State::READING_USER_REQUESTS)
    {
        if (LIKELY(nwrite + m_client_write_offset == m_client_write_size))
        {
            logger()->debug("Whole write to client completed");
            m_server.add_destination_read_request(this);
        }
        else
        {
            logger()->debug("Partial write to client occurred, re-add write request");
            m_client_write_offset += nwrite;
            m_server.add_client_write_request(this, m_client_write_size - m_client_write_offset, m_client_write_offset);
        }
        return;
    }

    m_write_client_buffer.erase(m_write_client_buffer.begin(), m_write_client_buffer.begin() + nwrite);
    if (!m_write_client_buffer.empty())
    {
        logger()->debug("Partial write to client occurred, re-add write request");
        this->write_to_client();
        return;
    }
    logger()->debug("Whole write to client completed");

    switch (m_state)
    {
    case State::READING_CLIENT_GREETING:
        assert(false);
        break;
    case State::READING_AUTH_METHODS:
        m_state = State::READING_CLIENT_CONNECTION_REQUEST;
        this->read_some_from_client(4);
        break;
    case State::READING_CLIENT_CONNECTION_REQUEST:
        assert(false);
        break;
    case State::READING_DOMAIN_NAME_LENGTH:
        assert(false);
        break;
    case State::READING_ADDRESS:
        assert(false);
        break;
    case State::CONNECTING_TO_DESTINATION:
        m_state = State::READING_USER_REQUESTS;
        m_server.add_destination_read_request(this);
        this->read_from_client();
        break;
    case State::READING_USER_REQUESTS:
        assert(false);
        break;
    }
}

void Client::handle_destination_connect()
{
    assert(m_state == State::CONNECTING_TO_DESTINATION);
    switch (m_address_type)
    {
    case ADDRESS_TYPE_IPV4:
        m_write_client_buffer.resize(10);
        m_write_client_buffer[0] = 0x05;  // protocol version
        m_write_client_buffer[1] = 0x00;  // request granted
        m_write_client_buffer[2] = 0x00;  // reserved
        m_write_client_buffer[3] = 0x01;  // IPv4
        std::memcpy(m_write_client_buffer.data() + 4, &m_ipv4_address, 4);
        std::memcpy(m_write_client_buffer.data() + 8, &m_port, 2);
        this->write_to_client();
        break;
    case ADDRESS_TYPE_IPV6:
        m_write_client_buffer.resize(22);
        m_write_client_buffer[0] = 0x05;  // protocol version
        m_write_client_buffer[1] = 0x00;  // request granted
        m_write_client_buffer[2] = 0x00;  // reserved
        m_write_client_buffer[3] = 0x04;  // IPv6
        std::memcpy(m_write_client_buffer.data() + 4, &m_ipv6_address, 16);
        std::memcpy(m_write_client_buffer.data() + 20, &m_port, 2);
        this->write_to_client();
        break;
#ifndef NDEBUG
    default:
        assert(false);
        break;
#endif
    }
}

void Client::handle_destination_read(unsigned nread)
{
    logger()->debug("CQE: read from destination, nread = {}", nread);
    m_client_write_offset = 0;
    m_client_write_size = nread;
    m_server.add_client_write_request(this, nread);
}

void Client::handle_destination_write(unsigned nwrite)
{
    logger()->debug("CQE: write to destination, nwrite = {}", nwrite);
    if (LIKELY(nwrite + m_destination_write_offset == m_destination_write_size))
    {
        logger()->debug("Whole write to destination completed");
        m_server.add_client_read_request(this);
    }
    else
    {
        logger()->debug("Partial write to destination occurred, re-add write request");
        m_destination_write_offset += nwrite;
        m_server.add_destination_write_request(this, m_destination_write_size - m_destination_write_offset, m_destination_write_offset);
    }
}

void Client::consume_bytes_from_read_buffer(unsigned nread)
{
    m_read_buffer.erase(m_read_buffer.begin(), m_read_buffer.begin() + nread);
}

Client::~Client()
{
    try
    {
        syscall_wrapper::close(fd);
        m_buffer_pool.return_buffer(m_buffer_index);
    }
    catch (...)
    {
        logger()->critical("Unhandled exception in Client::~Client()");
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace hw2
