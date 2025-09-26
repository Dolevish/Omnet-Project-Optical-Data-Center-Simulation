#ifndef __OPTICALSWITCH_H_
#define __OPTICALSWITCH_H_

#include <omnetpp.h>
#include <vector>
#include <queue>
#include "VermilionScheduler.h"

using namespace omnetpp;

class OpticalSwitch : public cSimpleModule
{
  private:
    // פרמטרים
    int numPorts;
    double reconfigurationDelay;
    double timeslotDuration;
    std::string schedulingAlgorithm;
    int k;  // Vermilion parameter

    // מצב פנימי
    std::vector<int> currentMatching;              // ה-matching הנוכחי
    std::vector<std::vector<int>> schedule;        // לוח זמנים מלא
    std::vector<std::queue<cMessage*>> inputQueues; // תורים לכל פורט כניסה
    int currentTimeslot;                           // timeslot נוכחי
    int scheduleIndex;                             // אינדקס בלוח הזמנים
    cMessage *reconfigureEvent;                    // אירוע reconfiguration
    bool isReconfiguring;                          // האם במצב reconfiguration

    // מנגנון תזמון שליחה
    std::vector<cMessage*> transmissionEvents;     // אירועי סיום שידור לכל פורט

    // Vermilion scheduler
    VermilionScheduler *vermilionScheduler;

    // סטטיסטיקות
    simsignal_t utilizationSignal;
    simsignal_t reconfigurationCountSignal;
    simsignal_t throughputSignal;
    simsignal_t packetSwitchedSignal;
    simsignal_t packetDroppedSignal;

    long totalReconfigurations;
    long totalPacketsSwitched;
    long totalPacketsDropped;

    // גודל מקסימלי לתור
    static const int MAX_QUEUE_SIZE = 1000;

    std::vector<std::vector<double>> generateTrafficMatrix();
    std::string detectTrafficPattern();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

  private:
    // פונקציות עזר
    void handlePacket(cMessage *msg);
    void forwardPacket(cMessage *msg, int inPort);
    void scheduleReconfiguration();
    void reconfigure();
    void updateSchedule();
    std::vector<int> computeRoundRobinMatching();
    void processQueuedPackets();
    void tryToSendFromQueue(int inPort);  // חדש!
};

#endif
