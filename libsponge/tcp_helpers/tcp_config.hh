#ifndef SPONGE_LIBSPONGE_TCP_CONFIG_HH
#define SPONGE_LIBSPONGE_TCP_CONFIG_HH

#include "address.hh"
#include "wrapping_integers.hh"
#include "tcp_segment.hh"

#include <cstddef>
#include <cstdint>
#include <optional>

//! Config for TCP sender and receiver
class TCPConfig {
  public:
    static constexpr size_t DEFAULT_CAPACITY = 64000;  //!< Default capacity
    static constexpr size_t MAX_PAYLOAD_SIZE = 1000;   //!< Conservative max payload size for real Internet
    static constexpr uint16_t TIMEOUT_DFLT = 1000;     //!< Default re-transmit timeout is 1 second
    static constexpr unsigned MAX_RETX_ATTEMPTS = 8;   //!< Maximum re-transmit attempts before giving up

    uint16_t rt_timeout = TIMEOUT_DFLT;       //!< Initial value of the retransmission timeout, in milliseconds
    size_t recv_capacity = DEFAULT_CAPACITY;  //!< Receive capacity, in bytes
    size_t send_capacity = DEFAULT_CAPACITY;  //!< Sender capacity, in bytes
    std::optional<WrappingInt32> fixed_isn{};
};

//! Config for classes derived from FdAdapter
class FdAdapterConfig {
  public:
    Address source{"0", 0};       //!< Source address and port
    Address destination{"0", 0};  //!< Destination address and port

    uint16_t loss_rate_dn = 0;  //!< Downlink loss rate (for LossyFdAdapter)
    uint16_t loss_rate_up = 0;  //!< Uplink loss rate (for LossyFdAdapter)
};

struct TCPSenderMessage
{
    WrappingInt32 seqno { 0 };
    bool SYN {};
    std::string payload {};
    bool FIN {};
    bool RST {};
    bool ACK {false};
    // How many sequence numbers does this segment use?
    size_t sequence_length() const { return SYN + payload.size() + FIN; }

    TCPSegment to_TCPSeg() const
    {
        TCPSegment t_seg;
        TCPHeader head;

        head.seqno = seqno;
        head.syn = SYN;
        head.fin = FIN;
        head.rst = RST;
        head.ack = ACK;

        t_seg.header() = head;
        auto s = payload;
        t_seg.payload() = Buffer{std::move(s)};
        return t_seg;
    }
};

//! \brief Official state names from the [TCP](\ref rfc::rfc793) specification
enum class TCPStatus {
    LISTEN = 0,   //!< Listening for a peer to connect
    SYN_RCVD,     //!< Got the peer's SYN
    SYN_SENT,     //!< Sent a SYN to initiate a connection
    ESTABLISHED,  //!< Three-way handshake complete
    CLOSE_WAIT,   //!< Remote side has sent a FIN, connection is half-open，对方发了FIN，现在我们发ACK
    LAST_ACK,     //!< Local side sent a FIN from CLOSE_WAIT, waiting for ACK，对方已经发来了FIN，我们发过ACK，现在我们发了FIN，等ACK
    FIN_WAIT_1,   //!< Sent a FIN to the remote side, not yet ACK'd
    FIN_WAIT_2,   //!< Received an ACK for previously-sent FIN
    CLOSING,      //!< Received a FIN just after we sent one，我们发过了FIN，对方现在发FIN了，我们还要发ACK
    TIME_WAIT,    //!< Both sides have sent FIN and ACK'd, waiting for 2 MSL
    CLOSED,       //!< A connection that has terminated normally
    RESET,        //!< A connection that terminated abnormally
};

#endif  // SPONGE_LIBSPONGE_TCP_CONFIG_HH
