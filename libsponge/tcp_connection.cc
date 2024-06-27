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
    _sender.ack_received(seg.header().ackno, seg.header().win);
    _receiver.segment_received(seg);
    _sender.fill_window();

    auto s = TCPState::state_summary(_sender);

    // 接收完对方发的消息，我们就发一条确认信息
    if (!_sender.segments_out().empty())
    {
        auto to_send_seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        // 如果对方是syn，我们就ack一下
        if (seg.header().syn)
            to_send_seg.header().ack = true;
        to_send_seg.header().ackno = _receiver.ackno().value();

        _segments_out.push(to_send_seg);
    }
}

bool TCPConnection::active() const { return {}; }

size_t TCPConnection::write(const string &data) {
    DUMMY_CODE(data);
    return {};
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

void TCPConnection::end_input_stream() {}

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
