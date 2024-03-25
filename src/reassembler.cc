#include "reassembler.hh"

#include <algorithm>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // Your code here.
  Writer& writer = output_.writer();
  uint64_t data_len = data.size();
  if ( data_len == 0 ) {
    if ( is_last_substring ) {
      writer.close();
    }
    return;
  }

  if ( is_last_substring ) {
    stream_end_ = true;
    last_index_ = first_index + data_len - 1;
  }

  if ( first_index + data_len <= first_unassembled_pos_ ) {
    return;
  }

  if ( first_index < first_unassembled_pos_ ) {
    data = data.substr( first_unassembled_pos_ - first_index );
    data_len -= ( first_unassembled_pos_ - first_index );
    first_index = first_unassembled_pos_;
  }

  if ( first_index + data_len > first_unassembled_pos_ + writer.available_capacity() ) {
    data = data.substr( 0, first_unassembled_pos_ + writer.available_capacity() - first_index );
    data_len = first_unassembled_pos_ + writer.available_capacity() - first_index;
  }

  uint64_t cur_begin = first_index, cur_end = first_index + data_len - 1;
  std::deque<std::pair<uint64_t, uint64_t>> new_intervals;
  bool insert_flag = false;

  for ( auto& [start, end] : intervals_ ) {
    if ( cur_end < start ) {
      if ( !insert_flag ) {
        new_intervals.push_back( { cur_begin, cur_end } );
        startpos_to_string_[cur_begin] = std::move( data );
        insert_flag = true;
      }
      new_intervals.push_back( { start, end } );
    } else if ( cur_begin > end ) {
      new_intervals.push_back( { start, end } );
    } else {
      if ( cur_begin >= start && cur_end <= end ) {
        return;
      } else if ( cur_begin <= start && cur_end >= end ) {
        startpos_to_string_.erase( start );
      } else if ( cur_begin > start ) {
        cur_begin = start;
        data = startpos_to_string_[start].append( std::move( data.substr( data_len - cur_end + end ) ) );
        data_len = data.size();
        startpos_to_string_.erase( start );
      } else {
        data.append( std::move( startpos_to_string_[start].substr( cur_end - start + 1 ) ) );
        cur_end = end;
        startpos_to_string_.erase( start );
      }
    }
  }

  if ( !insert_flag ) {
    new_intervals.push_back( { cur_begin, cur_end } );
    startpos_to_string_[cur_begin] = std::move( data );
  }

  while ( !new_intervals.empty() && first_unassembled_pos_ == new_intervals.begin()->first ) {
    writer.push( startpos_to_string_[first_unassembled_pos_] );
    startpos_to_string_.erase( first_unassembled_pos_ );
    first_unassembled_pos_ = new_intervals.begin()->second + 1;
    new_intervals.pop_front();

    if ( stream_end_ && first_unassembled_pos_ == last_index_ + 1 ) {
      writer.close();
    }
  }

  intervals_ = std::move( new_intervals );
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  uint64_t count = 0;
  for ( auto& [begin, end] : intervals_ ) {
    count += end - begin + 1;
  }
  return count;
}
