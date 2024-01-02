#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    // haven't connected
    if (!SYN_RCVD()) {
        // connect failure
        if (!header.syn)
            return;
        // create connection
        else {
            _flag_SYN = true;
            _isn = header.seqno;
            bool this_seg_eof = false;
            // close connection
            if (header.fin)
                _flag_FIN = this_seg_eof = true;
            string data = seg.payload().copy();
            _reassembler.push_substring(data, 0, this_seg_eof);
        }
    }
    // connection already exists
    else {
        // create connection twice
        if (header.syn)
            return;

        bool this_seg_eof = false;
        // close connection
        if (header.fin)
            _flag_FIN = this_seg_eof = true;
        // reassemble data
        string data = seg.payload().copy();
        uint64_t checkpoint = _reassembler.expect_index();
        uint64_t abs_seqno = unwrap(header.seqno, _isn, checkpoint);
        Index index = abs_seqno - 1;
        _reassembler.push_substring(data, index, this_seg_eof);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    // haven't connected
    if (!SYN_RCVD())
        return {};
    // connection already exists
    // there was once a bug (_flag_FIN && _reassembler.stream_out().input_ended()) ? 1 : 0 must be enclosed by ()!!!
    return wrap(1 + _reassembler.expect_index() + ((_flag_FIN && _reassembler.stream_out().input_ended()) ? 1 : 0),
                _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }