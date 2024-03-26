#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }

  if ( message.SYN && !isn_.has_value() ) {
    isn_ = std::make_optional<Wrap32>( message.seqno );
  }

  if ( isn_.has_value() ) {
    if ( message.SYN && message.seqno == isn_.value() ) {
      message.seqno = message.seqno + 1;
    }

    uint64_t first_index = message.seqno.unwrap( isn_.value(), reassembler_.get_first_unassembled_pos() + 1 ) - 1;

    bool is_last_substring = false;
    if ( message.FIN ) {
      recv_fin_ = true;
      fin_no_ = first_index + message.payload.size();
      is_last_substring = true;
    }

    reassembler_.insert( first_index, message.payload, is_last_substring );
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage resp;

  if ( reassembler_.reader().has_error() ) {
    resp.RST = true;
  } else {
    resp.window_size = reassembler_.window_size();
    if ( isn_.has_value() ) {
      uint64_t first_unassembled_pos = reassembler_.get_first_unassembled_pos();
      resp.ackno = std::make_optional<Wrap32>( Wrap32::wrap(
        first_unassembled_pos + 1 + ( recv_fin_ && first_unassembled_pos == fin_no_ ? 1 : 0 ), isn_.value() ) );
    }
  }

  return resp;
}
