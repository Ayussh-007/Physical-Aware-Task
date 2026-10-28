#include <iostream>
#include <random>
#include "demo_project.hpp"
using namespace std;
using V = vector<string>;

static int passed = 0, failed = 0, tcNo = 0;
#define CHECK(c) do { if (!(c)) { ok = false; cout << "      assertion failed: " #c " (line " << __LINE__ << ")\n"; } } while (0)
#define THROWS(expr, ex) do { bool got = false; try { expr; } catch (const ex&) { got = true; } if (!got) { ok = false; cout << "      expected " #ex " from: " #expr " (line " << __LINE__ << ")\n"; } } while (0)
#define TEST(name) static void name(bool& ok)
static void run(const char* label, void (*fn)(bool&)) {
    bool ok = true;
    try { fn(ok); } catch (const exception& e) { ok = false; cout << "      unexpected exception: " << e.what() << "\n"; }
    ++tcNo; (ok ? passed : failed)++;
    cout << (ok ? "[PASS] " : "[FAIL] ") << "TC" << tcNo << "  " << label << "\n";
}

TEST(t01) { auto s = buildDemo(); CHECK(s.getTask("T3")->name == "Fabricate PCB"); CHECK(s.getTask("X") == nullptr); }
TEST(t02) { auto s = buildDemo(); THROWS(s.addTask("T1", "dup"), invalid_argument); THROWS(s.addResource("R1", "dup"), invalid_argument); }
TEST(t03) { auto s = buildDemo(); THROWS(s.addDependency("T1", "T99"), out_of_range); THROWS(s.addTask("T10", "x", 1, 1, {"R99"}), out_of_range); }
TEST(t04) { auto s = buildDemo(); THROWS(s.addDependency("T1", "T1"), CycleError); }
TEST(t05) { auto s = buildDemo(); THROWS(s.addDependency("T9", "T1"), CycleError); CHECK(!s.succ["T9"].count("T1")); }
TEST(t06) {
    auto s = buildDemo(); auto o = s.topologicalOrder(); unordered_map<string, int> pos;
    for (size_t i = 0; i < o.size(); ++i) pos[o[i]] = (int)i;
    CHECK(o.size() == 9);
    for (auto& kv : s.succ) for (auto& v : kv.second) CHECK(pos[kv.first] < pos[v]);
}
TEST(t07) {
    Scheduler s; s.addTask("A", "a", 1, 3); s.addTask("B", "b", 1, 5); s.addTask("C", "c", 1, 5); s.addTask("D", "d", 1, 1);
    s.addDependency("C", "D");                                   // C unlocks one task -> wins the tie with B
    CHECK((s.recommendNext(3) == V{"C", "B", "A"}));
}
TEST(t08) {
    auto s = buildDemo(); s.setResourceAvailability("R1", false); s.markDone("T2");
    auto b = s.blockedTasks();
    CHECK(find(b["T4"].begin(), b["T4"].end(), "resource R1 unavailable") != b["T4"].end());
    auto r = s.recommendNext(5); CHECK(find(r.begin(), r.end(), "T4") == r.end());
}
TEST(t09) { auto s = buildDemo(); CHECK((s.blockedTasks()["T3"] == V{"waiting for T1"})); THROWS(s.markDone("T3"), runtime_error); }
TEST(t10) { auto s = buildDemo(); CHECK((s.downstream("T4") == IdSet{"T7", "T8", "T9"})); CHECK(s.downstream("T9").empty()); }
TEST(t11) {
    auto s = buildDemo(); s.tasks["T3"].plannedStart = 2; s.tasks["T5"].plannedStart = 4;
    auto c = s.detectConflicts(); CHECK(c.size() == 1); CHECK(c[0].resource == "R3" && c[0].first == "T3" && c[0].second == "T5");
}
TEST(t12) { auto s = buildDemo(); s.tasks["T3"].plannedStart = 2; s.tasks["T5"].plannedStart = 5; CHECK(s.detectConflicts().empty()); }
TEST(t13) { auto s = buildDemo(); auto [plan, un] = s.schedule(); CHECK(s.verifyPlan(plan)); CHECK(un.empty()); CHECK(plan.size() == 9); }
TEST(t14) {
    auto s = buildDemo(); auto affected = s.setResourceAvailability("R2", false);
    CHECK((affected == V{"T5", "T7", "T8", "T9"}));
    for (const char* u : {"T1", "T2", "T3", "T4", "T6"}) CHECK(find(affected.begin(), affected.end(), u) == affected.end());
    auto [plan, un] = s.schedule(); CHECK(un == affected); CHECK(s.verifyPlan(plan));
}
TEST(t15) {
    auto s = buildDemo(); s.removeTask("T5");
    CHECK(!s.tasks.count("T5")); CHECK(!s.succ["T3"].count("T5")); CHECK(!s.pred["T7"].count("T5"));
    CHECK(!s.users["R2"].count("T5")); CHECK(s.topologicalOrder().size() == 8);
}
TEST(t16) { auto s = buildDemo(); CHECK((s.markDone("T1") == V{"T3", "T6"})); }
TEST(t17) { Scheduler s; CHECK(s.recommendNext().empty()); CHECK(s.topologicalOrder().empty()); auto [p, u] = s.schedule(); CHECK(p.empty() && u.empty()); }
TEST(t18) { auto s = buildDemo(); s.setResourceAvailability("R2", false); s.setResourceAvailability("R2", true); auto [p, u] = s.schedule(); CHECK(u.empty()); CHECK(p.size() == 9); }
TEST(t19) {
    mt19937 rnd(7);
    auto rint = [&](int a, int b) { return (int)(rnd() % (unsigned)(b - a + 1)) + a; };
    for (int g = 0; g < 200; ++g) {
        Scheduler s; int n = rint(2, 30);
        for (int r = 0; r < 4; ++r) s.addResource("R" + to_string(r), "r");
        for (int i = 0; i < n; ++i) {
            V res; for (int r = 0; r < 4; ++r) if (rint(0, 3) == 0 && res.size() < 2) res.push_back("R" + to_string(r));
            s.addTask("T" + to_string(i), "t", rint(1, 5), rint(1, 9), res);
        }
        for (int e = 0; e < 2 * n; ++e) { int a = rint(0, n - 2), b = rint(a + 1, n - 1); s.addDependency("T" + to_string(a), "T" + to_string(b)); }
        auto [plan, un] = s.schedule();
        CHECK(un.empty()); CHECK((int)plan.size() == n); CHECK(s.verifyPlan(plan));
        auto o = s.topologicalOrder(); unordered_map<string, int> pos;
        for (size_t i = 0; i < o.size(); ++i) pos[o[i]] = (int)i;
        for (auto& kv : s.succ) for (auto& v : kv.second) CHECK(pos[kv.first] < pos[v]);
        if (!ok) break;
    }
}

int main() {
    run("Add tasks and look them up; look up a missing id", t01);
    run("Add duplicate task / resource", t02);
    run("Dependency or task with unknown id", t03);
    run("Self-dependency T1 -> T1", t04);
    run("Indirect cycle T9 -> T1 (graph unchanged)", t05);
    run("Topological order on demo project", t06);
    run("Priority tie-break (equal priority, one unlocks more)", t07);
    run("Task blocked by unavailable resource", t08);
    run("Task blocked by prerequisite; markDone on it", t09);
    run("BFS downstream impact (T4; leaf T9)", t10);
    run("Overlapping use of one resource", t11);
    run("Back-to-back use (finish = next start)", t12);
    run("Generated schedule is valid", t13);
    run("Resource failure replan scope", t14);
    run("Remove task T5 cleans all structures", t15);
    run("markDone unlocks successors", t16);
    run("Empty scheduler", t17);
    run("Resource fails then recovers", t18);
    run("200 random DAGs: valid order and schedule", t19);
    cout << "\n" << passed << " passed, " << failed << " failed (" << tcNo << " tests)\n";
    return failed ? 1 : 0;
}
