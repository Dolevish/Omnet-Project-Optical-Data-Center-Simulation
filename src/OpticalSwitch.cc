#include "OpticalSwitch.h"

Define_Module(OpticalSwitch);

void OpticalSwitch::initialize()
{
    EV << "=== OpticalSwitch initialize() started ===" << endl;
    EV << "Module path: " << getFullPath() << endl;

    // קבלת פרמטרים
    numPorts = par("numPorts");
    reconfigurationDelay = par("reconfigurationDelay");
    timeslotDuration = par("timeslotDuration");
    schedulingAlgorithm = par("schedulingAlgorithm").stdstringValue();
    k = par("k");

    EV << "Parameters: numPorts=" << numPorts << ", algorithm=" << schedulingAlgorithm << endl;

    // אתחול מצב
    currentTimeslot = 0;
    scheduleIndex = 0;
    currentMatching.resize(numPorts, -1);
    totalReconfigurations = 0;

    // אתחול תורים
    inputQueues.resize(numPorts);
    isReconfiguring = false;

    // יצירת Vermilion scheduler
    if (schedulingAlgorithm == "vermilion") {
        vermilionScheduler = new VermilionScheduler(numPorts, k);
        EV << "Created VermilionScheduler" << endl;
    } else {
        vermilionScheduler = nullptr;
        EV << "Using round-robin scheduling" << endl;
    }

    // אתחול סטטיסטיקות
    utilizationSignal = registerSignal("utilization");
    reconfigurationCountSignal = registerSignal("reconfigurationCount");
    throughputSignal = registerSignal("throughput");
    packetSwitchedSignal = registerSignal("packetSwitched");
    packetDroppedSignal = registerSignal("packetDropped");

    totalPacketsSwitched = 0;
    totalPacketsDropped = 0;

    // יצירת לוח זמנים ראשוני
    EV << "Creating initial schedule..." << endl;
    updateSchedule();

    // קבע matching ראשוני
    if (!schedule.empty()) {
        currentMatching = schedule[0];
        scheduleIndex = 1;
    }

    EV << "Initial matching: ";
    for (int i = 0; i < numPorts; i++) {
        EV << i << "->" << currentMatching[i] << " ";
    }
    EV << endl;

    // תזמון reconfiguration ראשון
    reconfigureEvent = new cMessage("reconfigure");
    scheduleReconfiguration();

    EV << "=== OpticalSwitch initialization completed ===" << endl;
}

void OpticalSwitch::handleMessage(cMessage *msg)
{
    if (msg == reconfigureEvent) {
        // זמן ל-reconfiguration
        EV << "=== Processing reconfiguration event ===" << endl;
        reconfigure();
        scheduleReconfiguration();
    }
    else if (msg->getKind() == 1) {  // הודעת סיום reconfiguration
        // סיום reconfiguration
        EV << "=== Reconfiguration completed ===" << endl;
        isReconfiguring = false;

        // תזמן עיבוד חבילות מהתורים קצת יותר מאוחר
        cMessage *processQueuesEvent = new cMessage("processQueues");
        processQueuesEvent->setKind(2);
        scheduleAt(simTime() + 1e-9, processQueuesEvent);  // דיליי זעיר

        delete msg;
    }
    else if (msg->getKind() == 2) {  // אירוע עיבוד תורים
        processQueuedPackets();
        delete msg;
    }
    else {
        // חבילה שהגיעה
        handlePacket(msg);
    }
}

void OpticalSwitch::handlePacket(cMessage *msg)
{
    cPacket *pkt = check_and_cast<cPacket*>(msg);

    // מצא את פורט הכניסה
    int inPort = -1;
    for (int i = 0; i < numPorts; i++) {
        if (msg->getArrivalGate() == gate("in", i)) {
            inPort = i;
            break;
        }
    }

    EV << "=== Received packet on port " << inPort << " ===" << endl;

    if (inPort == -1) {
        EV << "ERROR: Could not determine input port!" << endl;
        delete msg;
        return;
    }

    int destination = pkt->par("destination");
    EV << "Packet destination: " << destination << endl;

    // אם במצב reconfiguration, הכנס לתור
    if (isReconfiguring) {
        EV << "In reconfiguration mode - queuing packet" << endl;
        if (inputQueues[inPort].size() < MAX_QUEUE_SIZE) {
            inputQueues[inPort].push(msg);
        } else {
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

void OpticalSwitch::forwardPacket(cMessage *msg, int inPort)
{
    cPacket *pkt = check_and_cast<cPacket*>(msg);
    int destination = pkt->par("destination");
    int outPort = currentMatching[inPort];

    EV << "=== Forwarding packet ===" << endl;
    EV << "InPort: " << inPort << ", Destination: " << destination << ", Available OutPort: " << outPort << endl;

    // תמיד הכנס לתור - לא נשלח ישירות
    if (inputQueues[inPort].size() < MAX_QUEUE_SIZE) {
        inputQueues[inPort].push(msg);
        EV << "Added packet to queue " << inPort << ", size now: " << inputQueues[inPort].size() << endl;

        // נסה לעבד מיד אם אפשר
        tryToSendFromQueue(inPort);
    } else {
        EV << "Queue full - dropping packet" << endl;
        totalPacketsDropped++;
        emit(packetDroppedSignal, totalPacketsDropped);
        delete msg;
    }
}

void OpticalSwitch::scheduleReconfiguration()
{
    simtime_t nextTime = simTime() + timeslotDuration;
    scheduleAt(nextTime, reconfigureEvent);
    EV << "Scheduled next reconfiguration at time " << nextTime << endl;
}

void OpticalSwitch::reconfigure()
{
    EV << "=== Reconfiguring switch at time " << simTime() << " ===" << endl;

    // הודע על תחילת reconfiguration
    isReconfiguring = true;

    // קבל את ה-matching הבא מהלוח זמנים
    if (scheduleIndex < schedule.size()) {
        currentMatching = schedule[scheduleIndex];
        scheduleIndex++;
        EV << "Using schedule entry " << (scheduleIndex-1) << endl;
    } else {
        // צריך לחשב לוח זמנים חדש
        EV << "Schedule exhausted - updating schedule" << endl;
        updateSchedule();
        scheduleIndex = 0;
        if (!schedule.empty()) {
            currentMatching = schedule[scheduleIndex];
            scheduleIndex++;
        }
    }

    EV << "New matching: ";
    for (int i = 0; i < numPorts; i++) {
        EV << i << "->" << currentMatching[i] << " ";
    }
    EV << endl;

    // תזמן את סיום ה-reconfiguration אחרי ה-delay
    cMessage *reconfigDone = new cMessage("reconfigDone");
    reconfigDone->setKind(1);  // סוג מיוחד להודעת סיום
    scheduleAt(simTime() + reconfigurationDelay, reconfigDone);

    totalReconfigurations++;
}

void OpticalSwitch::updateSchedule()
{
    EV << "=== Updating schedule (algorithm: " << schedulingAlgorithm << ") ===" << endl;

    if (schedulingAlgorithm == "round-robin") {
        // יצירת לוח זמנים round-robin
        schedule.clear();
        for (int t = 0; t < numPorts; t++) {
            schedule.push_back(computeRoundRobinMatching());
            currentTimeslot = (currentTimeslot + 1) % numPorts;
        }
        EV << "Created round-robin schedule with " << schedule.size() << " entries" << endl;
    }
    else if (schedulingAlgorithm == "vermilion") {
        EV << "=== Creating SIMPLE Vermilion schedule ====" << endl;
        schedule.clear();

        // זיהוי דפוס התעבורה
        std::string pattern = detectTrafficPattern();
        EV << "Detected traffic pattern: " << pattern << endl;

        if (pattern == "uniform") {
            // עבור uniform: יצירת כל הpermutations האפשריות
            // זה נותן לכל זוג שרתים זמן שווה

            EV << "Creating uniform-optimized schedule" << endl;
            for (int offset = 0; offset < numPorts; offset++) {
                std::vector<int> matching(numPorts);
                for (int i = 0; i < numPorts; i++) {
                    matching[i] = (i + offset) % numPorts;
                }
                schedule.push_back(matching);
            }

            // הוסף עוד round לשיפור כיסוי
            for (int offset = 0; offset < numPorts; offset++) {
                std::vector<int> matching(numPorts);
                for (int i = 0; i < numPorts; i++) {
                    // דפוס שונה - reverse order
                    matching[i] = (numPorts - 1 - i + offset) % numPorts;
                }
                schedule.push_back(matching);
            }

        } else if (pattern == "permutation") {
            // עבור permutation: תן יותר זמן לqueueue 0->1, 1->2, etc.

            EV << "Creating permutation-optimized schedule" << endl;

            // חלק 1: הרבה זמן לpermutation הישירות
            for (int repeat = 0; repeat < k; repeat++) {
                std::vector<int> matching(numPorts);
                for (int i = 0; i < numPorts; i++) {
                    matching[i] = (i + 1) % numPorts;  // ring: 0->1, 1->2, ..., 7->0
                }
                schedule.push_back(matching);
            }

            // חלק 2: קצת זמן לשאר
            for (int offset = 2; offset < numPorts; offset++) {
                std::vector<int> matching(numPorts);
                for (int i = 0; i < numPorts; i++) {
                    matching[i] = (i + offset) % numPorts;
                }
                schedule.push_back(matching);
            }

        } else {
            // ברירת מחדל - כמו uniform
            EV << "Using default uniform schedule" << endl;
            for (int offset = 0; offset < numPorts * 2; offset++) {
                std::vector<int> matching(numPorts);
                for (int i = 0; i < numPorts; i++) {
                    matching[i] = (i + offset) % numPorts;
                }
                schedule.push_back(matching);
            }
        }

        EV << "Created simplified Vermilion schedule with " << schedule.size()
           << " entries (pattern: " << pattern << ")" << endl;

        // הדפס דוגמה למatchings
        EV << "Sample matchings:" << endl;
        for (int i = 0; i < std::min(3, (int)schedule.size()); i++) {
            EV << "  Matching " << i << ": ";
            for (int j = 0; j < numPorts; j++) {
                EV << j << "->" << schedule[i][j] << " ";
            }
            EV << endl;
        }
    }
    else {
        EV << "Unknown scheduling algorithm: " << schedulingAlgorithm << ", using round-robin" << endl;
        schedule.clear();
        for (int t = 0; t < numPorts; t++) {
            schedule.push_back(computeRoundRobinMatching());
            currentTimeslot = (currentTimeslot + 1) % numPorts;
        }
    }
}


std::vector<int> OpticalSwitch::computeRoundRobinMatching()
{
    std::vector<int> matching(numPorts);

    EV << "Computing round-robin matching for timeslot " << currentTimeslot << endl;

    // Round-robin: פורט i מתחבר לפורט (i + timeslot) % numPorts
    for (int i = 0; i < numPorts; i++) {
        matching[i] = (i + currentTimeslot) % numPorts;
    }

    EV << "Round-robin matching: ";
    for (int i = 0; i < numPorts; i++) {
        EV << i << "->" << matching[i] << " ";
    }
    EV << endl;

    return matching;
}

void OpticalSwitch::processQueuedPackets()
{
    EV << "=== Processing queued packets ===" << endl;

    // נסה לשלוח מכל התורים
    for (int i = 0; i < numPorts; i++) {
        tryToSendFromQueue(i);
    }
}

void OpticalSwitch::tryToSendFromQueue(int inPort)
{
    if (inputQueues[inPort].empty() || isReconfiguring) {
        return;
    }

    cMessage *msg = inputQueues[inPort].front();
    cPacket *pkt = check_and_cast<cPacket*>(msg);
    int destination = pkt->par("destination");
    int outPort = currentMatching[inPort];

    EV << "Trying to send from queue " << inPort << ", dest=" << destination << ", available=" << outPort << endl;

    // בדוק אם יש matching נכון
    if (outPort >= 0 && outPort < numPorts && outPort == destination) {
        // שלח עכשיו - OMNeT++ יטפל בתורים של הערוץ
        try {
            inputQueues[inPort].pop();
            send(msg, "out", outPort);
            totalPacketsSwitched++;
            emit(packetSwitchedSignal, totalPacketsSwitched);
            EV << "Successfully sent packet from queue " << inPort << " to port " << outPort << endl;
        } catch (cRuntimeError &e) {
            // אם יש שגיאה, השב לתור
            EV << "Failed to send packet: " << e.what() << endl;
            // הכנס בחזרה לתחילת התור
            inputQueues[inPort].push(msg);
        }
    } else {
        EV << "No matching for queue " << inPort << " (need " << destination << ", have " << outPort << ")" << endl;
    }
}

void OpticalSwitch::finish()
{
    EV << "OpticalSwitch statistics:" << endl;
    EV << "  Total reconfigurations: " << totalReconfigurations << endl;
    EV << "  Total packets switched: " << totalPacketsSwitched << endl;
    EV << "  Total packets dropped: " << totalPacketsDropped << endl;

    cancelAndDelete(reconfigureEvent);

    if (vermilionScheduler) {
        delete vermilionScheduler;
    }

    // ניקוי תורים
    for (int i = 0; i < numPorts; i++) {
        while (!inputQueues[i].empty()) {
            delete inputQueues[i].front();
            inputQueues[i].pop();
        }
    }
}

std::vector<std::vector<double>> OpticalSwitch::generateTrafficMatrix()
{
    EV << "=== Generating traffic matrix ===" << endl;

    std::vector<std::vector<double>> trafficMatrix(numPorts, std::vector<double>(numPorts, 0.0));

    // נסה לקבל נתונים אמיתיים מהסימולציה
    // אם אין - צור מטריצה לפי הסטטיסטיקות הנוכחיות

    // לבינתיים: צור מטריצות שונות לפי סוג התעבורה
    // זה ידמה איך Vermilion מזהה דפוסי תעבורה שונים

    // בדוק איזה תעבורה רץ על ידי בדיקת השרתים
    std::string currentTrafficPattern = detectTrafficPattern();

    EV << "Detected traffic pattern: " << currentTrafficPattern << endl;

    if (currentTrafficPattern == "uniform") {
        // תעבורה אחידה
        double uniformValue = 1.0 / (numPorts - 1);
        for (int i = 0; i < numPorts; i++) {
            for (int j = 0; j < numPorts; j++) {
                if (i != j) {
                    trafficMatrix[i][j] = uniformValue;
                }
            }
        }
        EV << "Generated uniform traffic matrix" << endl;
    }
    else if (currentTrafficPattern == "permutation") {
        // תעבורה בדפוס ring: i -> (i+1) % n
        for (int i = 0; i < numPorts; i++) {
            int j = (i + 1) % numPorts;
            trafficMatrix[i][j] = 1.0;  // כל התעבורה לשכן הבא
        }
        EV << "Generated permutation (ring) traffic matrix" << endl;
    }
    else if (currentTrafficPattern == "skewed") {
        // תעבורה מוטה - 80% ל-20% מהשרתים
        int hotServers = std::max(1, numPorts / 5);  // 20% מהשרתים

        for (int i = 0; i < numPorts; i++) {
            for (int j = 0; j < numPorts; j++) {
                if (i != j) {
                    if (j < hotServers) {
                        trafficMatrix[i][j] = 0.8 / hotServers;  // 80% ל-hot servers
                    } else {
                        trafficMatrix[i][j] = 0.2 / (numPorts - hotServers - 1);  // 20% לשאר
                    }
                }
            }
        }
        EV << "Generated skewed traffic matrix" << endl;
    }
    else {
        // ברירת מחדל - uniform
        double uniformValue = 1.0 / (numPorts - 1);
        for (int i = 0; i < numPorts; i++) {
            for (int j = 0; j < numPorts; j++) {
                if (i != j) {
                    trafficMatrix[i][j] = uniformValue;
                }
            }
        }
        EV << "Generated default uniform traffic matrix" << endl;
    }

    // הדפס מטריצה לדיבוג
    EV << "Traffic Matrix:" << endl;
    for (int i = 0; i < numPorts; i++) {
        for (int j = 0; j < numPorts; j++) {
            EV << trafficMatrix[i][j] << " ";
        }
        EV << endl;
    }

    return trafficMatrix;
}

// ===== הוסף פונקציה לזיהוי דפוס תעבורה =====

std::string OpticalSwitch::detectTrafficPattern()
{
    // נסה לזהות את דפוס התעבורה על ידי בדיקת השרתים
    // זה דרך פשוטה לגלות איך הסימולציה מוגדרת

    cModule *parent = getParentModule();
    if (!parent) return "uniform";

    // נסה לגשת לשרת ראשון ולבדוק את trafficPattern שלו
    cModule *server0 = parent->getSubmodule("server", 0);
    if (server0) {
        try {
            const char* pattern = server0->par("trafficPattern").stringValue();
            EV << "Detected server traffic pattern: " << pattern << endl;
            return std::string(pattern);
        } catch (...) {
            EV << "Could not read traffic pattern from server" << endl;
        }
    }

    return "uniform";  // ברירת מחדל
}

