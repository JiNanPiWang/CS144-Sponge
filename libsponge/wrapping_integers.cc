#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // 自然溢出，相当于取模
    return WrappingInt32 { isn + n };
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t max2_32 = static_cast<uint64_t>(1) << 32;
    uint64_t pos_0 = n.raw_value() - isn.raw_value();

    // 找离checkpoint最近的，可以加n个1<<32
    auto nums = checkpoint / max2_32;
    checkpoint %= max2_32;

    // n或n+1，checkpoint处于(n - 1) * max2_32到n * max2_32到(n + 1) * max2_32之间
    if ((checkpoint > pos_0) && (checkpoint - pos_0 > max2_32 / 2))
        return pos_0 + (nums + 1) * max2_32;
    else if ((pos_0 > checkpoint) && (pos_0 - checkpoint > max2_32 / 2) && (nums > 0))
        return pos_0 + (nums - 1) * max2_32;
    return pos_0 + nums * max2_32;
}
