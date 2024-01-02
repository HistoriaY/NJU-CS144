#include "tcp_connection.hh"

#include <iostream>
#include <limits>
// Dummy implementation of a TCP connection

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // rst bit
    if (seg.header().rst) {
        // LISTEN -> ignore rst
        if (LISTEN())
            return;
        // unclean shutdown
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }
    // update _time_since_last_segment_received
    _time_since_last_segment_received = 0;
    // give the segment to the TCPReceiver
    _receiver.segment_received(seg);
    // condition of _linger_after_streams_finish
    if (inbound_ended() && !_sender.stream_in().eof())
        _linger_after_streams_finish = false;
    // ack bit
    bool send_one = false;
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        send_one = fill_segments_out();
    }
    // incoming segment occupies sequence numbers
    if (seg.length_in_sequence_space() > 0 && !send_one) {
        _sender.fill_window();  // send possibly SYN
        send_one = fill_segments_out();
        // at least one segment is sent in reply
        if (!send_one) {
            _sender.send_empty_segment();
            fill_segments_out();
        }
    }
}

bool TCPConnection::active() const {
    // unclean shutdown
    if (_sender.stream_in().error() || _receiver.stream_out().error())
        return false;
    // clean shutdown
    bool checked = inbound_ended() && outbound_ended();
    if (checked) {
        if (!_linger_after_streams_finish)
            return false;
        else {
            if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout)
                return false;
            else
                return true;
        }
    }
    return true;
}

size_t TCPConnection::write(const string &data) {
    size_t write_len = _sender.stream_in().write(data);
    _sender.fill_window();
    fill_segments_out();
    return write_len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    // tell the TCPSender the passage of time
    _sender.tick(ms_since_last_tick);
    // abort the connection, and send a reset segment to the peer (an empty segment with the rst flag set),
    // if the number of consecutive retransmissions is more than an upper limit TCPConfig::MAX_RETX_ATTEMPTS
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        send_rst_seg();  // missed retransmission seg, instead, an empty seg with RST=1
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }
    fill_segments_out();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();  // automatically send FIN
    fill_segments_out();
}

void TCPConnection::connect() {
    _sender.fill_window();  // automatically send SYN
    fill_segments_out();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_rst_seg();
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::set_seg_ack_win(TCPSegment &seg) {
    optional<WrappingInt32> ackno = _receiver.ackno();
    if (ackno.has_value()) {
        seg.header().ack = 1;
        seg.header().ackno = ackno.value();
    }
    seg.header().win =
        static_cast<uint16_t>(min(_receiver.window_size(), static_cast<size_t>(numeric_limits<uint16_t>::max())));
}

bool TCPConnection::fill_segments_out() {
    bool send_one = false;
    while (!_sender.segments_out().empty()) {
        send_one = true;
        TCPSegment seg{std::move(_sender.segments_out().front())};
        _sender.segments_out().pop();
        set_seg_ack_win(seg);
        _segments_out.push(std::move(seg));
    }
    return send_one;
}

bool TCPConnection::inbound_ended() const { return _receiver.stream_out().input_ended(); }

bool TCPConnection::outbound_ended() const {
    return _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
           _sender.bytes_in_flight() == 0;
}

void TCPConnection::send_rst_seg() {
    _sender.send_empty_segment();
    TCPSegment RSTSeg{std::move(_sender.segments_out().front())};
    _sender.segments_out().pop();
    RSTSeg.header().rst = 1;
    _segments_out.push(std::move(RSTSeg));
}