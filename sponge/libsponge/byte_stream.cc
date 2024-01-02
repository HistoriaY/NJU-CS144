#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t write_len = min(remaining_capacity(), data.length());
    _buffer.append(BufferList(move(string{data.begin(), data.begin() + write_len})));
    _total_written += write_len;
    return write_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_len = min(buffer_size(), len);
    string &&total_str = _buffer.concatenate();
    return string{total_str.begin(), total_str.begin() + peek_len};
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_len = min(buffer_size(), len);
    _buffer.remove_prefix(pop_len);
    _total_read += pop_len;
    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // check len legality in peek_* and pop_* omit redundant check
    string &&read_str = peek_output(len);
    pop_output(len);
    return read_str;
}

void ByteStream::end_input() { _flag_input_ended = true; }

bool ByteStream::input_ended() const { return _flag_input_ended; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _total_written; }

size_t ByteStream::bytes_read() const { return _total_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }