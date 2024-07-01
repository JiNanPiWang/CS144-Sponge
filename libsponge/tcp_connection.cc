#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().buffer_size();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return last_segment_received_time;
}

void TCPConnection::segment_received(const TCPSegment &seg) {

    if (seg.header().rst)
    {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _linger_after_streams_finish = false;
        _sender.change_status(TCPStatus::RESET);
        return;
    }

    bool only_ack = false;
    if (seg.header().fin + seg.header().syn + seg.header().urg + seg.header().psh + seg.header().rst == 0 &&
        seg.payload().size() == 0 &&
        seg.header().ack == 1)
        only_ack = true;

    last_segment_received_time = 0;
    if (seg.header().syn)
    {
        if (_sender.get_status() == TCPStatus::SYN_SENT && !seg.header().ack) // 我们发了SYN，它没收到，然后它发了一个SYN
            _sender.change_status(TCPStatus::ESTABLISHED_ACK);
        else if (!seg.header().ack) // 只发SYN
            _sender.change_status(TCPStatus::SYN_RCVD);
        else // 发了SYN+ACK
            _sender.change_status(TCPStatus::SYN_ACK_RCVD);
    }
    if (only_ack && seg.header().ackno != _sender.next_seqno())
    {
        // 如果一个只有ACK的信息且ACK号错误，就不算
        return;
    }
    if (seg.header().ack && seg.header().ackno == _sender.next_seqno())
    {
        // 如果对方没有接收到我们的FIN，那么就不能变成FIN_WAIT_2
        if (_sender.get_status() == TCPStatus::FIN_WAIT_1)
        {
            _sender.change_status(TCPStatus::FIN_WAIT_2);
        }
        else if (_sender.get_status() == TCPStatus::LAST_ACK)  // 对方关闭，我们已发送FIN，现在接到ACK
        {
            if (!is_active_close) // 被动关闭
                _sender.change_status(TCPStatus::CLOSED);
            else
                _sender.change_status(TCPStatus::TIME_WAIT);
        }
    }
    if (seg.header().fin)
    {
        if (_sender.get_status() == TCPStatus::FIN_WAIT_2) // 我们发送了
            _sender.change_status(TCPStatus::CLOSING);
        else if (_sender.get_status() != TCPStatus::TIME_WAIT)  // 我们没发送，对面发了FIN
        {
            _sender.change_status(TCPStatus::CLOSE_WAIT);
            if (!is_active_close)
                _linger_after_streams_finish = false; // 被动关闭
        }
        else if (_sender.get_status() == TCPStatus::TIME_WAIT)
            _sender.change_status(TCPStatus::CLOSING);
    }
    if (!only_ack && _sender.get_status() == TCPStatus::ESTABLISHED)
        _sender.change_status(TCPStatus::ESTABLISHED_ACK);
    _sender.ack_received(seg.header().ackno, seg.header().win);
    _receiver.segment_received(seg);

    if (!only_ack) // 如果对方发的内容只有ACK，我们不回应
        send_front_seg();
}

bool TCPConnection::active() const {
    if (_sender.get_status() == TCPStatus::CLOSED || _sender.get_status() == TCPStatus::RESET)
        return false;
    return true;
}

size_t TCPConnection::write(const string &data) {
    return _sender.stream_in().write(data);
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    last_segment_received_time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    send_front_seg(true);
    if (time_since_last_segment_received() >= _cfg.rt_timeout * 10)
    {
        _linger_after_streams_finish = false;
        _sender.change_status(TCPStatus::CLOSED);
    }
}

// close的时候调用它
void TCPConnection::end_input_stream() {
    // 我发送了就是FIN_WAIT_1，其他的等接收了再说
    if (_sender.get_status() == TCPStatus::CLOSE_WAIT) // 对方发过FIN，我们发ACK了，现在我们要FIN，这是被动关闭
    {
        _sender.change_status(TCPStatus::LAST_ACK);
        _linger_after_streams_finish = false; // 对方先发FIN，就是被动关闭
    }
    else // 对方没FIN，我们FIN
    {
        _sender.change_status(TCPStatus::FIN_WAIT_1);
        is_active_close = true;
    }

    send_front_seg();
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    send_front_seg();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            _sender.change_status(TCPStatus::RESET);
            send_front_seg();
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_front_seg(bool without_fill_window) {
    if (!without_fill_window)
        _sender.fill_window();
    if (!_sender.segments_out().empty())
    {
        auto to_send_seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        // 发送的内容加入receiver
        if (_receiver.ackno().has_value())
            to_send_seg.header().ackno = _receiver.ackno().value();
        to_send_seg.header().win = _receiver.window_size();

        _segments_out.push(to_send_seg);
    }
}
