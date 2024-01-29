/*
 * Copyright (c) 2024 Max Plank Institute for Software Systems
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
 */

#ifndef HOMA_PFIFO_QUEUE_H
#define HOMA_PFIFO_QUEUE_H

#include <map>
#include <queue>

#include "ns3/queue.h"
#include "ns3/simulator.h"

namespace ns3
{

uint8_t HomaQPktTos_(Ptr<Packet> packet, uint8_t m_bands);

/* avoid warnings for "unused function" here */
template <typename Item>
static inline uint8_t HomaQPktTos(Ptr<Item> it, uint8_t m_bands)
  __attribute__((used));
template <>
uint8_t HomaQPktTos<Packet>(Ptr<Packet> packet, uint8_t m_bands) __attribute__((used));

template <typename Item>
static inline uint8_t HomaQPktTos(Ptr<Item> it, uint8_t m_bands) {
  return 0;
}

template <>
uint8_t HomaQPktTos<Packet>(Ptr<Packet> packet, uint8_t m_bands) {
  return HomaQPktTos_(packet, m_bands);
}


template <typename Item>
class HomaPFifoQueue : public Queue<Item>
{
  public:
    static TypeId GetTypeId();

    HomaPFifoQueue();
    ~HomaPFifoQueue() override;

    bool Enqueue(Ptr<Item> packet) override;
    Ptr<Item> Dequeue() override;
    Ptr<Item> Remove() override;
    Ptr<const Item> Peek() const override;

    void SetBands(uint8_t max);
    uint8_t GetBands() const;

    using Queue<Item>::GetCurrentSize;
    using Queue<Item>::GetMaxSize;

  protected:
    uint8_t m_bands;
    std::vector<std::list<Ptr<Item>>> m_queues;

    using Queue<Item>::m_nBytes;
    using Queue<Item>::m_nTotalReceivedBytes;
    using Queue<Item>::m_nPackets;
    using Queue<Item>::m_nTotalReceivedPackets;
    using Queue<Item>::m_traceEnqueue;
    using Queue<Item>::m_traceDequeue;
    using Queue<Item>::DropBeforeEnqueue;
    using Queue<Item>::DropAfterDequeue;

  private:
    NS_LOG_TEMPLATE_DECLARE; //!< redefinition of the log component
};

/**
 * Implementation of the templates declared above.
 */

template <typename Item>
TypeId
HomaPFifoQueue<Item>::GetTypeId()
{
    static TypeId tid =
        TypeId(GetTemplateClassName<HomaPFifoQueue<Item>>())
            .SetParent<Queue<Item>>()
            .SetGroupName("homa")
            .template AddConstructor<HomaPFifoQueue<Item>>()
            .AddAttribute("MaxSize",
                          "The max queue size",
                          QueueSizeValue(QueueSize("100p")),
                          MakeQueueSizeAccessor(&QueueBase::SetMaxSize, &QueueBase::GetMaxSize),
                          MakeQueueSizeChecker())
            .AddAttribute("NumBands",
                          "NUmber of band queues",
                          UintegerValue(8),
                          MakeUintegerAccessor(&HomaPFifoQueue<Item>::SetBands, &HomaPFifoQueue<Item>::GetBands),
                          MakeUintegerChecker<uint8_t> ())
            ;
    return tid;
}

template <typename Item>
HomaPFifoQueue<Item>::HomaPFifoQueue()
    : Queue<Item>(),
      NS_LOG_TEMPLATE_DEFINE("HomaPFifoQueue")
{
    NS_LOG_FUNCTION(this);
}

template <typename Item>
HomaPFifoQueue<Item>::~HomaPFifoQueue()
{
    NS_LOG_FUNCTION(this);
}

template <typename Item>
void
HomaPFifoQueue<Item>::SetBands(uint8_t bands)
{
    m_bands = bands;
    m_queues.resize(bands);
}

template <typename Item>
uint8_t
HomaPFifoQueue<Item>::GetBands() const
{
    return m_bands;
}

template <typename Item>
bool
HomaPFifoQueue<Item>::Enqueue(Ptr<Item> item)
{
    NS_LOG_FUNCTION(this << item);

    uint8_t band = HomaQPktTos(item, m_bands);

    if (GetCurrentSize() + item > GetMaxSize())
    {
        NS_LOG_LOGIC("Queue full -- dropping pkt");
        DropBeforeEnqueue(item);
        return false;
    }

    m_queues[band].push_back(item);

    uint32_t size = item->GetSize();
    m_nBytes += size;
    m_nTotalReceivedBytes += size;

    m_nPackets++;
    m_nTotalReceivedPackets++;

    NS_LOG_LOGIC("Pushed " << item << " into band " << band);

    NS_LOG_LOGIC("m_traceEnqueue (p)");
    m_traceEnqueue(item);

    return true;
}

template <typename Item>
Ptr<Item>
HomaPFifoQueue<Item>::Dequeue()
{
    NS_LOG_FUNCTION(this);

    if (m_nPackets.Get() == 0)
    {
        NS_LOG_LOGIC("Queue empty");
        return nullptr;
    }

    Ptr<Item> item = nullptr;

    int band;
    for (band = m_bands - 1; band >= 0; band--) {
      if (!m_queues[band].empty()) {
        item = m_queues[band].front();
        m_queues[band].pop_front();
        break;
      }
    }

    NS_ASSERT(item);

    NS_ASSERT(m_nBytes.Get() >= item->GetSize());
    NS_ASSERT(m_nPackets.Get() > 0);

    m_nBytes -= item->GetSize();
    m_nPackets--;

    NS_LOG_LOGIC("m_traceDequeue (p)");
    m_traceDequeue(item);

    NS_LOG_LOGIC("Popped " << item << " from band " << band);

    return item;
}

template <typename Item>
Ptr<Item>
HomaPFifoQueue<Item>::Remove()
{
    NS_LOG_FUNCTION(this);

    if (m_nPackets.Get() == 0)
    {
        NS_LOG_LOGIC("Queue empty");
        return nullptr;
    }

    Ptr<Item> item = nullptr;
    int band;
    for (band = m_bands - 1; band >= 0; band--) {
      if (!m_queues[band].empty()) {
        item = m_queues[band].front();
        m_queues[band].pop_front();
        break;
      }
    }

    NS_ASSERT(item);
    NS_ASSERT(m_nBytes.Get() >= item->GetSize());
    NS_ASSERT(m_nPackets.Get() > 0);

    m_nBytes -= item->GetSize();
    m_nPackets--;

    // packets are first dequeued and then dropped
    NS_LOG_LOGIC("m_traceDequeue (p)");
    m_traceDequeue(item);

    DropAfterDequeue(item);

    NS_LOG_LOGIC("Removed " << item << " from band " << band);

    return item;
}

template <typename Item>
Ptr<const Item>
HomaPFifoQueue<Item>::Peek() const
{
    NS_LOG_FUNCTION(this);

    if (m_nPackets.Get() == 0)
    {
        NS_LOG_LOGIC("Queue empty");
        return nullptr;
    }

    for (int band  = m_bands - 1; band >= 0; band--) {
      if (!m_queues[band].empty()) {
        return m_queues[band].front();
      }
    }

    NS_ABORT_MSG ("Found no item despite queue not empty");
    return nullptr;
}

extern template class HomaPFifoQueue<Packet>;
extern template class HomaPFifoQueue<QueueDiscItem>;

} // namespace ns3

#endif /* HOMA_PFIFO_QUEUE_H */
