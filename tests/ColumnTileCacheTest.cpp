#include "../src/ColumnTileCache.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QElapsedTimer>
#include <thread>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QString path;
    {
        constexpr std::size_t rows = 512, columns = 130;
        ColumnTileCache cache(3, columns, sizeof(std::uint32_t), 8ULL << 20);
        require(cache.enabled(), "Cache must open"); path = cache.fileName();
        require(QFileInfo(path).absolutePath() == EvoViewerTemp::directory().absolutePath(), "Gene-block files must stay in the dedicated EvoViewer temp folder");
        require(cache.column(0, rows, columns, 5).isEmpty(), "Unpublished data must miss");
        std::vector<std::uint32_t> data(rows * columns);
        for (std::size_t r = 0; r < rows; ++r) for (std::size_t c = 0; c < columns; ++c)
            data[r * columns + c] = static_cast<std::uint32_t>((r * columns + c) * 2654435761U);
        cache.offer(0, rows, columns, reinterpret_cast<const char*>(data.data()));
        const auto dense = cache.tile(0, rows, columns);
        require(dense.size() == data.size() * 4 && std::memcmp(dense.constData(), data.data(), dense.size()) == 0,
            "Bulk cached tile reads must match source values bit for bit, including partial final stripes");
        {
            QFile independent;
            const auto denseIndependent = cache.tile(0, rows, columns, &independent);
            require(denseIndependent == dense, "Independent readers must reproduce the same bulk tile");
        }
        for (std::size_t c = 0; c < columns; ++c) {
            const auto result = cache.column(0, rows, columns, c);
            require(result.size() == rows * sizeof(std::uint32_t), "Wrong decoded column size");
            for (std::size_t r = 0; r < rows; ++r)
                require(std::memcmp(result.constData() + r * 4, &data[r * columns + c], 4) == 0,
                    "Lossless raw values, including arbitrary floating-point bit patterns, must survive transposition");
        }
        const auto size = cache.bytes();
        // A grouped trait/DE reader consumes whole narrow stripes. Check its
        // sparse-column sums and detection counts against the original tile.
        for (const std::size_t column : {0U, 63U, 64U, 129U}) {
            const auto stripe = cache.stripe(0, rows, columns, column);
            const auto first = column / ColumnTileCache::stripeWidth * ColumnTileCache::stripeWidth;
            const auto width = std::min(ColumnTileCache::stripeWidth, columns - first);
            std::uint64_t sum = 0, expected = 0, detected = 0, expectedDetected = 0;
            for (std::size_t row = 0; row < rows; row += 3) {
                std::uint32_t value = 0;
                std::memcpy(&value, stripe.constData() + (row * width + column - first) * 4, 4);
                sum += value; detected += value != 0;
                expected += data[row * columns + column]; expectedDetected += data[row * columns + column] != 0;
            }
            require(sum == expected && detected == expectedDetected, "Cached analysis stripes must preserve sparse row-group means and detection counts");
        }
        cache.offer(0, rows, columns, reinterpret_cast<const char*>(data.data()));
        require(size == cache.bytes(), "Repeated scans must not append duplicate blocks");
        std::thread writer([&] { cache.offer(1, 51, columns, reinterpret_cast<const char*>(data.data())); });
        QFile independent;
        for (int i = 0; i < 100; ++i)
            require(cache.column(0, rows, columns, 129, &independent).size() == rows * 4, "Published genes must remain readable through an independent handle during another tile write");
        writer.join();
        require(cache.column(1, 51, columns, 129).size() == 51 * 4, "Boundary tile rows and partial final stripes must work");
        std::vector<std::uint32_t> sparse(rows * columns, 0);
        sparse[0] = 0x80000000U; // negative float zero must remain a literal
        sparse[63] = 0x7fc12345U; // preserve NaN payload
        sparse.back() = 0x7f800000U; // infinity in final partial stripe
        cache.offer(2, rows, columns, reinterpret_cast<const char*>(sparse.data()));
        require(cache.tile(2, rows, columns) == QByteArray(reinterpret_cast<const char*>(sparse.data()), sparse.size() * 4),
            "Sparse encoding must preserve zeros, signed zero, NaN payloads and infinities exactly");
        ColumnTileCache::Reader ahead;
        require(cache.prefetch(2, columns, ahead), "Dense analysis read-ahead must load one encoded tile");
        require(cache.stripe(2, rows, columns, 129, ahead) == cache.stripe(2, rows, columns, 129), "Read-ahead and direct stripe decode must agree");
        for (const auto column : {0U, 65U, 129U}) {
            const auto expected = cache.column(2, rows, columns, column);
            require(!expected.isEmpty() && cache.stripe(2, rows, columns, column, ahead, true) == expected,
                "Single-column decoding must preserve the exact values in every stripe");
        }
        require(cache.fileName() == path, "Every tile/gene must use the same file");
    }
    require(!QFileInfo::exists(path), "Cache must be removed with its last owner");
    for (const std::size_t bytes : {1U, 2U, 4U, 8U}) {
        ColumnTileCache zeros(1, 130, bytes, 1024);
        QByteArray data(51 * 130 * bytes, 0);
        zeros.offer(0, 51, 130, data.constData());
        require(zeros.bytes() == 0 && zeros.tile(0, 51, 130) == data, "All-zero frames must use no disk payload for every supported scalar size");
    }
    {
        ColumnTileCache capped(1, 128, 4, 1);
        std::vector<float> data(512 * 128, 1.0f);
        capped.offer(0, 512, 128, reinterpret_cast<const char*>(data.data()));
        require(capped.full() && capped.bytes() <= 1 && capped.column(0, 512, 128, 0).isEmpty(), "Quota must bound disk use and leave a safe source fallback");
    }
    {
        QElapsedTimer timer; timer.start();
        constexpr std::size_t rows = 512, columns = 7754, tiles = 8;
        ColumnTileCache cache(tiles, columns, 4, 256ULL << 20);
        std::vector<float> data(rows * columns, 0.0f);
        for (std::size_t i = 0; i < data.size(); i += 71) data[i] = float(i % 193) / 7;
        std::vector<std::thread> writers;
        for (std::size_t t = 0; t < tiles; ++t)
            writers.emplace_back([&, t] { cache.offer(t, rows, columns, reinterpret_cast<const char*>(data.data())); });
        for (auto& writer : writers) writer.join();
        const auto build = timer.elapsed(); timer.restart();
        for (std::size_t t = 0; t < tiles; ++t)
            require(cache.column(t, rows, columns, 7753).size() == rows * 4, "Representative-width last gene must be cached");
        std::cout << "7754-column synthetic tiles: build " << build << " ms; gene read " << timer.elapsed()
            << " ms; file " << cache.bytes() << " bytes\n";
    }
    {
        constexpr std::size_t cells = 2326579, rows = 512, columns = 64;
        constexpr std::size_t tiles = (cells + rows - 1) / rows, workers = 8;
        ColumnTileCache cache(tiles, columns, 4, 256ULL << 20);
        std::vector<float> data(rows * columns, 0.0f);
        for (std::size_t i = 0; i < data.size(); i += 17) data[i] = float(i % 31);
        std::vector<std::thread> threads;
        for (std::size_t w = 0; w < workers; ++w) threads.emplace_back([&, w] {
            for (std::size_t t = w; t < tiles; t += workers)
                cache.offer(t, std::min(rows, cells - t * rows), columns, reinterpret_cast<const char*>(data.data()));
        });
        for (auto& thread : threads) thread.join(); threads.clear();
        QElapsedTimer timer; timer.start();
        std::vector<float> result(cells);
        for (std::size_t w = 0; w < workers; ++w) threads.emplace_back([&, w] {
            QFile reader;
            for (std::size_t t = w; t < tiles; t += workers) {
                const auto count = std::min(rows, cells - t * rows);
                const auto values = cache.column(t, count, columns, 17, &reader);
                if (values.size() != count * 4) std::terminate();
                std::memcpy(result.data() + t * rows, values.constData(), values.size());
            }
        });
        for (auto& thread : threads) thread.join();
        for (std::size_t r = 0; r < cells; ++r)
            require(result[r] == data[(r % rows) * columns + 17], "Full-length cached gene values must preserve row order");
        std::cout << "2326579-cell synthetic cached gene: " << timer.elapsed() << " ms with " << workers << " readers\n";
    }
    std::cout << "PASS: lossless column layout, partial stripes/tiles, one file, concurrent reads, deduplication, disk quota, cleanup\n";
}
