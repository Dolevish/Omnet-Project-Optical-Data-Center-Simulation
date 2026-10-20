package opticaldatacenter;

// Full-duplex server links to a central, buffered optical circuit switch.
network OpticalDataCenter
{
    parameters:
        int numServers = default(8);
        int packetBytes = default(1500);
        double linkRate @unit(bps) = default(100Gbps);
        double propagationDelay @unit(s) = default(0s);
        @display("bgb=1100,650");
    submodules:
        server[numServers]: Server {
            parameters:
                numServers = parent.numServers;
                serverId = index;
                packetBytes = parent.packetBytes;
                @display("p=120,100,r,120;i=device/server");
        }
        opticalSwitch: OpticalSwitch {
            parameters:
                numPorts = parent.numServers;
                @display("p=550,400;i=device/switch");
        }
    connections:
        for i=0..numServers-1 {
            server[i].out --> { datarate = parent.linkRate; delay = parent.propagationDelay; } --> opticalSwitch.in[i];
            opticalSwitch.out[i] --> { datarate = parent.linkRate; delay = parent.propagationDelay; } --> server[i].in;
        }
}
