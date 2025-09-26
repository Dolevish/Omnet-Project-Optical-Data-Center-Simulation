package opticaldatacenter;

// מודול לניטור תעבורה ויצירת traffic matrix
simple TrafficMonitor
{
    parameters:
        @display("i=block/cogwheel");
        int numPorts = default(16);
        double monitoringInterval @unit(s) = default(1us);  // תדירות עדכון
        
        // סטטיסטיקות
        @signal[trafficDemand](type=double);
        @statistic[trafficDemand](title="traffic demand"; record=vector,mean);
        
    gates:
        // אין gates - מקבל מידע דרך method calls
}