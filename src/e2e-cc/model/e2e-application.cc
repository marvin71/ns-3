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
#include "e2e-component.h"
#include "e2e-config.h"

#include "ns3/applications-module.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("E2EApplication");

E2EApplication::E2EApplication(const E2EConfig& config, const std::string& type_id)
    : E2EComponent(config)
{
    NS_ABORT_MSG_IF(GetId().size() == 0, "Application has no id");
    NS_ABORT_MSG_IF(GetIdPath().size() != 3,
        "Application '" << GetId() << "' has invalid path length of " << GetIdPath().size());
    NS_ABORT_MSG_IF(GetType().size() == 0, "Application '" << GetId() << "' has no type");

    m_factory.SetTypeId(type_id);
    config.SetFactoryIfContained<TimeValue, Time>(m_factory, "StartTime", "StartTime");
    config.SetFactoryIfContained<TimeValue, Time>(m_factory, "StopTime", "StopTime");
}

Ptr<E2EApplication>
E2EApplication::CreateApplication(const E2EConfig& config)
{
    auto type_opt {config.Find("Type")};
    NS_ABORT_MSG_UNLESS(type_opt.has_value(), "Application has no type");
    std::string_view type {(*type_opt).value};
    (*type_opt).processed = true;

    if (type == "PacketSink")
    {
        return Create<E2EPacketSink>(config);
    }
    else if (type == "BulkSender")
    {
        return Create<E2EBulkSender>(config);
    }
    else if (type == "OnOff")
    {
        return Create<E2EOnOffApp>(config);
    }
    else
    {
        NS_ABORT_MSG("Unkown application type '" << type << "'");
    }
}

Ptr<Application> E2EApplication::GetApplication()
{
    return m_application;
}

E2EPacketSink::E2EPacketSink(const E2EConfig& config) : E2EApplication(config, "ns3::PacketSink")
{
    if (not config.SetFactoryIfContained<StringValue, std::string>(m_factory,
        "Protocol", "Protocol"))
    {
        NS_ABORT_MSG("Packet sink '" << GetId() << "' requires a protocol");
    }
    if (not config.SetFactoryIfContained<AddressValue, InetSocketAddress>(m_factory,
        "Local", "Local"))
    {
        NS_ABORT_MSG("Packet sink '" << GetId() << "' requires a local address");
    }
    m_application = m_factory.Create<Application>();
}

void
E2EPacketSink::AddProbe(const E2EConfig& config)
{
    Ptr<PacketSink> sink = StaticCast<PacketSink>(m_application);

    std::string_view type;
    if (auto t {config.Find("Type")}; t)
    {
        type = (*t).value;
        (*t).processed = true;
    }
    else
    {
        NS_ABORT_MSG("Probe does not have a type");
    }

    if (type == "Rx")
    {
        Ptr<E2EPeriodicSampleProbe<uint32_t>> probe
            = Create<E2EPeriodicSampleProbe<uint32_t>>(config);
        sink->TraceConnectWithoutContext("Rx", MakeBoundCallback(TraceRx,
            E2EPeriodicSampleProbe<uint32_t>::AddValue, probe));
    }
}

E2EBulkSender::E2EBulkSender(const E2EConfig& config)
    : E2EApplication(config, "ns3::BulkSendApplication")
{
    if (not config.SetFactoryIfContained<StringValue, std::string>(m_factory,
        "Protocol", "Protocol"))
    {
        NS_ABORT_MSG("Bulk send application '" << GetId() << "' requires a protocol");
    }
    if (not config.SetFactoryIfContained<AddressValue, InetSocketAddress>(m_factory,
        "Remote", "Remote"))
    {
        NS_ABORT_MSG("Bulk send application '" << GetId() << "' requires a remote address");
    }
    config.SetFactoryIfContained<UintegerValue, unsigned>(m_factory, "SendSize", "SendSize");
    config.SetFactoryIfContained<UintegerValue, unsigned>(m_factory, "MaxBytes", "MaxBytes");
    m_application = m_factory.Create<Application>();
}

void
E2EBulkSender::AddProbe(const E2EConfig& config)
{
    Ptr<BulkSendApplication> sender = StaticCast<BulkSendApplication>(m_application);

    std::string_view type;
    if (auto t {config.Find("Type")}; t)
    {
        type = (*t).value;
        (*t).processed = true;
    }
    else
    {
        NS_ABORT_MSG("Probe does not have a type");
    }

    TimeValue startTimeV;
    sender->GetAttribute("StartTime", startTimeV);
    Time startTime = startTimeV.Get() + MilliSeconds(10);

    if (type == "RTT")
    {
        Ptr<E2EPeriodicSampleProbe<Time>> probe =
            Create<E2EPeriodicSampleProbe<Time>>(config,
                MakeBoundCallback(TimeWriter, Time::Unit::MS));
        Simulator::Schedule(startTime, ConnectTraceToSocket<BulkSendApplication, Time>, sender,
            "RTT", probe, E2EPeriodicSampleProbe<Time>::UpdateValue);
    }
    else if (type == "Cwnd")
    {
        Ptr<E2EPeriodicSampleProbe<uint32_t>> probe =
            Create<E2EPeriodicSampleProbe<uint32_t>>(config);
        Simulator::Schedule(startTime, ConnectTraceToSocket<BulkSendApplication, uint32_t>, sender,
            "CongestionWindow", probe, E2EPeriodicSampleProbe<uint32_t>::UpdateValue);
    }
}

E2EOnOffApp::E2EOnOffApp(const E2EConfig& config) : E2EApplication(config, "ns3::OnOffApplication")
{
    if (not config.SetFactoryIfContained<StringValue, std::string>(m_factory,
        "Protocol", "Protocol"))
    {
        NS_ABORT_MSG("OnOff application '" << GetId() << "' requires a protocol");
    }
    if (not config.SetFactoryIfContained<AddressValue, InetSocketAddress>(m_factory,
        "Remote", "Remote"))
    {
        NS_ABORT_MSG("OnOff application '" << GetId() << "' requires a remote address");
    }
    config.SetFactoryIfContained<DataRateValue, DataRate>(m_factory, "DataRate", "DataRate");
    config.SetFactoryIfContained<UintegerValue, unsigned>(m_factory, "MaxBytes", "MaxBytes");
    config.SetFactoryIfContained<UintegerValue, unsigned>(m_factory, "PacketSize", "PacketSize");
    config.SetFactoryIfContained<StringValue, std::string>(m_factory, "OnTime", "OnTime");
    config.SetFactoryIfContained<StringValue, std::string>(m_factory, "OffTime", "OffTime");
    
    m_application = m_factory.Create<Application>();
}

} // namespace ns3