#ifndef __SERVER_H_
#define __SERVER_H_

#include <omnetpp.h>
#include <queue>
#include <map>

using namespace omnetpp;

// מבנה לייצוג flow
struct Flow {
    int flowId;
    int sourceId;
    int destinationId;
    simtime_t startTime;
    int totalBytes;
    int bytesReceived;
    int bytesRemaining;
};

class Server : public cSimpleModule
{
  private:

    // פרמטרים
    int serverId; //מזהה השרת
    int numServers; //סך כל השרתים ברשת
    double sendRate; //קצב השליחה בביטים לשנייה
    std::string trafficPattern; //סוג התנועה
    double skewFactor; // מת ההטיה בתעבורה "skewed"
    bool useWebSearchWorkload; //האם להשתמש בפרופיל זרימה לפי עומסי חיפוש

    // מצב פנימי
    std::queue<cMessage*> txQueue;      // תור שליחה
    cMessage *sendEvent;                // אירוע ליצירת חבילה
    cMessage *transmitEvent;            // אירוע לסיום שידור
    bool isTransmitting;                // האם בשידור כרגע

    // ניהול flows
    std::map<int, Flow> activeFlows;    // flows פעילים
    std::map<int, Flow> myFlows;        // flows שאני השולח
    int nextFlowId;

    // סטטיסטיקות
    simsignal_t packetSentSignal; // מספר החבילות שנשלחו
    simsignal_t packetReceivedSignal; // מספר החבילות שהתקבלו
    simsignal_t queueLengthSignal; //אורך התור
    simsignal_t flowCompletionTimeSignal; //זמן סיום זרימה

    long numPacketsSent;
    long numPacketsReceived;
    long numFlowsCompleted;

    // היסטוגרמה ל-FCT
    cHistogram fctHistogram;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

  private:
    // פונקציות עזר
    void scheduleNextFlow();
    void createNewFlow();
    int selectDestination();
    int generateFlowSize();
    int generateWebSearchFlowSize();
    void sendPacket();
    void handleReceivedPacket(cMessage *msg);
    void updateFlowProgress(int flowId, int bytes);
    void recordFlowCompletion(const Flow& flow);
};

#endif
