#ifndef OPTICAL_FLOW_PACKET_H
#define OPTICAL_FLOW_PACKET_H
#include <omnetpp.h>
#include <cstdint>

// Every packet carries the flow's origin time and identity, including when
// the first packet is lost. Local simulation only (no MPI serialization).
class FlowPacket : public omnetpp::cPacket {
  public:
    int source = -1;
    int destination = -1;
    int64_t flowId = 0;
    int totalBytes = 0;
    omnetpp::simtime_t flowStart;
    explicit FlowPacket(const char *name = "data") : cPacket(name) {}
    FlowPacket(const FlowPacket& other) = default;
    FlowPacket *dup() const override { return new FlowPacket(*this); }
};
#endif
