#pragma once
#include "XSCDebug.h"
#include <QTemporaryFile>
#include <QHash>
#include <list>
#include <memory>
#include <mutex>
#include <functional>
#include <stdexcept>

// One backing file per expression source. Only complete vectors are published.
// Reads/writes release the file lock between 1 MiB chunks, so a long write cannot
// monopolize access to other genes that are already ready.
class GeneExpressionCache {
    enum class EntryState { Writing, DiskReady, RamOnly };
public:
    using Values = std::shared_ptr<const std::vector<float>>;
    explicit GeneExpressionCache(std::size_t ramBudget, std::size_t diskBudget = 0, bool memoryOnly = false) : _ramBudget(ramBudget),
        _diskBudget(memoryOnly ? 0 : (diskBudget ? diskBudget : std::clamp<std::size_t>(ramBudget * 8, 512ULL << 20, 4ULL << 30))) {
        if (_diskBudget && !_file.open()) throw std::runtime_error("Cannot open expression cache file");
    }
    void put(const QString& dataset, const QString& gene, Values values,
        const std::function<bool()>& cancelled = {}) {
        XSC_TRACE("expression.cache.write");
        const QString key = dataset + QChar(0x1f) + gene;
        auto entry = std::make_shared<Entry>();
        const qint64 bytes = static_cast<qint64>(values->size() * sizeof(float));
        {
            const std::lock_guard<std::mutex> lock(_mutex);
            if (const auto existing = _entries.value(key)) {
                if (existing->count != values->size()) throw std::invalid_argument("Expression cache vector size changed");
                // A concurrent complete vector may be served from RAM while
                // the first writer finishes its disk copy.
                if (!existing->ram) retain(key, *existing, std::move(values));
                else _lru.splice(_lru.begin(), _lru, existing->position);
                return;
            }
            entry->offset = _nextOffset; entry->count = values->size();
            if (!_diskBudget || values->size() > (_diskBudget - static_cast<std::size_t>(_nextOffset)) / sizeof(float)) {
                // Keep recent interactive values in RAM when the backing file
                // reaches its cap; never grow the disk cache without a bound.
                entry->offset = -1;
                entry->state = EntryState::RamOnly;
                _entries.insert(key, entry);
                retain(key, *entry, std::move(values));
                return;
            }
            _nextOffset += bytes;
            _entries.insert(key, entry);
        }
        try {
            for (qint64 offset = 0; offset < bytes; offset += chunkSize) {
                if (cancelled && cancelled()) throw std::runtime_error("Cancelled");
                const qint64 size = std::min(chunkSize, bytes - offset);
                const std::lock_guard<std::mutex> lock(_mutex);
                if (!_file.seek(entry->offset + offset) ||
                    _file.write(reinterpret_cast<const char*>(values->data()) + offset, size) != size)
                    throw std::runtime_error("Expression cache write failed");
            }
            const std::lock_guard<std::mutex> lock(_mutex);
            if (!_file.flush()) throw std::runtime_error("Expression cache flush failed");
            if (cancelled && cancelled()) throw std::runtime_error("Cancelled");
            entry->state = EntryState::DiskReady;
            retain(key, *entry, std::move(values));
        }
        catch (...) {
            const std::lock_guard<std::mutex> lock(_mutex);
            _entries.remove(key);
            throw;
        }
    }
    Values read(const QString& dataset, const QString& gene, std::size_t expectedCount) {
        XSC_TRACE("expression.cache.read");
        const QString key = dataset + QChar(0x1f) + gene;
        std::shared_ptr<Entry> entry;
        {
            const std::lock_guard<std::mutex> lock(_mutex);
            entry = _entries.value(key);
            if (!entry || entry->count != expectedCount) return {};
            if (entry->ram) {
                _lru.splice(_lru.begin(), _lru, entry->position);
                XSC_TIMING_VALUE("expression.cache.ram-hit", 1);
                return entry->ram;
            }
            if (entry->state != EntryState::DiskReady) return {};
        }
        auto values = std::make_shared<std::vector<float>>(expectedCount);
        const qint64 bytes = static_cast<qint64>(expectedCount * sizeof(float));
        for (qint64 offset = 0; offset < bytes; offset += chunkSize) {
            const qint64 size = std::min(chunkSize, bytes - offset);
            const std::lock_guard<std::mutex> lock(_mutex);
            if (!_file.seek(entry->offset + offset) ||
                _file.read(reinterpret_cast<char*>(values->data()) + offset, size) != size)
                throw std::runtime_error("Expression cache read failed");
        }
        {
            const std::lock_guard<std::mutex> lock(_mutex);
            if (!entry->ram) retain(key, *entry, values);
        }
        return values;
    }
    QString fileName() const { return _file.fileName(); }
    std::size_t ramBytes() const { const std::lock_guard<std::mutex> lock(_mutex); return _ramBytes; }
private:
    struct Entry {
        qint64 offset = 0;
        std::size_t count = 0;
        EntryState state = EntryState::Writing;
        Values ram;
        std::list<QString>::iterator position;
    };
    void retain(const QString& key, Entry& entry, Values values) {
        const auto bytes = values->size() * sizeof(float);
        if (bytes > _ramBudget) return;
        while (_ramBytes + bytes > _ramBudget && !_lru.empty()) {
            const auto old = _entries.value(_lru.back());
            _ramBytes -= old->ram->size() * sizeof(float);
            old->ram.reset(); _lru.pop_back();
        }
        _lru.push_front(key); entry.position = _lru.begin();
        entry.ram = std::move(values); _ramBytes += bytes;
    }
    static constexpr qint64 chunkSize = 1024 * 1024;
    QTemporaryFile _file{EvoViewerTemp::fileTemplate(QStringLiteral("expression-vectors"))};
    mutable std::mutex _mutex;
    QHash<QString, std::shared_ptr<Entry>> _entries;
    std::list<QString> _lru;
    std::size_t _ramBudget = 0, _ramBytes = 0;
    std::size_t _diskBudget = 0;
    qint64 _nextOffset = 0;
};
