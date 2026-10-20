#include "VermilionScheduler.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

VermilionScheduler::VermilionScheduler(int n, int parameter) : numPorts(n), k(parameter)
{
    if (n < 2 || k < 1 || k > std::numeric_limits<int>::max() / n)
        throw std::invalid_argument("Vermilion requires numPorts >= 2 and a positive, representable k*numPorts");
}

VermilionScheduler::Schedule VermilionScheduler::roundRobin(int n)
{
    if (n < 2) throw std::invalid_argument("Round-robin requires at least two ports");
    Schedule result;
    // Offset zero would waste a complete slot on self-connections.
    for (int offset = 1; offset < n; ++offset) {
        std::vector<int> matching(n);
        for (int i = 0; i < n; ++i) matching[i] = (i + offset) % n;
        result.push_back(matching);
    }
    return result;
}

VermilionScheduler::Schedule VermilionScheduler::computeSchedule(const Matrix& demand) const
{
    if (demand.size() != static_cast<size_t>(numPorts))
        throw std::invalid_argument("Demand matrix must be square and match numPorts");
    for (const auto& row : demand) {
        if (row.size() != static_cast<size_t>(numPorts))
            throw std::invalid_argument("Demand matrix must be square and match numPorts");
        for (double value : row)
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("Demand entries must be finite and nonnegative");
    }
    long double maximum = 0;
    for (int i = 0; i < numPorts; ++i) {
        long double row = 0, column = 0;
        for (int j = 0; j < numPorts; ++j) {
            if (i == j) continue;
            row += demand[i][j];
            column += demand[j][i];
        }
        maximum = std::max(maximum, std::max(row, column));
    }
    if (maximum == 0) return roundRobin(numPorts);

    const int degree = k * numPorts;
    const int budget = (k - 1) * numPorts;
    std::vector<std::vector<int>> graph(numPorts, std::vector<int>(numPorts));
    std::vector<int> rowDeficit(numPorts, degree), columnDeficit(numPorts, degree);
    for (int i = 0; i < numPorts; ++i) {
        for (int j = 0; j < numPorts; ++j) {
            if (i == j) continue;
            // Baseline coverage prevents rounding starvation of small demands.
            int edges = 1 + static_cast<int>(std::floor((demand[i][j] / maximum) * budget));
            graph[i][j] = edges;
            rowDeficit[i] -= edges;
            columnDeficit[j] -= edges;
        }
    }
    // Balance BOTH sides. Diagonal padding later becomes idle time.
    for (int i = 0, j = 0; i < numPorts && j < numPorts;) {
        int padding = std::min(rowDeficit[i], columnDeficit[j]);
        graph[i][j] += padding;
        rowDeficit[i] -= padding;
        columnDeficit[j] -= padding;
        if (rowDeficit[i] == 0) ++i;
        if (columnDeficit[j] == 0) ++j;
    }
    Schedule result;
    result.reserve(degree);
    for (int slot = 0; slot < degree; ++slot) {
        std::vector<int> owner(numPorts, -1);
        std::function<bool(int, std::vector<bool>&)> augment =
            [&](int input, std::vector<bool>& seen) {
                for (int output = 0; output < numPorts; ++output) {
                    if (graph[input][output] == 0 || seen[output]) continue;
                    seen[output] = true;
                    if (owner[output] == -1 || augment(owner[output], seen)) {
                        owner[output] = input;
                        return true;
                    }
                }
                return false;
            };
        for (int input = 0; input < numPorts; ++input) {
            std::vector<bool> seen(numPorts, false);
            if (!augment(input, seen))
                throw std::logic_error("Regular bipartite graph has no perfect matching");
        }
        std::vector<int> matching(numPorts, -1);
        for (int output = 0; output < numPorts; ++output) {
            int input = owner[output];
            --graph[input][output];
            if (input != output) matching[input] = output;
        }
        result.push_back(matching);
    }
    return result;
}
