#include "../src/StoredZipRanges.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <thread>
#include <iostream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    const auto path = QDir(directory.path()).filePath("project.zip");
    QByteArray values(8 * 1024 * 1024, 0);
    for (int i = 0; i < values.size(); ++i) values[i] = char(i % 251);
    {
        QuaZip zip(path);
        if (!zip.open(QuaZip::mdCreate)) throw std::runtime_error("Cannot create archive");
        QuaZipFile stored(&zip);
        if (!stored.open(QIODevice::WriteOnly, QuaZipNewInfo("frames.ptpack"), nullptr, 0, 0, 0)) throw std::runtime_error("Cannot create stored entry");
        if (stored.write(values) != values.size()) throw std::runtime_error("Short archive write");
        stored.close();
        QuaZipFile compressed(&zip);
        if (!compressed.open(QIODevice::WriteOnly, QuaZipNewInfo("legacy.ptpack"))) throw std::runtime_error("Cannot create compressed entry");
        compressed.write(values); compressed.close(); zip.close();
    }
    const auto bytes = StoredZipRanges::read(path, "frames.ptpack", 100, 4096);
    if (!bytes || *bytes != values.mid(100, 4096)) throw std::runtime_error("Wrong stored frame offset");
    if (StoredZipRanges::read(path, "legacy.ptpack", 0, 10)) throw std::runtime_error("Compressed entries must use the compatible fallback");
    bool rejected = false;
    try { StoredZipRanges::read(path, "frames.ptpack", values.size() - 1, 2); }
    catch (const std::out_of_range&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Out-of-bounds range was accepted");
    QElapsedTimer timer; timer.start();
    std::atomic_bool failed{false};
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) workers.emplace_back([&, i] {
        for (int j = 0; j < 50; ++j) {
            const auto offset = (i * 65537 + j * 7919) % (values.size() - 262144);
            const auto data = StoredZipRanges::read(path, "frames.ptpack", offset, 262144);
            if (!data || *data != values.mid(offset, 262144)) failed = true;
        }
    });
    for (auto& worker : workers) worker.join();
    if (failed) throw std::runtime_error("Concurrent stored frame reads corrupted bytes");
    if (QDir(directory.path()).entryList(QDir::Files).size() != 1) throw std::runtime_error("Direct range reads created extraction files");
    std::cout << "PASS: stored offsets, range bounds, compressed fallback, 8 concurrent readers, no extraction files; " << timer.elapsed() << " ms\n";
}
