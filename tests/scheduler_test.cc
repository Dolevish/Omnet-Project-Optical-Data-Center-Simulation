#include "VermilionScheduler.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using Matrix = VermilionScheduler::Matrix;
using Schedule = VermilionScheduler::Schedule;

static void check(const Schedule& schedule, int n)
{
    assert(!schedule.empty());
    for (const auto& matching : schedule) {
        assert(matching.size() == static_cast<size_t>(n));
        std::vector<bool> occupied(n, false);
        for (int i = 0; i < n; ++i) {
            const int j = matching[i];
            assert(j >= -1 && j < n && j != i);
            if (j == -1) continue;
            assert(!occupied[j]);
            occupied[j] = true;
        }
    }
}

template <typename Callable> static void rejects(Callable call)
{
    bool rejected = false;
    try { call(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

int main()
{
    std::mt19937 rng(20261020);
    int cases = 0;
    for (int n : {2, 3, 4, 8, 16, 32}) {
        const auto rr = VermilionScheduler::roundRobin(n);
        check(rr, n);
        assert(rr.size() == static_cast<size_t>(n - 1));
        for (int i = 0; i < n; ++i) {
            std::vector<bool> seen(n);
            for (const auto& matching : rr) seen[matching[i]] = true;
            for (int j = 0; j < n; ++j) assert(seen[j] == (i != j));
        }
        for (int k : {1, 2, 3, 5}) {
            VermilionScheduler scheduler(n, k);
            Matrix zero(n, std::vector<double>(n));
            assert(scheduler.computeSchedule(zero) == rr);
            Matrix ring = zero;
            for (int i = 0; i < n; ++i) ring[i][(i + 1) % n] = 1;
            auto schedule = scheduler.computeSchedule(ring);
            check(schedule, n);
            for (int i = 0; i < n; ++i) {
                int count = 0;
                for (const auto& matching : schedule) count += matching[i] == (i + 1) % n;
                assert(count >= (k - 1) * n + 1);
            }
            for (int trial = 0; trial < 40; ++trial) {
                Matrix demand = zero;
                // Dense, sparse, and highly asymmetric hotspot cases.
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                        if (i != j && (trial % 3 == 0 || rng() % 5 == 0))
                            demand[i][j] = 1 + rng() % 100000;
                demand[0][n - 1] += 1;
                auto result = scheduler.computeSchedule(demand);
                check(result, n);
                assert(result.size() == static_cast<size_t>(k * n));
                std::vector<std::vector<int>> served(n, std::vector<int>(n));
                for (const auto& matching : result)
                    for (int i = 0; i < n; ++i)
                        if (matching[i] >= 0) ++served[i][matching[i]];
                long double maximum = 0;
                for (int i = 0; i < n; ++i) {
                    long double row = 0, col = 0;
                    for (int j = 0; j < n; ++j) { row += demand[i][j]; col += demand[j][i]; }
                    maximum = std::max(maximum, std::max(row, col));
                }
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                        if (i != j)
                            assert(served[i][j] >= 1 + std::floor(demand[i][j] / maximum * ((k - 1) * n)));
                assert(result == scheduler.computeSchedule(demand));
                ++cases;
            }
        }
    }
    rejects([] { VermilionScheduler(1, 3); });
    rejects([] { VermilionScheduler(4, 0); });
    rejects([] { VermilionScheduler(4, std::numeric_limits<int>::max()); });
    rejects([] { VermilionScheduler::roundRobin(1); });
    VermilionScheduler scheduler(2, 3);
    rejects([&] { scheduler.computeSchedule({{1}}); });
    rejects([&] { scheduler.computeSchedule({{0, 1}, {1}}); });
    rejects([&] { scheduler.computeSchedule({{0, -1}, {1, 0}}); });
    rejects([&] { scheduler.computeSchedule({{0, NAN}, {1, 0}}); });
    rejects([&] { scheduler.computeSchedule({{0, INFINITY}, {1, 0}}); });
    check(scheduler.computeSchedule({{0, 1e308}, {1e308, 0}}), 2);
    std::cout << "PASS: " << cases << " demand matrices, matching uniqueness, demand coverage, determinism, ring, empty, and invalid inputs\n";
}
