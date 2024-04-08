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

#ifndef HOMA_HEADER_H
#define HOMA_HEADER_H

#include "ns3/header.h"

namespace ns3
{
/**
 * \ingroup homa
 * \brief Application-level packet header for flows produced by MsgGeneratorAppTCP
 */
class MsgGeneratorTCPHeader : public Header
{
  public:
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    uint32_t f_size = 0;        /* Flow size in Bytes */
    uint16_t f_id = 0;          /* Flow ID */
    uint32_t bytes_recvd = 0;   /* Number of bytes received so far */
    uint8_t valid = false;      /* Whether this header is valid */
};
} // namespace ns3
#endif /* HOMA_HEADER */