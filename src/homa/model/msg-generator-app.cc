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

#include "msg-generator-app.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/callback.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/attribute-container.h"
#include "ns3/tuple.h"
#include "ns3/ipv4.h"

#include "ns3/udp-socket-factory.h"
#include "ns3/homa-socket-factory.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/simple-net-device.h"
#include "ns3/trace-source-accessor.h"


#include <algorithm>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MsgGeneratorApp");

NS_OBJECT_ENSURE_REGISTERED (MsgGeneratorApp);

TypeId
MsgGeneratorApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MsgGeneratorApp")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<MsgGeneratorApp>()
    .AddAttribute ("Protocol", "The type of protocol to use. This should be "
                    "a subclass of ns3::SocketFactory",
                    TypeIdValue (HomaSocketFactory::GetTypeId ()),
                    MakeTypeIdAccessor (&MsgGeneratorApp::m_tid),
                    // This should check for SocketFactory as a parent
                    MakeTypeIdChecker ())
    .AddAttribute ("MaxMsg",
                   "The total number of messages to send. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MsgGeneratorApp::m_maxMsgs),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute("Port",
                  "Port on which we listen for incoming packets.",
                  UintegerValue(0xffff),
                  MakeUintegerAccessor(&MsgGeneratorApp::m_localPort),
                  MakeUintegerChecker<uint16_t>())
    .AddAttribute ("PayloadSize",
                   "MTU for the network interface excluding the header sizes",
                   UintegerValue (1400),
                   MakeUintegerAccessor (&MsgGeneratorApp::m_maxPayloadSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RemoteClients",
                   "List of IP:Port combinations for the peers.",
                   //TypeId::ATTR_GET | TypeId::ATTR_SET, // do not set at construction time
                   AttributeContainerValue<StringValue>(),
                   MakeAttributeContainerAccessor<StringValue, ',', std::list>(
                      &MsgGeneratorApp::SetRemoteClients,
                      &MsgGeneratorApp::GetRemoteClients),
                MakeAttributeContainerChecker<StringValue, ',', std::list>(MakeStringChecker()))
    .AddAttribute ("Load",
                   "Link load",
                   DoubleValue (0.8),
                   MakeDoubleAccessor (&MsgGeneratorApp::m_load),
                   MakeDoubleChecker<double>())
    .AddAttribute ("AvgMsgSizePkts",
                   "AvgMsgSizePkts",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&MsgGeneratorApp::m_avgMsgSizePkts),
                   MakeDoubleChecker<double>())
    .AddAttribute ("MsgSizeCDF",
                   "Map for CDF of message sizes.",
                   //TypeId::ATTR_GET | TypeId::ATTR_SET, // do not set at construction time
                   AttributeContainerValue<TupleValue<DoubleValue,IntegerValue>, '+'>(),
                   MakeAttributeContainerAccessor<TupleValue<DoubleValue,IntegerValue>, '+', std::list>(
                      &MsgGeneratorApp::SetMsgSizeCDF,
                      &MsgGeneratorApp::GetMsgSizeCDF),
                   MakeAttributeContainerChecker<TupleValue<DoubleValue,IntegerValue>, '+', std::list>(
                      MakeTupleChecker<DoubleValue,IntegerValue>(
                        MakeDoubleChecker<double>(), MakeIntegerChecker<int>())))
    .AddAttribute ("MsgSizeDistFileName",
                   "File that contains the message size distribution",
                   StringValue (""),
                   MakeStringAccessor (&MsgGeneratorApp::m_msgSizeDistFileName),
                   MakeStringChecker())
    .AddTraceSource("Tx",
                    "A new packet is sent",
                    MakeTraceSourceAccessor(&MsgGeneratorApp::m_txTrace),
                    "ns3::Packet::TracedCallback")
    .AddTraceSource("Rx",
                    "A packet has been received",
                    MakeTraceSourceAccessor(&MsgGeneratorApp::m_rxTrace),
                    "ns3::Packet::AddressTracedCallback")                    
  ;
  ;
  return tid;
}

MsgGeneratorApp::MsgGeneratorApp()
  : m_socket (0),
    m_interMsgTime (0),
    m_msgSizePkts (0),
    m_remoteClient (0),
    m_totMsgCnt (0)
{
  NS_LOG_FUNCTION (this);
}

MsgGeneratorApp::~MsgGeneratorApp()
{
  NS_LOG_FUNCTION (this);
}

std::vector<std::string> MsgGeneratorApp::GetRemoteClients() const
{
  std::vector<std::string> v;
  for (auto rc: m_remoteClients) {
    std::stringstream ss;
    rc.GetIpv4().Print(ss);
    ss << ":" << rc.GetPort();
    v.push_back(ss.str());
  }
  return v;
}

void MsgGeneratorApp::SetRemoteClients(std::vector<std::string> remoteClients)
{
  m_remoteClients.clear();
  for (auto rc: remoteClients) {
    std::size_t colon = rc.find(':');
    NS_ABORT_MSG_IF (colon == std::string::npos,
      "No : found in RemoteClient spec");
    Ipv4Address ip(rc.substr(0, colon).c_str());
    uint16_t port = std::stoul(rc.substr(colon + 1));
    m_remoteClients.push_back(InetSocketAddress(ip, port));
  }
}

std::vector<std::tuple<double,int>> MsgGeneratorApp::GetMsgSizeCDF() const
{
  std::vector<std::tuple<double,int>> v;
  for (auto i: m_msgSizeCDF)
    v.push_back(std::make_tuple(i.first, i.second));
  return v;
}

void MsgGeneratorApp::SetMsgSizeCDF(std::vector<std::tuple<double,int>> cdf)
{
  m_msgSizeCDF.clear();
  for (auto e: cdf)
    m_msgSizeCDF[std::get<0>(e)] = std::get<1>(e);
}

void MsgGeneratorApp::ReadMsgSizeDist()
{
  NS_ABORT_MSG_UNLESS(m_msgSizeCDF.empty(), "Message size CDF already contains data");
  std::ifstream msgSizeDistFile;
  msgSizeDistFile.open(m_msgSizeDistFileName);
  NS_LOG_FUNCTION("Reading Msg Size Distribution From: " << m_msgSizeDistFileName);

  std::string line;
  std::istringstream lineBuffer;

  getline(msgSizeDistFile, line);
  lineBuffer.str(line);
  lineBuffer >> m_avgMsgSizePkts;

  double prob;
  int msgSizePkts;
  while (getline(msgSizeDistFile, line))
  {
    lineBuffer.clear();
    lineBuffer.str(line);
    lineBuffer >> msgSizePkts;
    lineBuffer >> prob;

    m_msgSizeCDF[prob] = msgSizePkts;
  }
  msgSizeDistFile.close();
}

void MsgGeneratorApp::Start (Time start)
{
  NS_LOG_FUNCTION (this);

  SetStartTime(start);
  DoInitialize();
}

void MsgGeneratorApp::Stop (Time stop)
{
  NS_LOG_FUNCTION (this);

  SetStopTime(stop);
}

void MsgGeneratorApp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  CancelNextEvent ();
  // chain up
  Application::DoDispose ();
}

void MsgGeneratorApp::StartApplication ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);

  Ptr<Node> node = GetNode();

  if (not m_msgSizeDistFileName.empty())
  {
    ReadMsgSizeDist();
  }

  NS_ABORT_MSG_IF(m_msgSizeCDF.empty(), "No message size CDF Set");

  ///
  m_load = std::max(0.0, std::min(m_load, 1.0));

  Ptr<NetDevice> netDevice = node->GetDevice (0);
  uint32_t mtu = netDevice->GetMtu ();

  uint64_t txRate;
  if (PointToPointNetDevice* p2pNetDevice = dynamic_cast<PointToPointNetDevice*>(&(*(netDevice)))) {
    txRate = p2pNetDevice->GetDataRate ().GetBitRate ();
  } else if (SimpleNetDevice* simpleNetDevice = dynamic_cast<SimpleNetDevice*>(&(*(netDevice)))) {
    txRate = simpleNetDevice->GetDataRate ().GetBitRate ();
  } else {
    NS_ABORT_MSG ("Unsupported net device");
  }
  NS_LOG_DEBUG("txRate: " << txRate);

  double avgPktLoadBytes = (double)(mtu + 64); // Account for the ctrl pkts each data pkt induce
  double avgInterMsgTime = (m_avgMsgSizePkts * avgPktLoadBytes * 8.0 ) / (((double)txRate) * m_load);

  m_interMsgTime = CreateObject<ExponentialRandomVariable> ();
  m_interMsgTime->SetAttribute ("Mean", DoubleValue (avgInterMsgTime));

  m_msgSizePkts = CreateObject<UniformRandomVariable> ();
  m_msgSizePkts->SetAttribute ("Min", DoubleValue (0));
  m_msgSizePkts->SetAttribute ("Max", DoubleValue (1));

  //////////////////

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  m_localIp = ipv4->GetAddress (1,0).GetLocal();

  m_socket = Socket::CreateSocket (node, m_tid);
  m_socket->Bind (InetSocketAddress(m_localIp, m_localPort));
  m_socket->SetRecvCallback (MakeCallback (&MsgGeneratorApp::ReceiveMessage, this));

  m_remoteClients.erase(
    std::remove_if(
        m_remoteClients.begin(),
        m_remoteClients.end(),
        [this](InetSocketAddress const &a) {
          return a.GetIpv4() == m_localIp && a.GetPort() == m_localPort;
        }
    ),
    m_remoteClients.end()
  );
  NS_ABORT_MSG_IF(m_remoteClients.empty(), "No remote clients");

  m_remoteClient = CreateObject<UniformRandomVariable> ();
  m_remoteClient->SetAttribute ("Min", DoubleValue (0));
  m_remoteClient->SetAttribute ("Max", DoubleValue (m_remoteClients.size()));

  ScheduleNextMessage ();
}

void MsgGeneratorApp::StopApplication ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);

  CancelNextEvent();
}

void MsgGeneratorApp::CancelNextEvent()
{
  NS_LOG_FUNCTION (this);

  if (!Simulator::IsExpired(m_nextSendEvent))
    Simulator::Cancel (m_nextSendEvent);
}

void MsgGeneratorApp::ScheduleNextMessage ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);

  if (Simulator::IsExpired(m_nextSendEvent))
  {
    m_nextSendEvent = Simulator::Schedule (Seconds (m_interMsgTime->GetValue ()),
                                           &MsgGeneratorApp::SendMessage, this);
  }
  else
  {
    NS_LOG_WARN("MsgGeneratorApp (" << this <<
                ") tries to schedule the next msg before the previous one is sent!");
  }
}

uint32_t MsgGeneratorApp::GetNextMsgSizeFromDist ()
{
  NS_LOG_FUNCTION(this);

  int msgSizePkts = -1;
  double rndValue = m_msgSizePkts->GetValue();
  for (auto it = m_msgSizeCDF.begin(); it != m_msgSizeCDF.end(); it++)
  {
    if (rndValue <= it->first)
    {
      msgSizePkts = it->second;
      break;
    }
  }

  NS_ASSERT(msgSizePkts >= 0);
  // Homa header can't handle msgs larger than 0xffff pkts
  msgSizePkts = std::min(0xffff, msgSizePkts);

  if (m_maxPayloadSize > 0)
    return m_maxPayloadSize * (uint32_t)msgSizePkts;
  else
    return GetNode ()->GetDevice (0)->GetMtu () * (uint32_t)msgSizePkts;

  // NOTE: If maxPayloadSize is not set, the generated messages will be
  //       slightly larger than the intended number of packets due to
  //       the addition of the protocol headers.
}

void MsgGeneratorApp::SendMessage ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);

  /* Decide which remote client to send to */
  double rndValue = m_remoteClient->GetValue ();
  int remoteClientIdx = (int) std::floor(rndValue);
  Address receiverAddr = m_remoteClients[remoteClientIdx];

  /* Decide on the message size to send */
  uint32_t msgSizeBytes = GetNextMsgSizeFromDist ();

  /* Create the message to send */
  Ptr<Packet> msg = Create<Packet> (msgSizeBytes);
  NS_LOG_LOGIC ("MsgGeneratorApp {" << this << ") generates a message of size: "
                << msgSizeBytes << " Bytes.");

  int sentBytes = m_socket->SendTo (msg, 0, receiverAddr);

  if (sentBytes > 0)
  {
    m_txTrace(msg);
    NS_LOG_INFO(sentBytes << " Bytes sent to " << receiverAddr);
    m_totMsgCnt++;
  }

  if (m_maxMsgs == 0 || m_totMsgCnt < m_maxMsgs)
  {
    ScheduleNextMessage ();
  }
}

void MsgGeneratorApp::ReceiveMessage (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this);

  Ptr<Packet> message;
  Address from;
  while ((message = socket->RecvFrom (from)))
  {
    m_rxTrace(message, from);
    NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () <<
                 " client received " << message->GetSize () << " bytes from " <<
                 InetSocketAddress::ConvertFrom (from).GetIpv4 () << ":" <<
                 InetSocketAddress::ConvertFrom (from).GetPort ());
  }
}

} // Namespace ns3
