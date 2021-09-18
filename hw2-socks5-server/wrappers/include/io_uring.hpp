#ifndef HW2_SOCKS5_SERVER_WRAPPERS_IO_URING_HPP_
#define HW2_SOCKS5_SERVER_WRAPPERS_IO_URING_HPP_

#include <hw2-wrappers/export.h>
#include <socket.hpp>

#include <liburing.h>

#include <arpa/inet.h>

#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace hw2::io_uring
{

class HW2_WRAPPERS_EXPORT Error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
    ~Error() override;
};

class HW2_WRAPPERS_EXPORT Probe
{
public:
    Probe(struct io_uring* ring)
        : m_probe(io_uring_get_probe_ring(ring))
    {
        if (m_probe == nullptr || !io_uring_opcode_supported(m_probe, IORING_OP_PROVIDE_BUFFERS))
        {
            throw Error("Buffer select is not supported");
        }
    }

    ~Probe()
    {
        std::free(m_probe);
    }

    const io_uring_probe* handle() const
    {
        return m_probe;
    }

private:
    io_uring_probe* m_probe;
};

struct ConnectionInfo
{
    enum class Type : __u16
    {
        ACCEPT,
        READ,
        WRITE,
        PROVIDE_BUF,
    };
    int fd;
    Type type;
    __u16 buffer_id;
};

static_assert (sizeof (ConnectionInfo) == sizeof (__u64), "");

class HW2_WRAPPERS_EXPORT BufferPool
{
public:
    BufferPool(std::size_t buffer_count, std::size_t buffer_size)
        : buffer_count(buffer_count)
        , buffer_size(buffer_size)
        , m_buffers(this->buffer_count * this->buffer_size)
    {
    }

    byte_t* buffer(std::size_t buffer_id)
    {
        return m_buffers.data() + buffer_id * buffer_size;
    }

    const byte_t* buffer(std::size_t buffer_id) const
    {
        return m_buffers.data() + buffer_id * buffer_size;
    }

    byte_t* data()
    {
        return m_buffers.data();
    }

    const byte_t* data() const
    {
        return m_buffers.data();
    }

    const std::size_t buffer_count;
    const std::size_t buffer_size;

private:
    std::vector<byte_t> m_buffers;
};

class HW2_WRAPPERS_EXPORT Ring
{
public:
    Ring(unsigned entries)
        : m_buffers(BUFFERS_COUNT, MAX_MESSAGE_LEN)
    {
        struct io_uring_params ring_params;
        std::memset(&ring_params, 0, sizeof(ring_params));

        if (io_uring_queue_init_params(entries, &m_ring, &ring_params) < 0)
        {
            std::perror("io_uring_queue_init_params");
            throw Error("io_uring_queue_init_params");
        }

        // check if IORING_FEAT_FAST_POLL is supported
        if (!(ring_params.features & IORING_FEAT_FAST_POLL))
            throw Error("IORING_FEAT_FAST_POLL not available in the kernel");

        // check if buffer selection is supported
        {
            hw2::io_uring::Probe probe(&m_ring);
        }

        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_provide_buffers(sqe, m_buffers.data(), m_buffers.buffer_size, m_buffers.buffer_count, group_id, 0);

        io_uring_submit(&m_ring);

        struct io_uring_cqe* cqe;
        io_uring_wait_cqe(&m_ring, &cqe);
        if (cqe->res < 0)
            throw Error("cqe->res = " + std::to_string(cqe->res));
        io_uring_cqe_seen(&m_ring, cqe);
    }

    struct io_uring* handle()
    {
        return &m_ring;
    }

    byte_t* buffer(std::size_t buffer_id)
    {
        return m_buffers.buffer(buffer_id);
    }

    const byte_t* buffer(std::size_t buffer_id) const
    {
        return m_buffers.buffer(buffer_id);
    }

    void accept(int fd, struct sockaddr *client_address, socklen_t client_len)
    {
        add_accept(fd, client_address, client_len, 0);
    }

    void read(int fd)
    {
        add_socket_read(fd, group_id, m_buffers.buffer_size, IOSQE_BUFFER_SELECT);
    }

    void read_n_bytes(int fd, std::size_t n)
    {

    }

    void write(int fd, __u16 bid, size_t message_size)
    {
        add_socket_write(fd, bid, message_size, 0);
    }

    void provide_buf(__u16 bid)
    {
        add_provide_buf(bid, group_id);
    }

    static constexpr std::size_t MAX_CONNECTIONS = 128;
    static constexpr std::size_t BACKLOG         = 512;
    static constexpr std::size_t MAX_MESSAGE_LEN = 32768;
    static constexpr std::size_t BUFFERS_COUNT   = MAX_CONNECTIONS;
    static constexpr int group_id = 1337;

private:
    void add_accept(int fd, struct sockaddr* client_address, socklen_t client_len, unsigned flags)
    {
        std::cerr << "add_accept enter" << std::endl;
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_accept(sqe, fd, client_address, &client_len, 0);
        io_uring_sqe_set_flags(sqe, flags);

        ConnectionInfo conn_i =
        {
            .fd = fd,
            .type = ConnectionInfo::Type::ACCEPT,
        };
        std::memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
        std::cerr << "add_accept leave" << std::endl;
    }

    void add_socket_read(int fd, unsigned gid, std::size_t message_size, unsigned flags)
    {
        std::cerr << "add_socket_read enter" << std::endl;
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_recv(sqe, fd, nullptr, message_size, 0);
        io_uring_sqe_set_flags(sqe, flags);
        sqe->buf_group = gid;

        ConnectionInfo conn_i =
        {
            .fd = fd,
            .type = ConnectionInfo::Type::READ,
        };
        std::memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
        std::cerr << "add_socket_read leave" << std::endl;
    }

    void add_socket_write(int fd, __u16 buffer_id, size_t message_size, unsigned flags)
    {
        std::cerr << "add_socket_write enter" << std::endl;
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_send(sqe, fd, m_buffers.buffer(buffer_id), message_size, 0);
        io_uring_sqe_set_flags(sqe, flags);

        ConnectionInfo conn_i =
        {
            .fd = fd,
            .type = ConnectionInfo::Type::WRITE,
            .buffer_id = buffer_id,
        };
        std::memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
        std::cerr << "add_socket_write leave" << std::endl;
    }

    void add_provide_buf(__u16 buffer_id, unsigned gid)
    {
        std::cerr << "add_provide_buf enter" << std::endl;
        struct io_uring_sqe* sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_provide_buffers(sqe, m_buffers.buffer(buffer_id), m_buffers.buffer_size, 1, gid, buffer_id);

        ConnectionInfo conn_i =
        {
            .fd = 0,
            .type = ConnectionInfo::Type::PROVIDE_BUF,
        };
        std::memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
        std::cerr << "add_provide_buf leave" << std::endl;
    }

    struct io_uring m_ring;

    BufferPool m_buffers;
};

//class HW2_WRAPPERS_EXPORT Session
//{
//public:
//    using Ptr = std::unique_ptr<Session>;

//    Session(Ring& ring, int fd)
//        : m_ring(ring)
//        , m_fd(fd)
//    {
//    }

//    enum class NextOperation
//    {
//        NONE,
//        READ,
//        WRITE,
//    };

//    NextOperation process_cqe(io_uring_cqe* cqe)
//    {
//        switch (m_previous_operation)
//        {
//        case PreviousOperation::READ:
//        {
//            int bytes_read = cqe->res;
//            if (bytes_read <= 0)
//            {
//                // handle error
//            }
//            m_read_buffer.resize(m_read_buffer.size() + bytes_read);
//            int buffer_id = cqe->flags >> 16;
//            std::memcpy(m_read_buffer.data() + m_read_buffer.size() - bytes_read, m_ring.buffer(buffer_id), bytes_read);
//            if (m_operation_size == std::size_t(-1) || m_operation_size <= m_read_buffer.size())
//            {
//                m_previous_operation = PreviousOperation::WRITE;
//                return NextOperation::WRITE;
//            }
//            m_ring.read(m_fd);
//            return NextOperation::NONE;
//        }
//        case PreviousOperation::WRITE:
//        {
//            if (m_operation_size == std::size_t(-1))
//            {
//                m_read_buffer.clear();
//            }
//            else
//            {
//                m_read_buffer.erase(m_read_buffer.begin(), m_read_buffer.begin() + m_operation_size);
//            }
//            break;
//        }
//        }
//    }

//    void queue_read()
//    {

//    }

//    void queue_read(std::size_t n)
//    {

//    }

//    void queue_write()
//    {

//    }


//private:
//    enum class PreviousOperation
//    {
//        READ,
//        WRITE,
//    };

//    Ring m_ring;
//    std::vector<byte_t> m_read_buffer;
//    std::vector<byte_t> m_write_buffer;
//    PreviousOperation m_previous_operation = PreviousOperation::READ;
//    int m_fd;
//    std::size_t m_operation_size = 0;
//};

class HW2_WRAPPERS_EXPORT SOCKS5Session
{
public:
    SOCKS5Session(Ring& ring, int fd)
        : m_ring(ring)
        , m_fd(fd)
    {
    }

    void process_request(/*std::span<byte_t> request, int fd,*/ __u16 bid, int bytes_read)
    {
        int fd = m_fd;

        byte_t* buffer = m_ring.buffer(bid);
        byte_t* output = buffer;
        switch (m_state)
        {
        case State::GREETING:
        {
            std::cerr << "> State::GREETING" << std::endl;
            if (bytes_read < 3)
                throw Error("Expected to have at least 3 bytes");
            if (buffer[0] != 0x05)
                throw Error("The first byte is expected to be 0x05!");

            std::size_t number_of_auth_methods = buffer[1];
            if (bytes_read < 2 + number_of_auth_methods)
                throw Error("Too small request");

            const byte_t* begin_of_auth_methods = buffer + 2;
            const byte_t* end_of_auth_methods = begin_of_auth_methods + number_of_auth_methods;
            static constexpr byte_t NOAUTH = 0x00;

            if (std::find(begin_of_auth_methods, end_of_auth_methods, NOAUTH) == end_of_auth_methods)
            {
                output[0] = 0x05;
                output[1] = 0xff;
            }

            m_state = State::CONNECTION_REQUEST;
            output[0] = 0x05;
            output[1] = 0x00;
            m_ring.write(fd, bid, 2);
            break;
        }
        case State::AUTHENTICATION:
        {
            std::cerr << "> State::AUTHENTICATION" << std::endl;
            // Currently not supported
            break;
        }
        case State::CONNECTION_REQUEST:
        {
            std::cerr << "> State::CONNECTION_REQUEST" << std::endl;
            const byte_t* current = buffer;

            if (bytes_read < 4)
                throw Error("Expected to have at least 4 bytes");
            if (*current != 0x05)
                throw Error("The first byte is expected to be 0x05!");

            ++current;
            const byte_t cmd = *current;
            switch (cmd)
            {
            // Establish a TCP/IP stream connection
            case 0x01:
                break;

            // Establish a TCP/IP port binding
            case 0x02:
            // Associate a UDP port
            case 0x03:
            // Unknown command
            default:
                output[0] = 0x05;  // SOCKS version
                output[0] = 0x07,  // Status: protocol error / unknown command
                output[0] = 0x00,  // Reserved
                output[0] = 0x01,  // Address type: IPv4 address
                std::memset(output, 0x00, 6);
                m_ring.write(fd, bid, 10);
                goto _end;
            }

            ++current;
            const byte_t reserved = *current;
            if (reserved != 0x00)
            {
                output[0] = 0x05;  // SOCKS version
                output[0] = 0x07,  // Status: protocol error / unknown command
                output[0] = 0x00,  // Reserved
                output[0] = 0x01,  // Address type: IPv4 address
                std::memset(output, 0x00, 6);
                m_ring.write(fd, bid, 10);
                goto _end;
            }

            // TODO: check request length

            ++current;
            const byte_t address_type = *current;
            switch (address_type)
            {
            // IPv4 address
            case 0x01:
                if (bytes_read < 4 + 4 + 2)
                    throw Error("Expected to have at least 4 + 4 + 2 bytes");
                break;

            // Domain name
            case 0x03:
            // IPv6 address
            case 0x04:
            // Unkown address type
            default:
                output[0] = 0x05;  // SOCKS version
                output[0] = 0x08,  // Status: address type not supported
                output[0] = 0x00,  // Reserved
                output[0] = 0x01,  // Address type: IPv4 address
                std::memset(output, 0x00, 6);
                m_ring.write(fd, bid, 10);
                goto _end;
            }

            ++current;
            std::memcpy(&m_address.sin_addr, current, 4);
            current += 4;
            std::memcpy(&m_address.sin_port, current, 2);

            std::cerr << "before creating SendSocket" << std::endl;
            m_socket = std::make_unique<SendSocket>(m_address);
            std::cerr << "after creating SendSocket" << std::endl;

            m_state = State::OTHER;

            output[0] = 0x05;  // SOCKS version
            output[1] = 0x00;  // Status: request granted
            output[2] = 0x00;  // Reserved
            output[3] = 0x01;  // Address type: IPv4 address
            std::memcpy(output + 4, &m_address.sin_addr, 4);
            std::memcpy(output + 8, &m_address.sin_port, 2);
            m_ring.write(fd, bid, 10);
            break;
        }
        case State::OTHER:
        {
            std::cerr << "> State::OTHER" << std::endl;
            m_socket->send({buffer, static_cast<std::size_t>(bytes_read)});
//            std::vector<byte_t> reply(2048);
//            m_socket->recv(std::span<byte_t>{ reply.data(), reply.data() + reply.size() });
            ssize_t len = m_socket->recv(std::span<byte_t>{ output, Ring::MAX_MESSAGE_LEN });
            m_ring.write(fd, bid, len);
        }
        }
        _end: ;
    }

private:    
    enum class State
    {
        GREETING,
        AUTHENTICATION,
        CONNECTION_REQUEST,
        OTHER,
    };

    State m_state = State::GREETING;

    sockaddr_in m_address;
    std::unique_ptr<SendSocket> m_socket;

    Ring& m_ring;
    int m_fd;
};

}  // namespace hw2::io_uring

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_IO_URING_HPP_
