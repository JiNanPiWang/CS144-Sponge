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
            _sender.change_status(TCPStatus::ESTABLISHED);
        else if (seg.header().syn && !seg.header().ack) // 对方只发SYN
            _sender.change_status(TCPStatus::SYN_RCVD);
        else if (seg.header().syn && seg.header().ack) // 对方发了SYN+ACK
            _sender.change_status(TCPStatus::SYN_ACK_RCVD);
    }
    uint64_t now_ack = _sender.unwrap_seq_num(_sender.get_ackno());
    if (only_ack && unwrap(seg.header().ackno, _sender.get_isn(), now_ack) < now_ack)
    {
        // 如果一个只有ACK的信息且ACK号小于当前ACK，就不算
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
        else if (_sender.get_status() == TCPStatus::SYN_SENT && only_ack)
            _sender.change_status(TCPStatus::ESTABLISHED);
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
    _sender.ack_received_with_state(seg.header().ackno, seg.header().win);
    _receiver.segment_received(seg);

    // 如果对方发的内容只有ACK，我们不回应，但是我们要发东西除外
    if (!only_ack || !_sender.stream_in().buffer_empty())
        send_front_seg();
}

bool TCPConnection::active() const {
    if (_sender.get_status() == TCPStatus::CLOSED || _sender.get_status() == TCPStatus::RESET)
        return false;
    return true;
}

size_t TCPConnection::write(const string &data) {
    auto write_num = _sender.stream_in().write(data);
    send_front_seg();
    return write_num;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    last_segment_received_time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
    {
        // 我们需要直接发送RST，不能去队列最前面内容发送，因为是强行停止，队列里面还有没发的内容
        make_send_RST();
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _sender.change_status(TCPStatus::RESET);
        return;
    }

    send_front_seg(true); // tick如果要重传，就把内容放入_segment...，我们直接发这个，不用去push
    if (time_since_last_segment_received() >= _cfg.rt_timeout * 10 &&
        _sender.stream_in().eof() &&
        _receiver.stream_out().eof())
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
            make_send_RST();
            _sender.change_status(TCPStatus::RESET);
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_front_seg(bool without_fill_window) {
    if (!without_fill_window) // 参数详见TCPConnection::tick
        _sender.fill_window_with_state();
    while (!_sender.segments_out().empty())
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

void TCPConnection::make_send_RST() {
    TCPSenderMessage to_trans { _sender.next_seqno(), false, "", false, false };
    to_trans.RST = true;
    auto to_send_seg = to_trans.to_TCPSeg();
    if (_receiver.ackno().has_value())
        to_send_seg.header().ackno = _receiver.ackno().value();
    to_send_seg.header().win = _receiver.window_size();
    _segments_out.push(to_send_seg);
}
