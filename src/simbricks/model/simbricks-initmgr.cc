/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

/*
 * Copyright 2020 Max Planck Institute for Software Systems
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

#include "simbricks-initmgr.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"

#include <iostream>
#include <csignal>
#include <poll.h>


namespace ns3 {
namespace simbricks {
namespace base {

static InitManager im_instance;

static void sigusr1_handler(int dummy)
{
  InitManager &mgr = InitManager::get();

  std::cout << "STATS: main_time=" << Simulator::Now ().ToInteger (Time::PS)
    << " tsc=" << rdtsc() << std::endl;

  uint64_t tx_comm_cycles = 0;
  uint64_t tx_block_cycles = 0;
  uint64_t tx_sync_cycles = 0;
  uint64_t rx_comm_cycles = 0;
  uint64_t rx_block_cycles = 0;

  for (Adapter *a: mgr.ready) {
    std::cout << "  " << a->getSocketPath() << ":"
      << " tx_comm_cycles=" << a->getCyclesTxComm()
      << " tx_block_cycles=" << a->getCyclesTxBlock()
      << " tx_sync_cycles=" << a->getCyclesTxSync()
      << " rx_comm_cycles=" << a->getCyclesRxComm()
      << " rx_block_cycles=" << a->getCyclesRxBlock()
      << std::endl;
    tx_comm_cycles += a->getCyclesTxComm();
    tx_block_cycles += a->getCyclesTxBlock();
    tx_sync_cycles += a->getCyclesTxSync();
    rx_comm_cycles += a->getCyclesRxComm();
    rx_block_cycles += a->getCyclesRxBlock();
  }

  std::cout << "  TOTAL:"
    << " tx_comm_cycles=" << tx_comm_cycles
    << " tx_block_cycles=" << tx_block_cycles
    << " tx_sync_cycles=" << tx_sync_cycles
    << " rx_comm_cycles=" << rx_comm_cycles
    << " rx_block_cycles=" << rx_block_cycles
    << std::endl;
}

InitManager::InitManager()
{
    static bool exists = false;
    NS_ABORT_MSG_IF (exists, "Only one InitManager instance must exist");
    exists = true;

    // also register usr1 signal handler (once)
    signal(SIGUSR1, sigusr1_handler);
}

InitManager& InitManager::get()
{
    return im_instance;
}

void InitManager::registerAdapter(Adapter &a)
{
    unconnected.insert(&a);
}

void InitManager::connected(Adapter &a)
{
    waitRx.insert(&a);
    unconnected.erase(&a);

    const unsigned max_handshake = 4069;
    std::vector<uint8_t> handshake(max_handshake);
    size_t len = a.introOutPrepare(handshake.data(), max_handshake);
    if (SimbricksBaseIfIntroSend(&a.baseIf, handshake.data(), len) != 0) {
        NS_ABORT_MSG("SimbricksBaseIfIntroSend failed");
    }
}

void InitManager::processEvents()
{
    nfds_t n = unconnected.size() + waitRx.size();
    NS_ABORT_MSG_IF(n == 0, "processEvents called without pending adapters");

    struct pollfd pfds[n];
    Adapter *as[n];
    bool conn[n];

    nfds_t i = 0;
    // add poll events for connecting adapters
    for (auto it = unconnected.begin(); it != unconnected.end(); ) {
        Adapter *a = *it;
        ++it;

        int fd = SimbricksBaseIfConnFd(&a->baseIf);
        if (fd < 0) {
            // may already be connected
            if (SimbricksBaseIfConnected(&a->baseIf) == 0) {
                connected(*a);
            } else {
                NS_ABORT_MSG("SimbricksBaseIfConnFd failed for unconnected");
            }
        } else {
            pfds[i].fd = fd;
            pfds[i].events = (a->isListen ? POLLIN : POLLOUT);
            pfds[i].revents = 0;
            as[i] = a;
            conn[i] = true;
            i++;
        }
    }

    // add poll events for handshaking adapters
    for (auto it = waitRx.begin(); it != waitRx.end(); ) {
        Adapter *a = *it;
        ++it;

        int fd = SimbricksBaseIfIntroFd(&a->baseIf);
        NS_ABORT_MSG_IF(fd < 0,
          "SimbricksBaseIfIntroFd failed for unconnected");

        pfds[i].fd = fd;
        pfds[i].events = POLLIN;
        pfds[i].revents = 0;
        as[i] = a;
        conn[i] = false;
        i++;
    }

    if (i == 0) {
        return;
    }

    int ret = poll(pfds, i, -1);
    NS_ABORT_MSG_IF(ret < 0, "poll failed");

    const unsigned max_handshake = 4069;
    std::vector<uint8_t> handshake(max_handshake);

    for (int k = 0; k < ret; k++) {
        Adapter *a = as[k];
        if (pfds[k].revents == 0) {
            continue;
        }
        if ((pfds[k].revents & (~(POLLIN | POLLOUT))) != 0) {
            NS_ABORT_MSG("invalid revents");
        }

        if (conn[k]) {
            int x = SimbricksBaseIfConnected(&a->baseIf);
            if (x == 0) {
                // one done :-)
                connected(*a);
            } else if (x < 0) {
                NS_ABORT_MSG("SimbricksBaseIfConnected failed");
            }
        } else {
            size_t len = max_handshake;
            int x =
                SimbricksBaseIfIntroRecv(&a->baseIf, handshake.data(), &len);
            if (x == 0) {
                // one done
                a->introInReceived(handshake.data(), len);
                waitRx.erase(a);
                ready.insert(a);
            } else if (x < 0) {
                NS_ABORT_MSG("SimbricksBaseIfIntroRecv failed");
            }
        }
    }
}

void
InitManager::waitReady(Adapter &a)
{
    // process events until this adapter is ready, in the process others may
    // be ready too, in which case they will already be removed from the lists

    while (true) {
        auto i = unconnected.find(&a);
        auto j = waitRx.find(&a);
        if (i == unconnected.end() && j == waitRx.end()) {
            // is in neither set, we're already done.
            return;
        }

        processEvents();
    }
}

} // namespace base
} // namespace simbricks
} // namespace gem5
