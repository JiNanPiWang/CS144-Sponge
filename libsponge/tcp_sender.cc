#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
{
    seqno_ = _isn;
    ackno_ = _isn;
    retrans_RTO = retx_timeout;
}

uint64_t TCPSender::bytes_in_flight() const {
    return unwrap_seq_num(seqno_) - unwrap_seq_num(ackno_);
}

void TCPSender::fill_window() 
{
    while (bytes_in_flight() < window_size_ || (now_status != TCPStatus::TIME_WAIT && _stream.input_ended()) )
    {
        // FIN会是最后一个消息
        // TODO: CLOSED需不需要考虑？
        if (now_status == TCPStatus::TIME_WAIT)
            return;
        TCPSenderMessage to_trans { seqno_, false, "", false, false };
        if ( _stream.error() )
            to_trans.RST = true;
        if ( now_status == TCPStatus::LISTEN ) // 还没开始，准备SYN
        {
            to_trans.seqno = _isn;
            to_trans.SYN = true;
            if (window_size_ == UINT32_MAX) // 如果没被初始化，就初始化
                window_size_ = 1;
            // has_SYN = true;
            now_status = TCPStatus::SYN_SENT;
        }
        else if (now_status == TCPStatus::SYN_RCVD) // 接收到ack+syn(ack)，发送ack(ack+syn)
        {
            to_trans.ACK = true;
            now_status = TCPStatus::ESTABLISHED;
        }
        else if (now_status == TCPStatus::FIN_WAIT_1)
        {
            // 客户端发送FIN，还确认了已经接收并处理了服务器到目前为止发送的所有数据，也就是ACK=1。
            to_trans.FIN = true;
            to_trans.ACK = true;
        }
        else if (now_status == TCPStatus::CLOSING)
        {
            to_trans.ACK = true;
            // 在这最后一个ACK发出后，客户端通常会进入TIME_WAIT状态，等待足够的时间以确保服务器收到了最后的确认，然后最终关闭连接。
            now_status = TCPStatus::TIME_WAIT;
            _stream.end_input();
        }
        // 已经被关闭了，准备FIN，且有空间发FIN；如果buffer大于等于window，那就是普通情况，等一下再发FIN
        // FIN不占payload的size，但是占window
        // 需要考虑MAX_PAYLOAD_SIZE，要不然一个10000大小的payload，分成10次发，会每次都带FIN

        // SPONGE
        // 普通情况，不是connection
        else if ( _stream.input_ended() &&
            _stream.buffer_size() + bytes_in_flight() < window_size_ &&
            _stream.buffer_size() <= TCPConfig::MAX_PAYLOAD_SIZE )
        {
            // 测试会调用close方法，就关闭了
            to_trans.FIN = true;
            now_status = TCPStatus::TIME_WAIT;
        }
        
        // start from last byte + 1，但是如果Bytestream里面只有SYN，那就提取不出来内容，需要取min得到0
        // push的数量，现在缓存了多少个减去发出还没确认的个数，bytes_buffered肯定是>=bytes_in_flight的
        // 同时循环也确定了bytes_in_flight() < window_size_，否则不进行push操作
        // 减to_trans.SYN的原因是可能SYN和data一起，会占一个位置
        auto push_num = _stream.buffer_size();
        push_num = min( push_num, window_size_ - bytes_in_flight() - to_trans.SYN);
        push_num = min( push_num, TCPConfig::MAX_PAYLOAD_SIZE );

        if ( push_num + to_trans.SYN + to_trans.FIN + to_trans.ACK == 0) // 如果所有内容全空，规格错误，就不发送
            return;

        string pop_str = _stream.read(push_num);

        to_trans.payload = pop_str;

        seqno_ = seqno_ + to_trans.payload.size() + to_trans.SYN + to_trans.FIN;
        _next_seqno += to_trans.payload.size() + to_trans.SYN + to_trans.FIN;


        _segments_out.push(to_trans.to_TCPSeg());
        flying_segments.push( to_trans.to_TCPSeg() );

        // 如果不是正常发送情况，那我们不需要重复发送
        if (now_status != TCPStatus::ESTABLISHED)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // receive后，测试的调用会自动触发push
    if (now_status != TCPStatus::LISTEN) {
        auto &new_ackno = ackno;
        auto &first_fly_ele = flying_segments.front();
        // ackno不能大于seqno
        if (unwrap_seq_num(new_ackno) > unwrap_seq_num(seqno_))
            return;
        // 新的ack也不能小于老的ack，否则无效
        if ( unwrap_seq_num(new_ackno) < unwrap_seq_num(ackno_))
            return;
        // 接收过的信息就不接收了，也就是现在收到的ack必须大于fly第一个的ack，除非要发FIN
        if (!flying_segments.empty() && unwrap_seq_num(new_ackno) <= unwrap_seq_num(first_fly_ele.header().seqno) &&
            !_stream.input_ended())
            return;
        // 如果接收的ack不够弹出fly的第一个块，那么ack不算数
        if (!flying_segments.empty() &&
            unwrap_seq_num(new_ackno) > unwrap_seq_num(ackno_) &&
            unwrap_seq_num(new_ackno) - unwrap_seq_num(ackno_) <
                first_fly_ele.payload().size() + first_fly_ele.header().syn + first_fly_ele.header().fin)
            return;

        // 如果发过来的不是对SYN的ACK，我们才pop，syn和data和fin一起
        if (unwrap_seq_num(ackno_) == 0)
        {
            // ackno_ = ackno_ + 1;
            // 如果connection没有改变状态，就是默认情况，直接连接
            if (now_status != TCPStatus::SYN_RCVD)
                now_status = TCPStatus::ESTABLISHED;
        }

        ackno_ = ackno;

        // 当前ackno已经超过了之前的
        while (!flying_segments.empty() && unwrap_seq_num(ackno_) > unwrap_seq_num(flying_segments.front().header().seqno))
            flying_segments.pop();
    }
    if (window_size != 0)
    {
        window_size_ = window_size;
        retrans_cnt = 0;
        retrans_timer = 0;
        retrans_RTO = _initial_retransmission_timeout;
        zero_window = false;
    }
    else // window_size = 0，特判
    {
        window_size_ = 1;
        zero_window = true;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) 
{
    // retransmit ackno_ to seqno_ - 1
    if (flying_segments.empty())
        return;
    retrans_timer += ms_since_last_tick;
    if ( retrans_timer >= retrans_RTO && now_status != TCPStatus::TIME_WAIT )
    {
        if (window_size_ != 0 && !zero_window) // 重传时间翻倍
            retrans_RTO <<= 1;

        if (!flying_segments.empty())
        {
            _segments_out.push(flying_segments.front());  // 重传第一段
        }
        else
        {
            retrans_cnt = 0;
            retrans_RTO = _initial_retransmission_timeout;
        }
        retrans_timer = 0;
        retrans_cnt++;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const
{
    return retrans_cnt;
}

void TCPSender::send_empty_segment()
{
    TCPSenderMessage t{ seqno_, false, "", false, _stream.input_ended()};
    _segments_out.push(t.to_TCPSeg());
}

uint64_t TCPSender::unwrap_seq_num( const WrappingInt32& num ) const
{
    // or this->input_.writer().bytes_pushed()
    return unwrap(num, _isn, _stream.bytes_read());
}

void TCPSender::change_status(TCPStatus status) {
    now_status = status;
}

TCPStatus TCPSender::get_status() const {
    return now_status;
}
unsigned int TCPSender::get_retrans_timer() const {
    return retrans_timer;
}
