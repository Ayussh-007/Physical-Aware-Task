# Physical-Aware Task Scheduling and Resource Coordination System

> **DSA Microproject:** Graph (DAG), Priority Queue (Max-Heap), Kahn's Algorithm, and Iterative DFS Cycle Detection with a Desktop GUI in C++17 & Raylib.

![C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)
![Raylib](https://img.shields.io/badge/GUI-Raylib%20%2B%20Raygui-red.svg)
![Build](https://img.shields.io/badge/Build-Makefile-brightgreen.svg)

---

## 📌 Project Overview
In physical engineering environments (hardware prototyping, fabrication labs, specialized testing), tasks cannot simply be scheduled according to logical dependencies alone—they require access to constrained, exclusive physical apparatus (e.g., 3D printers, soldering stations, lab benches, specialists).

This system provides a full-featured graph-based scheduler with interactive GUI visualization:
- **Interactive DAG Visualization:** Automatically computes topological depth layers and draws directed dependency arrows.
- **Physical Resource Tracking:** Models physical equipment/personnel with real-time availability toggles.
- **Failure Impact Analysis (BFS):** Simulates equipment failure by tracing downstream affected tasks.
- **Cycle Prevention (DFS):** Rejects invalid task dependencies before insertion using an iterative DFS reachability check.
- **Serial Earliest-Start Dispatcher:** Integrates Kahn's algorithm with priority queues and physical resource reservation calendars.
- **Interactive Gantt Timeline:** Displays scheduled task intervals and makespan.

---

## 🧠 Core Data Structures & Operations

| Component | Data Structure | Complexity | Purpose |
| :--- | :--- | :--- | :--- |
| **Task Graph (DAG)** | `std::unordered_map<string, vector<string>>` | $\mathcal{O}(1)$ avg lookup | Dual adjacency lists: `succ` (forward) and `pred` (reverse). |
| **Entity Tables** | `std::unordered_map<string, Task>` | $\mathcal{O}(1)$ avg lookup | Constant-time retrieval of task & physical resource state. |
| **Resource User Index** | `std::unordered_map<string, set<string>>` | $\mathcal{O}(1)$ avg lookup | Reverse index mapping each resource to tasks that require it. |
| **Recommendation Engine** | `std::priority_queue` (Max-Heap) | $\mathcal{O}(R \log R)$ | Recommends the next ready task by `(priority DESC, outdegree DESC, id ASC)`. |
| **Cycle Prevention** | Explicit stack + `unordered_set` (DFS) | $\mathcal{O}(V + E)$ | Iterative DFS reachability check from $post$ to $pre$ prior to edge insertion. |
| **Topological Sort** | In-degree table + FIFO Queue | $\mathcal{O}(V + E)$ | Kahn's algorithm for deterministic task ordering. |
| **Serial Scheduler** | Kahn's algorithm + Max-Heap | $\mathcal{O}((V + E) \log V)$ | Serial earliest-start dispatch respecting resource exclusivity. |
| **Impact Analysis** | FIFO Queue + `unordered_set` (BFS) | $\mathcal{O}(V + E)$ | Computes downstream transitive closure when resources fail. |

---

## 🛠️ Build and Run

### Prerequisites
- C++17 compiler (`g++` or `clang++`)
- `raylib` library

### Building with Makefile
```bash
# Build the GUI application
make gui

# Run the application
./gui
```

### Manual Compilation Commands

#### Windows (`w64devkit` / MinGW-w64)
```cmd
g++ -std=c++17 -O2 main.cpp -I. -lraylib -lopengl32 -lgdi32 -lwinmm -o scheduler_demo.exe
scheduler_demo.exe
```

#### Linux (Ubuntu / Debian / Fedora / Arch)
```bash
g++ -std=c++17 -O2 main.cpp -I. -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o scheduler_demo
./scheduler_demo
```

#### macOS (Homebrew Clang++)
```bash
clang++ -std=c++17 -O2 main.cpp -I/opt/homebrew/include -I. -L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -o scheduler_demo
./scheduler_demo
```

---

## 🚀 Preloaded Demo Case Study

### Physical Resources
- **R1:** 3D Printer (Equipment)
- **R2:** Soldering Station (Equipment)
- **R3:** Electronics Lab (Location)
- **R4:** Dr. Mehta (Specialist)

### Tasks
- **T1:** Design PCB layout (Duration: 2, Priority: 5)
- **T2:** Design enclosure (Duration: 2, Priority: 3)
- **T3:** Fabricate PCB (Duration: 3, Priority: 5, Needs: R3, Prereqs: T1)
- **T4:** Print enclosure (Duration: 4, Priority: 3, Needs: R1, Prereqs: T2)
- **T5:** Solder components (Duration: 3, Priority: 4, Needs: R2+R3, Prereqs: T3)
- **T6:** Write firmware (Duration: 5, Priority: 2, Prereqs: T1)
- **T7:** Assemble prototype (Duration: 2, Priority: 4, Needs: R3, Prereqs: T4+T5)
- **T8:** Functional test (Duration: 2, Priority: 5, Needs: R3+R4, Prereqs: T6+T7)
- **T9:** Documentation (Duration: 3, Priority: 1, Prereqs: T8)

---

## 🧪 Demo Flow Walkthrough

1. **Initial State:** Tasks `T1` and `T2` appear **Green (Ready)** with 0 in-degree. All other tasks are **Grey (Blocked)**.
2. **Recommend Next:** Click **"Recommend Next Task"** $\to$ **T1** is highlighted with a gold ring (highest priority and out-degree).
3. **Run Schedule:** Click **"Run Serial Schedule"** $\to$ Gantt timeline displays makespan **15 time units**.
4. **Simulate Equipment Failure:** Untick **R2: Soldering Station** $\to$ `T5`, `T7`, `T8`, `T9` immediately turn **Red (Affected)** and are dropped as unschedulable.
5. **Cycle Detection:** Enter dependency `T9 -> T1` and click **"+ Add"** $\to$ Iterative DFS flags the cycle and displays a red error banner.
