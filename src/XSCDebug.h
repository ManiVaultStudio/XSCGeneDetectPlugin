#pragma once

// ============================================================================
//  Debug and timing switch for the XSC gene-detection plugin
//
//    XSC_DEBUG 1  -> print debug messages and timings (qDebug)
//    XSC_DEBUG 0  -> print nothing; every XSC_TRACE / XSC_LOG compiles to nothing
//
//  Change the value below to switch the whole plugin on or off.
// ============================================================================
#define XSC_DEBUG 0

#if XSC_DEBUG

#include <QDebug>
#include <QString>

#include <chrono>

namespace xsc::debug {

/** Prints the entry and the elapsed time of the enclosing scope. */
class ScopeTimer {
public:
    explicit ScopeTimer(const char* name) :
        _name(name),
        _start(std::chrono::steady_clock::now())
    {
        qDebug().noquote() << "[XSC] >" << _name;
    }

    ~ScopeTimer()
    {
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - _start).count();
        qDebug().noquote() << "[XSC] <" << _name << "took" << QString::number(elapsed, 'f', 3) << "ms";
    }

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    const char*                           _name;
    std::chrono::steady_clock::time_point _start;
};

} // namespace xsc::debug

#define XSC_CONCAT_INNER(a, b) a##b
#define XSC_CONCAT(a, b) XSC_CONCAT_INNER(a, b)

/** Timing and entry message for the rest of the enclosing scope. */
#define XSC_TRACE(name) xsc::debug::ScopeTimer XSC_CONCAT(xscTrace_, __LINE__)(name)

namespace xsc::debug {

/** Prints one debug line: every argument is streamed in order. */
template <class... Args>
inline void log(const Args&... args)
{
    QDebug debug = qDebug().noquote();
    debug << "[XSC]";
    (debug << ... << args);
}

} // namespace xsc::debug

/** Debug message: XSC_LOG("rows:", rows, "blocks:", blocks); */
#define XSC_LOG(...) xsc::debug::log(__VA_ARGS__)

#else

#define XSC_TRACE(name) static_cast<void>(0)
#define XSC_LOG(...)    static_cast<void>(0)

#endif
