#include "../src/ColumnTileCache.h"
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <thread>
#include <iostream>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
    QuaZip zip(app.arguments()[1]); if (!zip.open(QuaZip::mdUnzip)) return 3;
    QString entry; quint64 largest = 0;
    if (zip.goToFirstFile()) do {
        QuaZipFileInfo64 info; if (!zip.getCurrentFileInfo(&info)) return 4;
        if (info.name.endsWith(".ptpack") && info.method == 0 && info.uncompressedSize > largest) { entry = info.name; largest = info.uncompressedSize; }
    } while (zip.goToNextFile());
    if (entry.isEmpty() || !zip.setCurrentFile(entry)) return 5;
    QuaZipFile member(&zip); if (!member.open(QIODevice::ReadOnly)) return 6;
    const auto offset = unzGetCurrentFileZStreamPos64(zip.getUnzFile());
    QFile file(app.arguments()[1]); if (!file.open(QIODevice::ReadOnly)) return 7;
    constexpr std::size_t rows = 512, columns = 7754, rawSize = rows * columns * 4;
    std::vector<QByteArray> samples;
    const auto magic = QByteArray::fromHex("28b52ffd");
    for (unsigned region = 0; region < 8; ++region) {
        const auto first = largest * region / 8;
        if (!file.seek(offset + first)) return 8;
        const auto bytes = file.read(std::min<quint64>(16ULL << 20, largest - first));
        qsizetype position = 0;
        while ((position = bytes.indexOf(magic, position)) >= 0) {
            const auto size = ZSTD_findFrameCompressedSize(bytes.constData() + position, bytes.size() - position);
            const auto raw = ZSTD_getFrameContentSize(bytes.constData() + position, bytes.size() - position);
            if (!ZSTD_isError(size) && raw == rawSize) {
                QByteArray decoded(rawSize, Qt::Uninitialized);
                const auto result = ZSTD_decompress(decoded.data(), decoded.size(), bytes.constData() + position, size);
                if (!ZSTD_isError(result) && result == rawSize) { samples.push_back(decoded); break; }
            }
            ++position;
        }
    }
    if (samples.empty()) return 9;
    ColumnTileCache cache(samples.size(), columns, 4, 512ULL << 20);
    QElapsedTimer timer; timer.start();
    std::vector<std::thread> threads;
    for (std::size_t t = 0; t < samples.size(); ++t) threads.emplace_back([&, t] { cache.offer(t, rows, columns, samples[t].constData()); });
    for (auto& thread : threads) thread.join(); threads.clear();
    const auto packMs = timer.elapsed(); timer.restart();
    for (std::size_t t = 0; t < samples.size(); ++t) {
        if (cache.tile(t, rows, columns) != samples[t]) return 10;
    }
    std::cout << "REAL PROJECT: " << samples.size() << " geographically sampled expression tiles; " << cache.summary().toStdString() << "\n";
    std::cout << "pack " << packMs << " ms; exact full-tile validation " << timer.elapsed() << " ms\n";
    std::cout << "Estimated full cache from sampled ratio: " << (double(cache.bytes()) / (samples.size() * rawSize) * (2326579ULL * columns * 4) / 1e9) << " GB (estimate, not a coverage guarantee)\n";
}
