// Electronics-prototype case study used by the demo and the tests.
#pragma once
#include "scheduler.hpp"

inline Scheduler buildDemo() {
    Scheduler s;
    s.addResource("R1", "3D Printer", "equipment");
    s.addResource("R2", "Soldering Station", "equipment");
    s.addResource("R3", "Electronics Lab", "location");
    s.addResource("R4", "Dr. Mehta (test specialist)", "person");
    s.addTask("T1", "Design PCB layout",  2, 5);
    s.addTask("T2", "Design enclosure",   2, 3);
    s.addTask("T3", "Fabricate PCB",      3, 5, {"R3"});
    s.addTask("T4", "Print enclosure",    4, 3, {"R1"});
    s.addTask("T5", "Solder components",  3, 4, {"R2", "R3"});
    s.addTask("T6", "Write firmware",     5, 2);
    s.addTask("T7", "Assemble prototype", 2, 4, {"R3"});
    s.addTask("T8", "Functional test",    2, 5, {"R3", "R4"});
    s.addTask("T9", "Documentation",      3, 1);
    const char* edges[][2] = {{"T1","T3"},{"T1","T6"},{"T2","T4"},{"T3","T5"},{"T4","T7"},
                              {"T5","T7"},{"T7","T8"},{"T6","T8"},{"T8","T9"}};
    for (auto& e : edges) s.addDependency(e[0], e[1]);
    return s;
}
