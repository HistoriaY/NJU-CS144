#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

class Timer {
  private:
    uint32_t _accumulation_time = {0};
    uint32_t _timeout = {0};
    bool _flag_running = {false};

  public:
    Timer(){};
    bool expired() const { return _flag_running && _accumulation_time >= _timeout; }
    bool running() const { return _flag_running; }
    void elapse(uint32_t interval) {
        if (_flag_running)
            _accumulation_time += interval;
    }
    void set_timeout(uint32_t timeout) { _timeout = timeout; }
    void start(uint32_t timeout) {
        _flag_running = true;
        _accumulation_time = 0;
        set_timeout(timeout);
    }
    void restart(uint32_t timeout) { start(timeout); }
    void stop() { _flag_running = false; }
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    // begin new added private member
    std::queue<TCPSegment> _segments_outstanding{};
    Timer _timer{};
    unsigned int _RTO;
    unsigned int _consecutive_retransmissions{0};
    uint16_t _swnd{1};  // sender window
    size_t _bytes_in_flight{0};
    bool _flag_SYN{false};        // whether SYN has been sent
    bool _flag_FIN{false};        // whether FIN has been sent
    bool _flag_rwnd_zero{false};  // whether rwnd == 0
    // send a segment that has been loaded(seqno,[syn],[fin],payload)
    void send_segment(TCPSegment &seg);
    size_t swnd_remain_space();
    // end new added private member
  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

    bool SYN_SENT() const { return _flag_SYN; }
    bool FIN_SENT() const { return _flag_FIN; }
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
