#pragma once
#include "EvoViewerTemp.h"

// Timings stay enabled independently of verbose debug output.
#define XSC_DEBUG 0
#ifndef XSC_TIMING
#define XSC_TIMING 1
#endif

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QString>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include <unordered_map>

namespace xsc::timing {
using Clock = std::chrono::steady_clock;
inline std::atomic<std::uint64_t> nextId{ 1 };
inline thread_local std::vector<std::uint64_t> parents;

// Buffered CSV: every scope is recorded without a console write for each tile.
class Recorder {
public:
    Recorder() {
        _path = EvoViewerTemp::directory().filePath(QStringLiteral("XSCGeneDetect-timings-%1-%2.csv")
            .arg(QCoreApplication::applicationPid()).arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")));
        _file.setFileName(_path);
        if (_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            _file.write("event,id,parent,thread,main_thread,time_ms,duration_ms,operation,value\n");
            _file.flush();
            qInfo().noquote() << "[XSC timing] Full operation log:" << _path;
        }
    }
    ~Recorder() { flush(); }
    static Recorder& instance() { static Recorder recorder; return recorder; }
    QString path() const { return _path; }
    void record(const char* event, std::uint64_t id, std::uint64_t parent, const char* operation,
        double duration = 0.0, double value = 0.0) noexcept {
        try {
            const auto now = Clock::now();
            const auto ms = std::chrono::duration<double, std::milli>(now - _start).count();
            const auto thread = reinterpret_cast<quintptr>(QThread::currentThreadId());
            const bool main = QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread();
            QByteArray label(operation);
            label.replace("\"", "\"\"");
            const QByteArray line = QByteArray(event) + ',' + QByteArray::number(id) + ',' + QByteArray::number(parent) + ','
                + QByteArray::number(thread, 16) + ',' + (main ? "1" : "0") + ',' + QByteArray::number(ms, 'f', 3) + ','
                + QByteArray::number(duration, 'f', 3) + ",\"" + label + "\"," + QByteArray::number(value, 'g', 12) + '\n';
            const std::lock_guard<std::mutex> lock(_mutex);
            _buffer += line;
            if (_buffer.size() >= 65536 || now - _lastFlush >= std::chrono::seconds(1)) flushLocked(now);
        } catch (...) { } // diagnostics must never break analysis
    }
    void flush() noexcept {
        try { const std::lock_guard<std::mutex> lock(_mutex); flushLocked(Clock::now()); } catch (...) { }
    }
private:
    void flushLocked(Clock::time_point now) {
        if (_file.isOpen() && !_buffer.isEmpty()) { _file.write(_buffer); _file.flush(); _buffer.clear(); }
        _lastFlush = now;
    }
    QString _path;
    QFile _file;
    QByteArray _buffer;
    std::mutex _mutex;
    Clock::time_point _start = Clock::now(), _lastFlush = Clock::now();
};

// Async spans explicitly finish at publication, covering queue wait and reading.
class AsyncSpan {
public:
    explicit AsyncSpan(const char* name) : _name(name), _id(nextId.fetch_add(1)),
        _parent(parents.empty() ? 0 : parents.back()), _start(Clock::now()) {
        Recorder::instance().record("begin", _id, _parent, _name);
    }
    ~AsyncSpan() { finish(); }
    void finish() noexcept {
        if (_finished.exchange(true)) return;
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - _start).count();
        Recorder::instance().record("end", _id, _parent, _name, ms);
        if (ms >= 50.0) qInfo().noquote() << "[XSC timing]" << _name << QString::number(ms, 'f', 3) << "ms";
        Recorder::instance().flush();
    }
    std::uint64_t id() const { return _id; }
private:
    const char* _name;
    std::uint64_t _id, _parent;
    Clock::time_point _start;
    std::atomic_bool _finished{ false };
};

class ScopeTimer {
public:
    explicit ScopeTimer(const char* name) : _name(name), _id(nextId.fetch_add(1)),
        _parent(parents.empty() ? 0 : parents.back()), _start(Clock::now()) {
        Recorder::instance().record("begin", _id, _parent, _name);
        parents.push_back(_id);
    }
    ~ScopeTimer() noexcept {
        parents.pop_back();
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - _start).count();
        Recorder::instance().record("end", _id, _parent, _name, ms);
        if (_parent == 0) Recorder::instance().flush();
    }
    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;
private:
    const char* _name;
    std::uint64_t _id, _parent;
    Clock::time_point _start;
};
inline void counter(const char* name, double value) {
    Recorder::instance().record("counter", nextId.fetch_add(1), parents.empty() ? 0 : parents.back(), name, 0, value);
}

// Sorting comparators can run millions of times: sample their CPU cost rather
// than writing two log records per comparison and changing the measured workload.
class HotScopeTimer {
    struct Sample { std::uint64_t calls = 0, measured = 0; double ms = 0; };
    inline static thread_local std::unordered_map<const char*, Sample> samples;
public:
    explicit HotScopeTimer(const char* name) : _name(name), _sample(&samples[name]) {
        ++_sample->calls;
        _measure = _sample->calls == 1 || (_sample->calls % 128) == 0;
        if (_measure) _start = Clock::now();
    }
    ~HotScopeTimer() noexcept {
        if (_measure) {
            _sample->ms += std::chrono::duration<double, std::milli>(Clock::now() - _start).count();
            ++_sample->measured;
        }
        if (_sample->calls == 1 || (_sample->calls % 4096) == 0) {
            const double estimate = _sample->measured ? _sample->ms * _sample->calls / _sample->measured : 0;
            Recorder::instance().record("sample", nextId.fetch_add(1), 0, _name, estimate, static_cast<double>(_sample->calls));
        }
    }
private:
    const char* _name;
    Sample* _sample;
    bool _measure = false;
    Clock::time_point _start;
};
}

#define XSC_CONCAT_INNER(a, b) a##b
#define XSC_CONCAT(a, b) XSC_CONCAT_INNER(a, b)
#if XSC_TIMING
#define XSC_TRACE(name) xsc::timing::ScopeTimer XSC_CONCAT(xscTrace_, __LINE__)(name)
#define XSC_TIMING_VALUE(name, value) xsc::timing::counter(name, static_cast<double>(value))
#define XSC_HOT_TRACE(name) xsc::timing::HotScopeTimer XSC_CONCAT(xscHotTrace_, __LINE__)(name)
#else
#define XSC_TRACE(name) static_cast<void>(0)
#define XSC_TIMING_VALUE(name, value) static_cast<void>(0)
#define XSC_HOT_TRACE(name) static_cast<void>(0)
#endif
#if XSC_DEBUG
namespace xsc::debug {
template <class... Args> inline void log(const Args&... args) {
    QDebug debug = qDebug().noquote(); debug << "[XSC]"; (debug << ... << args);
}
}
#define XSC_LOG(...) xsc::debug::log(__VA_ARGS__)
#else
#define XSC_LOG(...) static_cast<void>(0)
#endif
