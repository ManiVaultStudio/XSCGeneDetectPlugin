#include "../src/ColumnTileCache.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <iostream>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2) return 2;
    QFile file(app.arguments()[1]); if (!file.open(QIODevice::ReadOnly)) return 3;
    std::vector<QByteArray> samples;
    const auto magic = QByteArray::fromHex("28b52ffd");
    for (int region = 0; region < 4; ++region) {
        file.seek(file.size() * region / 4);
        const auto bytes = file.read(8 << 20);
        qsizetype position = 0; int count = 0;
        while (count < 32 && (position = bytes.indexOf(magic, position)) >= 0) {
            const auto remaining = bytes.size() - position;
            const auto encoded = ZSTD_findFrameCompressedSize(bytes.constData() + position, remaining);
            const auto raw = ZSTD_getFrameContentSize(bytes.constData() + position, remaining);
            if (ZSTD_isError(encoded) || raw == ZSTD_CONTENTSIZE_ERROR || raw == ZSTD_CONTENTSIZE_UNKNOWN || raw > (1 << 20) || raw % 4) { ++position; continue; }
            QByteArray decoded(raw, Qt::Uninitialized);
            const auto result = ZSTD_decompress(decoded.data(), decoded.size(), bytes.constData() + position, encoded);
            if (!ZSTD_isError(result) && result == raw) { samples.push_back(decoded); ++count; }
            position += encoded;
        }
    }
    std::uint64_t rawBytes = 0, zeros = 0, values = 0;
    for (const auto& sample : samples) {
        rawBytes += sample.size();
        for (qsizetype i = 0; i < sample.size(); i += 4) { std::uint32_t value; std::memcpy(&value, sample.constData() + i, 4); zeros += value == 0; ++values; }
    }
    std::cout << samples.size() << " real frames; raw " << rawBytes << " bytes; zero bits " << (values ? 100.0 * zeros / values : 0) << "%\n";
    auto* context = ZSTD_createCCtx();
    for (const int level : {-3, -12, -30, -60}) {
        QElapsedTimer timer; timer.start(); std::uint64_t encoded = 0;
        for (const auto& sample : samples) {
            QByteArray output(ZSTD_compressBound(sample.size()), Qt::Uninitialized);
            const auto size = ZSTD_compressCCtx(context, output.data(), output.size(), sample.constData(), sample.size(), level);
            if (ZSTD_isError(size)) return 4;
            encoded += size;
        }
        std::cout << "zstd " << level << ": " << timer.elapsed() << " ms, " << encoded << " bytes (" << 100.0 * encoded / rawBytes << "%)\n";
    }
    ZSTD_freeCCtx(context);
    ColumnTileCache sparse(samples.size(), 64, 4, 128ULL << 20);
    QElapsedTimer timer; timer.start();
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto rows = samples[i].size() / (64 * 4);
        if (!rows || samples[i].size() % (64 * 4)) continue;
        sparse.offer(i, rows, 64, samples[i].constData());
        if (sparse.tile(i, rows, 64) != samples[i]) return 5;
    }
    std::cout << "adaptive zero-mask + fallback, write + roundtrip: " << timer.elapsed() << " ms, " << sparse.bytes() << " bytes\n";
}
