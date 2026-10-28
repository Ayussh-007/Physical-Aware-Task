// Physical-Aware Task Scheduling and Resource Coordination System (C++17)
// DSA Microproject Desktop GUI Demo using raylib & raygui
#include "raylib.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#pragma GCC diagnostic pop

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>
#include <queue>
#include <algorithm>
#include <cmath>

// ============================================================================
// DATA STRUCTURES
// ============================================================================
enum class Status { Pending, Done };

struct Task {
    std::string id, name;
    int duration = 1, priority = 1;
    std::vector<std::string> resources;
    Status status = Status::Pending;
};

struct Resource {
    std::string id, name, kind;
    bool available = true;
};

// Max-heap item for recommendation: (priority DESC, outdegree DESC, id ASC)
struct RecommendItem { int priority, outdegree; std::string id; };
struct RecommendCompare {
    bool operator()(const RecommendItem& a, const RecommendItem& b) const {
        if (a.priority != b.priority) return a.priority < b.priority;
        if (a.outdegree != b.outdegree) return a.outdegree < b.outdegree;
        return a.id > b.id; // tie-breaker: smaller id preferred
    }
};

// Max-heap item for serial scheduling: (priority DESC, id ASC)
struct SchedItem { int priority; std::string id; };
struct SchedCompare {
    bool operator()(const SchedItem& a, const SchedItem& b) const {
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.id > b.id;
    }
};

// Core DSA System State holding graph, hash tables, and priority queues
struct SystemState {
    // 1. Hash tables: O(1) avg lookup for tasks and resources; Resource -> Tasks reverse index
    std::unordered_map<std::string, Task> tasks;
    std::unordered_map<std::string, Resource> resources;
    std::unordered_map<std::string, std::set<std::string>> users;

    // 2. DAG: Adjacency lists for task dependencies
    std::unordered_map<std::string, std::vector<std::string>> succ; // u -> direct dependents
    std::unordered_map<std::string, std::vector<std::string>> pred; // v -> direct prerequisites

    // Serial Schedule Plan: task -> (start_time, finish_time)
    std::map<std::string, std::pair<int, int>> plan;
    std::vector<std::string> unschedulable;
    int makespan = 0;

    // UI interactive states
    std::unordered_set<std::string> affectedTasks; // computed via BFS downstream impact
    std::string recommendedTaskId = "", selectedTaskId = "T1", errorMessage = "";
    std::vector<std::string> activityLog;

    void log(const std::string& msg) {
        activityLog.push_back(msg);
        if (activityLog.size() > 8) activityLog.erase(activityLog.begin());
    }

    // ------------------------------------------------------------------------
    // DATA STRUCTURE OPERATIONS & ALGORITHMS
    // ------------------------------------------------------------------------

    // Hash table insertion: O(1)
    void addResource(const std::string& id, const std::string& name, const std::string& kind = "equipment") {
        resources[id] = Resource{id, name, kind, true};
        users[id];
    }

    // Hash table & inverted index insertion: O(k) where k = resources
    void addTask(const std::string& id, const std::string& name, int dur, int pri, const std::vector<std::string>& res = {}) {
        tasks[id] = Task{id, name, dur, pri, res, Status::Pending};
        succ[id]; pred[id];
        for (const auto& r : res) users[r].insert(id);
    }

    // CORE LOGIC 1: Iterative DFS reachability check to detect cycles: O(V + E)
    bool reachable(const std::string& src, const std::string& dst) const {
        std::vector<std::string> stack = {src};
        std::unordered_set<std::string> visited = {src};
        while (!stack.empty()) {
            std::string u = stack.back(); stack.pop_back();
            if (u == dst) return true;
            auto it = succ.find(u);
            if (it != succ.end()) {
                for (const auto& v : it->second) {
                    if (visited.insert(v).second) stack.push_back(v);
                }
            }
        }
        return false;
    }

    // CORE LOGIC 1: add_dependency with cycle rejection: O(V + E)
    bool addDependency(const std::string& pre, const std::string& post) {
        errorMessage = "";
        if (!tasks.count(pre) || !tasks.count(post)) {
            errorMessage = "Error: Tasks '" + pre + "' and '" + post + "' must both exist!";
            log(errorMessage); return false;
        }
        if (pre == post) {
            errorMessage = "Error: Self-loop detected (" + pre + " -> " + post + ")!";
            log(errorMessage); return false;
        }
        // If post can already reach pre, adding pre -> post creates a directed cycle
        if (reachable(post, pre)) {
            errorMessage = "Cycle Error: " + pre + " -> " + post + " creates a cycle!";
            log("[CYCLE DETECTED] Rejected: " + pre + " -> " + post);
            return false;
        }
        auto& sList = succ[pre];
        if (std::find(sList.begin(), sList.end(), post) == sList.end()) {
            succ[pre].push_back(post);
            pred[post].push_back(pre);
            log("Added dependency: " + pre + " -> " + post);
        }
        updateImpactAndPlan();
        return true;
    }

    // CORE LOGIC 2: Topological sorting via Kahn's Algorithm (in-degree + queue): O(V + E)
    std::vector<std::string> topologicalOrder() const {
        std::unordered_map<std::string, int> indeg;
        for (const auto& [id, _] : tasks) {
            auto it = pred.find(id);
            indeg[id] = (it != pred.end()) ? (int)it->second.size() : 0;
        }
        std::vector<std::string> zero;
        for (const auto& [id, deg] : indeg) if (deg == 0) zero.push_back(id);
        std::sort(zero.begin(), zero.end());
        std::queue<std::string> q;
        for (const auto& id : zero) q.push(id);

        std::vector<std::string> order;
        while (!q.empty()) {
            std::string u = q.front(); q.pop();
            order.push_back(u);
            auto it = succ.find(u);
            if (it != succ.end()) {
                for (const auto& v : it->second) {
                    if (--indeg[v] == 0) q.push(v);
                }
            }
        }
        return order;
    }

    // CORE LOGIC 3: Blocked tasks analysis: O(V + E)
    std::map<std::string, std::vector<std::string>> blockedTasks() const {
        std::map<std::string, std::vector<std::string>> out;
        for (const auto& [id, t] : tasks) {
            if (t.status == Status::Done) continue;
            std::vector<std::string> reasons;
            if (pred.count(id)) {
                for (const auto& p : pred.at(id)) {
                    if (tasks.at(p).status != Status::Done) reasons.push_back("waiting for " + p);
                }
            }
            for (const auto& r : t.resources) {
                if (!resources.at(r).available) reasons.push_back("resource " + r + " unavailable");
            }
            if (!reasons.empty()) out[id] = reasons;
        }
        return out;
    }

    std::vector<std::string> readyTasks() const {
        auto blocked = blockedTasks();
        std::vector<std::string> ready;
        for (const auto& [id, t] : tasks) {
            if (t.status == Status::Pending && !blocked.count(id)) ready.push_back(id);
        }
        std::sort(ready.begin(), ready.end());
        return ready;
    }

    // CORE LOGIC 4: Next-task recommendation using std::priority_queue (max-heap): O(R log R)
    std::string recommendNext() {
        auto ready = readyTasks();
        if (ready.empty()) { recommendedTaskId = ""; log("Recommend: No ready tasks"); return ""; }
        std::priority_queue<RecommendItem, std::vector<RecommendItem>, RecommendCompare> heap;
        for (const auto& id : ready) {
            int outdeg = succ.count(id) ? (int)succ.at(id).size() : 0;
            heap.push(RecommendItem{tasks.at(id).priority, outdeg, id});
        }
        recommendedTaskId = heap.top().id;
        selectedTaskId = recommendedTaskId;
        log("Recommended next: " + recommendedTaskId + " (Pri: " + std::to_string(tasks.at(recommendedTaskId).priority) +
            ", Outdeg: " + std::to_string(heap.top().outdegree) + ")");
        return recommendedTaskId;
    }

    // CORE LOGIC 6: Downstream Impact using BFS: O(V + E)
    std::unordered_set<std::string> computeDownstreamImpact() const {
        std::unordered_set<std::string> affected;
        std::queue<std::string> q;
        for (const auto& [rid, res] : resources) {
            if (!res.available && users.count(rid)) {
                for (const auto& tid : users.at(rid)) {
                    if (tasks.at(tid).status == Status::Pending && affected.insert(tid).second) q.push(tid);
                }
            }
        }
        while (!q.empty()) {
            std::string u = q.front(); q.pop();
            auto it = succ.find(u);
            if (it != succ.end()) {
                for (const auto& v : it->second) {
                    if (affected.insert(v).second) q.push(v);
                }
            }
        }
        return affected;
    }

    std::vector<std::string> singleTaskDownstream(const std::string& id) const {
        std::vector<std::string> res;
        std::unordered_set<std::string> visited = {id};
        std::queue<std::string> q;
        q.push(id);
        while (!q.empty()) {
            std::string u = q.front(); q.pop();
            auto it = succ.find(u);
            if (it != succ.end()) {
                for (const auto& v : it->second) {
                    if (visited.insert(v).second) { q.push(v); res.push_back(v); }
                }
            }
        }
        std::sort(res.begin(), res.end());
        return res;
    }

    // CORE LOGIC 5: Serial scheduling using Kahn + priority queue: O((V + E) log V)
    void schedule() {
        plan.clear(); unschedulable.clear(); makespan = 0;
        std::unordered_set<std::string> pend;
        for (const auto& [id, t] : tasks) if (t.status == Status::Pending) pend.insert(id);

        std::unordered_map<std::string, int> indeg;
        for (const auto& tid : pend) {
            int d = 0;
            if (pred.count(tid)) {
                for (const auto& p : pred.at(tid)) if (pend.count(p)) d++;
            }
            indeg[tid] = d;
        }

        std::priority_queue<SchedItem, std::vector<SchedItem>, SchedCompare> heap;
        for (const auto& [tid, deg] : indeg) if (deg == 0) heap.push(SchedItem{tasks.at(tid).priority, tid});

        std::unordered_map<std::string, int> resFree;
        for (const auto& [rid, _] : resources) resFree[rid] = 0;

        while (!heap.empty()) {
            std::string u = heap.top().id; heap.pop();
            const Task& task = tasks.at(u);

            bool resBlocked = false;
            for (const auto& r : task.resources) {
                if (!resources.at(r).available) { resBlocked = true; break; }
            }
            if (resBlocked) continue; // downstream tasks remain unschedulable

            int start = 0;
            if (pred.count(u)) {
                for (const auto& p : pred.at(u)) if (plan.count(p)) start = std::max(start, plan[p].second);
            }
            for (const auto& r : task.resources) start = std::max(start, resFree[r]);

            int finish = start + task.duration;
            plan[u] = {start, finish};
            makespan = std::max(makespan, finish);
            for (const auto& r : task.resources) resFree[r] = finish;

            if (succ.count(u)) {
                for (const auto& v : succ.at(u)) {
                    if (pend.count(v) && --indeg[v] == 0) heap.push(SchedItem{tasks.at(v).priority, v});
                }
            }
        }
        for (const auto& tid : pend) if (!plan.count(tid)) unschedulable.push_back(tid);
        std::sort(unschedulable.begin(), unschedulable.end());
    }

    void updateImpactAndPlan() {
        affectedTasks = computeDownstreamImpact();
        schedule();
    }

    // CORE LOGIC 7: Mark ready task done and unlock dependents: O(deg+(u))
    bool markDone(const std::string& id) {
        if (!tasks.count(id)) return false;
        auto blocked = blockedTasks();
        if (blocked.count(id)) {
            errorMessage = "Cannot mark done: " + id + " is blocked!";
            log(errorMessage); return false;
        }
        tasks[id].status = Status::Done;
        if (recommendedTaskId == id) recommendedTaskId = "";

        auto readyNow = readyTasks();
        std::vector<std::string> unlocked;
        if (succ.count(id)) {
            for (const auto& s : succ.at(id)) {
                if (std::find(readyNow.begin(), readyNow.end(), s) != readyNow.end()) unlocked.push_back(s);
            }
        }
        std::string unMsg = unlocked.empty() ? "None" : "";
        for (size_t i = 0; i < unlocked.size(); ++i) unMsg += (i ? ", " : "") + unlocked[i];
        log("Marked " + id + " DONE. Unlocked: [" + unMsg + "]");
        updateImpactAndPlan();
        return true;
    }

    // PRELOADED DEMO (Electronics Prototype Case Study)
    void loadPreloadedDemo() {
        tasks.clear(); resources.clear(); users.clear(); succ.clear(); pred.clear();
        errorMessage = ""; recommendedTaskId = ""; selectedTaskId = "T1";
        addResource("R1", "3D Printer", "equipment");
        addResource("R2", "Soldering Station", "equipment");
        addResource("R3", "Electronics Lab", "location");
        addResource("R4", "Dr. Mehta", "specialist");
        addTask("T1", "Design PCB layout", 2, 5);
        addTask("T2", "Design enclosure", 2, 3);
        addTask("T3", "Fabricate PCB", 3, 5, {"R3"});
        addTask("T4", "Print enclosure", 4, 3, {"R1"});
        addTask("T5", "Solder components", 3, 4, {"R2", "R3"});
        addTask("T6", "Write firmware", 5, 2);
        addTask("T7", "Assemble prototype", 2, 4, {"R3"});
        addTask("T8", "Functional test", 2, 5, {"R3", "R4"});
        addTask("T9", "Documentation", 3, 1);
        for (const auto& [p, s] : {std::pair<std::string,std::string>{"T1","T3"},{"T1","T6"},{"T2","T4"},{"T3","T5"},
                                    {"T4","T7"},{"T5","T7"},{"T6","T8"},{"T7","T8"},{"T8","T9"}}) {
            succ[p].push_back(s); pred[s].push_back(p);
        }
        updateImpactAndPlan();
        log("Loaded electronics prototype demo (T1-T9, R1-R4)");
    }
};

// ============================================================================
// GUI & RENDERING
// ============================================================================
static Color GetResourceColor(const std::string& resId) {
    if (resId == "R1") return Color{ 230, 126, 34, 255 };  // Orange
    if (resId == "R2") return Color{ 155, 89, 182, 255 };  // Purple
    if (resId == "R3") return Color{ 26, 188, 156, 255 };   // Teal
    if (resId == "R4") return Color{ 233, 30, 99, 255 };   // Pink
    return Color{ 52, 152, 219, 255 };                    // Blue
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1200, 720, "Physical-Aware Task Scheduling & Resource Coordination | DSA Demo");
    SetTargetFPS(60);

    SystemState sys;
    sys.loadPreloadedDemo();

    char fromBuf[16] = "T9", toBuf[16] = "T1";
    bool editFrom = false, editTo = false;

    const Color BG_COLOR = { 17, 20, 29, 255 }, PANEL_BG = { 25, 30, 44, 255 }, PANEL_BORDER = { 42, 51, 74, 255 };
    const Color TEXT_PRIMARY = { 235, 240, 248, 255 }, TEXT_MUTED = { 145, 158, 180, 255 };
    const Color COL_READY = { 46, 204, 113, 255 }, COL_BLOCKED = { 90, 100, 118, 255 }, COL_DONE = { 52, 152, 219, 255 };
    const Color COL_AFFECTED = { 231, 76, 60, 255 }, COL_GOLD = { 241, 196, 15, 255 };

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();

        // --------------------------------------------------------------------
        // Topological Depth Layout: layer(u) = max(layer(p) + 1 for p in pred)
        // --------------------------------------------------------------------
        std::unordered_map<std::string, int> nodeLayer;
        auto topo = sys.topologicalOrder();
        for (const auto& u : topo) {
            int d = 0;
            if (sys.pred.count(u)) {
                for (const auto& p : sys.pred.at(u)) d = std::max(d, nodeLayer[p] + 1);
            }
            nodeLayer[u] = d;
        }
        int maxLayer = 0;
        for (const auto& [_, l] : nodeLayer) maxLayer = std::max(maxLayer, l);

        std::map<int, std::vector<std::string>> layers;
        for (const auto& [id, _] : sys.tasks) layers[nodeLayer[id]].push_back(id);
        for (auto& [_, vec] : layers) std::sort(vec.begin(), vec.end());

        const Rectangle centerRect = { 280, 45, 590, 405 };
        std::unordered_map<std::string, Vector2> nodePos;
        for (const auto& [l, tList] : layers) {
            float x = centerRect.x + 45.0f + (maxLayer > 0 ? (float)l * (centerRect.width - 90.0f) / maxLayer : 0.0f);
            int count = (int)tList.size();
            for (int i = 0; i < count; ++i) {
                float y = centerRect.y + 45.0f + (float)(i + 1) * (centerRect.height - 90.0f) / (count + 1);
                nodePos[tList[i]] = Vector2{ x, y };
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, centerRect)) {
            for (const auto& [id, pos] : nodePos) {
                if (CheckCollisionPointCircle(mouse, pos, 22.0f)) { sys.selectedTaskId = id; break; }
            }
        }

        BeginDrawing();
        ClearBackground(BG_COLOR);

        // Header Bar
        DrawRectangle(0, 0, 1200, 36, Color{ 20, 24, 36, 255 });
        DrawLine(0, 36, 1200, 36, PANEL_BORDER);
        DrawText("PHYSICAL-AWARE TASK SCHEDULING & RESOURCE COORDINATION SYSTEM", 20, 10, 15, TEXT_PRIMARY);
        DrawText("Graph (DAG) + Priority Queue (Max-Heap) Microproject", 770, 11, 13, TEXT_MUTED);

        // ====================================================================
        // 1. LEFT PANEL (Controls)
        // ====================================================================
        const Rectangle leftRect = { 15, 45, 250, 405 };
        DrawRectangleRounded(leftRect, 0.04f, 4, PANEL_BG);
        DrawRectangleLinesEx(leftRect, 1.0f, PANEL_BORDER);
        DrawText("PHYSICAL RESOURCES", 28, 55, 13, TEXT_PRIMARY);

        float resY = 78.0f;
        const std::string rids[] = { "R1", "R2", "R3", "R4" };
        for (const auto& rid : rids) {
            Resource& r = sys.resources[rid];
            bool prev = r.available;
            DrawRectangle((int)28, (int)resY + 3, 9, 9, GetResourceColor(rid));
            GuiCheckBox(Rectangle{ 45, resY, 16, 16 }, (r.id + ": " + r.name).c_str(), &r.available);
            if (r.available != prev) {
                sys.log(r.id + " toggled " + (r.available ? "AVAILABLE" : "UNAVAILABLE"));
                sys.updateImpactAndPlan();
            }
            resY += 26.0f;
        }

        DrawLine(25, (int)resY + 4, 255, (int)resY + 4, PANEL_BORDER);
        DrawText("ACTIONS", 28, (int)resY + 12, 13, TEXT_PRIMARY);

        float btnY = resY + 32.0f;
        if (GuiButton(Rectangle{ 28, btnY, 224, 26 }, "Recommend Next Task")) sys.recommendNext();
        if (GuiButton(Rectangle{ 28, btnY + 30.0f, 224, 26 }, "Mark Selected Done")) sys.markDone(sys.selectedTaskId);
        if (GuiButton(Rectangle{ 28, btnY + 60.0f, 224, 26 }, "Run Serial Schedule")) {
            sys.schedule(); sys.log("Schedule: Makespan = " + std::to_string(sys.makespan));
        }
        if (GuiButton(Rectangle{ 28, btnY + 90.0f, 108, 25 }, "Topo Order")) {
            auto order = sys.topologicalOrder(); std::string o = "";
            for (size_t i = 0; i < order.size(); ++i) o += (i ? "->" : "") + order[i];
            sys.log("Topo: " + o);
        }
        if (GuiButton(Rectangle{ 144, btnY + 90.0f, 108, 25 }, "Reset Demo")) sys.loadPreloadedDemo();
        btnY += 122.0f;
        DrawLine(25, (int)btnY, 255, (int)btnY, PANEL_BORDER);
        DrawText("ADD DEPENDENCY", 28, (int)btnY + 8, 13, TEXT_PRIMARY);
        btnY += 26.0f;

        DrawText("From:", 28, (int)btnY + 4, 11, TEXT_MUTED);
        if (GuiTextBox(Rectangle{ 65, btnY, 38, 22 }, fromBuf, 16, editFrom)) editFrom = !editFrom;
        DrawText("To:", 112, (int)btnY + 4, 11, TEXT_MUTED);
        if (GuiTextBox(Rectangle{ 134, btnY, 38, 22 }, toBuf, 16, editTo)) editTo = !editTo;

        if (GuiButton(Rectangle{ 180, btnY, 72, 22 }, "+ Add")) sys.addDependency(fromBuf, toBuf);
        btnY += 27.0f;
        if (GuiButton(Rectangle{ 28, btnY, 224, 22 }, "Demo: Try T9 -> T1 (Cycle)")) {
            snprintf(fromBuf, sizeof(fromBuf), "T9");
            snprintf(toBuf, sizeof(toBuf), "T1");
            sys.addDependency("T9", "T1");
        }

        // ====================================================================
        // 2. CENTRE PANEL (DAG Canvas)
        // ====================================================================
        DrawRectangleRounded(centerRect, 0.02f, 4, PANEL_BG);
        DrawRectangleLinesEx(centerRect, 1.0f, PANEL_BORDER);
        DrawText("TASK DEPENDENCY GRAPH (DAG - TOPOLOGICAL LAYERS)", (int)centerRect.x + 15, (int)centerRect.y + 10, 13, TEXT_PRIMARY);

        // Legend
        for (const auto& [c, lbl, lx, isRing] : {
            std::tuple<Color,const char*,int,bool>{COL_READY,"Ready",20,false}, {COL_BLOCKED,"Blocked",80,false},
            {COL_DONE,"Done",150,false}, {COL_AFFECTED,"Affected",210,false}, {COL_GOLD,"Recommended",300,true}
        }) {
            if (isRing) DrawCircleLines((int)centerRect.x + lx, (int)centerRect.y + 390, 6, c);
            else DrawCircle((int)centerRect.x + lx, (int)centerRect.y + 390, 5, c);
            DrawText(lbl, (int)centerRect.x + lx + 9, (int)centerRect.y + 384, 11, TEXT_MUTED);
        }

        // Draw DAG Edges
        for (const auto& [u, dests] : sys.succ) {
            if (!nodePos.count(u)) continue;
            Vector2 p1 = nodePos[u];
            for (const auto& v : dests) {
                if (!nodePos.count(v)) continue;
                Vector2 p2 = nodePos[v];
                float dx = p2.x - p1.x, dy = p2.y - p1.y, len = std::sqrt(dx * dx + dy * dy);
                if (len < 1.0f) continue;
                float nx = dx / len, ny = dy / len;
                Vector2 start = { p1.x + nx * 19.0f, p1.y + ny * 19.0f };
                Vector2 end   = { p2.x - nx * 19.0f, p2.y - ny * 19.0f };

                Color edgeCol = Color{ 80, 95, 125, 200 };
                if (sys.affectedTasks.count(u) && sys.affectedTasks.count(v)) edgeCol = Color{ 231, 76, 60, 180 };
                DrawLineEx(start, end, 1.8f, edgeCol);
                Vector2 left  = { end.x - nx * 7.0f - ny * 4.0f, end.y - ny * 7.0f + nx * 4.0f };
                Vector2 right = { end.x - nx * 7.0f + ny * 4.0f, end.y - ny * 7.0f - nx * 4.0f };
                DrawTriangle(end, left, right, edgeCol);
            }
        }

        // Draw DAG Nodes
        auto blockedMap = sys.blockedTasks();
        for (const auto& [id, pos] : nodePos) {
            const Task& t = sys.tasks[id];
            Color nodeCol = COL_BLOCKED;
            if (t.status == Status::Done) nodeCol = COL_DONE;
            else if (sys.affectedTasks.count(id)) nodeCol = COL_AFFECTED;
            else if (!blockedMap.count(id)) nodeCol = COL_READY;

            if (id == sys.recommendedTaskId) {
                DrawCircle((int)pos.x, (int)pos.y, 24.0f, Color{ 241, 196, 15, 60 });
                DrawCircleLines((int)pos.x, (int)pos.y, 24.0f, COL_GOLD);
                DrawCircleLines((int)pos.x, (int)pos.y, 23.0f, COL_GOLD);
            }
            if (id == sys.selectedTaskId) DrawCircleLines((int)pos.x, (int)pos.y, 21.0f, WHITE);

            DrawCircle((int)pos.x, (int)pos.y, 18.0f, nodeCol);
            DrawCircleLines((int)pos.x, (int)pos.y, 18.0f, Color{ 255, 255, 255, 50 });

            int tw = MeasureText(id.c_str(), 12);
            DrawText(id.c_str(), (int)pos.x - tw / 2, (int)pos.y - 6, 12, WHITE);

            std::string sub = "d:" + std::to_string(t.duration) + " p:" + std::to_string(t.priority);
            int subW = MeasureText(sub.c_str(), 10);
            DrawText(sub.c_str(), (int)pos.x - subW / 2, (int)pos.y + 20, 10, TEXT_MUTED);
        }

        // Cycle Error Banner
        if (!sys.errorMessage.empty()) {
            DrawRectangleRounded(Rectangle{ centerRect.x + 20, centerRect.y + 35, centerRect.width - 40, 26 }, 0.2f, 4, Color{ 231, 76, 60, 230 });
            DrawText(sys.errorMessage.c_str(), (int)centerRect.x + 35, (int)centerRect.y + 41, 12, WHITE);
        }

        // ====================================================================
        // 3. RIGHT PANEL (Task Details & Activity Log)
        // ====================================================================
        const Rectangle rightRect = { 885, 45, 300, 405 };
        DrawRectangleRounded(rightRect, 0.03f, 4, PANEL_BG);
        DrawRectangleLinesEx(rightRect, 1.0f, PANEL_BORDER);
        DrawText("SELECTED TASK DETAILS", (int)rightRect.x + 15, (int)rightRect.y + 12, 13, TEXT_PRIMARY);

        float dy = rightRect.y + 34.0f;
        if (sys.tasks.count(sys.selectedTaskId)) {
            const Task& sel = sys.tasks[sys.selectedTaskId];
            DrawText((sel.id + ": " + sel.name).c_str(), (int)rightRect.x + 15, (int)dy, 14, TEXT_PRIMARY);
            dy += 22.0f;

            std::string st = "READY";
            Color stCol = COL_READY;
            if (sel.status == Status::Done) { st = "DONE"; stCol = COL_DONE; }
            else if (sys.affectedTasks.count(sel.id)) { st = "AFFECTED"; stCol = COL_AFFECTED; }
            else if (blockedMap.count(sel.id)) { st = "BLOCKED"; stCol = COL_BLOCKED; }

            DrawRectangleRounded(Rectangle{ rightRect.x + 15, dy, (float)MeasureText(st.c_str(), 10) + 12, 16 }, 0.3f, 3, stCol);
            DrawText(st.c_str(), (int)rightRect.x + 21, (int)dy + 3, 10, WHITE);
            dy += 22.0f;

            DrawText(("Duration: " + std::to_string(sel.duration) + " units | Priority: " + std::to_string(sel.priority)).c_str(), (int)rightRect.x + 15, (int)dy, 11, TEXT_MUTED); dy += 18.0f;
            std::string resStr = "Resources: ";
            if (sel.resources.empty()) resStr += "None";
            for (size_t i = 0; i < sel.resources.size(); ++i) resStr += (i ? ", " : "") + sel.resources[i] + " (" + sys.resources[sel.resources[i]].name + ")";
            DrawText(resStr.c_str(), (int)rightRect.x + 15, (int)dy, 11, TEXT_MUTED); dy += 18.0f;

            if (blockedMap.count(sel.id)) {
                DrawText("Blocked Reasons:", (int)rightRect.x + 15, (int)dy, 11, Color{ 231, 76, 60, 255 }); dy += 14.0f;
                for (const auto& rsn : blockedMap[sel.id]) {
                    DrawText((" * " + rsn).c_str(), (int)rightRect.x + 18, (int)dy, 10, Color{ 240, 140, 140, 255 }); dy += 13.0f;
                }
            } else if (sel.status == Status::Pending) {
                DrawText("Blocked Reasons: None (Ready to run)", (int)rightRect.x + 15, (int)dy, 11, COL_READY); dy += 16.0f;
            }

            auto ds = sys.singleTaskDownstream(sel.id);
            std::string dsStr = "Downstream: ";
            if (ds.empty()) dsStr += "None";
            for (size_t i = 0; i < ds.size(); ++i) dsStr += (i ? ", " : "") + ds[i];
            DrawText(dsStr.c_str(), (int)rightRect.x + 15, (int)dy, 11, TEXT_MUTED); dy += 22.0f;
        }

        DrawLine((int)rightRect.x + 10, (int)dy, (int)rightRect.x + (int)rightRect.width - 10, (int)dy, PANEL_BORDER);
        dy += 8.0f;
        DrawText("SYSTEM ACTIVITY LOG", (int)rightRect.x + 15, (int)dy, 12, TEXT_PRIMARY);
        dy += 18.0f;

        for (const auto& logMsg : sys.activityLog) {
            Color logCol = TEXT_MUTED;
            if (logMsg.find("CYCLE") != std::string::npos || logMsg.find("Error") != std::string::npos) logCol = Color{ 240, 110, 110, 255 };
            else if (logMsg.find("Recommended") != std::string::npos) logCol = COL_GOLD;
            else if (logMsg.find("DONE") != std::string::npos) logCol = COL_DONE;
            DrawText(logMsg.c_str(), (int)rightRect.x + 15, (int)dy, 10, logCol);
            dy += 14.0f;
        }

        // ====================================================================
        // 4. BOTTOM PANEL (Gantt Chart Timeline)
        // ====================================================================
        const Rectangle botRect = { 15, 460, 1170, 245 };
        DrawRectangleRounded(botRect, 0.02f, 4, PANEL_BG);
        DrawRectangleLinesEx(botRect, 1.0f, PANEL_BORDER);
        DrawText("GANTT SCHEDULE TIMELINE (Serial Earliest-Start Dispatcher)", (int)botRect.x + 15, (int)botRect.y + 10, 13, TEXT_PRIMARY);

        std::string msText = "Makespan: " + std::to_string(sys.makespan) + " units";
        DrawRectangleRounded(Rectangle{ botRect.x + 580, botRect.y + 8, (float)MeasureText(msText.c_str(), 11) + 16, 20 }, 0.3f, 3, Color{ 39, 174, 96, 220 });
        DrawText(msText.c_str(), (int)botRect.x + 588, (int)botRect.y + 12, 11, WHITE);

        if (!sys.unschedulable.empty()) {
            std::string unStr = "Unschedulable: ";
            for (size_t i = 0; i < sys.unschedulable.size(); ++i) unStr += (i ? ", " : "") + sys.unschedulable[i];
            DrawText(unStr.c_str(), (int)botRect.x + 750, (int)botRect.y + 12, 11, Color{ 231, 76, 60, 255 });
        } else {
            DrawText("Status: All tasks schedulable", (int)botRect.x + 750, (int)botRect.y + 12, 11, COL_READY);
        }

        float chartX = botRect.x + 130.0f;
        float chartY = botRect.y + 36.0f;
        float chartW = botRect.width - 150.0f;
        int maxTime = std::max(sys.makespan, 16);
        float timeScale = chartW / (float)maxTime;

        for (int t = 0; t <= maxTime; ++t) {
            float gx = chartX + (float)t * timeScale;
            DrawLine((int)gx, (int)chartY + 14, (int)gx, (int)botRect.y + (int)botRect.height - 10, Color{ 38, 46, 64, 255 });
            std::string ts = std::to_string(t);
            DrawText(ts.c_str(), (int)gx - MeasureText(ts.c_str(), 10) / 2, (int)chartY, 10, TEXT_MUTED);
        }

        const std::string orderedTasks[] = { "T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "T9" };
        float rowY = chartY + 16.0f;
        for (const auto& tid : orderedTasks) {
            if (!sys.tasks.count(tid)) continue;
            const Task& t = sys.tasks[tid];

            Color lblCol = (tid == sys.selectedTaskId) ? WHITE : TEXT_MUTED;
            DrawText(tid.c_str(), (int)botRect.x + 15, (int)rowY + 2, 11, lblCol);
            DrawText(t.name.substr(0, 14).c_str(), (int)botRect.x + 35, (int)rowY + 2, 10, TEXT_MUTED);

            if (sys.plan.count(tid)) {
                int st = sys.plan[tid].first, fn = sys.plan[tid].second;
                float bx = chartX + (float)st * timeScale, bw = (float)(fn - st) * timeScale;
                Color barCol = t.resources.empty() ? Color{ 52, 152, 219, 255 } : GetResourceColor(t.resources[0]);
                DrawRectangleRounded(Rectangle{ bx, rowY, bw, 16 }, 0.25f, 3, barCol);
                DrawRectangleLinesEx(Rectangle{ bx, rowY, bw, 16 }, 1.0f, Color{ 255, 255, 255, 60 });

                std::string barTxt = tid + " [" + std::to_string(st) + "-" + std::to_string(fn) + "]";
                if (!t.resources.empty()) barTxt += " (" + t.resources[0] + ")";
                DrawText(barTxt.c_str(), (int)bx + 4, (int)rowY + 3, 10, WHITE);
            } else {
                float bw = chartW * 0.45f;
                DrawRectangleRounded(Rectangle{ chartX, rowY, bw, 16 }, 0.25f, 3, Color{ 120, 30, 30, 160 });
                DrawRectangleLinesEx(Rectangle{ chartX, rowY, bw, 16 }, 1.0f, Color{ 231, 76, 60, 180 });
                DrawText("[UNSCHEDULABLE - Resource or prerequisite blocked]", (int)chartX + 8, (int)rowY + 3, 10, Color{ 250, 160, 160, 255 });
            }
            rowY += 21.0f;
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
