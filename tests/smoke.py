#!/usr/bin/env python3
"""Run the actual OMNeT++ executable and check end-to-end invariants (stdlib only)."""
import math
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(config, extra=(), expect_error=None):
    with tempfile.TemporaryDirectory(prefix="optical-smoke-") as directory:
        command = [
            str(ROOT / "simulations/run"), "-u", "Cmdenv", "-c", config, "-r", "0",
            "--sim-time-limit=20ms", "--warmup-period=0s", "--repeat=1",
            "--*.numServers=4", "--*.server[*].sendRate=8Gbps",
            "--*.server[*].trafficStopTime=5ms", "--*.opticalSwitch.queueCapacity=100000",
            f"--result-dir={directory}", *extra,
        ]
        result = subprocess.run(command, text=True, capture_output=True, timeout=120, env=os.environ.copy())
        if expect_error:
            assert result.returncode != 0, "Invalid configuration unexpectedly succeeded"
            assert expect_error in result.stdout + result.stderr, result.stdout + result.stderr
            return None
        assert result.returncode == 0, result.stdout + result.stderr
        scalars = {}
        for path in Path(directory).glob("*.sca"):
            for line in path.read_text().splitlines():
                if line.startswith("scalar "):
                    _, module, name, value = shlex.split(line)
                    scalars[module, name] = float(value)
        assert scalars, "No scalar results were written"
        return scalars


def total(data, name):
    return sum(value for (module, metric), value in data.items()
               if ".server[" in module and metric == name)


def switch(data, name):
    return data["OpticalDataCenter.opticalSwitch", name]


def conservation(data, lossless=False):
    received = switch(data, "packetsReceived")
    dispatched = switch(data, "packetsSwitched")
    dropped = switch(data, "packetsDropped")
    queued = switch(data, "packetsQueued")
    assert received == dispatched + dropped + queued
    assert total(data, "packetsReceived") <= dispatched <= total(data, "packetsSent")
    assert 0 <= switch(data, "outputUtilization") <= 1
    if lossless:
        assert dropped == queued == total(data, "flowsRejected") == 0
        assert total(data, "packetsSent") == received == total(data, "packetsReceived")
        assert total(data, "flowsGenerated") == total(data, "flowsCompleted") > 0
        assert total(data, "flowsIncompleteAtReceiver") == total(data, "flowsPendingAtSource") == 0
        assert all(value > 0 for (_, name), value in data.items()
                   if name == "flowCompletionTime:mean" and math.isfinite(value))


def main():
    workloads = {}
    for algorithm in ("Vermilion", "RoundRobin"):
        for pattern in ("Uniform", "Permutation", "Skewed", "WebSearch"):
            config = f"{algorithm}_{pattern}"
            data = run(config)
            conservation(data, lossless=True)
            generated = total(data, "flowsGenerated")
            if pattern in workloads:
                assert generated == workloads[pattern], "Paired workloads diverged"
            workloads[pattern] = generated
            print(f"PASS {config}: {int(generated)} flows completed, no loss, queues drained")
        data = run(f"{algorithm}_Uniform", ["--*.server[*].sendRate=0bps"])
        conservation(data)
        assert total(data, "flowsGenerated") == total(data, "packetsReceived") == 0
        print(f"PASS {algorithm}: zero traffic")

    for pattern in ("Skewed", "WebSearch"):
        data = run(f"Vermilion_{pattern}", ["--*.numServers=2"])
        conservation(data, lossless=True)
        print(f"PASS two-server {pattern}: no self-traffic or recursion")
    data = run("Vermilion_Uniform", ["--*.opticalSwitch.queueCapacity=1"])
    conservation(data)
    assert switch(data, "packetsDropped") > 0
    assert total(data, "flowsCompleted") < total(data, "flowsGenerated")
    print("PASS finite-buffer loss accounting")
    data = run("Vermilion_Uniform", ["--*.server[*].maxPendingFlows=1", "--*.server[*].sendRate=1Tbps"])
    conservation(data)
    assert total(data, "flowsRejected") > 0
    print("PASS source flow admission limit")
    data = run("RoundRobin_Permutation", ["--*.opticalSwitch.reconfigurationDelay=0s"])
    conservation(data, lossless=True)
    print("PASS zero reconfiguration delay")
    data = run("Vermilion_Uniform", ["--*.propagationDelay=1us"])
    conservation(data, lossless=True)
    print("PASS propagation delay")
    for options, message in [
        (["--*.numServers=1"], "Invalid server"),
        (['--*.server[*].trafficPattern="typo"'], "Unknown trafficPattern"),
        (['--*.opticalSwitch.schedulingAlgorithm="typo"'], "Unknown schedulingAlgorithm"),
        (["--*.opticalSwitch.reconfigurationDelay=10us"], "reconfigurationDelay"),
        (["--*.opticalSwitch.timeslotDuration=0.51us"], "full packet must fit"),
        (["--*.server[*].sendRate=-1bps"], "Invalid server"),
    ]:
        run("Vermilion_Uniform", options, expect_error=message)
    print("PASS six invalid-configuration checks")
    print("All simulation smoke tests passed.")


if __name__ == "__main__":
    main()
