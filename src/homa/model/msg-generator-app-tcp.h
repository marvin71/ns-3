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

#ifndef MSG_GENERATOR_APP_TCP_H
#define MSG_GENERATOR_APP_TCP_H

#include <cstddef>
#include <map>
#include <unordered_map>

#include "ns3/application.h"
#include "ns3/random-variable-stream.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/event-id.h"
#include "ns3/traced-callback.h"

#include "msg-generator-tcp-header.h"


namespace ns3 {

class RandomVariableStream;
class InetSocketAddress;

/**
 * \ingroup applications
 * \defgroup msg-generator-app MsgGeneratorApp
 *
 * This application generates messages according
 * to a given workload (message rate and message
 * size) distribution. In addition to sending
 * messages into the network, the application is
 * also able to receive them.
 */
class MsgGeneratorAppTCP : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  MsgGeneratorAppTCP ();

  virtual ~MsgGeneratorAppTCP();

  std::vector<std::string> GetRemoteClients() const;
  void SetRemoteClients(std::vector<std::string> remoteClients);

  std::vector<std::tuple<double,int>> GetMsgSizeCDF() const;
  void SetMsgSizeCDF(std::vector<std::tuple<double,int>> cdf);

  void ReadMsgSizeDist();

  /*void SetWorkload (double load,
                    std::map<double,int> msgSizeCDF,
                    double avgMsgSizePkts);*/

  void Start (Time start);

  void Stop (Time stop);

protected:
  virtual void DoDispose (void);
private:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop

  //helpers
  /**
   * \brief Cancel the pending event.
   */
  void CancelNextEvent ();

  /**
   * \brief Schedule the next message to send.
   */
  void ScheduleNextMessage ();

  /**
   * \brief Determine the next msg size in bytes based on the set workload
   */
  uint32_t GetNextMsgSizeFromDist ();

  /**
   * \brief Send a message
   */
  void SendMessage ();

  /**
   * \brief Receive a message from the protocol socket
   */
  void ReceiveMessage (Ptr<Socket> socket);
    /**
   * \brief Handle an incoming connection
   * \param socket the incoming connection socket
   * \param from the address the connection is from
   */
  void HandleAccept(Ptr<Socket> socket, const Address& from);
  /**
   * \brief Handle an connection close
   * \param socket the connected socket
   */
  void HandlePeerClose(Ptr<Socket> socket);
  /**
   * \brief Handle an connection error
   * \param socket the connected socket
   */
  void HandlePeerError(Ptr<Socket> socket);

  void HandleConnectionSucceeded(Ptr<Socket> socket);

  void HandleConnectionFailed(Ptr<Socket> socket);


  Ptr<Socket>       m_socket_listen;        //!< The socket this app uses to Listen
  std::list<Ptr<Socket>> m_sockets_accepted; //!< The accepted sockets, order doesn't indicate anything

  std::vector<Ptr<Socket>>       m_sockets_send;        //!< The socket list this app uses to send msgs
  TypeId            m_tid;           //!< The type of the socket used
  EventId           m_nextSendEvent; //!< Event id of pending "send msg" event

  Ipv4Address     m_localIp;         //!< Local IP address to bind
  uint16_t        m_localPort;       //!< Local port number to bind
  std::vector<InetSocketAddress> m_remoteClients; //!< List of clients that this app can send to
  std::map<double,int> m_msgSizeCDF; //!< The CDF of msg sizes {cum. prob. -> msg size in pkts}
  std::string     m_msgSizeDistFileName; //!< The file to read message size distribution from

  // map from IPv4 of sender to application header from first packet of current message
  std::unordered_map<uint32_t, MsgGeneratorTCPHeader> recv_header{};

  Ptr<ExponentialRandomVariable>  m_interMsgTime; //!< rng for rate of message generation in sec/msg
  Ptr<UniformRandomVariable>      m_msgSizePkts;  //!< rng to choose msg size from the set workload
  Ptr<UniformRandomVariable>      m_remoteClient; //!< rng to choose remote client to send msg to

  uint32_t          m_maxPayloadSize;//!< Maximum size of packet payloads
  uint16_t          m_totMsgCnt;     //!< Total number of messages sent so far
  uint16_t          m_maxMsgs;       //!< Maximum number of messages allowed to be sent
  uint16_t          m_msgId;         //!< The next message id
  size_t num_connected = 0;

    /// Traced Callback: sent packets
  TracedCallback<Ptr<const Packet>> m_txTrace;
    /// Traced Callback: received packets, source address.
  TracedCallback<Ptr<const Packet>, const Address&> m_rxTrace;

  TracedCallback<uint32_t, const Ipv4Address&, const Ipv4Address&, uint16_t, uint16_t, int>
      m_msgBeginTrace;
  TracedCallback<uint32_t, const Ipv4Address&, const Ipv4Address&, uint16_t, uint16_t, int>
      m_msgFinishTrace;

  double m_avgMsgSizePkts;
  double m_load;
};

} // namespace ns3
#endif
