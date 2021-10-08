#ifndef HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
#define HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_

#include <socket.hpp>

#include <liburing.h>
#include <netdb.h>

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

    static constexpr unsigned HALF_BUFFER_SIZE = (1 << 14);
    // 0: receive from client, send to destination
    // 1: receive from destination, send to client
    static constexpr unsigned BUFFER_SIZE = HALF_BUFFER_SIZE * 2;

    BufferPool(unsigned nconnections);

    [[nodiscard]] unsigned obtain_free_buffer();
    void return_buffer(unsigned buffer_index);

    [[nodiscard]] std::span<const iovec> get_iovecs() const;
    [[nodiscard]] std::span<iovec> get_iovecs();

    [[nodiscard]] std::span<const byte_t> buffer_half0(unsigned buffer_index) const;
    [[nodiscard]] std::span<byte_t> buffer_half0(unsigned buffer_index);
    [[nodiscard]] std::span<const byte_t> buffer_half1(unsigned buffer_index) const;
    [[nodiscard]] std::span<byte_t> buffer_half1(unsigned buffer_index);

    const unsigned total_buffer_count;

private:
    std::vector<byte_t> m_buffers;
    std::queue<unsigned> m_free_buffers;
    std::vector<iovec> m_iovecs;
};

class IoUring;

class Client
{
public:
    Client(int fd, IoUring& server, BufferPool& buffers);
    ~Client();

    void write_to_client();
    void read_from_client();
    void read_some_from_client(unsigned n);

    [[nodiscard]] std::span<byte_t> buffer0() { return m_buffer0; }
    [[nodiscard]] std::span<const byte_t> buffer0() const { return m_buffer0; }
    [[nodiscard]] std::span<byte_t> buffer1() { return m_buffer1; }
    [[nodiscard]] std::span<const byte_t> buffer1() const { return m_buffer1; }

    void handle_client_read(unsigned nread);
    void handle_client_write(unsigned nwrite);
    void handle_destination_connect();
    void handle_destination_read(unsigned nread);
    void handle_destination_write(unsigned nwrite);

    [[nodiscard]] Socket* destination_socket() { return m_destination_socket.get(); }

    const int fd;
    bool fail = false;

private:
    void consume_bytes_from_read_buffer(unsigned nread);

    void read_client_greeting();
    void read_auth_methods();
    void read_client_connection_request();
    void read_domain_name_length();
    void read_address();

    void translate_errno(int error_code);
    void send_fail_message(byte_t error_code = 0x01);

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
    const unsigned m_buffer_index;

public:
    const unsigned buffer0_index;
    const unsigned buffer1_index;

private:

    std::span<byte_t> m_buffer0;
    std::span<byte_t> m_buffer1;

    std::function<bool()> m_is_read_completed;
    // TODO: investigate if std::deque is more suitable in terms of performance
    std::vector<byte_t> m_read_buffer;
    std::vector<byte_t> m_write_client_buffer;

    unsigned m_auth_methods_count;
    byte_t m_auth_method;
    Command m_command;
    AddressType m_address_type;
    unsigned m_domain_name_length;
    std::string m_domain_name;
    in_addr m_ipv4_address;
    in6_addr m_ipv6_address;
    in_port_t m_port;

    std::unique_ptr<Socket> m_destination_socket;

    unsigned m_client_write_size;
    unsigned m_client_write_offset;
    unsigned m_destination_write_size;
    unsigned m_destination_write_offset;
};

class IoUring
{
public:
    IoUring(const MainSocket& socket, unsigned nconnections, bool kernel_polling = false);

    ~IoUring();

    void event_loop();

    void add_client_accept_request(struct sockaddr_in* client_addr, socklen_t* client_addr_len);
    void add_client_read_request(Client* client);
    void add_client_write_request(Client* client, unsigned nbytes, unsigned offset = 0);
    void add_destination_connect_request(Client* client);
    void add_destination_read_request(Client* client);
    void add_destination_write_request(Client* client, unsigned nbytes, unsigned offset = 0);

private:
    void handle_accept(const io_uring_cqe* cqe);

    const MainSocket& m_socket;
    EventPool m_event_pool;
    BufferPool m_buffer_pool;
    io_uring m_ring;
    sockaddr_in m_client_addr;
    socklen_t m_client_addr_len = sizeof(m_client_addr);
    bool m_is_root;
};


}  // namespace hw2

#endif  // HW2_SOCKS5_SERVER_SERVER_EVENT_LOOP_HPP_
