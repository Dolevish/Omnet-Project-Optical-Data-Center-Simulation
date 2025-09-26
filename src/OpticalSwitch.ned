package opticaldatacenter;

// מודול המתג האופטי הראשי - SIMPLE MODULE
simple OpticalSwitch
{
    parameters:
        @display("i=device/switch");
        int numPorts = default(16);  // מספר הפורטים
        
        // פרמטרים למיתוג
        double reconfigurationDelay @unit(s) = default(0.5us);  // זמן החלפת קונפיגורציה
        double timeslotDuration @unit(s) = default(10us);       // משך זמן לכל תצורה
        string schedulingAlgorithm = default("vermilion");      // אלגוריתם: round-robin, vermilion
        
        // פרמטר k עבור Vermilion
        int k = default(3);
        
        // סטטיסטיקות
        @signal[utilization](type=double);
        @signal[reconfigurationCount](type=long);
        @signal[throughput](type=double);
        
        @statistic[utilization](title="link utilization"; record=vector,mean);
        @statistic[reconfigurationCount](title="reconfigurations"; record=count);
        @statistic[throughput](title="throughput"; record=vector,mean);
        
    gates:
        input in[numPorts];
        output out[numPorts];
}