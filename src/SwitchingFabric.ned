package opticaldatacenter;

// מודול ה-switching fabric - מבצע את המיתוג בפועל
simple SwitchingFabric
{
    parameters:
        @display("i=block/switch");
        int numPorts = default(16);
        
        // סטטיסטיקות
        @signal[packetSwitched](type=long);
        @signal[packetDropped](type=long);
        @signal[bufferSize](type=long);
        
        @statistic[packetSwitched](title="packets switched"; record=count,vector);
        @statistic[packetDropped](title="packets dropped"; record=count,vector);
        @statistic[bufferSize](title="buffer size"; record=vector,max,timeavg);
        
    gates:
        input in[numPorts];
        output out[numPorts];
}