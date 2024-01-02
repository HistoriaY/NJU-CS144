#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t isn_64 = isn.raw_value();
    return WrappingInt32{static_cast<uint32_t>(isn_64 + n)};
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
    const uint64_t mod = static_cast<uint64_t>(1) << 32;
    uint64_t offset = (n - isn >= 0) ? uint64_t(n - isn) : uint64_t(n - isn + mod);
    uint64_t right_abs_seqno = 0;
    uint64_t left_abs_seqno = 0;

    if (offset >= checkpoint)
        return offset;

    right_abs_seqno = offset;
    // fastly set value to approximate checkpoint
    right_abs_seqno |= ((checkpoint >> 32) << 32);
    if (right_abs_seqno <= checkpoint)
        right_abs_seqno += mod;
    left_abs_seqno = right_abs_seqno - mod;
    return (checkpoint - left_abs_seqno < right_abs_seqno - checkpoint) ? left_abs_seqno : right_abs_seqno;
}