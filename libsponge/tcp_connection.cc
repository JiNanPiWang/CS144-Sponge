#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return {}; }

size_t TCPConnection::bytes_in_flight() const { return {}; }

size_t TCPConnection::unassembled_bytes() const { return {}; }

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (seg.header().syn)
        _sender.change_status(TCPStatus::SYN_RCVD);
    if (seg.header().ack)
    {
        if (_sender.get_status() == TCPStatus::FIN_WAIT_1)
            _sender.change_status(TCPStatus::FIN_WAIT_2);
    }
    if (seg.header().fin)
    {
        // TODO: 被动关闭待补充
        if (_sender.get_status() == TCPStatus::FIN_WAIT_2)
            _sender.change_status(TCPStatus::CLOSING);
    }
    _sender.ack_received(seg.header().ackno, seg.header().win);
    _receiver.segment_received(seg);
    _sender.fill_window();

    // 接收完对方发的消息，我们就发一条确认信息
    auto to_send_seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    // 如果对方是syn，我们就ack一下
    if (seg.header().syn)
        to_send_seg.header().ack = true;
    to_send_seg.header().ackno = _receiver.ackno().value();

    _segments_out.push(to_send_seg);
}

bool TCPConnection::active() const {
    auto sta = _sender.get_status();
    return sta != TCPStatus::CLOSED;
}

size_t TCPConnection::write(const string &data) {
    DUMMY_CODE(data);
    return {};
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    auto x =  this->state();
    if (_sender.get_retrans_timer() >= _cfg.rt_timeout * 10)
    {
        _linger_after_streams_finish = false;
        _sender.change_status(TCPStatus::CLOSED);
    }
}

// close的时候调用它
void TCPConnection::end_input_stream() {
    // 我发送了就是FIN_WAIT_1，其他的等接收了再说
    _sender.change_status(TCPStatus::FIN_WAIT_1);
    _sender.fill_window();

    // connection需要整合sender和receiver的内容，具体看lab4 pdf的Fig1
    auto seg_to_send = _sender.segments_out().front();
    seg_to_send.header().ackno = this->_receiver.ackno().value();
    seg_to_send.header().win = this->_receiver.window_size();

    _segments_out.push(seg_to_send);
    _sender.segments_out().pop();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
