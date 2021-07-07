#ifndef HW2_SOCKS5_SERVER_WRAPPERS_IO_URING_HPP_
#define HW2_SOCKS5_SERVER_WRAPPERS_IO_URING_HPP_

#include <hw2-wrappers/export.h>

#include <liburing.h>

#include <cstring>
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
    __u16 bid;
};

static_assert (sizeof (ConnectionInfo) == sizeof (__u64), "");

class HW2_WRAPPERS_EXPORT Ring
{
public:
    Ring(unsigned entries)
        : m_buffers(BUFFERS_COUNT * MAX_MESSAGE_LEN)
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
        hw2::io_uring::Probe probe(&m_ring);

        m_sqe = io_uring_get_sqe(&m_ring);
        io_uring_prep_provide_buffers(m_sqe, m_buffers.data(), MAX_MESSAGE_LEN, BUFFERS_COUNT, group_id, 0);

        io_uring_submit(&m_ring);
        io_uring_wait_cqe(&m_ring, &m_cqe);
        if (m_cqe->res < 0)
            throw Error("cqe->res = " + std::to_string(m_cqe->res));
        io_uring_cqe_seen(&m_ring, m_cqe);
    }

    struct io_uring* handle()
    {
        return &m_ring;
    }

    void accept(int fd, struct sockaddr *client_address, socklen_t client_len)
    {
        add_accept(&m_ring, fd, client_address, client_len, 0);
    }

    void read(int fd)
    {
        add_socket_read(&m_ring, fd, group_id, MAX_MESSAGE_LEN, IOSQE_BUFFER_SELECT);
    }

    void write(int fd, __u16 bid, size_t message_size)
    {
        add_socket_write(&m_ring, fd, bid, message_size, 0);
    }

    void provide_buf(__u16 bid)
    {
        add_provide_buf(&m_ring, bid, group_id);
    }

private:
    void add_accept(struct io_uring* ring, int fd, struct sockaddr *client_address, socklen_t client_len, unsigned flags)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        io_uring_prep_accept(sqe, fd, client_address, &client_len, 0);
        io_uring_sqe_set_flags(sqe, flags);

        ConnectionInfo conn_i =
        {
            .fd = fd,
            .type = ConnectionInfo::Type::ACCEPT,
        };
        memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
    }

    void add_socket_read(struct io_uring* ring, int fd, unsigned gid, std::size_t message_size, unsigned flags)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        io_uring_prep_recv(sqe, fd, NULL, message_size, 0);
        io_uring_sqe_set_flags(sqe, flags);
        sqe->buf_group = gid;

        ConnectionInfo conn_i =
        {
            .fd = fd,
            .type = ConnectionInfo::Type::READ,
        };
        memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
    }

    void add_socket_write(struct io_uring* ring, int fd, __u16 bid, size_t message_size, unsigned flags)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        io_uring_prep_send(sqe, fd, m_buffers.data() + MAX_MESSAGE_LEN * bid, message_size, 0);
        io_uring_sqe_set_flags(sqe, flags);

        ConnectionInfo conn_i =
        {
            .fd = fd,
            .type = ConnectionInfo::Type::WRITE,
            .bid = bid,
        };
        memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
    }

    void add_provide_buf(struct io_uring* ring, __u16 bid, unsigned gid)
    {
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        io_uring_prep_provide_buffers(sqe, m_buffers.data() + MAX_MESSAGE_LEN * bid, MAX_MESSAGE_LEN, 1, gid, bid);

        ConnectionInfo conn_i =
        {
            .fd = 0,
            .type = ConnectionInfo::Type::PROVIDE_BUF,
        };
        memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
    }

    struct io_uring m_ring;
    struct io_uring_sqe* m_sqe;
    struct io_uring_cqe* m_cqe;

    static constexpr std::size_t MAX_CONNECTIONS = 4096;
    static constexpr std::size_t BACKLOG         = 512;
    static constexpr std::size_t MAX_MESSAGE_LEN = 2048;
    static constexpr std::size_t BUFFERS_COUNT   = MAX_CONNECTIONS;
    static constexpr int group_id = 1337;

    std::vector<char> m_buffers;
};

}  // namespace hw2::io_uring

#endif  // HW2_SOCKS5_SERVER_WRAPPERS_IO_URING_HPP_
