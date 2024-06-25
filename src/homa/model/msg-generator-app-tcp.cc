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

#include "msg-generator-app-tcp.h"
#include "msg-generator-tcp-header.h"

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
#include "ns3/tcp-socket-factory.h"
#include "ns3/homa-socket-factory.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/simple-net-device.h"
#include "ns3/trace-source-accessor.h"

#include <algorithm>
#include <sstream>

namespace ns3 {

// for easier printing of sockets
std::ostream&
operator<<(std::ostream& os, Ptr<Socket> socket)
{
    Address addr;
    if (!socket->GetPeerName(addr))
    {
        InetSocketAddress iaddr = InetSocketAddress::ConvertFrom(addr);
        return os << iaddr.GetIpv4() << ":" << iaddr.GetPort();
    }
    return os;
}

NS_LOG_COMPONENT_DEFINE ("MsgGeneratorAppTCP");

NS_OBJECT_ENSURE_REGISTERED (MsgGeneratorAppTCP);

TypeId
MsgGeneratorAppTCP::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MsgGeneratorAppTCP")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<MsgGeneratorAppTCP>()
    .AddAttribute ("Protocol", "The type of protocol to use. This should be "
                    "a subclass of ns3::SocketFactory",
                    TypeIdValue (TcpSocketFactory::GetTypeId()),
                    MakeTypeIdAccessor (&MsgGeneratorAppTCP::m_tid),
                    // This should check for SocketFactory as a parent
                    MakeTypeIdChecker ())
    .AddAttribute ("MaxMsg",
                   "The total number of messages to send. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MsgGeneratorAppTCP::m_maxMsgs),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute("Port",
                  "Port on which we listen for incoming packets.",
                  UintegerValue(0xffff),
                  MakeUintegerAccessor(&MsgGeneratorAppTCP::m_localPort),
                  MakeUintegerChecker<uint16_t>())
    .AddAttribute ("PayloadSize",
                   "MTU for the network interface excluding the header sizes",
                   UintegerValue (1400),
                   MakeUintegerAccessor (&MsgGeneratorAppTCP::m_maxPayloadSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RemoteClients",
                   "List of IP:Port combinations for the peers.",
                   //TypeId::ATTR_GET | TypeId::ATTR_SET, // do not set at construction time
                   AttributeContainerValue<StringValue>(),
                   MakeAttributeContainerAccessor<StringValue, ',', std::list>(
                      &MsgGeneratorAppTCP::SetRemoteClients,
                      &MsgGeneratorAppTCP::GetRemoteClients),
                MakeAttributeContainerChecker<StringValue, ',', std::list>(MakeStringChecker()))
    .AddAttribute ("Load",
                   "Link load",
                   DoubleValue (0.8),
                   MakeDoubleAccessor (&MsgGeneratorAppTCP::m_load),
                   MakeDoubleChecker<double>())
    .AddAttribute ("AvgMsgSizePkts",
                   "AvgMsgSizePkts",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&MsgGeneratorAppTCP::m_avgMsgSizePkts),
                   MakeDoubleChecker<double>())
    .AddAttribute ("MsgSizeCDF",
                   "Map for CDF of message sizes.",
                   //TypeId::ATTR_GET | TypeId::ATTR_SET, // do not set at construction time
                   AttributeContainerValue<TupleValue<DoubleValue,IntegerValue>, '+'>(),
                   MakeAttributeContainerAccessor<TupleValue<DoubleValue,IntegerValue>, '+', std::list>(
                      &MsgGeneratorAppTCP::SetMsgSizeCDF,
                      &MsgGeneratorAppTCP::GetMsgSizeCDF),
                   MakeAttributeContainerChecker<TupleValue<DoubleValue,IntegerValue>, '+', std::list>(
                      MakeTupleChecker<DoubleValue,IntegerValue>(
                        MakeDoubleChecker<double>(), MakeIntegerChecker<int>())))
    .AddAttribute ("MsgSizeDistFileName",
                   "File that contains the message size distribution",
                   StringValue (""),
                   MakeStringAccessor (&MsgGeneratorAppTCP::m_msgSizeDistFileName),
                   MakeStringChecker())
    .AddTraceSource("Tx",
                    "A new packet is sent",
                    MakeTraceSourceAccessor(&MsgGeneratorAppTCP::m_txTrace),
                    "ns3::Packet::TracedCallback")
    .AddTraceSource("Rx",
                    "A packet has been received",
                    MakeTraceSourceAccessor(&MsgGeneratorAppTCP::m_rxTrace),
                    "ns3::Packet::AddressTracedCallback")
    // this is to produce traces equivalent to the Homa ones
    .AddTraceSource ("MsgBegin",
                  "Trace source indicating a message has been delivered to "
                  "the HomaL4Protocol by the sender application layer.",
                  MakeTraceSourceAccessor (&MsgGeneratorAppTCP::m_msgBeginTrace),
                  "ns3::Packet::TracedCallback")
    .AddTraceSource ("MsgFinish",
                     "Trace source indicating a message has been delivered to "
                     "the receiver application by the HomaL4Protocol layer.",
                     MakeTraceSourceAccessor (&MsgGeneratorAppTCP::m_msgFinishTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("NoSpace",
                     "New message could not be scheduled since it does not fit into any"
                     "of the tcp socket send buffers.",
                     MakeTraceSourceAccessor (&MsgGeneratorAppTCP::m_noSpace),
                     "ns3::Packet::TracedCallback")
  ;
  ;
  return tid;
}

MsgGeneratorAppTCP::MsgGeneratorAppTCP()
  : m_socket_listen (0),
    m_interMsgTime (0),
    m_msgSizePkts (0),
    m_remoteClient (0),
    m_totMsgCnt (0),
    m_msgId (0)
{
  NS_LOG_FUNCTION (this);
}

MsgGeneratorAppTCP::~MsgGeneratorAppTCP()
{
  NS_LOG_FUNCTION (this);
}

std::vector<std::string> MsgGeneratorAppTCP::GetRemoteClients() const
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

void MsgGeneratorAppTCP::SetRemoteClients(std::vector<std::string> remoteClients)
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

std::vector<std::tuple<double,int>> MsgGeneratorAppTCP::GetMsgSizeCDF() const
{
  std::vector<std::tuple<double,int>> v;
  for (auto i: m_msgSizeCDF)
    v.push_back(std::make_tuple(i.first, i.second));
  return v;
}

void MsgGeneratorAppTCP::SetMsgSizeCDF(std::vector<std::tuple<double,int>> cdf)
{
  m_msgSizeCDF.clear();
  for (auto e: cdf)
    m_msgSizeCDF[std::get<0>(e)] = std::get<1>(e);
}

void MsgGeneratorAppTCP::ReadMsgSizeDist()
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

void MsgGeneratorAppTCP::Start (Time start)
{
  NS_LOG_FUNCTION (this);

  SetStartTime(start);
  DoInitialize();
}

void MsgGeneratorAppTCP::Stop (Time stop)
{
  NS_LOG_FUNCTION (this);

  SetStopTime(stop);
}

void MsgGeneratorAppTCP::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  CancelNextEvent ();
  // chain up
  Application::DoDispose ();
}

void MsgGeneratorAppTCP::StartApplication ()
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


  NS_LOG_DEBUG("Set socket: " << m_tid);


  m_socket_listen = Socket::CreateSocket (node, m_tid);
  m_socket_listen->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                              MakeCallback(&MsgGeneratorAppTCP::HandleAccept, this));
  m_socket_listen->SetCloseCallbacks(MakeCallback(&MsgGeneratorAppTCP::HandlePeerClose, this),
                              MakeCallback(&MsgGeneratorAppTCP::HandlePeerError, this));
  NS_LOG_INFO(Simulator::Now ().GetNanoSeconds () << " listen to " << m_localIp << " " << m_localPort);
  m_socket_listen->Bind (InetSocketAddress(m_localIp, m_localPort));
  int status = m_socket_listen->Listen();
  NS_ASSERT_MSG(!status, "m_socket_listen->Listen() failed");

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

  // Connect the sockets for sending
  for (size_t i = 0; i < m_remoteClients.size(); ++i)
  {
      Ptr<Socket> socket;
      socket = Socket::CreateSocket(node, m_tid);
      socket->SetConnectCallback(MakeCallback(&MsgGeneratorAppTCP::HandleConnectionSucceeded, this),
                                 MakeCallback(&MsgGeneratorAppTCP::HandleConnectionFailed, this));
      socket->Connect(m_remoteClients[i]);
      m_sockets_send.push_back(socket);
  }

  // give TCP sockets the chance to connect and apply an initial delay to prevent synchronous TCP
  // slow start
  Ptr<UniformRandomVariable> initialDelayDist = CreateObject<UniformRandomVariable>();
  uint32_t initialDelay = initialDelayDist->GetInteger(0, 1000);
  Simulator::Schedule(Seconds(1) + MilliSeconds(initialDelay), &MsgGeneratorAppTCP::ScheduleNextMessage, this);
}

void MsgGeneratorAppTCP::StopApplication ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << m_localIp);

  CancelNextEvent();
  for (auto socket : m_sockets_send)
  {
    socket->Close();
  }
  while (!m_sockets_accepted.empty()) // these are accepted sockets, close them
  {
    Ptr<Socket> acceptedSocket = m_sockets_accepted.front();
    m_sockets_accepted.pop_front();
    acceptedSocket->Close();
  }
  if (m_socket_listen)
  {
    m_socket_listen->Close();
    m_socket_listen->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
  }
}

void MsgGeneratorAppTCP::CancelNextEvent()
{
  NS_LOG_FUNCTION (this);

  if (!Simulator::IsExpired(m_nextSendEvent))
    Simulator::Cancel (m_nextSendEvent);
}

void MsgGeneratorAppTCP::ScheduleNextMessage ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << m_localIp);

  if (Simulator::IsExpired(m_nextSendEvent))
  {
    m_nextSendEvent = Simulator::Schedule (Seconds (m_interMsgTime->GetValue ()),
                                           &MsgGeneratorAppTCP::SendMessage, this);
  }
  else
  {
    NS_LOG_WARN("MsgGeneratorAppTCP (" << m_localIp <<
                ") tries to schedule the next msg before the previous one is sent!");
  }
}

uint32_t MsgGeneratorAppTCP::GetNextMsgSizeFromDist ()
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

  if (m_maxPayloadSize > 0) {
    return m_maxPayloadSize * (uint32_t)msgSizePkts;
  }
  else {
    return GetNode ()->GetDevice (0)->GetMtu () * (uint32_t)msgSizePkts;
  }

  // NOTE: If maxPayloadSize is not set, the generated messages will be
  //       slightly larger than the intended number of packets due to
  //       the addition of the protocol headers.
}

void
MsgGeneratorAppTCP::SendMessage()
{
    NS_LOG_FUNCTION(Simulator::Now().GetNanoSeconds() << m_localIp);

    /* Decide on the message size to send */
    uint32_t msgSizeBytes = GetNextMsgSizeFromDist();

    MsgGeneratorTCPHeader header{};

    // Don't send the next message on a socket that does not have enough capacity
    // in its buffer to store the messsage. Hopefully only a few sockets do not
    // have enough capacity. Need to investigate.
    std::vector<int> availableSockets{};
    for (size_t i = 0; i < m_sockets_send.size(); ++i)
    {
        if (m_sockets_send[i]->GetTxAvailable() < (msgSizeBytes + header.GetSerializedSize()))
        {
            continue;
        }
        availableSockets.emplace_back(i);
    }

    if (not availableSockets.empty())
    {
      if (availableSockets.size() != m_sockets_send.size())
        {
          m_noSpace(msgSizeBytes, m_localIp, m_msgId,
                    m_sockets_send.size() - availableSockets.size(), "some sockets full");
        }
        int idx = m_remoteClient->GetInteger(0, availableSockets.size() - 1);
        int remoteClientIdx = availableSockets[idx];
        InetSocketAddress receiverAddr = m_remoteClients[remoteClientIdx];

        

        /* Create the message to send */
        Ptr<Packet> msg = Create<Packet>(msgSizeBytes);
        NS_LOG_LOGIC("MsgGeneratorAppTCP {" << m_localIp << ") generates a message of size: "
                                            << msgSizeBytes << " Bytes.");

        /* add the Homa header for tracing */
        header.f_size = msgSizeBytes;
        header.f_id = m_msgId++;
        msg->AddHeader(header);

        Ptr<Socket> socket = m_sockets_send[remoteClientIdx];
        int sentBytes = socket->Send(msg);
        int expectedSent = msgSizeBytes + header.GetSerializedSize();
        NS_ASSERT_MSG(sentBytes == expectedSent,
                      "only sent " << sentBytes << " out of " << expectedSent << "Bytes");

        m_totMsgCnt++;
        m_txTrace(msg);
        m_msgBeginTrace(header.f_size, m_localIp, receiverAddr.GetIpv4(), 0, 0, header.f_id);
    }
    else
    {
      m_noSpace(msgSizeBytes, m_localIp, m_msgId, 0, "no space left");
    }

    if (m_maxMsgs == 0 || m_totMsgCnt < m_maxMsgs)
    {
        ScheduleNextMessage();
    }
}

void
MsgGeneratorAppTCP::ReceiveMessage(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this);

    Ptr<Packet> message;
    Address addr;
    socket->GetPeerName(addr);
    InetSocketAddress sender = InetSocketAddress::ConvertFrom(addr);
    while (true)
    {
        MsgGeneratorTCPHeader& header = recv_header.at(sender.GetIpv4().Get());
        /* receive header if we didn't get one yet*/
        if (!header.valid)
        {
            Ptr<Packet>& pp = partial_headers[sender.GetIpv4().Get()];
            int ppSize = 0;
            if (pp)
            {
              ppSize = pp->GetSize();
            }
            message = socket->Recv(header.GetSerializedSize() - ppSize, 0);
            if (!message)
            {
                break;
            }
            if (pp)
            {
                pp->AddAtEnd(message);
                message = pp;
            }
            if (message->GetSize() < header.GetSerializedSize())
            {
                if (not pp)
                {
                    pp = message;
                }
                break;
            }
            NS_ASSERT_MSG(message->GetSize() == header.GetSerializedSize(),
                          "message should exactly contain header");
            message->PeekHeader(header);
            header.valid = true;
            pp = nullptr;
        }

        message = socket->Recv(header.f_size - header.bytes_recvd, 0);
        if (!message) {
          break;
        }

        m_rxTrace(message, addr);
        NS_LOG_INFO(Simulator::Now().GetNanoSeconds()
                    << " client received " << message->GetSize() << " bytes from " << socket);

        header.bytes_recvd += message->GetSize();

        /* flow completed, trace completion */
        if (header.bytes_recvd >= header.f_size) {
            NS_ASSERT_MSG(header.bytes_recvd == header.f_size,
                          "bytes_received=" << header.bytes_recvd << " f_size=" << header.f_size);
            m_msgFinishTrace(header.f_size, sender.GetIpv4(), m_localIp, 0, 0, header.f_id);

            header.valid = false;
            header.bytes_recvd = 0;
        }
    }
}

void
MsgGeneratorAppTCP::HandlePeerClose(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
MsgGeneratorAppTCP::HandlePeerError(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
MsgGeneratorAppTCP::HandleConnectionSucceeded(Ptr<Socket> socket)
{
    num_connected++;
    NS_LOG_DEBUG(Simulator::Now().GetNanoSeconds()
                 << "ns (" << m_localIp << ") Connection established to " << socket
                 << ". Currently connected: " << num_connected << " of " << m_sockets_send.size());
}

void
MsgGeneratorAppTCP::HandleConnectionFailed(Ptr<Socket> socket)
{
    NS_ABORT_MSG_IF(false, "connecting to " << socket << " failed");
}

void
MsgGeneratorAppTCP::HandleAccept(Ptr<Socket> s, const Address& from)
{
    recv_header.emplace(InetSocketAddress::ConvertFrom(from).GetIpv4().Get(),
                        MsgGeneratorTCPHeader{});
    partial_headers.emplace(InetSocketAddress::ConvertFrom(from).GetIpv4().Get(),
                            nullptr);
    s->SetRecvCallback(MakeCallback(&MsgGeneratorAppTCP::ReceiveMessage, this));
    s->SetCloseCallbacks(MakeCallback(&MsgGeneratorAppTCP::HandlePeerClose, this),
                         MakeCallback(&MsgGeneratorAppTCP::HandlePeerError, this));
    m_sockets_accepted.push_back(s);

    NS_LOG_INFO(Simulator::Now().GetNanoSeconds()
                << "ns (" << m_localIp << ") Accept "
                << InetSocketAddress::ConvertFrom(from).GetIpv4() << ":"
                << InetSocketAddress::ConvertFrom(from).GetPort());
}

} // Namespace ns3
