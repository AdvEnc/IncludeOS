// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015-2016 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#define DEBUG
#define DEBUG2

#include <net/tcp.hpp>
#include <alloca.h>

using namespace std;
using namespace net;


TCP::TCP(IPStack& inet) :
  inet_(inet),
  listeners_(),
  connections_(),
  writeq(),
  used_ports(),
  MAX_SEG_LIFETIME(30s)
{
  inet.on_transmit_queue_available(transmit_avail_delg::from<TCP,&TCP::process_writeq>(this));
}

/*
  Note: There is different approaches to how to handle listeners & connections.
  Need to discuss and decide for the best one.

  Best solution(?):
  Preallocate a pool with listening connections.
  When threshold is reach, remove/add new ones, similar to TCP window.

  Current solution:
  Simple.
*/
TCP::Connection& TCP::bind(Port port) {
  // Already a listening socket.
  if(listeners_.find(port) != listeners_.end()) {
    throw TCPException{"Port is already taken."};
  }
  auto& connection = (listeners_.emplace(port, Connection{*this, port})).first->second;
  debug("<TCP::bind> Bound to port %i \n", port);
  connection.open(false);
  return connection;
}

/*
  Active open a new connection to the given remote.

  @WARNING: Callback is added when returned (TCP::connect(...).onSuccess(...)),
  and open() is called before callback is added.
*/
TCP::Connection_ptr TCP::connect(Socket remote) {
  auto port = next_free_port();
  std::shared_ptr<Connection> connection = add_connection(port, remote);
  connection->open(true);
  return connection;
}

/*
  Active open a new connection to the given remote.
*/
void TCP::connect(Socket remote, Connection::ConnectCallback callback) {
  auto port = next_free_port();
  auto connection = add_connection(port, remote);
  connection->onConnect(callback).open(true);
}

TCP::Seq TCP::generate_iss() {
  // Do something to get a iss.
  return rand();
}

/*
  TODO: Check if there is any ports free.
*/
TCP::Port TCP::next_free_port() {

  if(++current_ephemeral_ == 0) {
    current_ephemeral_ = 1025;
    // TODO: Can be taken
  }
  // Avoid giving a port that is bound to a service.
  while(listeners_.find(current_ephemeral_) != listeners_.end())
    current_ephemeral_++;

  return current_ephemeral_;
}

/*
  Expensive look up if port is in use.
*/
bool TCP::port_in_use(const TCP::Port port) const {
  if(listeners_.find(port) != listeners_.end())
    return true;

  for(auto conn : connections_) {
    if(conn.first.first == port)
      return true;
  }
  return false;
}


uint16_t TCP::checksum(TCP::Packet_ptr packet) {
  // TCP header
  TCP::Header* tcp_hdr = &(packet->header());
  // Pseudo header
  TCP::Pseudo_header pseudo_hdr;

  int tcp_length = packet->tcp_length();

  pseudo_hdr.saddr.whole = packet->src().whole;
  pseudo_hdr.daddr.whole = packet->dst().whole;
  pseudo_hdr.zero = 0;
  pseudo_hdr.proto = IP4::IP4_TCP;
  pseudo_hdr.tcp_length = htons(tcp_length);

  union Sum{
    uint32_t whole;
    uint16_t part[2];
  } sum;

  sum.whole = 0;

  // Compute sum of pseudo header
  for (uint16_t* it = (uint16_t*)&pseudo_hdr; it < (uint16_t*)&pseudo_hdr + sizeof(pseudo_hdr)/2; it++)
    sum.whole += *it;

  // Compute sum sum the actual header and data
  for (uint16_t* it = (uint16_t*)tcp_hdr; it < (uint16_t*)tcp_hdr + tcp_length/2; it++)
    sum.whole+= *it;

  // The odd-numbered case
  if (tcp_length & 1) {
    debug("<TCP::checksum> ODD number of bytes. 0-pading \n");
    union {
      uint16_t whole;
      uint8_t part[2];
    } last_chunk;
    last_chunk.part[0] = ((uint8_t*)tcp_hdr)[tcp_length - 1];
    last_chunk.part[1] = 0;
    sum.whole += last_chunk.whole;
  }

  debug2("<TCP::checksum: sum: 0x%x, half+half: 0x%x, TCP checksum: 0x%x, TCP checksum big-endian: 0x%x \n",
         sum.whole, sum.part[0] + sum.part[1], (uint16_t)~((uint16_t)(sum.part[0] + sum.part[1])), htons((uint16_t)~((uint16_t)(sum.part[0] + sum.part[1]))));

  Sum final_sum { (uint32_t)sum.part[0] + (uint32_t)sum.part[1] };

  if (final_sum.part[1])
    final_sum.part[0] += final_sum.part[1];

  return ~final_sum.whole;
}

void TCP::bottom(net::Packet_ptr packet_ptr) {
  // Translate into a TCP::Packet. This will be used inside the TCP-scope.
  auto packet = std::static_pointer_cast<TCP::Packet>(packet_ptr);
  debug("<TCP::bottom> TCP Packet received - Source: %s, Destination: %s \n",
        packet->source().to_string().c_str(), packet->destination().to_string().c_str());

  // Do checksum
  if(checksum(packet)) {
    debug("<TCP::bottom> TCP Packet Checksum != 0 \n");
  }

  Connection::Tuple tuple { packet->dst_port(), packet->source() };

  // Try to find the receiver
  auto conn_it = connections_.find(tuple);
  // Connection found
  if(conn_it != connections_.end()) {
    debug("<TCP::bottom> Connection found: %s \n", conn_it->second->to_string().c_str());
    conn_it->second->segment_arrived(packet);
  }
  // No connection found
  else {
    // Is there a listener?
    auto listen_conn_it = listeners_.find(packet->dst_port());
    debug("<TCP::bottom> No connection found - looking for listener..\n");
    // Listener found => Create listening Connection
    if(listen_conn_it != listeners_.end()) {
      auto& listen_conn = listen_conn_it->second;
      debug("<TCP::bottom> Listener found: %s ...\n", listen_conn.to_string().c_str());
      auto connection = (connections_.emplace(tuple, std::make_shared<Connection>(listen_conn)).first->second);
      // Set remote
      connection->set_remote(packet->source());
      debug("<TCP::bottom> ... Creating connection: %s \n", connection->to_string().c_str());

      connection->segment_arrived(packet);
    }
    // No listener found
    else {
      drop(packet);
    }
  }
}

void TCP::process_writeq(size_t packets) {
  debug2("<TCP::process_writeq> size=%u p=%u\n", writeq.size(), packets);
  // foreach connection who wants to write
  while(packets and !writeq.empty()) {
    auto conn = writeq.front();
    writeq.pop_back();
    conn->offer(packets);
    conn->set_queued(false);
  }
}

size_t TCP::send(Connection_ptr conn, const char* buffer, size_t n) {
  size_t written{0};
  auto packets = inet_.transmit_queue_available();

  debug2("<TCP::send> Send request for %u bytes\n", n);

  if(packets > 0) {
    written += conn->send(buffer, n, packets);
  }
  // if connection still can send (means there wasn't enough packets)
  // only requeue if not already queued
  if(conn->can_send() and !conn->is_queued()) {
    debug2("<TCP::send> Conn queued.\n");
    writeq.push_back(conn);
    conn->set_queued(true);
  }

  return written;
}

/*
  Show all connections for TCP as a string.

  Format:
  [Protocol][Recv][Send][Local][Remote][State]

  TODO: Make sure Recv, Send, In, Out is correct and add them to output. Also, alignment?
*/
string TCP::to_string() const {
  // Write all connections in a cute list.
  stringstream ss;
  ss << "LISTENING SOCKETS:\n";
  for(auto listen_it : listeners_) {
    ss << listen_it.second.to_string() << "\n";
  }
  ss << "\nCONNECTIONS:\n" <<  "Proto\tRecv\tSend\tIn\tOut\tLocal\t\t\tRemote\t\t\tState\n";
  for(auto con_it : connections_) {
    auto& c = *(con_it.second);
    ss << "tcp4\t"
       << " " << "\t" << " " << "\t"
       << " " << "\t" << " " << "\t"
       << c.local().to_string() << "\t\t" << c.remote().to_string() << "\t\t"
       << c.state().to_string() << "\n";
  }
  return ss.str();
}


TCP::Connection_ptr TCP::add_connection(Port local_port, TCP::Socket remote) {
  return        (connections_.emplace(
                                      Connection::Tuple{ local_port, remote },
                                      std::make_shared<Connection>(*this, local_port, remote))
                 ).first->second;
}

void TCP::close_connection(TCP::Connection& conn) {
  debug("<TCP::close_connection> Closing connection: %s \n", conn.to_string().c_str());
  connections_.erase(conn.tuple());
}

void TCP::drop(TCP::Packet_ptr) {
  //debug("<TCP::drop> Packet was dropped - no recipient: %s \n", packet->destination().to_string().c_str());
}

void TCP::transmit(TCP::Packet_ptr packet) {
  // Generate checksum.
  packet->set_checksum(TCP::checksum(packet));
  //if(packet->has_data())
  //  printf("<TCP::transmit> S: %u\n", packet->seq());
  _network_layer_out(packet);
}
