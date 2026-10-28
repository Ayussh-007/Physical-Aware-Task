// Physical-Aware Task Scheduling and Resource Coordination System  (C++17)
// Data structures used
//   * Graph (DAG, adjacency lists) : task dependencies          (succ / pred)
//   * Hash tables (unordered_map)  : task table, resource table, resource->tasks index
//   * Priority queue (binary heap) : next-task recommendation and scheduling order
#pragma once
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class Status { Pending, Done };

struct CycleError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Task {
    std::string id, name;
    int duration = 1, priority = 1;
    std::vector<std::string> resources;
    int plannedStart = -1;                       // -1 = not planned manually
    Status status = Status::Pending;
};

struct Resource {
    std::string id, name, kind;
    bool available = true;
};

using Plan = std::map<std::string, std::pair<int, int>>;   // task -> (start, finish)
using IdSet = std::set<std::string>;

class Scheduler {
public:
    std::unordered_map<std::string, Task> tasks;         // hash table: id -> Task
    std::unordered_map<std::string, Resource> resources; // hash table: id -> Resource
    std::unordered_map<std::string, IdSet> succ;         // adjacency list: task -> dependents
    std::unordered_map<std::string, IdSet> pred;         // reverse adjacency: task -> prerequisites
    std::unordered_map<std::string, IdSet> users;        // hash index: resource -> tasks using it

    // ------------------------------------------------------------ insertion
    void addResource(const std::string& id, const std::string& name,
                     const std::string& kind = "equipment") {                       // O(1)
        if (resources.count(id)) throw std::invalid_argument("resource " + id + " already exists");
        resources[id] = Resource{id, name, kind, true};
        users[id];
    }

    void addTask(const std::string& id, const std::string& name, int duration = 1, int priority = 1,
                 const std::vector<std::string>& res = {}, int plannedStart = -1) {   // O(k)
        if (tasks.count(id)) throw std::invalid_argument("task " + id + " already exists");
        if (duration <= 0) throw std::invalid_argument("duration must be positive");
        for (const auto& r : res)
            if (!resources.count(r)) throw std::out_of_range("unknown resource " + r);
        tasks[id] = Task{id, name, duration, priority, res, plannedStart, Status::Pending};
        succ[id]; pred[id];
        for (const auto& r : res) users[r].insert(id);
    }

    void addDependency(const std::string& pre, const std::string& post) {          // O(V+E) (cycle check)
        if (!tasks.count(pre) || !tasks.count(post)) throw std::out_of_range("both tasks must exist");
        if (pre == post || reachable(post, pre))
            throw CycleError(pre + " -> " + post + " would create a cycle");
        succ[pre].insert(post);
        pred[post].insert(pre);
    }

    // Bulk loading (benchmarks): caller guarantees the edge keeps the graph acyclic.
    void addDependencyUnchecked(const std::string& pre, const std::string& post) {  // O(log d)
        succ[pre].insert(post);
        pred[post].insert(pre);
    }

    // ------------------------------------------------------------- deletion
    void removeTask(const std::string& id) {                       // O(deg log deg + k log m)
        if (!tasks.count(id)) throw std::out_of_range(id);
        for (const auto& p : pred[id]) succ[p].erase(id);
        for (const auto& s : succ[id]) pred[s].erase(id);
        for (const auto& r : tasks[id].resources) users[r].erase(id);
        tasks.erase(id); succ.erase(id); pred.erase(id);
    }

    // ------------------------------------------------------------ searching
    const Task* getTask(const std::string& id) const {                             // O(1) average
        auto it = tasks.find(id);
        return it == tasks.end() ? nullptr : &it->second;
    }

    // ------------------------------------------- sorting (Kahn's algorithm)
    std::vector<std::string> topologicalOrder() const {                            // O(V+E)
        std::unordered_map<std::string, int> indeg;
        for (const auto& kv : tasks) indeg[kv.first] = (int)pred.at(kv.first).size();
        std::vector<std::string> start;
        for (const auto& kv : indeg) if (kv.second == 0) start.push_back(kv.first);
        std::sort(start.begin(), start.end());               // repeatable output order
        std::queue<std::string> q;
        for (auto& s : start) q.push(s);
        std::vector<std::string> order;
        while (!q.empty()) {
            std::string u = q.front(); q.pop();
            order.push_back(u);
            for (const auto& v : succ.at(u))
                if (--indeg[v] == 0) q.push(v);
        }
        if (order.size() != tasks.size()) throw CycleError("graph contains a cycle");
        return order;
    }

    // ------------------------------------------------- traversal (BFS)
    IdSet downstream(const std::string& id) const {                                // O(V+E)
        IdSet r = bfs({id});
        r.erase(id);
        return r;
    }

    // ------------------------------------------- blocked / ready detection
    // {task id -> reasons} for every pending task that cannot start now.
    std::map<std::string, std::vector<std::string>> blockedTasks() const {         // O(V+E)
        std::map<std::string, std::vector<std::string>> out;
        for (const auto& kv : tasks) {
            const Task& t = kv.second;
            if (t.status == Status::Done) continue;
            std::vector<std::string> reasons;
            for (const auto& p : pred.at(t.id))
                if (tasks.at(p).status != Status::Done) reasons.push_back("waiting for " + p);
            for (const auto& r : t.resources)
                if (!resources.at(r).available) reasons.push_back("resource " + r + " unavailable");
            if (!reasons.empty()) out[t.id] = reasons;
        }
        return out;
    }

    std::vector<std::string> readyTasks() const {                                  // sorted by id
        auto blocked = blockedTasks();
        std::vector<std::string> r;
        for (const auto& kv : tasks)
            if (kv.second.status == Status::Pending && !blocked.count(kv.first)) r.push_back(kv.first);
        std::sort(r.begin(), r.end());
        return r;
    }

    // -------------------------------------- priority queue recommendation
    // Top-k feasible tasks: highest priority first; ties -> unlocks most tasks; then smaller id.
    std::vector<std::string> recommendNext(int k = 1) const {                      // O(V+E + k log R)
        struct Item { int pri; int outdeg; std::string id; };
        auto lower = [](const Item& a, const Item& b) {      // "a ranks below b"
            if (a.pri != b.pri) return a.pri < b.pri;
            if (a.outdeg != b.outdeg) return a.outdeg < b.outdeg;
            return a.id > b.id;
        };
        std::vector<Item> items;
        for (const auto& id : readyTasks())
            items.push_back({tasks.at(id).priority, (int)succ.at(id).size(), id});
        std::priority_queue<Item, std::vector<Item>, decltype(lower)>
            heap(lower, std::move(items));                                          // O(R) heapify
        std::vector<std::string> out;
        while (k-- > 0 && !heap.empty()) { out.push_back(heap.top().id); heap.pop(); }
        return out;
    }

    // Marks a ready task finished; returns the tasks it unlocked.
    std::vector<std::string> markDone(const std::string& id) {
        if (!tasks.count(id)) throw std::out_of_range(id);
        if (blockedTasks().count(id)) throw std::runtime_error(id + " is blocked");
        tasks[id].status = Status::Done;
        auto ready = readyTasks();
        std::vector<std::string> unlocked;
        for (const auto& s : succ[id])
            if (std::binary_search(ready.begin(), ready.end(), s)) unlocked.push_back(s);
        return unlocked;
    }

    // ------------------------------------------------ resource conflicts
    struct Conflict { std::string resource, first, second; };

    // Overlapping planned intervals on the same resource (sort + sweep).
    std::vector<Conflict> detectConflicts() const {                 // O(m log m) per resource
        std::vector<Conflict> out;
        std::vector<std::string> rids;
        for (const auto& kv : users) rids.push_back(kv.first);
        std::sort(rids.begin(), rids.end());
        for (const auto& rid : rids) {
            struct Iv { int s, f; std::string id; };
            std::vector<Iv> iv;
            for (const auto& t : users.at(rid)) {
                const Task& k = tasks.at(t);
                if (k.plannedStart >= 0) iv.push_back({k.plannedStart, k.plannedStart + k.duration, t});
            }
            if (iv.empty()) continue;
            std::sort(iv.begin(), iv.end(), [](const Iv& a, const Iv& b) {
                return std::tie(a.s, a.f, a.id) < std::tie(b.s, b.f, b.id); });
            Iv cur = iv[0];
            for (size_t i = 1; i < iv.size(); ++i) {
                if (iv[i].s < cur.f) out.push_back({rid, cur.id, iv[i].id});
                if (iv[i].f > cur.f) cur = iv[i];
            }
        }
        return out;
    }

    // Tasks planned to start before a prerequisite has finished: (prerequisite, task).
    std::vector<std::pair<std::string, std::string>> dependencyViolations() const {
        std::vector<std::pair<std::string, std::string>> bad;
        std::vector<std::string> ids;
        for (const auto& kv : tasks) ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (const auto& v : ids)
            for (const auto& p : pred.at(v)) {
                const Task &a = tasks.at(p), &b = tasks.at(v);
                if (a.plannedStart >= 0 && b.plannedStart >= 0 &&
                    a.plannedStart + a.duration > b.plannedStart)
                    bad.emplace_back(p, v);
            }
        return bad;
    }

    // ----------------------------------------------------- replanning
    // Changes a resource's state; returns only the tasks whose plan is affected.
    std::vector<std::string> setResourceAvailability(const std::string& rid, bool available) {  // O(V+E)
        resources.at(rid).available = available;
        std::vector<std::string> direct;
        for (const auto& t : users.at(rid))
            if (tasks.at(t).status == Status::Pending) direct.push_back(t);
        if (direct.empty()) return {};
        IdSet seen = bfs(direct);
        return std::vector<std::string>(seen.begin(), seen.end());      // std::set -> already sorted
    }

    // --------------------------------------------------- scheduling
    // Serial schedule generation: Kahn's algorithm + priority heap.
    // Returns the plan {task -> (start, finish)} and the tasks that could not be scheduled.
    std::pair<Plan, std::vector<std::string>> schedule() const {                  // O((V+E) log V)
        IdSet pend;
        for (const auto& kv : tasks) if (kv.second.status == Status::Pending) pend.insert(kv.first);
        std::unordered_map<std::string, int> indeg;
        for (const auto& t : pend) {
            int d = 0;
            for (const auto& p : pred.at(t)) d += pend.count(p) ? 1 : 0;
            indeg[t] = d;
        }
        using Item = std::pair<int, std::string>;                            // (priority, id)
        auto lower = [](const Item& a, const Item& b) {
            return a.first != b.first ? a.first < b.first : a.second > b.second;
        };
        std::priority_queue<Item, std::vector<Item>, decltype(lower)> heap(lower);
        for (const auto& kv : indeg) if (kv.second == 0) heap.push({tasks.at(kv.first).priority, kv.first});
        std::unordered_map<std::string, int> resFree;
        for (const auto& kv : resources) resFree[kv.first] = 0;
        Plan plan;
        while (!heap.empty()) {
            std::string u = heap.top().second; heap.pop();
            const Task& task = tasks.at(u);
            bool blocked = false;
            for (const auto& r : task.resources) if (!resources.at(r).available) blocked = true;
            if (blocked) continue;                       // blocked: successors are not released
            int start = 0;
            for (const auto& p : pred.at(u)) {
                auto it = plan.find(p);
                if (it != plan.end()) start = std::max(start, it->second.second);
            }
            for (const auto& r : task.resources) start = std::max(start, resFree[r]);
            plan[u] = {start, start + task.duration};
            for (const auto& r : task.resources) resFree[r] = start + task.duration;
            for (const auto& v : succ.at(u))
                if (pend.count(v) && --indeg[v] == 0) heap.push({tasks.at(v).priority, v});
        }
        std::vector<std::string> unscheduled;
        for (const auto& t : pend) if (!plan.count(t)) unscheduled.push_back(t);
        return {plan, unscheduled};
    }

    // Independent checker used by the tests: dependencies + resource exclusivity.
    bool verifyPlan(const Plan& plan) const {
        for (const auto& kv : plan)
            for (const auto& p : pred.at(kv.first)) {
                auto it = plan.find(p);
                if (it != plan.end() && it->second.second > kv.second.first) return false;
            }
        for (const auto& ru : users) {
            std::vector<std::pair<int, int>> iv;
            for (const auto& t : ru.second) {
                auto it = plan.find(t);
                if (it != plan.end()) iv.push_back(it->second);
            }
            std::sort(iv.begin(), iv.end());
            for (size_t i = 0; i + 1 < iv.size(); ++i) if (iv[i].second > iv[i + 1].first) return false;
        }
        return true;
    }

private:
    bool reachable(const std::string& src, const std::string& dst) const {        // iterative DFS
        std::vector<std::string> stack{src};
        std::unordered_set<std::string> seen{src};
        while (!stack.empty()) {
            std::string u = stack.back(); stack.pop_back();
            if (u == dst) return true;
            for (const auto& v : succ.at(u))
                if (seen.insert(v).second) stack.push_back(v);
        }
        return false;
    }

    IdSet bfs(const std::vector<std::string>& sources) const {   // O(V+E) traversal, sorted result
        std::unordered_set<std::string> seen(sources.begin(), sources.end());
        std::queue<std::string> q;
        for (const auto& s : sources) q.push(s);
        while (!q.empty()) {
            std::string u = q.front(); q.pop();
            for (const auto& v : succ.at(u))
                if (seen.insert(v).second) q.push(v);
        }
        return IdSet(seen.begin(), seen.end());
    }
};
