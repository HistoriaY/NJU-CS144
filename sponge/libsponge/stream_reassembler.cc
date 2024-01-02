#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _unassembled_bytes(capacity), _byte_stored(capacity, false), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const Index index, const bool eof) {
    Index index_bound = _expect_index + _capacity - _output.buffer_size();
    // omit bytes that already assembled
    Index begin = max(index, _expect_index);
    // discard whole data
    if (begin >= index_bound)
        return;
    // omit bytes that out of bound
    Index end = min(index_bound, index + data.length());
    // discard whole data
    if (begin > end)
        return;
    // judge whether last byte enter _unassembled_bytes
    if (eof && end == index + data.length()) {
        _flag_eof = true;
        _eof_index = end;
    }
    // push [begin,end) into _unassembled_bytes
    _size_unassembled_bytes += push_unassembled_bytes(data, index, begin, end);
    // assemble possible newly contiguous substring
    _size_unassembled_bytes -= push_output();

    if (_flag_eof && empty())
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _size_unassembled_bytes; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }

size_t StreamReassembler::push_unassembled_bytes(const std::string &data, Index index, Index begin, Index end) {
    size_t count = 0;
    for (Index cur_index = begin; cur_index < end; ++cur_index) {
        if (!_byte_stored[cur_index % _capacity]) {
            _unassembled_bytes[cur_index % _capacity] = data[cur_index - index];
            _byte_stored[cur_index % _capacity] = true;
            ++count;
        }
    }
    return count;
}

size_t StreamReassembler::push_output() {
    size_t count = 0;
    if (_byte_stored[_expect_index % _capacity]) {
        size_t i = _expect_index % _capacity;
        string write_str = "";
        while (_byte_stored[i % _capacity]) {
            write_str.push_back(_unassembled_bytes[i % _capacity]);
            _byte_stored[i % _capacity] = false;
            ++count;
            ++i;
        }
        _output.write(write_str);
    }
    _expect_index += count;
    return count;
}