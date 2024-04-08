/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 Stanford University
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
 *
 * Author: Serhat Arslan <sarslan@stanford.edu>
 */

/*
 * The heade implementation is partly based on the one provided by
 * https://github.com/PlatformLab/HomaModule/blob/master/homa_impl.h
 * However it does not completely follow the way Home Linux Kernel is
 * implemented for the sake of easier management
 */

#include "msg-generator-tcp-header.h"

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(MsgGeneratorTCPHeader);

TypeId
MsgGeneratorTCPHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::MsgGeneratorTCPHeader")
                            .SetParent<Header>()
                            .SetGroupName("Internet")
                            .AddConstructor<MsgGeneratorTCPHeader>();
    return tid;
}

TypeId
MsgGeneratorTCPHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
MsgGeneratorTCPHeader::GetSerializedSize() const
{
    return sizeof(f_size) + sizeof(f_id);
}

void
MsgGeneratorTCPHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteHtonU32(f_size);
    start.WriteHtonU16(f_id);
}

uint32_t
MsgGeneratorTCPHeader::Deserialize(Buffer::Iterator start)
{
    f_size = start.ReadNtohU32();
    f_id = start.ReadNtohU16();

    return GetSerializedSize();
}

void 
MsgGeneratorTCPHeader::Print (std::ostream &os) const
{
  os << "size: " << f_size << " id: " << f_id;
}

} // namespace ns3