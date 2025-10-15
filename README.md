# Optical Data Center Simulation in OMNeT++

## 1. Overview

This project presents a simulation of a Data Center based on a central optical switch, built using the OMNeT++ network simulator.
The main goal of the project is to examine and analyze the performance of different scheduling algorithms in the optical switch under various traffic patterns.

The simulation models a network that includes a number of servers connected to a single optical switch. The servers generate data traffic, and the optical switch is responsible for routing the traffic between them. The main challenge in optical switches is the need to reconfigure the communication links, a process that takes time and affects network performance.

The project compares two main scheduling algorithms:
* **Round-Robin:** A basic and simple algorithm for establishing connections.
* **Vermilion:** An advanced algorithm that attempts to create an optimal schedule based on the traffic demands (Traffic Matrix) in the network.

Performance is measured under two primary traffic patterns:
* **Uniform:** Each server sends data to every other server with equal probability.
* **Permutation:** Each server sends data to a predetermined destination, for example, server `i` sends to server `i+1`.

## 2. Project Structure

The project consists of the following files and components:

### Network Definition Files (`.ned`)

* `OpticalDataCenter.ned`: Defines the main network topology, including the servers (`Server`), the optical switch (`OpticalSwitch`), and their interconnections.
* `OpticalSwitch.ned`: Defines the optical switch module.
* `Server.ned`: Defines the server module.

### Implementation Files (`.cc`, `.h`)

* **`Server.cc`**:
    * Implements the server's logic.
    * Responsible for generating traffic (Flows) and breaking them down into packets.
    * Generates different traffic patterns (`uniform`, `permutation`, `skewed`) according to the simulation settings.
    * Collects statistics on Flow Completion Time.

* **`OpticalSwitch.cc`**:
    * Implements the logic of the optical switch, which is the core of the simulation.
    * Manages queues for each input port.
    * Performs reconfiguration of the internal links in the switch at fixed time intervals (`timeslotDuration`).
    * Handles packet forwarding according to the current connection state (`currentMatching`).
    * Executes the selected scheduling algorithm (`round-robin` or `vermilion`) to determine the next connection schedule.

* **`VermilionScheduler.cc`**:
    * An implementation of the Vermilion scheduling algorithm.
    * The algorithm receives a traffic demand matrix and computes a schedule (a series of `matchings`) for the switch.
    * The process includes normalizing and rounding the matrix, building a corresponding graph, and decomposing it into a series of perfect matchings.

* **`TrafficMonitor.cc`**:
    * A module responsible for monitoring network traffic and creating a Traffic Matrix.

### Configuration Files (`.ini`)

* **`omnetpp.ini`**:
    * The main configuration file for the simulation.
    * Defines global simulation parameters, such as the number of servers, simulation duration, and data transmission rates.
    * Defines four main test scenarios (Configurations):
        1.  `[Config Vermilion_Uniform]`: Runs the Vermilion algorithm with uniform traffic.
        2.  `[Config Vermilion_Permutation]`: Runs the Vermilion algorithm with permutation traffic.
        3.  `[Config RoundRobin_Uniform]`: Runs the Round-Robin algorithm with uniform traffic.
        4.  `[Config RoundRobin_Permutation]`: Runs the Round-Robin algorithm with permutation traffic.

## 3. How to Run the Simulation

To run the simulation, follow these steps:

1.  **Ensure OMNeT++ is installed**: Make sure the OMNeT++ environment is installed and properly configured.
2.  **Load the Project**: Open the OMNeT++ IDE and import the project.
3.  **Build the Project**: Click the Build button to compile all source files.
4.  **Run the Simulation**:
    * Open the `omnetpp.ini` file within the IDE.
    * Select one of the predefined run configurations (e.g., `Vermilion_Uniform`).
    * Click the "Run" button to start the simulation.

The simulation results, including vector (`.vec`) and scalar (`.sca`) files, will be saved in the `simulations/results` directory. You can use the graphical tools in OMNeT++ to analyze these results.

## 4. Dependencies

* **OMNeT++**: Version 5.0 or higher is required.
