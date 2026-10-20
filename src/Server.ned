package opticaldatacenter;

simple Server
{
    parameters:
        int serverId = default(0);
        int numServers = default(8);
        int packetBytes = default(1500);
        int maxPendingFlows = default(10000);
        // Mean offered traffic rate. The channel controls serialization speed.
        double sendRate @unit(bps) = default(1Gbps);
        string trafficPattern = default("uniform");
        double skewFactor = default(0.8);
        bool useWebSearchWorkload = default(false);
        // -1s generates throughout the run; finite values allow a drain phase.
        double trafficStopTime @unit(s) = default(-1s);
        @display("i=device/server");
        @signal[packetSent](type=long);
        @signal[packetReceived](type=long);
        @signal[pendingFlows](type=long);
        @signal[flowRejected](type=long);
        @signal[flowCompletionTime](type=simtime_t);
        @signal[packetDelay](type=simtime_t);
        @statistic[packetSent](record=count);
        @statistic[packetReceived](record=count);
        @statistic[pendingFlows](record=timeavg,max,vector);
        @statistic[flowRejected](record=count);
        @statistic[flowCompletionTime](unit=s; record=count,mean,max,histogram,vector);
        @statistic[packetDelay](unit=s; record=mean,max,histogram);
    gates:
        input in;
        output out;
}
