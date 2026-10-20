package opticaldatacenter;

simple OpticalSwitch
{
    parameters:
        int numPorts = default(8);
        // Capacity in packets, shared by all virtual output queues at an input.
        int queueCapacity = default(1000);
        // A slot includes reconfiguration downtime when the matching changes.
        double reconfigurationDelay @unit(s) = default(0.5us);
        double timeslotDuration @unit(s) = default(10us);
        string schedulingAlgorithm = default("vermilion");
        int k = default(3);
        @display("i=device/switch");
        @signal[packetSwitched](type=long);
        @signal[packetDropped](type=long);
        @signal[bufferSize](type=long);
        @signal[reconfigurationCount](type=long);
        @statistic[packetSwitched](title="Packets dispatched"; record=count);
        @statistic[packetDropped](title="Packets dropped at switch"; record=count);
        @statistic[bufferSize](title="Queued packets"; record=timeavg,max,vector);
        @statistic[reconfigurationCount](title="Matching changes"; record=count);
    gates:
        input in[numPorts];
        output out[numPorts];
}
