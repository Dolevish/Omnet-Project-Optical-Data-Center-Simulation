#include "Server.h"

Define_Module(Server);

void Server::initialize()
{
    // קבלת פרמטרים
    serverId = getIndex();
    numServers = getParentModule()->par("numServers");
    sendRate = par("sendRate");
    trafficPattern = par("trafficPattern").stdstringValue();
    skewFactor = par("skewFactor");
    useWebSearchWorkload = par("useWebSearchWorkload");

    // אתחול מצב פנימי
    sendEvent = new cMessage("sendEvent");
    transmitEvent = new cMessage("transmitEvent");
    isTransmitting = false;
    nextFlowId = 0;

    // אתחול סטטיסטיקות
    packetSentSignal = registerSignal("packetSent");
    packetReceivedSignal = registerSignal("packetReceived");
    queueLengthSignal = registerSignal("queueLength");
    flowCompletionTimeSignal = registerSignal("flowCompletionTime");

    numPacketsSent = 0;
    numPacketsReceived = 0;
    numFlowsCompleted = 0;

    // הגדרת היסטוגרמה ל-FCT - תיקון ל-OMNeT++ 6
    fctHistogram.setName("flowCompletionTime");
    // במקום setNumCells, נשתמש ב-setBinSize או נשאיר את ברירת המחדל
    fctHistogram.setMode(cHistogram::MODE_AUTO);
    fctHistogram.setRange(0, 1.0);  // טווח של 0 עד 1 שנייה

    EV << "=== Server " << serverId << " initialized with " << numServers << " total servers ===" << endl;

    // תזמון ה-flow הראשון
    scheduleNextFlow();
}

void Server::handleMessage(cMessage *msg)
{
    EV << "=== Server " << serverId << " received message: " << msg->getClassName() << " ===" << endl;

    if (msg == sendEvent) {
        EV << "Server " << serverId << ": Time to create new flow" << endl;
        createNewFlow();
        scheduleNextFlow();
    }
    else if (msg == transmitEvent) {
        EV << "Server " << serverId << ": Transmission complete" << endl;
        isTransmitting = false;
        sendPacket();
    }
    else {
        EV << "Server " << serverId << ": Received external packet from gate: " << msg->getArrivalGate()->getName() << endl;
        handleReceivedPacket(msg);
    }
}

void Server::scheduleNextFlow()
{
    // תזמון לפי Poisson process
    double interArrivalTime;

    if (useWebSearchWorkload) {
        // Web search workload - inter-arrival מבוסס על flow size
        interArrivalTime = exponential(0.001);  // 1ms בממוצע
    } else {
        // חישוב inter-arrival time לפי קצב השליחה
        double avgFlowSize = 100000;  // 100KB בממוצע
        double flowRate = sendRate / (avgFlowSize * 8);
        interArrivalTime = exponential(1.0 / flowRate);
    }

    scheduleAt(simTime() + interArrivalTime, sendEvent);
}

void Server::createNewFlow()
{
    EV << "=== Server " << serverId << " creating new flow ===" << endl;

    // יצירת flow חדש
    Flow flow;
    flow.flowId = nextFlowId++;
    flow.sourceId = serverId;
    flow.destinationId = selectDestination();
    flow.startTime = simTime();
    flow.totalBytes = generateFlowSize();
    flow.bytesReceived = 0;
    flow.bytesRemaining = flow.totalBytes;

    EV << "New flow: ID=" << flow.flowId << ", Source=" << flow.sourceId
       << ", Dest=" << flow.destinationId << ", Size=" << flow.totalBytes << endl;

    // שמירת ה-flow
    myFlows[flow.flowId] = flow;

    // יצירת חבילה ראשונה
    cPacket *pkt = new cPacket("data");
    pkt->setByteLength(std::min(1500, flow.bytesRemaining));  // MTU = 1500
    pkt->addPar("flowId") = flow.flowId;
    pkt->addPar("source") = flow.sourceId;
    pkt->addPar("destination") = flow.destinationId;
    pkt->addPar("totalBytes") = flow.totalBytes;
    pkt->addPar("isFirstPacket") = true;

    EV << "Created packet: " << pkt->getName() << ", Size=" << pkt->getByteLength()
       << ", Dest=" << pkt->par("destination").longValue() << endl;

    // הוספה לתור
    txQueue.push(pkt);
    emit(queueLengthSignal, (long)txQueue.size());

    EV << "Queue size after adding: " << txQueue.size() << endl;

    // ניסיון לשלוח אם אפשר
    if (!isTransmitting) {
        EV << "Not transmitting - trying to send packet" << endl;
        sendPacket();
    } else {
        EV << "Currently transmitting - packet queued" << endl;
    }
}

int Server::selectDestination()
{
    if (trafficPattern == "uniform") {
        // בחירה אקראית אחידה
        int dest;
        do {
            dest = intrand(numServers);
        } while (dest == serverId);

        EV << "Server " << serverId << " selecting destination " << dest << " (uniform)" << endl;
        return dest;
    }
    else if (trafficPattern == "permutation") {
        // תמורה פשוטה: i -> (i+1) % n
        int dest = (serverId + 1) % numServers;
        EV << "Server " << serverId << " selecting destination " << dest << " (permutation)" << endl;
        return dest;
    }
    else if (trafficPattern == "skewed") {
        // 80% מהתעבורה ל-20% מהשרתים
        if (uniform(0, 1) < skewFactor) {
            int hotServers = std::max(1, (int)(numServers * 0.2));
            int dest = intrand(hotServers);
            EV << "Server " << serverId << " selecting destination " << dest << " (skewed-hot)" << endl;
            return dest;
        } else {
            int dest;
            do {
                dest = intrand(numServers);
            } while (dest == serverId);
            EV << "Server " << serverId << " selecting destination " << dest << " (skewed-normal)" << endl;
            return dest;
        }
    }
    else if (trafficPattern == "websearch") {
        // Web search pattern - דומה ל-skewed
        return selectDestination();  // נשתמש באותו pattern
    }

    return 0;
}

int Server::generateFlowSize()
{
    if (useWebSearchWorkload) {
        return generateWebSearchFlowSize();
    } else {
        // גודל קבוע או מבוסס על התפלגות
        return intuniform(10000, 1000000);  // 10KB - 1MB
    }
}

int Server::generateWebSearchFlowSize()
{
    // התפלגות גדלי flows מבוססת על web search workload
    // 70% קטנים (1-10KB), 20% בינוניים (10KB-1MB), 10% גדולים (1-10MB)
    double rand = uniform(0, 1);

    if (rand < 0.7) {
        // Small flows (70%)
        return intuniform(1000, 10000);
    } else if (rand < 0.9) {
        // Medium flows (20%)
        return intuniform(10000, 1000000);
    } else {
        // Large flows (10%)
        return intuniform(1000000, 10000000);
    }
}

void Server::sendPacket()
{
    EV << "=== Server " << serverId << " sendPacket() called ===" << endl;
    EV << "Queue size: " << txQueue.size() << ", isTransmitting: " << isTransmitting << endl;

    if (txQueue.empty() || isTransmitting) {
        EV << "Cannot send: queue empty or transmitting" << endl;
        return;
    }

    // קח חבילה מהתור
    cPacket *pkt = check_and_cast<cPacket*>(txQueue.front());
    txQueue.pop();

    EV << "Sending packet: " << pkt->getName() << ", Dest=" << pkt->par("destination").longValue() << endl;
    EV << "Sending via gate: " << gate("out")->getFullPath() << endl;

    // עדכון סטטיסטיקות
    numPacketsSent++;
    emit(packetSentSignal, numPacketsSent);
    emit(queueLengthSignal, (long)txQueue.size());

    // שלח את החבילה
    send(pkt->dup(), "out");
    EV << "Packet sent!" << endl;

    // חשב זמן שידור
    double transmissionTime = pkt->getBitLength() / sendRate;
    isTransmitting = true;
    scheduleAt(simTime() + transmissionTime, transmitEvent);

    EV << "Transmission time: " << transmissionTime << "s" << endl;

    // בדוק אם צריך לשלוח עוד חבילות מאותו flow
    int flowId = pkt->par("flowId");
    if (myFlows.find(flowId) != myFlows.end()) {
        Flow& flow = myFlows[flowId];
        flow.bytesRemaining -= pkt->getByteLength();

        if (flow.bytesRemaining > 0) {
            // יצירת חבילה נוספת לאותו flow
            cPacket *nextPkt = new cPacket("data");
            nextPkt->setByteLength(std::min(1500, flow.bytesRemaining));
            nextPkt->addPar("flowId") = flowId;
            nextPkt->addPar("source") = flow.sourceId;
            nextPkt->addPar("destination") = flow.destinationId;
            nextPkt->addPar("totalBytes") = flow.totalBytes;
            nextPkt->addPar("isFirstPacket") = false;

            txQueue.push(nextPkt);
            emit(queueLengthSignal, (long)txQueue.size());
        }
    }

    delete pkt;
}

void Server::handleReceivedPacket(cMessage *msg)
{
    cPacket *pkt = check_and_cast<cPacket*>(msg);

    int destination = pkt->par("destination");
    int flowId = pkt->par("flowId");
    int source = pkt->par("source");
    bool isFirstPacket = pkt->par("isFirstPacket");

    if (destination == serverId) {
        // החבילה הגיעה ליעד
        numPacketsReceived++;
        emit(packetReceivedSignal, numPacketsReceived);

        // עדכון מידע על ה-flow
        if (isFirstPacket) {
            // flow חדש
            Flow flow;
            flow.flowId = flowId;
            flow.sourceId = source;
            flow.destinationId = destination;
            flow.startTime = simTime();
            flow.totalBytes = pkt->par("totalBytes");
            flow.bytesReceived = 0;

            activeFlows[flowId] = flow;
        }

        // עדכון התקדמות
        updateFlowProgress(flowId, pkt->getByteLength());

        EV << "Server " << serverId << " received packet from "
           << source << " (flow " << flowId << ")" << endl;
    }

    delete pkt;
}

void Server::updateFlowProgress(int flowId, int bytes)
{
    if (activeFlows.find(flowId) != activeFlows.end()) {
        Flow& flow = activeFlows[flowId];
        flow.bytesReceived += bytes;

        // בדוק אם ה-flow הושלם
        if (flow.bytesReceived >= flow.totalBytes) {
            recordFlowCompletion(flow);
            activeFlows.erase(flowId);
            numFlowsCompleted++;
        }
    }
}

void Server::recordFlowCompletion(const Flow& flow)
{
    // חישוב FCT
    simtime_t fct = simTime() - flow.startTime;

    // רישום בהיסטוגרמה
    fctHistogram.collect(fct.dbl());

    // פליטת סיגנל
    emit(flowCompletionTimeSignal, fct);

    EV << "Flow " << flow.flowId << " completed: "
       << flow.totalBytes << " bytes in " << fct << " seconds" << endl;
}

void Server::finish()
{
    // הדפסת סטטיסטיקות סופיות
    EV << "Server " << serverId << " statistics:" << endl;
    EV << "  Packets sent: " << numPacketsSent << endl;
    EV << "  Packets received: " << numPacketsReceived << endl;
    EV << "  Flows completed: " << numFlowsCompleted << endl;

    // רישום היסטוגרמה
    recordStatistic(&fctHistogram);

    // ניקוי
    cancelAndDelete(sendEvent);
    cancelAndDelete(transmitEvent);

    while (!txQueue.empty()) {
        delete txQueue.front();
        txQueue.pop();
    }
}
