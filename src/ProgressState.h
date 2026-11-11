#pragma once
#include <QString>
#include <atomic>
#include <memory>
#include <mutex>
#include <algorithm>

struct OperationProgressState {
    std::atomic<float> value{0};
    std::atomic<std::size_t> done{0}, total{0};
    std::mutex mutex;
    QString stage = "Queued";
    void phase(const QString& text, float base) {
        { const std::lock_guard<std::mutex> lock(mutex); stage = text; }
        done = 0; total = 0; advance(0, 0, base, base);
    }
    void advance(std::size_t completed, std::size_t count, float from, float to) {
        auto previous = done.load();
        while (completed > previous && !done.compare_exchange_weak(previous, completed)) {}
        total = count;
        const float next = std::clamp(count ? from + (to-from) * float(completed)/float(count) : from, 0.0f, 0.999f);
        auto current = value.load();
        while (next > current && !value.compare_exchange_weak(current, next)) {}
    }
};
