#ifndef OPTICAL_VERMILION_SCHEDULER_H
#define OPTICAL_VERMILION_SCHEDULER_H
#include <vector>

// Demand-weighted bipartite decomposition inspired by Vermilion.
// A matching maps input -> output; -1 denotes an idle (padding) edge.
// Independent of OMNeT++ so the scheduling invariants can be unit tested.
class VermilionScheduler {
  public:
    using Matrix = std::vector<std::vector<double>>;
    using Schedule = std::vector<std::vector<int>>;
    VermilionScheduler(int numPorts, int k);
    Schedule computeSchedule(const Matrix& demand) const;
    static Schedule roundRobin(int numPorts);
  private:
    int numPorts;
    int k;
};
#endif
