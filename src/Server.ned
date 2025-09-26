package opticaldatacenter;

// מודול השרת - מייצר תעבורה ומקבל חבילות
simple Server
{
    parameters:
        @display("i=device/server");
        
        // פרמטרים ליצירת תעבורה
        double sendRate @unit(bps) = default(10Gbps);  // קצב שליחה
        string trafficPattern = default("uniform");     // דפוס תעבורה: uniform, permutation, skewed, websearch
        
        // פרמטרים לזיהוי
        int serverId = default(0);
        int numServers = default(16);
        
        // פרמטרים ל-skewed traffic
        double skewFactor = default(0.8);  // 80% מהתעבורה ל-20% מהשרתים
        
        // פרמטרים ל-websearch workload
        bool useWebSearchWorkload = default(false);
        
        // סטטיסטיקות
        @signal[packetSent](type=long);
        @signal[packetReceived](type=long);
        @signal[queueLength](type=long);
        @signal[flowCompletionTime](type=simtime_t);
        
        @statistic[packetSent](title="packets sent"; record=count,vector);
        @statistic[packetReceived](title="packets received"; record=count,vector);
        @statistic[queueLength](title="queue length"; record=vector,timeavg,max);
        @statistic[flowCompletionTime](title="flow completion time"; record=vector,histogram);
        
    gates:
        output out;
        input in;
}