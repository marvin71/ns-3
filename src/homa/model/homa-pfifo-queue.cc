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

#include "homa-pfifo-queue.h"

#include <linux/ip.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("HomaPFifoQueue");

uint8_t HomaQPktTos_(Ptr<Packet> packet, uint8_t m_bands)
{
  struct iphdr ip_h;
  packet->CopyData((uint8_t *) &ip_h, sizeof(ip_h));

  uint8_t tos = ip_h.tos & 0x3f;
  if (tos >= m_bands) {
    NS_LOG_WARN("Truncating band as tos (" << tos << ") > maximum (" << m_bands
      << ")");
    return m_bands - 1;
  }
  return tos;
}

NS_OBJECT_TEMPLATE_CLASS_DEFINE(HomaPFifoQueue, Packet);
NS_OBJECT_TEMPLATE_CLASS_DEFINE(HomaPFifoQueue, QueueDiscItem);

} // namespace ns3
