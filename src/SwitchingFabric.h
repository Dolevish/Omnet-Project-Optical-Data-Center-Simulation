#ifndef __SWITCHINGFABRIC_H_
#define __SWITCHINGFABRIC_H_

#include <omnetpp.h>
#include <vector>
#include <queue>

using namespace omnetpp;

class SwitchingFabric : public cSimpleModule
{
  private:
    // פרמטרים
    int numPorts;

    // מצב פנימי
    std::vector<int> currentMatching;              // ה-matching הנוכחי
    std::vector<std::queue<cMessage*>> inputQueues; // תורים לכל פורט כניסה
    bool isReconfiguring;                          // האם במצב reconfiguration

    // סטטיסטיקות
    simsignal_t packetSwitchedSignal;
    simsignal_t packetDroppedSignal;
    simsignal_t bufferSizeSignal;

    long totalPacketsSwitched;
    long totalPacketsDropped;

    // גודל מקסימלי לתור
    static const int MAX_QUEUE_SIZE = 1000;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

  public:
    // API ציבורי למתג
    void setMatching(const std::vector<int>& matching);
    void setReconfiguring(bool reconfig);
    void processQueuedPackets();

  private:
    // פונקציות עזר
    void forwardPacket(cMessage *msg, int inPort);
    void updateBufferStatistics();
};

#endif
