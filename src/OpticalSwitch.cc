#include "OpticalSwitch.h"
#include <algorithm>
#include <cmath>
using namespace omnetpp;
Define_Module(OpticalSwitch);

void OpticalSwitch::initialize()
{
    numPorts = par("numPorts");
    queueCapacity = par("queueCapacity");
    slotDuration = par("timeslotDuration");
    reconfigurationDelay = par("reconfigurationDelay");
    const std::string algorithm = par("schedulingAlgorithm").stdstringValue();
    if (numPorts < 2 || queueCapacity < 1 || slotDuration <= SIMTIME_ZERO ||
        reconfigurationDelay < SIMTIME_ZERO || reconfigurationDelay >= slotDuration)
        throw cRuntimeError("Require ports >= 2, queueCapacity > 0, and 0 <= reconfigurationDelay < timeslotDuration");
    if (algorithm == "vermilion")
        scheduler.reset(new VermilionScheduler(numPorts, par("k").intValue()));
    else if (algorithm != "round-robin")
        throw cRuntimeError("Unknown schedulingAlgorithm: %s", algorithm.c_str());

    switchedSignal = registerSignal("packetSwitched");
    droppedSignal = registerSignal("packetDropped");
    bufferSignal = registerSignal("bufferSize");
    reconfigurationSignal = registerSignal("reconfigurationCount");
    queues.resize(numPorts);
    queuedPerInput.assign(numPorts, 0);
    queuedBytes.assign(numPorts, std::vector<double>(numPorts, 0));
    const int packetBytes = getParentModule()->par("packetBytes");
    for (int i = 0; i < numPorts; ++i) {
        queues[i].resize(numPorts);
        auto *channel = gate("out", i)->getTransmissionChannel();
        if (!channel || !std::isfinite(channel->getNominalDatarate()) || channel->getNominalDatarate() <= 0)
            throw cRuntimeError("Every output requires a positive finite datarate channel");
        cPacket maximumPacket;
        maximumPacket.setByteLength(packetBytes);
        if (packetBytes < 1 || channel->calculateDuration(&maximumPacket) > slotDuration - reconfigurationDelay)
            throw cRuntimeError("A full packet must fit in the active part of a timeslot");
        channels.push_back(channel);
        auto *event = new cMessage("outputReady");
        event->setKind(i);
        txEvents.push_back(event);
    }
    slotEvent = new cMessage("slotBoundary");
    // Close the old slot before any same-time arrival/completion can send.
    slotEvent->setSchedulingPriority(-1);
    readyEvent = new cMessage("reconfigurationComplete");
    updateSchedule();
    matching = schedule[scheduleIndex++];
    slotEnd = simTime() + slotDuration;
    scheduleAt(slotEnd, slotEvent);
    emit(bufferSignal, 0L);
}

void OpticalSwitch::handleMessage(cMessage *msg)
{
    if (msg == slotEvent) {
        beginSlot();
    } else if (msg == readyEvent) {
        reconfiguring = false;
        drainQueues();
    } else if (msg->isSelfMessage()) {
        // The event is indexed by output: its owning input may change by now.
        const int output = msg->getKind();
        for (int input = 0; input < numPorts; ++input)
            if (matching[input] == output) trySend(input);
    } else {
        auto *packet = check_and_cast<FlowPacket *>(msg);
        const int input = packet->getArrivalGate()->getIndex();
        const int destination = packet->destination;
        if (input < 0 || input >= numPorts || destination < 0 || destination >= numPorts ||
            destination == input || packet->source != input || packet->getByteLength() <= 0)
            throw cRuntimeError("Invalid packet source, destination, or size");
        ++received;
        if (queuedPerInput[input] >= queueCapacity) {
            ++dropped;
            emit(droppedSignal, 1L);
            delete packet;
            return;
        }
        queues[input][destination].push(packet);
        queuedBytes[input][destination] += packet->getByteLength();
        ++queuedPerInput[input];
        emit(bufferSignal, ++queued);
        trySend(input);
    }
}

void OpticalSwitch::updateSchedule()
{
    schedule = scheduler ? scheduler->computeSchedule(queuedBytes) : VermilionScheduler::roundRobin(numPorts);
    scheduleIndex = 0;
}

void OpticalSwitch::beginSlot()
{
    if (scheduleIndex == schedule.size()) updateSchedule();
    auto next = schedule[scheduleIndex++];
    slotEnd = simTime() + slotDuration;
    scheduleAt(slotEnd, slotEvent);
    if (next == matching) {
        drainQueues();
        return;
    }
    matching = std::move(next);
    ++reconfigurations;
    emit(reconfigurationSignal, 1L);
    if (reconfigurationDelay == SIMTIME_ZERO) {
        drainQueues();
    } else {
        reconfiguring = true;
        scheduleAt(simTime() + reconfigurationDelay, readyEvent);
    }
}

void OpticalSwitch::drainQueues()
{
    for (int input = 0; input < numPorts; ++input) trySend(input);
}

void OpticalSwitch::trySend(int input)
{
    const int output = matching[input];
    if (reconfiguring || output < 0 || queues[input][output].empty()) return;
    auto *channel = channels[output];
    if (channel->isBusy()) return; // Existing completion event will resume service.
    auto *packet = queues[input][output].front();
    const simtime_t duration = channel->calculateDuration(packet);
    // Do not transmit across a reconfiguration boundary.
    if (simTime() + duration > slotEnd) return;
    // A same-time arrival may precede the previous completion event.
    cancelEvent(txEvents[output]);
    queues[input][output].pop();
    queuedBytes[input][output] -= packet->getByteLength();
    --queuedPerInput[input];
    emit(bufferSignal, --queued);
    transmittedBits += packet->getBitLength();
    busySeconds += duration.dbl();
    send(packet, "out", output);
    ++switched;
    emit(switchedSignal, 1L);
    scheduleAt(channel->getTransmissionFinishTime(), txEvents[output]);
}

void OpticalSwitch::finish()
{
    recordScalar("packetsReceived", received);
    recordScalar("packetsSwitched", switched);
    recordScalar("packetsDropped", dropped);
    recordScalar("packetsQueued", queued);
    recordScalar("reconfigurations", reconfigurations);
    recordScalar("dispatchedBits", transmittedBits);
    // Remove time extending past a simulation stopped during transmission.
    double elapsedBusy = busySeconds;
    for (auto *channel : channels)
        elapsedBusy -= std::max(0.0, (channel->getTransmissionFinishTime() - simTime()).dbl());
    recordScalar("outputUtilization", simTime() > SIMTIME_ZERO ? elapsedBusy / (numPorts * simTime().dbl()) : 0);
}

OpticalSwitch::~OpticalSwitch()
{
    cancelAndDelete(slotEvent);
    cancelAndDelete(readyEvent);
    for (auto *event : txEvents) cancelAndDelete(event);
    for (auto& input : queues)
        for (auto& queue : input)
            while (!queue.empty()) { delete queue.front(); queue.pop(); }
}
