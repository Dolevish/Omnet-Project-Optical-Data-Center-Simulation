#ifndef OPTICAL_SERVER_H
#define OPTICAL_SERVER_H
#include <omnetpp.h>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include "FlowPacket.h"

class Server : public omnetpp::cSimpleModule {
  public:
    ~Server() override;
  protected:
    void initialize() override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void finish() override;
  private:
    struct Flow {
        int64_t id;
        int destination, totalBytes, remaining;
        omnetpp::simtime_t start;
    };
    struct Reception {
        int totalBytes;
        int receivedBytes;
        omnetpp::simtime_t start;
    };
    int serverId = 0, numServers = 0, packetBytes = 0, maxPendingFlows = 0;
    int64_t nextFlowId = 0;
    double sendRate = 0, skewFactor = 0;
    bool webWorkload = false;
    std::string trafficPattern;
    omnetpp::simtime_t trafficStopTime;
    omnetpp::cMessage *flowEvent = nullptr, *txEvent = nullptr;
    omnetpp::cChannel *channel = nullptr;
    std::deque<Flow> pendingFlows;
    std::map<std::pair<int, int64_t>, Reception> receptions;
    long flowsGenerated = 0, flowsRejected = 0, flowsCompleted = 0;
    long packetsSent = 0, packetsReceived = 0;
    double bytesReceived = 0;
    omnetpp::simsignal_t sentSignal, receivedSignal, queueSignal, fctSignal, rejectedSignal, delaySignal;
    void scheduleNextFlow();
    void createFlow();
    int selectDestination();
    int generateFlowSize();
    void sendPacket();
    void receivePacket(FlowPacket *packet);
};
#endif
