#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  uint64_t res = 0;
  if ( !abs_pos_to_meg_.empty() ) {
    for ( auto& [pos, msg] : abs_pos_to_meg_ ) {
      res += msg.sequence_length();
    }
  }
  return res;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return timer_.get_retrans_time();
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  TCPSenderMessage msg;
  if ( input_.has_error() ) {
    msg.RST = true;
    msg.seqno = Wrap32::wrap( next_abs_send_pos_, isn_ );
    transmit( msg );
    return;
  }

  //接收窗口为0的情况，发送带有一定会被接受方拒绝的序列号的一个byte
  Reader& rd = input_.reader();
  if ( next_abs_send_pos_ == 0 ) {
    msg.SYN = true;
  }

  msg.seqno = Wrap32::wrap( next_abs_send_pos_, isn_ );
  uint64_t send_window_left_space = abs_ackno_ + recv_window_size_ - next_abs_send_pos_;
  uint64_t max_send_bytes_num
    = min( min( send_window_left_space, rd.bytes_buffered() ), TCPConfig::MAX_PAYLOAD_SIZE );
  if ( recv_window_size_ == 0 ) {
    if ( !check_recv_window_ ) {
      if ( rd.bytes_buffered() >= 1 ) {
        max_send_bytes_num = 1;
      }
      check_recv_window_ = true;
    } else {
      return;
    }
  }

  msg.payload = rd.peek().substr( 0, max_send_bytes_num );
  rd.pop( max_send_bytes_num );
  if ( rd.is_finished() && !fin_sended_ ) {
    if ( ( recv_window_size_ != 0 && send_window_left_space - max_send_bytes_num > 0 )
         || ( recv_window_size_ == 0 && max_send_bytes_num == 0 ) ) {
      msg.FIN = true;
      fin_sended_ = true;
    }
  }

  if ( msg.sequence_length() != 0 ) {
    next_abs_send_pos_ += msg.sequence_length();
    abs_pos_to_meg_[next_abs_send_pos_ - 1] = msg;
    transmit( msg );
    if ( !timer_.is_running() ) {
      timer_.run();
    }

    if ( send_window_left_space - msg.sequence_length() > 0 && rd.bytes_buffered() > 0 ) {
      push( transmit );
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  TCPSenderMessage msg;
  if ( input_.has_error() ) {
    msg.RST = true;
  }
  msg.seqno = Wrap32::wrap( next_abs_send_pos_, isn_ );
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  if ( msg.RST ) {
    input_.set_error();
    return;
  }

  uint64_t abs_ackno = msg.ackno->unwrap( isn_, next_abs_send_pos_ );

  //这里是小于还是小于等于待定？
  if ( abs_ackno < abs_ackno_ || abs_ackno > next_abs_send_pos_ ) {
    return;
  }

  bool dup_ack = abs_ackno_ == abs_ackno;
  abs_ackno_ = abs_ackno;
  recv_window_size_ = msg.window_size;

  if ( !dup_ack ) {
    auto end_iter = abs_pos_to_meg_.lower_bound( abs_ackno_ );
    abs_pos_to_meg_.erase( abs_pos_to_meg_.begin(), end_iter );
    timer_.reset();
    if ( !abs_pos_to_meg_.empty() ) {
      timer_.run();
    }

    if ( check_recv_window_ ) {
      check_recv_window_ = false;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  bool check_window = false;
  if ( recv_window_size_ == 0 && check_recv_window_ ) {
    check_window = true;
  }
  bool is_timeout = timer_.increase_time( ms_since_last_tick, check_window );
  if ( is_timeout ) {
    transmit( abs_pos_to_meg_.begin()->second );
  }
}
