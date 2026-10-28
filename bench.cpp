#include <chrono>
#include <cstdio>
#include <random>
#include "scheduler.hpp"
using namespace std;

static Scheduler makeGraph(int n, int deg = 3) {
    mt19937 rnd(1); Scheduler s;
    for (int r = 0; r < 10; ++r) s.addResource("R" + to_string(r), "r");
    for (int i = 0; i < n; ++i) {
        vector<string> res; if (rnd() % 10 < 3) res.push_back("R" + to_string(rnd() % 10));
        s.addTask("T" + to_string(i), "t", rnd() % 5 + 1, rnd() % 9 + 1, res);
    }
    for (int i = 1; i < n; ++i) for (int d = 0; d < deg; ++d) s.addDependencyUnchecked("T" + to_string(rnd() % i), "T" + to_string(i));
    return s;
}
template <class F> static double bestMs(F f, int reps = 3) {
    double best = 1e18;
    for (int i = 0; i < reps; ++i) {
        auto t0 = chrono::steady_clock::now(); f();
        best = min(best, chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count());
    }
    return best;
}
int main() {
    printf("%8s | %8s | %17s | %9s | %14s | %9s\n", "tasks V", "edges E", "topological_order", "schedule", "recommend_next", "replan");
    for (int n : {10000, 20000, 40000, 80000, 160000}) {
        Scheduler s = makeGraph(n); size_t E = 0; for (auto& kv : s.succ) E += kv.second.size();
        double a = bestMs([&] { auto o = s.topologicalOrder(); if (o.empty()) puts(""); });
        double b = bestMs([&] { auto p = s.schedule(); if (p.first.empty()) puts(""); });
        double c = bestMs([&] { auto r = s.recommendNext(); if (r.size() > 1) puts(""); });
        double d = bestMs([&] { auto r = s.setResourceAvailability("R0", true); if (r.size() > (size_t)n) puts(""); });
        printf("%8d | %8zu | %14.2f ms | %6.2f ms | %11.2f ms | %6.2f ms\n", n, E, a, b, c, d);
    }
}
