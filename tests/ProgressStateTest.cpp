#include "../src/ProgressState.h"
#include <thread>
#include <vector>
#include <iostream>
#include <stdexcept>
int main()
{
    OperationProgressState state;
    state.phase("Read", 0.02f);
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) workers.emplace_back([&, i] {
        for (std::size_t n = i; n < 8193; n += 8) state.advance(n + 1, 8193, .02f, .65f);
    });
    for (auto& worker : workers) worker.join();
    if (state.value < .649f || state.done != 8193) throw std::runtime_error("Parallel progress regressed");
    state.phase("Statistics", .65f); state.advance(25, 25, .65f, .99f);
    if (state.value < .989f || state.value >= 1.0f) throw std::runtime_error("100% must be reserved for publication");
    state.advance(0, 0, 0, 1);
    if (state.value < .989f) throw std::runtime_error("Queued work must not reset progress");
    std::cout << "PASS: concurrent monotonic progress, empty stages, publication boundary\n";
}
