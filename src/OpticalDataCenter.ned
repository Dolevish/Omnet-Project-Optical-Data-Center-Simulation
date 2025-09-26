package opticaldatacenter;

// רשת ה-Data Center האופטית הראשית
network OpticalDataCenter
{
    parameters:
        int numServers = default(16);  // מספר השרתים ברשת
        @display("bgb=1200,800;bgl=2");
        
    submodules:
        // מערך של שרתים
        server[numServers]: Server {
            parameters:
                numServers = parent.numServers;  // העבר את הפרמטר מהרשת
                serverId = index;  // הוסף את זה!
                @display("p=100,100,r,150;i=device/server");
        }
        
        // מתג אופטי מרכזי
        opticalSwitch: OpticalSwitch {
            parameters:
                numPorts = parent.numServers;
                @display("p=600,400;i=device/switch");
        }
        
    connections:
        // חיבור כל שרת למתג האופטי
        for i=0..numServers-1 {
            server[i].out --> {datarate = 100Gbps; delay = 0s;} --> opticalSwitch.in[i];
            opticalSwitch.out[i] --> {datarate = 100Gbps; delay = 0s;} --> server[i].in;
        }
}