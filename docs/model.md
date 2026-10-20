# Model and scheduling details

## Traffic generation

Each server draws exponential flow inter-arrival times. For mean flow size `E[B]` bytes and `sendRate` in bits/second, the mean inter-arrival time is `8 E[B] / sendRate`. The default flow size is discrete uniform from 10,000 to 1,000,000 bytes (mean 505,000).

The optional synthetic web-search mixture uses:

| Probability | Size in bytes | Conditional mean |
|---|---:|---:|
| 0.70 | 1,000–10,000 | 5,500 |
| 0.20 | 10,000–1,000,000 | 505,000 |
| 0.10 | 1,000,000–10,000,000 | 5,500,000 |

Its mean is 654,850 bytes. Both workloads use the configured offered rate. `websearch` selects skewed destinations and this mixture; `useWebSearchWorkload=true` also enables the mixture with another destination pattern.

For skewed traffic, the first `max(1, floor(N/5))` servers form the hot group. The source is removed from both destination groups. `skewFactor` chooses the hot group; if that group is empty for a particular source, the cold group is used. No flow targets itself.

The source queues flow descriptors, not all of their packets. Each service opportunity creates one packet from the next active flow and moves any remainder to the tail. The physical channel determines serialization time. A source admission cap bounds pending descriptors; a rejected flow still consumes the same workload random draws.

## Switch operation

Each input has one FIFO for each destination (virtual output queues), sharing a per-input capacity in packets. A packet is admitted or dropped on arrival. Scheduling uses the exact queued-byte totals maintained on enqueue/dequeue.

A matching maps each input to a distinct output or to `-1` (idle). It is held for one slot. The initial matching starts at time zero. A different matching incurs the configured reconfiguration delay at the beginning of its slot. Repeating the same matching incurs no downtime. Packets are sent only if the entire output serialization fits before the next slot boundary; propagation may finish later.

One reusable completion event per output resumes queue service when the channel becomes available. Slot boundaries run before ordinary same-time events. Exceptions are not used as flow control, and messages are sent once rather than duplicated.

## Scheduling

Round-Robin cycles offsets `1..N-1`, so all non-self pairs are covered once per schedule. There is no all-self slot.

The demand-aware path uses the following bounded integer decomposition:

1. Ignore diagonal demand and normalize by the maximum row or column sum `M`.
2. For each non-self pair, create `1 + floor(D[i,j] / M * (k-1)N)` edges. The baseline edge keeps low-volume pairs from disappearing through rounding.
3. Pad row and column deficits until **both** sides have degree `kN`. Padding may use diagonal edges, which represent idle slots at that input.
4. Extract `kN` perfect matchings with augmenting paths, decrementing only edges actually present in the graph. Translate diagonal edges to `-1`.
5. Recompute from current queue occupancy after the schedule is exhausted. With zero off-diagonal demand, use the Round-Robin schedule.

Every nonempty-demand schedule has `kN` entries. A larger `k` trades faster response for more demand weighting. The baseline plus padding can reserve unused capacity. No optimality, throughput bound, or exact equivalence to a published Vermilion implementation is claimed.

## Accounting and observation windows

Every packet carries its source, source-local flow ID, destination, total flow size, and an exact simulation-time origin. Receivers key flows by `(source, flowId)` and can register a flow from any packet. A missing first packet therefore cannot make later packets invisible. Lost bytes prevent completion; there is no retransmission.

FCT includes source waiting, serialization, propagation, switch waiting, and reconfiguration effects. Packet delay starts when a packet is created for source transmission, so it excludes earlier source-flow waiting.

Switch conservation is checked as:

```
packetsReceived = packetsSwitched + packetsDropped + packetsQueued
```

`packetsSwitched` counts output dispatches; a run can stop while dispatched packets are still in flight. Source sent counts can similarly exceed switch arrivals. The lossless drain tests wait long enough for these differences to vanish and require all generated flows to complete.

Signal-derived statistics follow OMNeT++ warm-up filtering. Explicit finish-time counters, goodput, and utilization cover the whole run. Utilization subtracts any transmission time extending beyond termination. No FCT is recorded for unfinished flows; complete-loss flows have no receive-side entry. Receive-side incomplete counts are therefore not a substitute for comparing generated, rejected, and completed totals.

For finite workloads, stop generation before the simulation limit and inspect whether pending and queued counts reach zero. There is no automatic guarantee that a chosen drain duration is sufficient.

## Limits

- Buffered packet abstraction, not a bufferless physical optical fabric.
- No control-message overhead, TCP, ACKs, retransmission, congestion control, or optical impairments.
- No MPI packet serialization or parallel/distributed runtime support.
- Receiver metadata for incomplete flows is kept until termination. Very long lossy runs can accumulate entries.
- No measured web-search trace or benchmark dataset is included.
- Changes to queue service and offered-load semantics invalidate numerical comparisons with results produced by the original code.
