#include "router.hh"

#include <iostream>
#include <limits>
#include <cmath>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // Your code here.
  route_rules_.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.

    for ( auto& interface : _interfaces ) {
      auto& datagrams_received = interface->datagrams_received();
      while ( !datagrams_received.empty() ) {
        InternetDatagram dgram = datagrams_received.front();
        datagrams_received.pop();
        if ( dgram.header.ttl == 0 || --dgram.header.ttl == 0 ) {
          continue;
        }

        auto rule_index = select_leave_interface( dgram );
        if ( rule_index.has_value() ) {
          dgram.header.compute_checksum();    //数据包一旦发生变化要重新计算包头的校验码
          route_entry& match_entry = route_rules_[rule_index.value()];
          if (match_entry.next_hop.has_value()) {
            _interfaces[match_entry.interface_num]->send_datagram( dgram, match_entry.next_hop.value() );
          } else {
            //如果目的地址与网络接口直连，那么下一跳的地址就是最终的目的地址
            _interfaces[match_entry.interface_num]->send_datagram( dgram, Address::from_ipv4_numeric(dgram.header.dst) );
          }
        }
      }
    }
}

std::optional<size_t> Router::select_leave_interface(const InternetDatagram& dgram) const {
  std::optional<size_t> res {};
  uint8_t max_match_prefix_len = 0;
  uint32_t ip_addr = dgram.header.dst;
  int size = route_rules_.size();
  int defalut_route_index = -1;

  for (int i = 0; i < size; ++i) {
    auto &entry = route_rules_[i];
    if (entry.route_prefix == 0) {
      defalut_route_index = i;
    }

    if ((ip_addr & ~static_cast<uint32_t>(pow(2, 32 - entry.prefix_length) - 1)) == entry.route_prefix) {
      if (max_match_prefix_len < entry.prefix_length) {
        res = i;
        max_match_prefix_len = entry.prefix_length;
      }
    }
  }

  if (!res.has_value() && defalut_route_index != -1) {
    res = defalut_route_index;
  }

  return res;
}
