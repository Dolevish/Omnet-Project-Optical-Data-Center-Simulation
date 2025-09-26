#include "TrafficMonitor.h"

Define_Module(TrafficMonitor);

void TrafficMonitor::initialize()
{
    // קבלת פרמטרים
    numPorts = par("numPorts");
    monitoringInterval = par("monitoringInterval");

    // אתחול מטריצות
    trafficMatrix.resize(numPorts, std::vector<double>(numPorts, 0.0));
    demandMatrix.resize(numPorts, std::vector<double>(numPorts, 0.0));

    // אתחול סטטיסטיקות
    trafficDemandSignal = registerSignal("trafficDemand");

    // תזמון עדכון תקופתי
    updateEvent = new cMessage("updateTrafficMatrix");
    scheduleAt(simTime() + monitoringInterval, updateEvent);
}

void TrafficMonitor::handleMessage(cMessage *msg)
{
    if (msg == updateEvent) {
        // עדכון תקופתי של המטריצות
        updateMatrices();
        scheduleAt(simTime() + monitoringInterval, updateEvent);
    }
}

void TrafficMonitor::recordPacket(int source, int destination, int bytes)
{
    // רישום תעבורה במטריצה
    if (source >= 0 && source < numPorts &&
        destination >= 0 && destination < numPorts) {
        demandMatrix[source][destination] += bytes;
    }
}

void TrafficMonitor::updateMatrices()
{
    // העתק demand matrix ל-traffic matrix
    trafficMatrix = demandMatrix;

    // חישוב סך התעבורה
    double totalTraffic = 0;
    for (int i = 0; i < numPorts; i++) {
        for (int j = 0; j < numPorts; j++) {
            totalTraffic += trafficMatrix[i][j];
        }
    }

    emit(trafficDemandSignal, totalTraffic);

    EV << "Traffic matrix updated at time " << simTime() << endl;
    EV << "Total traffic: " << totalTraffic << " bytes" << endl;
}

std::vector<std::vector<double>> TrafficMonitor::getTrafficMatrix()
{
    return trafficMatrix;
}

std::vector<std::vector<double>> TrafficMonitor::getNormalizedTrafficMatrix()
{
    auto normalized = trafficMatrix;
    normalizeMatrix(normalized);
    return normalized;
}

void TrafficMonitor::normalizeMatrix(std::vector<std::vector<double>>& matrix)
{
    // מצא את הסכום המקסימלי של שורה/עמודה
    double maxSum = 0;

    for (int i = 0; i < numPorts; i++) {
        double rowSum = 0;
        double colSum = 0;

        for (int j = 0; j < numPorts; j++) {
            rowSum += matrix[i][j];
            colSum += matrix[j][i];
        }

        maxSum = std::max(maxSum, std::max(rowSum, colSum));
    }

    // נרמל את המטריצה
    if (maxSum > 0) {
        for (int i = 0; i < numPorts; i++) {
            for (int j = 0; j < numPorts; j++) {
                matrix[i][j] /= maxSum;
            }
        }
    }
}

void TrafficMonitor::resetTrafficMatrix()
{
    // איפוס המטריצות
    for (int i = 0; i < numPorts; i++) {
        for (int j = 0; j < numPorts; j++) {
            trafficMatrix[i][j] = 0.0;
            demandMatrix[i][j] = 0.0;
        }
    }
}

void TrafficMonitor::finish()
{
    // הדפסת traffic matrix סופית
    EV << "Final traffic matrix:" << endl;
    for (int i = 0; i < numPorts; i++) {
        for (int j = 0; j < numPorts; j++) {
            if (trafficMatrix[i][j] > 0) {
                EV << "  [" << i << "->" << j << "]: "
                   << trafficMatrix[i][j] << " bytes" << endl;
            }
        }
    }

    cancelAndDelete(updateEvent);
}
