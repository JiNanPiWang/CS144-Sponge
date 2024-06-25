#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (data.empty()) // FOR SPONGE，感觉不太对，但是过了
    {
        if (eof && fragments_map.empty())
            _output.end_input();
        return;
    }
    bool changed_tail = false;
    auto push_str = data;
    auto push_index = index;
    
    // 如果remaining_capacity < push_str.size()，map就只存substr，并且只要有空间，就读
    if ( unassembled_bytes() >= _output.remaining_capacity() )
        return;

    // 如果push_index >= current_pos，就正常存
    // 如果push_index < current_pos <= push_index + push_str.size()的，也按current_pos插入

    // push_index + push_str.size() <= current_pos + _output.remaining_capacity()
    // 只能插current_pos到current_pos + _output.remaining_capacity()的内容进来
    if (push_index >= current_pos) // cur在fir左边，存remaining_capacity内的内容
    {
        if (push_index + push_str.size() > current_pos + _output.remaining_capacity())
        {
            push_str = push_str.substr( 0, current_pos + _output.remaining_capacity() - push_index );
            changed_tail = true;
        }
    }
    else // cur在fir右边，详见check1.md配图
    {
        if (push_index + push_str.size() >= current_pos) // 从current_pos开始截取push_str
            push_str = push_str.substr( current_pos - push_index );
        else
            return;
        if (push_str.size() > _output.remaining_capacity()) // 只取容量内的push_str
        {
            push_str = push_str.substr( 0, _output.remaining_capacity() );
            changed_tail = true;
        }
        push_index = current_pos;
    }

    // pending_bytes_ += push_str.size();
    if (fragments_map.find(push_index) != fragments_map.end() && push_str.size() <= fragments_map[push_index].size())
        return;
    fragments_map[push_index] = std::move( push_str );

    if ( eof && !changed_tail )
        close_flag = true;


    while ( !fragments_map.empty() && fragments_map.begin()->first <= current_pos) // 可以插入了，只插入在范围内的
    {
        auto& cur_str = fragments_map.begin()->second;
        //  pending_bytes_ -= cur_str.size();
        // 判断字符串是否在范围内，不在，就不算
        if (fragments_map.begin()->first + cur_str.size() < current_pos)
        {
            fragments_map.erase( fragments_map.begin() );
            continue;
        }
        // 修剪字符串
        if (fragments_map.begin()->first != current_pos)
            cur_str = cur_str.substr( current_pos - fragments_map.begin()->first );

        auto new_pos = current_pos + cur_str.size();
        _output.write( cur_str );

        fragments_map.erase( fragments_map.begin() );
        current_pos = new_pos;
    }

    if ( close_flag && fragments_map.empty() )
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const {
    if (fragments_map.empty())
        return 0;
    uint64_t pos = fragments_map.begin()->first;
    uint64_t result = 0;
    for (auto &p : fragments_map)
    {
        if (p.first >= pos)
            result += p.second.size();
        else if (p.first + p.second.size() >= pos)
            result += p.first + p.second.size() - pos;
        else
            continue;
        pos = p.first + p.second.size();
    }
    return result;
}

bool StreamReassembler::empty() const {
    return fragments_map.empty();
}
