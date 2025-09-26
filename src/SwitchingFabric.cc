#include "SwitchingFabric.h"
#include "TrafficMonitor.h"

Define_Module(SwitchingFabric);

void SwitchingFabric::initialize()
{
    // קבלת פרמטרים
    numPorts = par("numPorts");

    // אתחול מצב
    currentMatching.resize(numPorts, -1);
    inputQueues.resize(numPorts);
    isReconfiguring = false;

    // אתחול סטטיסטיקות
    packetSwitchedSignal = registerSignal("packetSwitched");
    packetDroppedSignal = registerSignal("packetDropped");
    bufferSizeSignal = registerSignal("bufferSize");

    totalPacketsSwitched = 0;
    totalPacketsDropped = 0;

    // התחל עם matching לא תקף - חכה למתג לקבוע
    for (int i = 0; i < numPorts; i++) {
        currentMatching[i] = -1;  // לא תקף עד שהמתג יגדיר
    }

    EV << "=== SwitchingFabric initialized with " << numPorts << " ports ===" << endl;
    EV << "Initial matching: ";
    for (int i = 0; i < numPorts; i++) {
        EV << i << "->" << currentMatching[i] << " ";
    }
    EV << endl;
}

void SwitchingFabric::handleMessage(cMessage *msg)
{
    EV << "=== SwitchingFabric received packet ===" << endl;
    EV << "Message class: " << msg->getClassName() << endl;
    EV << "Arrival gate: " << msg->getArrivalGate()->getFullName() << endl;

    cPacket *pkt = check_and_cast<cPacket*>(msg);

    // מצא את פורט הכניסה
    int inPort = -1;
    for (int i = 0; i < numPorts; i++) {
        if (msg->getArrivalGate() == gate("in", i)) {
            inPort = i;
            break;
        }
    }

    EV << "Input port: " << inPort << endl;

    if (inPort == -1) {
        EV << "ERROR: Could not determine input port!" << endl;
        error("Could not determine input port");
    }

    // בדוק אם יש פרמטרים בחבילה
    if (pkt->hasPar("destination")) {
        int destination = pkt->par("destination");
        EV << "Packet destination: " << destination << endl;
    } else {
        EV << "ERROR: Packet has no destination parameter!" << endl;
        delete msg;
        return;
    }

    // עדכן את TrafficMonitor
    int source = pkt->par("source");
    int destination = pkt->par("destination");
    int bytes = pkt->getByteLength();

    TrafficMonitor *monitor = check_and_cast<TrafficMonitor*>(
        getParentModule()->getSubmodule("trafficMonitor"));
    monitor->recordPacket(source, destination, bytes);

    // אם במצב reconfiguration, הכנס לתור
    if (isReconfiguring) {
        EV << "In reconfiguration mode - queuing packet" << endl;
        if (inputQueues[inPort].size() < MAX_QUEUE_SIZE) {
            inputQueues[inPort].push(msg);
            updateBufferStatistics();
        } else {
            // תור מלא - זרוק את החבילה
            EV << "Queue full - dropping packet" << endl;
            totalPacketsDropped++;
            emit(packetDroppedSignal, totalPacketsDropped);
            delete msg;
        }
        return;
    }

    // נסה להעביר את החבילה
    forwardPacket(msg, inPort);
}

void SwitchingFabric::forwardPacket(cMessage *msg, int inPort)
{
    cPacket *pkt = check_and_cast<cPacket*>(msg);
    int destination = pkt->par("destination");

    // בדוק את ה-matching הנוכחי
    int outPort = currentMatching[inPort];

    EV << "=== Forwarding packet ===" << endl;
    EV << "InPort: " << inPort << ", Destination: " << destination << ", Available OutPort: " << outPort << endl;

    if (outPort >= 0 && outPort < numPorts && outPort == destination) {
        // ה-matching מתאים - העבר את החבילה
        EV << "Perfect match - forwarding packet from port " << inPort << " to port " << outPort << endl;
        send(msg, "out", outPort);
        totalPacketsSwitched++;
        emit(packetSwitchedSignal, totalPacketsSwitched);

        EV << "Switched packet from port " << inPort
           << " to port " << outPort << endl;
    } else {
        // ה-matching לא מתאים - הכנס לתור
        EV << "No match - queuing packet. Required: " << destination << ", Available: " << outPort << endl;
        if (inputQueues[inPort].size() < MAX_QUEUE_SIZE) {
            inputQueues[inPort].push(msg);
            updateBufferStatistics();
            EV << "Packet queued in port " << inPort << ", queue size: " << inputQueues[inPort].size() << endl;
        } else {
            // תור מלא
            EV << "Queue full - dropping packet" << endl;
            totalPacketsDropped++;
            emit(packetDroppedSignal, totalPacketsDropped);
            delete msg;
        }
    }
}

void SwitchingFabric::setMatching(const std::vector<int>& matching)
{
    currentMatching = matching;

    EV << "=== New matching set ===" << endl;
    EV << "Matching: ";
    for (int i = 0; i < numPorts; i++) {
        EV << i << "->" << matching[i] << " ";
    }
    EV << endl;
}

void SwitchingFabric::setReconfiguring(bool reconfig)
{
    isReconfiguring = reconfig;
    EV << "=== Reconfiguration mode: " << (reconfig ? "ON" : "OFF") << " ===" << endl;
}

void SwitchingFabric::processQueuedPackets()
{
    EV << "=== Processing queued packets ===" << endl;

    // לאחר reconfiguration, נסה להעביר חבילות מהתורים
    for (int i = 0; i < numPorts; i++) {
        EV << "Checking queue " << i << ", size: " << inputQueues[i].size() << endl;

        while (!inputQueues[i].empty() && !isReconfiguring) {
            cMessage *msg = inputQueues[i].front();
            cPacket *pkt = check_and_cast<cPacket*>(msg);
            int destination = pkt->par("destination");

            EV << "Processing queued packet: dest=" << destination << ", available=" << currentMatching[i] << endl;

            if (currentMatching[i] == destination) {
                // אפשר להעביר
                inputQueues[i].pop();
                send(msg, "out", currentMatching[i]);
                totalPacketsSwitched++;
                emit(packetSwitchedSignal, totalPacketsSwitched);
                EV << "Forwarded queued packet from port " << i << " to port " << currentMatching[i] << endl;
            } else {
                // עדיין לא מתאים - השאר בתור
                EV << "Still no match - leaving in queue" << endl;
                break;
            }
        }
    }

    updateBufferStatistics();
}

void SwitchingFabric::updateBufferStatistics()
{
    // חישוב סך גודל התורים
    long totalBufferSize = 0;
    for (int i = 0; i < numPorts; i++) {
        totalBufferSize += inputQueues[i].size();
    }

    emit(bufferSizeSignal, totalBufferSize);
}

void SwitchingFabric::finish()
{
    EV << "SwitchingFabric statistics:" << endl;
    EV << "  Total packets switched: " << totalPacketsSwitched << endl;
    EV << "  Total packets dropped: " << totalPacketsDropped << endl;

    // ניקוי תורים
    for (int i = 0; i < numPorts; i++) {
        while (!inputQueues[i].empty()) {
            delete inputQueues[i].front();
            inputQueues[i].pop();
        }
    }
}
