/*
 * Copyright (c) 2024 Max Planck Institute for Software Systems
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ptp-queue.h"

#include <net/ethernet.h>
#include <linux/ip.h>
#include <linux/udp.h>

namespace ns3
{

struct ptp_v2_hdr {
  uint8_t msg_type;
  uint8_t version_ptp;
  uint16_t msg_len;
  uint8_t subdomain;
  uint8_t _rsvd0;
  uint16_t flags;
  uint64_t correction;
  uint32_t _rsvd1;
  uint8_t src_port_id[10];
  uint16_t seq_id;
  uint8_t ctrl;
  uint8_t log_msg_period;
} __attribute__((packed));

NS_LOG_COMPONENT_DEFINE("PTPQueue");

NS_OBJECT_TEMPLATE_CLASS_DEFINE(PTPQueue, Packet);
NS_OBJECT_TEMPLATE_CLASS_DEFINE(PTPQueue, QueueDiscItem);

void PTPAddResidenceTime_(Ptr<Packet> packet, Time enq_time) {
  NS_LOG_FUNCTION(packet << enq_time);

  /* TODO: not sure if there is a way to check the ethertype here. That seems to
     be stored only in the SimpleTag packet tag, a class only defined in
     simple-net-device.cc. */

  size_t buf_size = packet->GetSize();
  uint8_t *buffer = new uint8_t[buf_size];
  packet->CopyData(buffer, buf_size);

  /* there is probably a more ns-3-y way of doing this */
  struct iphdr *ip_h = (struct iphdr *) buffer;
  struct udphdr *udp_h = (struct udphdr *) (ip_h + 1);
  struct ptp_v2_hdr *ptp_h = (struct ptp_v2_hdr *) (udp_h + 1);

  if (buf_size >= sizeof(*ip_h) + sizeof(*udp_h) + sizeof(*ptp_h) &&
      ip_h->protocol == 17 &&
      (be16toh(udp_h->dest) == 319 || be16toh(udp_h->dest) == 320) &&
      (ptp_h->version_ptp == 2) &&
      (ptp_h->msg_type == 0 || ptp_h->msg_type == 1 || ptp_h->msg_type == 9)) {

    ptp_h->correction =
        htobe64(be64toh(ptp_h->correction) + enq_time.ToInteger(Time::NS));

    Ptr<Packet> new_pkt = Create<Packet> (buffer, buf_size);
    packet->RemoveAtEnd(buf_size);
    packet->AddAtEnd(new_pkt);
  }

  delete[] buffer;
}

} // namespace ns3
