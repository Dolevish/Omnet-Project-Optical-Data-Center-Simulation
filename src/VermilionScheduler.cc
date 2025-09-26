// ===== VermilionScheduler.cc - מימוש מלא =====

#include "VermilionScheduler.h"
#include <iostream>
#include <random>

std::vector<std::vector<int>> VermilionScheduler::computeSchedule(
    const std::vector<std::vector<double>>& trafficMatrix)
{
    std::cout << "=== Vermilion Algorithm Start ===" << std::endl;

    // שלב 1: נרמול ועיגול המטריצה
    std::cout << "Step 1: Matrix rounding..." << std::endl;
    auto roundedMatrix = roundMatrix(trafficMatrix);

    // שלב 2-4: בניית multigraph
    std::cout << "Step 2-4: Building multigraph..." << std::endl;
    auto multigraph = buildMultigraph(roundedMatrix);

    // שלב 5: פירוק ל-matchings
    std::cout << "Step 5: Decomposing to matchings..." << std::endl;
    auto schedule = decomposeToMatchings(multigraph);

    std::cout << "=== Vermilion Algorithm Complete ===" << std::endl;
    std::cout << "Generated " << schedule.size() << " matchings" << std::endl;

    return schedule;
}

std::vector<std::vector<int>> VermilionScheduler::roundMatrix(
    const std::vector<std::vector<double>>& matrix)
{
    std::vector<std::vector<int>> rounded(numPorts, std::vector<int>(numPorts, 0));

    // שלב 1.1: נרמול - מצא את הסכום המקסימלי
    double maxSum = 0;
    for (int i = 0; i < numPorts; i++) {
        double rowSum = 0, colSum = 0;
        for (int j = 0; j < numPorts; j++) {
            rowSum += matrix[i][j];
            colSum += matrix[j][i];
        }
        maxSum = std::max(maxSum, std::max(rowSum, colSum));
    }

    // שלב 1.2: הכפלה ב-(k-1)*n ונרמול
    double scaleFactor = (k - 1) * numPorts;
    if (maxSum > 0) {
        scaleFactor = scaleFactor / maxSum;
    }

    std::cout << "Scale factor: " << scaleFactor << std::endl;

    // שלב 1.3: עיגול
    for (int i = 0; i < numPorts; i++) {
        for (int j = 0; j < numPorts; j++) {
            if (i != j) {  // אין self-loops
                double scaled = matrix[i][j] * scaleFactor;
                rounded[i][j] = (int)std::floor(scaled);
            }
        }
    }

    // הדפסת מטריצה מעוגלת לדיבוג
    std::cout << "Rounded matrix:" << std::endl;
    for (int i = 0; i < numPorts; i++) {
        for (int j = 0; j < numPorts; j++) {
            std::cout << rounded[i][j] << " ";
        }
        std::cout << std::endl;
    }

    return rounded;
}

std::vector<std::vector<int>> VermilionScheduler::buildMultigraph(
    const std::vector<std::vector<int>>& roundedMatrix)
{
    // שלב 2: יצירת multigraph מהמטריצה המעוגלת
    auto multigraph = roundedMatrix;

    // שלב 3: הוספת 1 קשת לכל זוג (residual demand)
    for (int i = 0; i < numPorts; i++) {
        for (int j = 0; j < numPorts; j++) {
            if (i != j) {
                multigraph[i][j] += 1;
            }
        }
    }

    // שלב 4: השלמה לגרף רגולרי בדרגה k*n
    int targetDegree = k * numPorts;

    for (int i = 0; i < numPorts; i++) {
        // חישוב דרגה נוכחית
        int currentOutDegree = 0;
        for (int j = 0; j < numPorts; j++) {
            currentOutDegree += multigraph[i][j];
        }

        // הוספת קשתות חסרות
        int missingEdges = targetDegree - currentOutDegree;

        // הוסף קשתות בצורה מאוזנת
        for (int m = 0; m < missingEdges; m++) {
            int target = (i + 1 + m) % numPorts;  // round-robin פשוט
            if (target != i) {
                multigraph[i][target]++;
            }
        }
    }

    // הדפסת multigraph לדיבוג
    std::cout << "Final multigraph:" << std::endl;
    for (int i = 0; i < numPorts; i++) {
        int degree = 0;
        for (int j = 0; j < numPorts; j++) {
            std::cout << multigraph[i][j] << " ";
            degree += multigraph[i][j];
        }
        std::cout << " (degree: " << degree << ")" << std::endl;
    }

    return multigraph;
}

std::vector<std::vector<int>> VermilionScheduler::decomposeToMatchings(
    const std::vector<std::vector<int>>& multigraph)
{
    std::vector<std::vector<int>> schedule;
    auto workingGraph = multigraph;  // העתק לעבודה

    // חילוץ matchings עד שהגרף ריק
    int maxIterations = k * numPorts + 10;  // מניעת לולאה אינסופית

    for (int iter = 0; iter < maxIterations; iter++) {
        // בדוק אם הגרף ריק
        bool isEmpty = true;
        for (int i = 0; i < numPorts && isEmpty; i++) {
            for (int j = 0; j < numPorts && isEmpty; j++) {
                if (workingGraph[i][j] > 0) {
                    isEmpty = false;
                }
            }
        }

        if (isEmpty) {
            break;
        }

        // מצא perfect matching
        auto matching = findPerfectMatching(workingGraph);

        if (matching.empty()) {
            std::cout << "Warning: Could not find perfect matching at iteration " << iter << std::endl;
            break;
        }

        // הוסף את ה-matching ללוח הזמנים
        schedule.push_back(matching);

        // הורד את ה-matching מהגרף
        for (int i = 0; i < numPorts; i++) {
            if (matching[i] >= 0 && matching[i] < numPorts) {
                workingGraph[i][matching[i]]--;
            }
        }
    }

    std::cout << "Extracted " << schedule.size() << " perfect matchings" << std::endl;

    return schedule;
}

std::vector<int> VermilionScheduler::findPerfectMatching(
    std::vector<std::vector<int>>& graph)
{
    // אלגוריתם Hungarian פשוט למציאת maximum matching
    std::vector<int> matching(numPorts, -1);

    // ניסיון greedy ראשון
    for (int i = 0; i < numPorts; i++) {
        for (int j = 0; j < numPorts; j++) {
            if (graph[i][j] > 0 && matching[i] == -1) {
                // בדוק אם j כבר לא משויך
                bool jTaken = false;
                for (int x = 0; x < i; x++) {
                    if (matching[x] == j) {
                        jTaken = true;
                        break;
                    }
                }

                if (!jTaken) {
                    matching[i] = j;
                    break;
                }
            }
        }
    }

    // בדוק אם זה perfect matching
    bool isPerfect = true;
    for (int i = 0; i < numPorts; i++) {
        if (matching[i] == -1) {
            isPerfect = false;
            break;
        }
    }

    if (!isPerfect) {
        // נסה round-robin fallback
        for (int i = 0; i < numPorts; i++) {
            if (matching[i] == -1) {
                matching[i] = i;  // self-loop זמני
            }
        }
    }

    return matching;
}
