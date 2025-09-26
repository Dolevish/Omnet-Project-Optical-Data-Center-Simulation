// ===== VermilionScheduler.h - עדכון =====

#ifndef __VERMILIONSCHEDULER_H_
#define __VERMILIONSCHEDULER_H_

#include <vector>
#include <algorithm>
#include <cmath>

class VermilionScheduler
{
private:
    int numPorts;
    int k;  // פרמטר k של Vermilion

public:
    VermilionScheduler(int n, int k_param) : numPorts(n), k(k_param) {}

    // חישוב לוח זמנים מלא לפי אלגוריתם Vermilion
    std::vector<std::vector<int>> computeSchedule(
        const std::vector<std::vector<double>>& trafficMatrix);

private:
    // שלב 1: נרמול ועיגול מטריצה
    std::vector<std::vector<int>> roundMatrix(
        const std::vector<std::vector<double>>& matrix);

    // שלב 2-4: בניית multigraph
    std::vector<std::vector<int>> buildMultigraph(
        const std::vector<std::vector<int>>& roundedMatrix);

    // שלב 5: פירוק ל-perfect matchings
    std::vector<std::vector<int>> decomposeToMatchings(
        const std::vector<std::vector<int>>& multigraph);

    // עזר: מציאת perfect matching במולטיגרף
    std::vector<int> findPerfectMatching(
        std::vector<std::vector<int>>& graph);

    // עזר: אלגוריתם Hungarian לmatching
    bool findAugmentingPath(
        const std::vector<std::vector<int>>& graph,
        std::vector<int>& matching,
        std::vector<bool>& visited,
        int u);
};

#endif
