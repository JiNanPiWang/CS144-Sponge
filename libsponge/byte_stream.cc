#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : capacity_(capacity) {}

size_t ByteStream::write(const string &data) {
    if (data.empty() || input_ended())
        return 0;
    if ( data.size() > this->remaining_capacity() )
    {
        auto write_num = this->remaining_capacity();
        this->cumulatively_bytes_writen += this->remaining_capacity();
        this->str += data.substr( 0, this->remaining_capacity() );
        return write_num;
    }
    else
    {
        this->cumulatively_bytes_writen += data.size();
        this->str += data;
        return data.size();
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (buffer_size() < len)
    {
        return str;
    }
    else
    {
        return str.substr(0, len);
    }
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    cumulatively_bytes_popped += min(str.size(), len);
    str = str.substr(len);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto s = str;
    if (buffer_size() <= len)
    {
        pop_output(buffer_size());
        return s;
    }
    else
    {
        pop_output(len);
        return s.substr(0, len);
    }
}

void ByteStream::end_input() {
    has_closed = true;
}

bool ByteStream::input_ended() const {
    return has_closed;
}

size_t ByteStream::buffer_size() const {
    return str.size();
}

bool ByteStream::buffer_empty() const {
    return str.empty();
}

bool ByteStream::eof() const {
    return input_ended() && buffer_empty();
}

size_t ByteStream::bytes_written() const {
    return cumulatively_bytes_writen;
}

size_t ByteStream::bytes_read() const {
    return cumulatively_bytes_popped;
}

size_t ByteStream::remaining_capacity() const {
    return capacity_ - buffer_size();
}
