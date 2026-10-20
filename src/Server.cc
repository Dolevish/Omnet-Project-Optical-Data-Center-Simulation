#include "Server.h"
#include <algorithm>
#include <cmath>
#include <vector>
using namespace omnetpp;
Define_Module(Server);

void Server::initialize()
{
    serverId = par("serverId");
    numServers = par("numServers");
    packetBytes = par("packetBytes");
    maxPendingFlows = par("maxPendingFlows");
    sendRate = par("sendRate");
    skewFactor = par("skewFactor");
    trafficPattern = par("trafficPattern").stdstringValue();
    webWorkload = par("useWebSearchWorkload").boolValue() || trafficPattern == "websearch";
    trafficStopTime = par("trafficStopTime");
    if (numServers < 2 || serverId < 0 || serverId >= numServers || packetBytes < 1 ||
        maxPendingFlows < 1 || !std::isfinite(sendRate) || sendRate < 0 ||
        !std::isfinite(skewFactor) || skewFactor < 0 || skewFactor > 1 ||
        (trafficStopTime < SIMTIME_ZERO && trafficStopTime != SimTime(-1)))
        throw cRuntimeError("Invalid server dimensions, rate, skewFactor, or trafficStopTime");
    if (trafficPattern != "uniform" && trafficPattern != "permutation" &&
        trafficPattern != "skewed" && trafficPattern != "websearch")
        throw cRuntimeError("Unknown trafficPattern: %s", trafficPattern.c_str());
    channel = gate("out")->getTransmissionChannel();
    if (!channel || !std::isfinite(channel->getNominalDatarate()) || channel->getNominalDatarate() <= 0)
        throw cRuntimeError("Server requires a positive finite datarate channel");
    sentSignal = registerSignal("packetSent");
    receivedSignal = registerSignal("packetReceived");
    queueSignal = registerSignal("pendingFlows");
    fctSignal = registerSignal("flowCompletionTime");
    rejectedSignal = registerSignal("flowRejected");
    delaySignal = registerSignal("packetDelay");
    flowEvent = new cMessage("newFlow");
    txEvent = new cMessage("transmissionComplete");
    emit(queueSignal, 0L);
    scheduleNextFlow();
}

void Server::handleMessage(cMessage *msg)
{
    if (msg == flowEvent) {
        createFlow();
        scheduleNextFlow();
    } else if (msg == txEvent) {
        sendPacket();
    } else {
        receivePacket(check_and_cast<FlowPacket *>(msg));
    }
}

void Server::scheduleNextFlow()
{
    if (sendRate == 0) return;
    // Match the actual size distribution; sendRate is offered load, not line rate.
    const double meanBytes = webWorkload ? 0.7 * 5500 + 0.2 * 505000 + 0.1 * 5500000 : 505000;
    simtime_t interval = exponential(meanBytes * 8 / sendRate);
    if (interval <= SIMTIME_ZERO) interval = SimTime::fromRaw(1);
    const simtime_t next = simTime() + interval;
    if (trafficStopTime < SIMTIME_ZERO || next < trafficStopTime)
        scheduleAt(next, flowEvent);
}

void Server::createFlow()
{
    // Consume workload RNG draws even if admission fails: paired experiments
    // keep the same offered workload when scheduler-dependent drops differ.
    const int destination = selectDestination();
    const int bytes = generateFlowSize();
    const int64_t id = nextFlowId++;
    ++flowsGenerated;
    if (pendingFlows.size() >= static_cast<size_t>(maxPendingFlows)) {
        ++flowsRejected;
        emit(rejectedSignal, 1L);
        return;
    }
    pendingFlows.push_back({id, destination, bytes, bytes, simTime()});
    emit(queueSignal, static_cast<long>(pendingFlows.size()));
    sendPacket();
}

int Server::selectDestination()
{
    if (trafficPattern == "permutation") return (serverId + 1) % numServers;
    if (trafficPattern == "skewed" || trafficPattern == "websearch") {
        const int hotCount = std::max(1, numServers / 5);
        std::vector<int> hot, cold;
        for (int i = 0; i < numServers; ++i)
            if (i != serverId) (i < hotCount ? hot : cold).push_back(i);
        const bool preferHot = uniform(0, 1) < skewFactor;
        const auto& targets = (preferHot && !hot.empty()) || cold.empty() ? hot : cold;
        return targets[intrand(static_cast<int>(targets.size()))];
    }
    // Uniform without rejection loops or self destinations, including N=2.
    const int target = intrand(numServers - 1);
    return target >= serverId ? target + 1 : target;
}

int Server::generateFlowSize()
{
    if (!webWorkload) return intuniform(10000, 1000000);
    const double sample = uniform(0, 1);
    if (sample < 0.7) return intuniform(1000, 10000);
    if (sample < 0.9) return intuniform(10000, 1000000);
    return intuniform(1000000, 10000000);
}

void Server::sendPacket()
{
    if (pendingFlows.empty() || channel->isBusy()) return;
    cancelEvent(txEvent);
    Flow flow = pendingFlows.front();
    pendingFlows.pop_front();
    const int length = std::min(packetBytes, flow.remaining);
    auto *packet = new FlowPacket;
    packet->source = serverId;
    packet->destination = flow.destination;
    packet->flowId = flow.id;
    packet->totalBytes = flow.totalBytes;
    packet->flowStart = flow.start;
    packet->setByteLength(length);
    flow.remaining -= length;
    // Packet round-robin across active source flows; no duplicated packets.
    if (flow.remaining > 0) pendingFlows.push_back(flow);
    emit(queueSignal, static_cast<long>(pendingFlows.size()));
    send(packet, "out");
    ++packetsSent;
    emit(sentSignal, 1L);
    scheduleAt(channel->getTransmissionFinishTime(), txEvent);
}

void Server::receivePacket(FlowPacket *packet)
{
    if (packet->destination != serverId || packet->source < 0 || packet->source >= numServers ||
        packet->source == serverId || packet->totalBytes < packet->getByteLength() ||
        packet->getByteLength() <= 0 || packet->flowStart > simTime())
        throw cRuntimeError("Invalid received flow metadata");
    ++packetsReceived;
    bytesReceived += packet->getByteLength();
    emit(receivedSignal, 1L);
    emit(delaySignal, simTime() - packet->getCreationTime());
    const auto key = std::make_pair(packet->source, packet->flowId);
    auto inserted = receptions.emplace(key, Reception{packet->totalBytes, 0, packet->flowStart});
    auto& flow = inserted.first->second;
    if (flow.totalBytes != packet->totalBytes || flow.start != packet->flowStart)
        throw cRuntimeError("Inconsistent metadata within a flow");
    flow.receivedBytes += packet->getByteLength();
    if (flow.receivedBytes > flow.totalBytes) throw cRuntimeError("Flow received more bytes than expected");
    if (flow.receivedBytes == flow.totalBytes) {
        emit(fctSignal, simTime() - flow.start);
        ++flowsCompleted;
        receptions.erase(key);
    }
    delete packet;
}

void Server::finish()
{
    recordScalar("flowsGenerated", flowsGenerated);
    recordScalar("flowsRejected", flowsRejected);
    recordScalar("flowsCompleted", flowsCompleted);
    recordScalar("flowsPendingAtSource", pendingFlows.size());
    recordScalar("flowsIncompleteAtReceiver", receptions.size());
    recordScalar("packetsSent", packetsSent);
    recordScalar("packetsReceived", packetsReceived);
    recordScalar("bytesReceived", bytesReceived);
    recordScalar("goodputBps", simTime() > SIMTIME_ZERO ? bytesReceived * 8 / simTime().dbl() : 0);
}

Server::~Server()
{
    cancelAndDelete(flowEvent);
    cancelAndDelete(txEvent);
}
