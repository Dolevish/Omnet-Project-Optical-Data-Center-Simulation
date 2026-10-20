#ifndef OPTICAL_SWITCH_H
#define OPTICAL_SWITCH_H
#include <omnetpp.h>
#include <memory>
#include <queue>
#include <vector>
#include "FlowPacket.h"
#include "VermilionScheduler.h"

class OpticalSwitch : public omnetpp::cSimpleModule {
  public:
    ~OpticalSwitch() override;
  protected:
    void initialize() override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void finish() override;
  private:
    int numPorts = 0;
    int queueCapacity = 0;
    bool reconfiguring = false;
    omnetpp::simtime_t slotDuration, reconfigurationDelay, slotEnd;
    omnetpp::cMessage *slotEvent = nullptr, *readyEvent = nullptr;
    std::vector<omnetpp::cMessage *> txEvents;
    std::vector<omnetpp::cChannel *> channels;
    std::vector<std::vector<std::queue<FlowPacket *>>> queues;
    std::vector<long> queuedPerInput;
    VermilionScheduler::Matrix queuedBytes;
    VermilionScheduler::Schedule schedule;
    std::vector<int> matching;
    size_t scheduleIndex = 0;
    std::unique_ptr<VermilionScheduler> scheduler;
    long received = 0, switched = 0, dropped = 0, queued = 0, reconfigurations = 0;
    double transmittedBits = 0, busySeconds = 0;
    omnetpp::simsignal_t switchedSignal, droppedSignal, bufferSignal, reconfigurationSignal;
    void updateSchedule();
    void beginSlot();
    void trySend(int input);
    void drainQueues();
};
#endif
