#ifndef HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
#define HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_

#include <socket.hpp>

#include <liburing.h>
#include <netdb.h>

#include <cassert>
#include <functional>
#include <memory>
#include <queue>
#include <span>
#include <vector>

namespace hw2
{

using byte_t = unsigned char;

enum class EventType
{
    CLIENT_ACCEPT = 0,
    CLIENT_READ,
    CLIENT_WRITE,
    DESTINATION_CONNECT,
    DESTINATION_READ,
    DESTINATION_WRITE,
};

class Client;

struct Event
{
    std::size_t id;
    Client* client;
    EventType type;
};

class EventPool
{
public:
    EventPool(std::size_t nconnections);

    [[nodiscard]] Event& obtain_event();
    void return_event(const Event& event);

    const std::size_t total_events_count;

private:
    std::vector<Event> m_events;
    std::queue<std::size_t> m_free_events;
};

class BufferPool
{
public:
    class InsufficientBuffersException : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
        ~InsufficientBuffersException() override;
    };

    static constexpr std::size_t HALF_BUFFER_SIZE = (1 << 14);
    // 0: receive from client, send to destination
    // 1: receive from destination, send to client
    static constexpr std::size_t BUFFER_SIZE = HALF_BUFFER_SIZE * 2;

    BufferPool(std::size_t nconnections);

    [[nodiscard]] std::size_t obtain_free_buffer();
    void return_buffer(std::size_t buffer_index);

    [[nodiscard]] std::span<const byte_t> buffer_half0(std::size_t buffer_index) const;
    [[nodiscard]] std::span<byte_t> buffer_half0(std::size_t buffer_index);
    [[nodiscard]] std::span<const byte_t> buffer_half1(std::size_t buffer_index) const;
    [[nodiscard]] std::span<byte_t> buffer_half1(std::size_t buffer_index);

    const std::size_t total_buffer_count;

private:
    std::vector<byte_t> m_buffers;
    std::queue<std::size_t> m_free_buffers;
};

class IoUring;

class Client
{
public:
    Client(int fd, IoUring& server, BufferPool& buffers);
    ~Client();

    void write_to_client();
    void read_from_client();
    void read_some_from_client(std::size_t n);

    [[nodiscard]] std::span<byte_t> buffer0() { return m_buffer0; }
    [[nodiscard]] std::span<const byte_t> buffer0() const { return m_buffer0; }
    [[nodiscard]] std::span<byte_t> buffer1() { return m_buffer1; }
    [[nodiscard]] std::span<const byte_t> buffer1() const { return m_buffer1; }

    void handle_client_read(std::size_t nread);
    void handle_client_write(std::size_t nwrite);
    void handle_dst_connect();
    void handle_dst_read(std::size_t nread);
    void handle_dst_write(std::size_t nwrite);

    [[nodiscard]] Socket* destination_socket() { return m_destination_socket.get(); }

    const int fd;
    bool fail = false;

private:
    void consume_bytes_from_read_buffer(std::size_t nread);

    void read_client_greeting();
    void read_auth_methods();
    void read_client_connection_request();
    void read_domain_name_length();
    void read_address();

    void send_fail_message();

    void connect_ipv4_destination();
    void connect_ipv6_destination();

private:
    enum class State
    {
        READING_CLIENT_GREETING,
        READING_AUTH_METHODS,
        READING_CLIENT_CONNECTION_REQUEST,
        READING_DOMAIN_NAME_LENGTH,
        READING_ADDRESS,
        CONNECTING_TO_DESTINATION,
        READING_USER_REQUESTS,
    };

    enum Command
    {
        COMMAND_CONNECT = 0x01,
        COMMAND_BIND = 0x02,
        COMMAND_UDP_ASSOCIATE = 0x03,
    };

    enum AddressType
    {
        ADDRESS_TYPE_IPV4 = 0x01,
        ADDRESS_TYPE_DOMAIN_NAME = 0x03,
        ADDRESS_TYPE_IPV6 = 0x04,
    };

private:
    State m_state = State::READING_CLIENT_GREETING;

    IoUring& m_server;
public:
    std::size_t awaiting_events_count = 0;

private:
    BufferPool& m_buffer_pool;

public:
    const std::size_t buffer_index;

private:
    std::span<byte_t> m_buffer0;
    std::span<byte_t> m_buffer1;

    std::function<bool()> m_is_read_completed;
    // TODO: change to deque
    std::vector<byte_t> m_read_buffer;
    // TODO: change to deque
    std::vector<byte_t> m_write_client_buffer;

    std::size_t m_auth_methods_count;
    std::size_t m_auth_method;
    Command m_command;
    AddressType m_address_type;
    std::size_t m_domain_name_length;
    std::string m_domain_name;
    in_addr m_ipv4_address;
    in6_addr m_ipv6_address;
    in_port_t m_port;

    std::unique_ptr<Socket> m_destination_socket;
};

class IoUring
{
public:
    IoUring(const MainSocket& socket, std::size_t nconnections);

    ~IoUring();

    void event_loop();

    void add_client_accept_request(struct sockaddr_in* client_addr, socklen_t* client_addr_len);
    void add_client_read_request(Client* client);
    void add_client_write_request(Client* client, std::size_t nbytes);
    void add_dst_connect_request(Client* client);
    void add_dst_read_request(Client* client);
    void add_dst_write_request(Client* client, std::size_t nbytes);

private:
    void handle_accept(const io_uring_cqe* cqe);

    const MainSocket& m_socket;
    EventPool m_event_pool;
    BufferPool m_buffer_pool;
    io_uring m_ring;
    sockaddr_in m_client_addr;
    socklen_t m_client_addr_len = sizeof(m_client_addr);
};


}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
