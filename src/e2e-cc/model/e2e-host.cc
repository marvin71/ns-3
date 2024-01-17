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

#include "e2e-host.h"

#include "ns3/simbricks-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("E2EHost");

E2EHost::E2EHost(const E2EConfig& config) : E2EComponent(config)
{
    NS_ABORT_MSG_IF(GetId().size() == 0, "Host has no id");
    NS_ABORT_MSG_IF(GetIdPath().size() != 2,
        "Host '" << GetId() << "' has invalid path length of " << GetIdPath().size());
    NS_ABORT_MSG_IF(GetType().size() == 0, "Host '" << GetId() << "' has no type");
}

Ptr<E2EHost>
E2EHost::CreateHost(const E2EConfig& config)
{
    auto type_opt {config.Find("Type")};
    NS_ABORT_MSG_UNLESS(type_opt.has_value(), "Host has no type");
    std::string_view type {*type_opt};

    if (type == "Simbricks")
    {
        return Create<E2ESimbricksHost>(config);
    }
    else if (type == "SimpleNs3")
    {
        return Create<E2ESimpleNs3Host>(config);
    }
    else
    {
        NS_ABORT_MSG("Unkown host type '" << type << "'");
    }
}

Ptr<NetDevice>
E2EHost::GetNetDevice()
{
    return m_netDevice;
}

void
E2EHost::AddApplication(Ptr<E2EApplication> application)
{
    NS_ABORT_MSG("Applications are not supported for host '" << GetId()
        << "' with type '" << GetType() << "'");
}

E2ESimbricksHost::E2ESimbricksHost(const E2EConfig& config) : E2EHost(config)
{
    Ptr<simbricks::SimbricksNetDevice> netDevice =
        CreateObject<simbricks::SimbricksNetDevice>();
    if (not config.SetAttrIfContained<StringValue, std::string>(netDevice,
        "UnixSocket", "UnixSocket"))
    {
        NS_LOG_WARN("No Unix socket path for Simbricks host '" << GetId() << "' given.");
    }
    config.SetAttrIfContained<TimeValue, Time>(netDevice, "SyncDelay", "SyncDelay");
    config.SetAttrIfContained<TimeValue, Time>(netDevice, "PollDelay", "PollDelay");
    config.SetAttrIfContained<TimeValue, Time>(netDevice, "EthLatency", "EthLatency");
    config.SetAttrIfContained<IntegerValue, int>(netDevice, "Sync", "Sync");

    netDevice->Start();

    m_netDevice = netDevice;
}

E2ESimpleNs3Host::E2ESimpleNs3Host(const E2EConfig& config) : E2EHost(config)
{
    m_node = CreateObject<Node>();

    ObjectFactory deviceFactory;
    deviceFactory.SetTypeId("ns3::SimpleNetDevice");
    config.SetFactoryIfContained<DataRateValue, DataRate>(deviceFactory, "DataRate", "DataRate");
    //Todo: is setting the mtu of SimpleNetDevice supported this way? No, it is not!
    //config.SetFactoryIfContained<UintegerValue, unsigned>(deviceFactory, "Mtu", "Mtu");

    ObjectFactory queueFactory;
    queueFactory.SetTypeId("ns3::DropTailQueue<Packet>");
    config.SetFactoryIfContained<QueueSizeValue, QueueSize>(queueFactory, "QueueSize", "MaxSize");

    ObjectFactory channelFactory;
    channelFactory.SetTypeId("ns3::SimpleChannel");
    config.SetFactoryIfContained<TimeValue, Time>(channelFactory, "Delay", "Delay");

    // Create net devices
    Ptr<SimpleNetDevice> netDevice = deviceFactory.Create<SimpleNetDevice>();
    m_netDevice = netDevice;
    m_outerNetDevice = deviceFactory.Create<SimpleNetDevice>();
    //device->SetAttribute("PointToPointMode", BooleanValue(m_pointToPointMode));
    // Allocate and set mac addresses
    m_netDevice->SetAddress(Mac48Address::Allocate());
    m_outerNetDevice->SetAddress(Mac48Address::Allocate());

    m_node->AddDevice(m_outerNetDevice);

    m_channel = channelFactory.Create<SimpleChannel>();
    netDevice->SetChannel(m_channel);
    m_outerNetDevice->SetChannel(m_channel);
    Ptr<Queue<Packet>> innerQueue = queueFactory.Create<Queue<Packet>>();
    netDevice->SetQueue(innerQueue);
    Ptr<Queue<Packet>> outerQueue = queueFactory.Create<Queue<Packet>>();
    m_outerNetDevice->SetQueue(outerQueue);
    
    if (m_enableFlowControl)
    {
        // Aggregate a NetDeviceQueueInterface object
        Ptr<NetDeviceQueueInterface> innerNdqi = CreateObject<NetDeviceQueueInterface>();
        innerNdqi->GetTxQueue(0)->ConnectQueueTraces(innerQueue);
        m_netDevice->AggregateObject(innerNdqi);

        Ptr<NetDeviceQueueInterface> outerNdqi = CreateObject<NetDeviceQueueInterface>();
        outerNdqi->GetTxQueue(0)->ConnectQueueTraces(outerQueue);
        m_outerNetDevice->AggregateObject(outerNdqi);
    }

    // Set congestion control algorithm
    if (auto algo {config.Find("CongestionControl")}; algo)
    {
        TypeId tid = TypeId::LookupByName(std::string(*algo));
        std::stringstream nodeId;
        nodeId << m_node->GetId();
        std::string specificNode = "/NodeList/" + nodeId.str() + "/$ns3::TcpL4Protocol/SocketType";
        Config::Set(specificNode, TypeIdValue(tid));
    }

    SetIpAddress();
}

void
E2ESimpleNs3Host::AddApplication(Ptr<E2EApplication> application)
{
    m_node->AddApplication(application->GetApplication());
    AddE2EComponent(application);
}

void
E2ESimpleNs3Host::SetIpAddress()
{
    std::string_view ipString;
    if (auto ip {m_config.Find("Ip")}; ip)
    {
        ipString = *ip;
    }
    else
    {
        NS_LOG_WARN("No IP configuration found for node '" << GetId() << "'");
        return;
    }

    std::string_view ip {ipString};
    std::string_view netmask {ipString};
    auto pos {ip.find('/')};
    NS_ABORT_MSG_IF(pos == std::string_view::npos,
        "IP '" << ipString << "' for node '" << GetId() << "' is invalid");
    ip.remove_suffix(ip.size() - pos);
    netmask.remove_prefix(pos);

    InternetStackHelper stack;
    stack.Install(m_node);

    Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4,
                    "NetDevice is associated"
                    " with a node without IPv4 stack installed -> fail "
                    "(maybe need to use InternetStackHelper?)");

    int32_t interface = ipv4->GetInterfaceForDevice(m_outerNetDevice);
    if (interface == -1)
    {
        interface = ipv4->AddInterface(m_outerNetDevice);
    }
    NS_ASSERT_MSG(interface >= 0, "Interface index not found");

    Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress(Ipv4Address(std::string(ip).c_str()),
        Ipv4Mask(std::string(netmask).c_str()));
    ipv4->AddAddress(interface, ipv4Addr);
    ipv4->SetMetric(interface, 1);
    ipv4->SetUp(interface);
}

} // namespace ns3
