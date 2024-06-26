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

#ifndef E2E_HOST_H
#define E2E_HOST_H

#include "e2e-application.h"
#include "e2e-component.h"

#include "ns3/simple-channel.h"
#include "ns3/simple-net-device.h"

namespace ns3
{

/* ... */
class E2EHost : public E2EComponent
{
  public:
    E2EHost(const E2EConfig& config);

    static Ptr<E2EHost> CreateHost(const E2EConfig& config);

    Ptr<NetDevice> GetNetDevice();
    virtual Ptr<Node> GetNode();
    virtual void AddApplication(Ptr<E2EApplication> application);

  protected:
    Ptr<NetDevice> m_netDevice;
};

class E2ESimbricksHost : public E2EHost
{
  public:
    E2ESimbricksHost(const E2EConfig& config);
};

class E2ESimpleNs3Host : public E2EHost
{
  public:
    E2ESimpleNs3Host(const E2EConfig& config);

    Ptr<Node> GetNode() override;
    void AddApplication(Ptr<E2EApplication> application) override;

  private:
    Ptr<Node> m_node;
    Ptr<SimpleNetDevice> m_outerNetDevice;
    Ptr<SimpleChannel> m_channel;

    bool m_enableFlowControl = true;

    void SetIpAddress();
};

} // namespace ns3

#endif /* E2E_HOST_H */