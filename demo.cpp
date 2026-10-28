#include <iostream>
#include "demo_project.hpp"
using namespace std;

template <class C> static string join(const C& c, const string& sep = ", ") {
    string r; for (const auto& x : c) { if (!r.empty()) r += sep; r += x; } return r;
}
template <class C> static string list(const C& c) {
    string r = "["; bool first = true;
    for (const auto& x : c) { if (!first) r += ", "; first = false; r += "'" + x + "'"; } return r + "]";
}

int main() {
    Scheduler s = buildDemo();
    cout << "1. Topological order      : " << list(s.topologicalOrder()) << "\n";
    cout << "2. Ready tasks            : " << list(s.readyTasks()) << "\n";
    cout << "3. Recommended next (top2): " << list(s.recommendNext(2)) << "\n";
    cout << "4. Blocked tasks:\n";
    for (auto& kv : s.blockedTasks()) cout << "      " << kv.first << " -> " << join(kv.second, "; ") << "\n";

    auto [plan, un] = s.schedule();
    cout << "5. Schedule (start, finish):\n";
    vector<pair<pair<int,int>, string>> rows;
    for (auto& kv : plan) rows.push_back({kv.second, kv.first});
    sort(rows.begin(), rows.end());
    int makespan = 0;
    for (auto& r : rows) {
        cout << "      " << r.second << " " << s.tasks[r.second].name << string(20 - s.tasks[r.second].name.size(), ' ')
             << "(" << r.first.first << ", " << r.first.second << ")\n";
        makespan = max(makespan, r.first.second);
    }
    cout << "      makespan = " << makespan << " | unschedulable: " << list(un)
         << " | valid: " << (s.verifyPlan(plan) ? "true" : "false") << "\n";

    cout << "6. Downstream impact of T3: " << list(s.downstream("T3")) << "\n";
    cout << "7. Soldering station fails -> replan scope: " << list(s.setResourceAvailability("R2", false)) << "\n";
    auto [plan2, un2] = s.schedule();
    vector<string> ok; for (auto& kv : plan2) ok.push_back(kv.first);
    cout << "      schedulable now: " << list(ok) << " | unschedulable: " << list(un2) << "\n";
    cout << "      recommend_next : " << list(s.recommendNext()) << "\n";
    s.setResourceAvailability("R2", true);

    Scheduler s2 = buildDemo();
    s2.tasks["T3"].plannedStart = 2; s2.tasks["T5"].plannedStart = 4; s2.tasks["T7"].plannedStart = 5;
    cout << "8. Conflicts in manual plan (T3@2, T5@4, T7@5):\n      [";
    bool f = true;
    for (auto& c : s2.detectConflicts()) { cout << (f ? "" : ", ") << "('" << c.resource << "', '" << c.first << "', '" << c.second << "')"; f = false; }
    cout << "]\n      Dependency violations: [";
    f = true;
    for (auto& v : s2.dependencyViolations()) { cout << (f ? "" : ", ") << "('" << v.first << "', '" << v.second << "')"; f = false; }
    cout << "]\n";

    Scheduler s3 = buildDemo();
    cout << "9. Execution simulation:\n";
    for (int step = 1; step <= 9; ++step) {
        string t = s3.recommendNext()[0];
        auto unlocked = s3.markDone(t);
        cout << "      step " << step << ": do " << t << " (" << s3.tasks[t].name << ") -> unlocks " << list(unlocked) << "\n";
    }
}
