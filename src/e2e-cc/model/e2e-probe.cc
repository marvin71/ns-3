/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

/*
 * Copyright 2023 Max Planck Institute for Software Systems
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "e2e-application.h"
#include "e2e-probe.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("E2EProbe");

E2EProbe::E2EProbe(const E2EConfig& config) : E2EComponent(config)
{
    NS_ABORT_MSG_IF(GetId().empty(), "Probe has no id");
    NS_ABORT_MSG_IF(GetType().empty(), "Probe '" << GetId() << "' has no type");
}

void
TraceRx(void func(uint32_t, Ptr<E2EPeriodicSampleProbe<uint32_t>>),
        Ptr<E2EPeriodicSampleProbe<uint32_t>> probe,
        Ptr<const Packet> packet,
        const Address& address)
{
    func(packet->GetSize(), probe);
}

void
TraceHomaMsgBegin (Ptr<OutputStreamWrapper> stream,
                   Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr,
                   uint16_t sport, uint16_t dport, int txMsgId)
{
  NS_LOG_DEBUG("+ " << Simulator::Now ().GetNanoSeconds ()
                << " " << msg->GetSize()
                << " " << saddr << ":" << sport
                << " "  << daddr << ":" << dport
                << " " << txMsgId);

  *stream->GetStream () << "+ " << Simulator::Now ().GetNanoSeconds ()
      << " " << msg->GetSize()
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport
      << " " << txMsgId << std::endl;
}

void
TraceHomaMsgFinish (Ptr<OutputStreamWrapper> stream,
                    Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr,
                    uint16_t sport, uint16_t dport, int txMsgId)
{
  NS_LOG_DEBUG("- " << Simulator::Now ().GetNanoSeconds ()
                << " " << msg->GetSize()
                << " " << saddr << ":" << sport
                << " "  << daddr << ":" << dport
                << " " << txMsgId);

  *stream->GetStream () << "- " << Simulator::Now ().GetNanoSeconds ()
      << " " << msg->GetSize()
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport
      << " " << txMsgId << std::endl;
}

E2ETracer::E2ETracer(const E2EConfig& config) : E2EProbe(config)
{
    if (auto file {config.Find("File")}; file)
    {
        m_stream = Create<OutputStreamWrapper>(std::string(file->value), std::ios::out);
        file->processed = true;
    }
    else
    {
        NS_ABORT_MSG("probe has no file name");
    }
}

} // namespace ns3