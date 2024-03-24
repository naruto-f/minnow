#include "byte_stream.hh"
#include <cstring>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), buf_(capacity_, '\0') {}

bool Writer::is_closed() const
{
  // Your code here.
  return close_flag_;
}

void Writer::push( string data )
{
  // Your code here.
  uint64_t data_len = data.size();
  uint64_t push_len = min(available_capacity(), data_len);

  memmove(buf_.data() + next_push_pos_, data.data(), push_len);
  next_push_pos_ += push_len;
  push_count_ += push_len;

  return;
}

void Writer::close()
{
  // Your code here.
  close_flag_ = true;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - next_push_pos_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return push_count_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return close_flag_ && pop_count_ == push_count_;
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return pop_count_;
}

string_view Reader::peek() const
{
  // Your code here.
  return {buf_.data(), next_push_pos_};
}

void Reader::pop( uint64_t len )
{
  // Your code here.

  /* 每次pop时都需要将缓冲区后一部分的所有数据位移，效率太低，应该使用环形缓冲区 */
  pop_count_ += len;
  memmove(buf_.data(), buf_.data() + len, next_push_pos_ - len);
  next_push_pos_ -= len;


//  start_pos += len;
//  pop_count_ += len;
//  if (start_pos == next_push_pos_) {
//    start_pos = 0;
//    next_push_pos_ = 0;
//  } else {
//    memmove(buf_.data(), buf_.data() + start_pos, next_push_pos_ - start_pos);
//    next_push_pos_ -= start_pos;
//    start_pos = 0;
//  }
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return next_push_pos_;
}
