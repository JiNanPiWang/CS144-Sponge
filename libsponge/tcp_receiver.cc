#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().rst)
        _reassembler.stream_out().set_error();
    if (seg.header().syn)
        ISN = seg.header().seqno;
    else if (!ackno_base.has_value())
        return;
    // 应该是seqno位置-当前位置 > this->reassembler_.writer().available_capacity(), UINT16_MAX
    // 但是比较难实现，下面是无奈之举，简单判断是不是在ISN之前
    else if (unwrap(seg.header().seqno, ISN, absolute_seqno) <=
             unwrap(ISN, ISN, absolute_seqno))
        return;
    _reassembler.push_substring(
        seg.payload().copy(),
        unwrap( seg.header().seqno, ISN, absolute_seqno ) - 1 + seg.header().syn,
        seg.header().fin );
    absolute_seqno += seg.payload().size() + seg.header().syn + seg.header().fin;
    ackno_base = ackno_base.value_or(ISN) + seg.header().syn;
    if (seg.header().fin) // FIN代表当前payload是end of the stream
        fin = true;
    if (fin && _reassembler.empty())
        _reassembler.stream_out().input_ended();
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    // ackno: reassembler_.writer().bytes_pushed()，写成功了了几个，ackno就是几，再加ISN等
    auto pushedB = _reassembler.stream_out().bytes_written();
    if (_reassembler.stream_out().input_ended()) // ackno加上最后的FIN
        pushedB++;
    return ackno_base.has_value() ? optional<WrappingInt32>( ackno_base.value() + pushedB) : nullopt;
}

size_t TCPReceiver::window_size() const {
    return _reassembler.stream_out().remaining_capacity();
}
