#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  uint32_t next_ip = next_hop.ipv4_numeric();

  EthernetFrame frame {};
  frame.header.src = ethernet_address_;
  if (ip_to_eth_.count(next_ip)) {
    frame.header.dst = ip_to_eth_[next_ip].first;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.payload = serialize(dgram);
  } else {
    if (datagrams_will_send_.count(next_ip) && datagrams_will_send_[next_ip].second < 5000) {
      datagrams_will_send_[next_ip].first.push(dgram);
      return;
    }

    ARPMessage arp_msg {};
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    arp_msg.sender_ethernet_address = ethernet_address_;
    arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
    arp_msg.target_ip_address = next_ip;

    for (int i = 0; i < 6; ++i) {
      frame.header.dst[i] = 255;
    }
    frame.header.type = EthernetHeader::TYPE_ARP;
    frame.payload = serialize(arp_msg);
    datagrams_will_send_[next_ip].first.push(dgram);
    datagrams_will_send_[next_ip].second = 0;
  }
  transmit(frame);
}

static bool is_broadcast_addr(const EthernetAddress& eth_addr) {
  for (int i = 0; i < 6; ++i) {
    if (eth_addr[i] != 255) {
      return false;
    }
  }
  return true;
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  if (frame.header.dst != ethernet_address_ && !is_broadcast_addr(frame.header.dst)) {
    return;
  }

  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram dgram {};
    bool parse_success = parse(dgram, frame.payload);
    if (parse_success) {
      datagrams_received_.push(std::move(dgram));
    }
  } else {
    ARPMessage arp_msg {};
    bool parse_success = parse(arp_msg, frame.payload);
    if (parse_success) {
      if (ip_to_eth_.count(arp_msg.sender_ip_address) == 0) {
        ip_to_eth_[arp_msg.sender_ip_address] = { arp_msg.sender_ethernet_address, 0};
        if (datagrams_will_send_.count(arp_msg.sender_ip_address)) {
          EthernetFrame continue_frame {};
          continue_frame.header.src = ethernet_address_;
          continue_frame.header.dst = ip_to_eth_[arp_msg.sender_ip_address].first;
          continue_frame.header.type = EthernetHeader::TYPE_IPv4;
          while (!datagrams_will_send_[arp_msg.sender_ip_address].first.empty()) {
            continue_frame.payload = serialize(datagrams_will_send_[arp_msg.sender_ip_address].first.front());
            transmit(continue_frame);
            datagrams_will_send_[arp_msg.sender_ip_address].first.pop();
          }
          datagrams_will_send_.erase(arp_msg.sender_ip_address);
        }
      }

      if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST) {
        if (arp_msg.target_ip_address == ip_address_.ipv4_numeric()) {
          ARPMessage arp_reply {};
          arp_reply.opcode = ARPMessage::OPCODE_REPLY;
          arp_reply.sender_ethernet_address = ethernet_address_;
          arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
          arp_reply.target_ip_address = arp_msg.sender_ip_address;
          arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;

          EthernetFrame arp_frame {};
          arp_frame.header.src = ethernet_address_;
          arp_frame.header.dst = arp_msg.sender_ethernet_address;
          arp_frame.header.type = EthernetHeader::TYPE_ARP;
          arp_frame.payload = serialize(arp_reply);
          transmit(arp_frame);
        }
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  std::vector<uint32_t> need_erase_arp_entrys;
  for (auto &[ip, eth_info] : ip_to_eth_) {
    if (eth_info.second + ms_since_last_tick > 30000) {
      need_erase_arp_entrys.push_back(ip);
    } else {
      eth_info.second += ms_since_last_tick;
    }
  }

  for (auto &ip : need_erase_arp_entrys) {
    ip_to_eth_.erase(ip);
  }

  for (auto &[ip, send_info] : datagrams_will_send_) {
    send_info.second += ms_since_last_tick;
  }
}
