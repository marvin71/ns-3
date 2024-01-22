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

#ifndef PTP_QUEUE_H
#define PTP_QUEUE_H

#include <map>

#include "ns3/drop-tail-queue.h"
#include "ns3/simulator.h"

namespace ns3
{

void PTPAddResidenceTime_(Ptr<Packet> packet, Time enq_time);

/* avoid warnings for "unused function" here */
template <typename Item>
static inline void PTPAddResidenceTime(Ptr<Item> it, Time enq_time)
  __attribute__((used));
template <>
void PTPAddResidenceTime<Packet>(Ptr<Packet> packet,
    Time enq_time) __attribute__((used));

template <typename Item>
static inline void PTPAddResidenceTime(Ptr<Item> it, Time enq_time) {
  /* todo: maybe warn?*/
}

template <>
void PTPAddResidenceTime<Packet>(Ptr<Packet> packet,
    Time enq_time) {
  PTPAddResidenceTime_(packet, enq_time);
}

/**
 * \ingroup queue
 *
 * \brief A tail-drop queue that implements a transparent ptp clock.
 */
template <typename Item>
class PTPQueue : public DropTailQueue<Item>
{
  public:
    static TypeId GetTypeId();

    PTPQueue();

    ~PTPQueue() override;

    bool Enqueue(Ptr<Item> item) override;
    Ptr<Item> Dequeue() override;
    Ptr<Item> Remove() override;

  private:
    std::map<Ptr<Item>, Time> m_qtime;

    NS_LOG_TEMPLATE_DECLARE; //!< redefinition of the log component
};

/**
 * Implementation of the templates declared above.
 */

template <typename Item>
TypeId
PTPQueue<Item>::GetTypeId()
{
    static TypeId tid =
        TypeId(GetTemplateClassName<PTPQueue<Item>>())
            .SetParent<DropTailQueue<Item>>()
            .SetGroupName("PTP")
            .template AddConstructor<PTPQueue<Item>>()
            ;
    return tid;
}

template <typename Item>
PTPQueue<Item>::PTPQueue()
    : DropTailQueue<Item>(),
      NS_LOG_TEMPLATE_DEFINE("PTPQueue")
{
    NS_LOG_FUNCTION(this);
}

template <typename Item>
PTPQueue<Item>::~PTPQueue()
{
    NS_LOG_FUNCTION(this);
}

template <typename Item>
bool
PTPQueue<Item>::Enqueue(Ptr<Item> item)
{
    NS_LOG_FUNCTION(this << item);

    if (!DropTailQueue<Item>::Enqueue(item))
      return false;

    m_qtime[item] = Simulator::Now();

    return true;
}

template <typename Item>
Ptr<Item>
PTPQueue<Item>::Dequeue()
{
    NS_LOG_FUNCTION(this);

    Ptr<Item> item = DropTailQueue<Item>::Dequeue();

    auto it = m_qtime.find(item);
    Time enq_time = Simulator::Now() - it->second;
    m_qtime.erase(it);

    PTPAddResidenceTime(item, enq_time);

    return item;
}

template <typename Item>
Ptr<Item>
PTPQueue<Item>::Remove()
{
    NS_LOG_FUNCTION(this);

    Ptr<Item> item = DropTailQueue<Item>::Remove();

    m_qtime.erase(item);
    return item;
}

extern template class PTPQueue<Packet>;
extern template class PTPQueue<QueueDiscItem>;

} // namespace ns3

#endif /* PTP_QUEUE_H */
