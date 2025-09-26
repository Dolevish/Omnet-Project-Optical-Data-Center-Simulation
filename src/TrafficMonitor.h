#ifndef __TRAFFICMONITOR_H_
#define __TRAFFICMONITOR_H_

#include <omnetpp.h>
#include <vector>

using namespace omnetpp;

class TrafficMonitor : public cSimpleModule
{
  private:
    // פרמטרים
    int numPorts;
    double monitoringInterval;

    // Traffic matrix
    std::vector<std::vector<double>> trafficMatrix;     // מטריצת תעבורה נוכחית
    std::vector<std::vector<double>> demandMatrix;      // דרישות מצטברות

    // אירועים
    cMessage *updateEvent;

    // סטטיסטיקות
    simsignal_t trafficDemandSignal;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

  public:
    // API ציבורי
    void recordPacket(int source, int destination, int bytes);
    std::vector<std::vector<double>> getTrafficMatrix();
    std::vector<std::vector<double>> getNormalizedTrafficMatrix();
    void resetTrafficMatrix();

  private:
    // פונקציות עזר
    void updateMatrices();
    void normalizeMatrix(std::vector<std::vector<double>>& matrix);
};

#endif
