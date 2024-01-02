#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _RTO(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // haven't connected
    if (!SYN_SENT()) {
        _flag_SYN = true;
        TCPSegment seg;
        seg.header().syn = 1;
        seg.header().seqno = next_seqno();
        send_segment(seg);
        return;
    }
    // wait until connected
    if (!_segments_outstanding.empty() && _segments_outstanding.front().header().syn)
        return;
    // _flag_FIN means FIN has been sent, return
    if (FIN_SENT())
        return;
    // already connected
    while (!FIN_SENT() && !_stream.buffer_empty() && swnd_remain_space()) {
        TCPSegment seg;
        seg.header().seqno = next_seqno();
        size_t payload_size = min({TCPConfig::MAX_PAYLOAD_SIZE, swnd_remain_space(), _stream.buffer_size()});
        string &&payload_str = _stream.read(payload_size);
        seg.payload() = Buffer{std::move(payload_str)};
        // judge whether to send FIN
        // which means may miss a FIN when _stream.eof() && swnd_remain_space() == payload_size
        if (_stream.eof() && (swnd_remain_space() > payload_size)) {
            _flag_FIN = true;
            seg.header().fin = 1;
        }
        send_segment(seg);
    }
    // send possibly misssed FIN when there has swnd space for it
    if (!FIN_SENT() && _stream.eof() && swnd_remain_space()) {
        _flag_FIN = true;
        TCPSegment seg;
        seg.header().seqno = next_seqno();
        seg.header().fin = 1;
        send_segment(seg);
    }
    return;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // what if window_size<new(_bytes_in_flight)??????
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    // there was once a bug
    // TCPSenderTestHarness test{"Impossible ackno (beyond next seqno) is ignored", cfg};
    if (abs_ackno > _next_seqno)
        return;
    while (!_segments_outstanding.empty()) {
        TCPSegment &front_seg = _segments_outstanding.front();
        uint64_t abs_seqno = unwrap(front_seg.header().seqno, _isn, _next_seqno);
        // not new ack
        if (abs_ackno < abs_seqno + front_seg.length_in_sequence_space())
            break;
        // new ack
        _bytes_in_flight -= front_seg.length_in_sequence_space();
        _segments_outstanding.pop();
        _RTO = _initial_retransmission_timeout;
        if (!_segments_outstanding.empty())
            _timer.restart(_RTO);
        else
            _timer.stop();
        _consecutive_retransmissions = 0;
    }
    _flag_rwnd_zero = (window_size == 0);
    _swnd = max(window_size, uint16_t{1});
    if (swnd_remain_space())
        fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.running())
        return;
    _timer.elapse(ms_since_last_tick);
    if (_timer.expired()) {
        _segments_out.push(_segments_outstanding.front());
        if (!_flag_rwnd_zero) {
            ++_consecutive_retransmissions;
            _RTO <<= 1;
        }
        _timer.restart(_RTO);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}

void TCPSender::send_segment(TCPSegment &seg) {
    _segments_out.push(seg);
    _segments_outstanding.push(seg);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    if (!_timer.running())
        _timer.start(_RTO);
}

// there was once a bug, in case: _swnd < _bytes_in_flight
size_t TCPSender::swnd_remain_space() { return (_swnd >= _bytes_in_flight) ? _swnd - _bytes_in_flight : 0; }