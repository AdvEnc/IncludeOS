// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015 Oslo and Akershus University College of Applied Sciences
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

//#define DEBUG // Debug supression

#include <os>
#include <list>
#include <net/inet4>
#include <vector>

using namespace std;
using namespace net;

std::shared_ptr<net::Inet4<VirtioNet> > inet;

void Service::start()
{
  // Assign an IP-address, using Hårek-mapping :-)
  auto& eth0 = hw::Dev::eth<0,VirtioNet>();
  auto& mac = eth0.mac();

  inet = std::make_shared<net::Inet4<VirtioNet>>(eth0);
  auto& timer = hw::PIT::instance();

  inet->network_config({ mac.part[2],mac.part[3],mac.part[4],mac.part[5] },
                       { 255,255,0,0 }, // Netmask
                       { 10,0,0,1 }, // Gateway
                       { 8,8,8,8 });  // DNS

  printf("Service IP address: %s \n", inet->ip_addr().str().c_str());

  // UDP
  UDP::port_t port = 4242;
  auto& conn = inet->udp().bind(port);

  conn.on_read([&] (UDP::addr_t addr, UDP::port_t port, const char* data, int len) {
      string received = std::string(data,len);

      if (received == "SUCCESS") {
        INFO("Test 2", "Client says SUCCESS");
        return;
      }

      INFO("Test 2","Starting UDP-test. Got UDP data from %s: %i: %s",
           addr.str().c_str(), port, received.c_str());

      const int packets { 2400 };

      string first_reply {string("Received '") + received +
          "'. Expect " + to_string(packets) + " packets in 1s\n" };

      // Send the first packet, and then wait for ARP
      conn.sendto(addr, port, first_reply.c_str(), first_reply.size());

      timer.onTimeout(1s, [&conn, addr, port, data, len]() {

          auto bufcount = inet->buffers_available();
          size_t packetsize = inet->ip_obj().MDDS() - sizeof(UDP::udp_header);
          INFO("Test 2", "Trying to transmit %i UDP packets of size %i at maximum throttle",
               packets, packetsize);

          for (int i = 0; i < packets; i++) {
            char c = (char)('A' + (i % 26));
            string send (packetsize, c);
            //printf("<TEST> %i nr. of  %c \n", send.size(), c);
            conn.sendto(addr, port, send.c_str() , send.size());
          }

          CHECK(1,"UDP-transmission didn't panic");
          auto bufcount2 = inet->buffers_available();

          INFO("UDP Transmision tests","OK");
        });
    });


  /*
    WARNING: Subscribing to this event will overwrite the Inet delegate. So Don't

    eth0.on_transmit_queue_available([](size_t s){
    CHECKSERT(s,"There is now room for %i packets in transmit queue", s);
    }); */

  timer.onTimeout(200ms,[=](){
      const int packets { 600 };
      INFO("Test 1", "Trying to transmit %i ethernet packets at maximum throttle", packets);
      for (int i=0; i < packets; i++){
        auto pckt = inet->createPacket(inet->MTU());
        Ethernet::header* hdr = reinterpret_cast<Ethernet::header*>(pckt->buffer());
        hdr->dest.major = Ethernet::addr::BROADCAST_FRAME.major;
        hdr->dest.minor = Ethernet::addr::BROADCAST_FRAME.minor;
        hdr->type = Ethernet::ETH_ARP;
        inet->link().transmit(pckt);
      }

      CHECK(1,"Transmission didn't panic");
      INFO("Test 1", "Done. Send some UDP-data to %s:%i to continue test",
           inet->ip_addr().str().c_str(), port);


    });


}
