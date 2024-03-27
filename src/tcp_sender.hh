#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cassert>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>

class RetransTimer
{
public:
  RetransTimer( uint64_t initial_RTO_ms ) : initial_RTO_ms_( initial_RTO_ms ), cur_RTO_ms_( initial_RTO_ms_ ) {}

  void run() { run_flag_ = true; }

  bool is_running() const { return run_flag_; }

  bool timeout() const { return cur_ms_ >= cur_RTO_ms_; }

  uint64_t get_retrans_time() const { return retrans_time_; }

  bool increase_time( uint64_t count, bool check_recv_window = false )
  {
    if ( !is_running() ) {
      return false;
    }

    bool is_timeout = false;
    cur_ms_ += count;
    if ( timeout() ) {
      is_timeout = true;
      if ( !check_recv_window ) {
        cur_RTO_ms_ *= 2;
      }
      cur_ms_ = 0;
      ++retrans_time_;
    }

    return is_timeout;
  }

  void reset()
  {
    cur_RTO_ms_ = initial_RTO_ms_;
    cur_ms_ = 0;
    retrans_time_ = 0;
    run_flag_ = false;
  }

private:
  bool run_flag_ { false };
  uint64_t initial_RTO_ms_;
  uint64_t cur_RTO_ms_;
  uint64_t cur_ms_ { 0 };
  uint64_t retrans_time_ { 0 };
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), timer_( initial_RTO_ms_ )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  RetransTimer timer_;

  uint16_t recv_window_size_ { 1 }; //这个初值设定存疑
  uint64_t abs_ackno_ { 0 };
  bool fin_sended_ { false };
  bool check_recv_window_ { false };

  uint64_t next_abs_send_pos_ { 0 };

  //发送的tcp段的起始绝对索引到该段内容的映射
  std::map<uint64_t, TCPSenderMessage> abs_pos_to_meg_ {};
};
