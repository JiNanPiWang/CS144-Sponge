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
    auto write_num = min(data.size(), this->remaining_capacity());
    this->cumulatively_bytes_writen += write_num;
    this->str_vec.emplace_back(data.substr( 0, write_num)) ;
    return write_num;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ans;
    if (str_vec.empty())
        return ans;
    if (buffer_size() <= len)
    {
        for (auto &x : str_vec)
            ans += x;
        return ans;
    }
    size_t pop_len = 0;
    size_t pop_pos = 0;
    while (pop_pos < str_vec.size() && pop_len + str_vec[pop_pos].size() < len)
    {
        pop_len += str_vec[pop_pos].size();
        ans += str_vec[pop_pos];
        pop_pos++;
    }
    ans += str_vec[0].substr(len - pop_len);
    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (str_vec.empty())
        return;
    if (buffer_size() <= len)
    {
        cumulatively_bytes_popped += buffer_size();
        str_vec.clear();
        return;
    }
    size_t pop_len = 0;
    size_t pop_pos = 0;
    while (pop_pos < str_vec.size() && pop_len + str_vec[pop_pos].size() < len)
    {
        pop_len += str_vec[pop_pos].size();
        ++pop_pos;
    }
    str_vec.erase(str_vec.begin(), str_vec.begin() + pop_pos);
    str_vec[0] = str_vec[0].substr(len - pop_len);
    cumulatively_bytes_popped += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans;
    if (str_vec.empty())
        return ans;
    if (buffer_size() <= len)
    {
        for (auto &x : str_vec)
            ans += x;
        cumulatively_bytes_popped += buffer_size();
        str_vec.clear();
        return ans;
    }
    size_t pop_len = 0;
    size_t pop_pos = 0;
    while (pop_pos < str_vec.size() && pop_len + str_vec[pop_pos].size() < len)
    {
        pop_len += str_vec[pop_pos].size();
        ans += str_vec[pop_pos];
        pop_pos++;
    }
    str_vec.erase(str_vec.begin(), str_vec.begin() + pop_pos);
    ans += str_vec[0].substr(len - pop_len);
    str_vec[0] = str_vec[0].substr(len - pop_len);
    cumulatively_bytes_popped += len;
    return ans;
}

void ByteStream::end_input() {
    has_closed = true;
}

bool ByteStream::input_ended() const {
    return has_closed;
}

size_t ByteStream::buffer_size() const {
    size_t sum = 0;
    for (auto &x : str_vec)
        sum += x.size();
    return sum;
}

bool ByteStream::buffer_empty() const {
    return str_vec.empty();
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
