#include "../src/XSCDebug.h"
#include <QCoreApplication>
#include <thread>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    {
        XSC_TRACE("test.main");
        xsc::timing::AsyncSpan async("test.async");
        std::thread worker([&] {
            XSC_TRACE("test.worker");
            XSC_TIMING_VALUE("test.jobs", 8193);
            async.finish();
        });
        worker.join();
    }
    xsc::timing::Recorder::instance().flush();
    QFile file(xsc::timing::Recorder::instance().path());
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing timing file");
    const auto lines = file.readAll().split('\n');
    int begins = 0, ends = 0, main = 0, worker = 0, counters = 0;
    for (const auto& line : lines) {
        const auto fields = line.split(',');
        if (fields.size() != 9) continue;
        if (fields[0] == "begin") ++begins;
        if (fields[0] == "end") {
            ++ends;
            if (fields[4] == "1") ++main; else if (fields[4] == "0") ++worker;
            if (fields[6].toDouble() < 0) throw std::runtime_error("Negative duration");
        }
        if (fields[0] == "counter" && fields[8].toInt() == 8193) ++counters;
    }
    if (begins != 3 || ends != 3 || main != 1 || worker != 2 || counters != 1)
        throw std::runtime_error("Incomplete scopes, duplicated async finish, or incorrect thread attribution");
    std::cout << "PASS: scoped/async timings, worker attribution, and job counters\n";
}
