#pragma once
#include "StoredZipRanges.h"
#include "ColumnTileCache.h"
#include <QStorageInfo>
#include <QDir>
#include <PointData/PointData.h>
#include <PointData/storage/tiled/PointDataTiledStorage.h>
#include <Project.h>
#include <CoreInterface.h>
#include <util/CodecRegistry.h>
#include <util/BlobCodec.h>
#include <condition_variable>
#include <list>
#include <cstring>
#include <atomic>

// Read-only plugin adapter: use public descriptor/codec APIs, never private core
// cache methods or a modified PointData DLL. The returned lease owns its bytes.
class PackedTileAccess {
    using Storage = mv::pointdata::tiled::PointDataTiledStorage;
    struct Entry {
        enum class State { Loading, Ready, Failed };
        std::shared_ptr<Storage> owner;
        mv::pointdata::tiled::TileDescriptor descriptor;
        QByteArray bytes;
        State state = State::Loading;
        std::exception_ptr error;
        std::condition_variable changed;
    };
    struct Cache {
        std::mutex mutex;
        QHash<QString, std::shared_ptr<Entry>> entries;
        std::list<QString> order;
        std::size_t bytes = 0;
        std::shared_ptr<Storage> columnOwner;
        std::shared_ptr<ColumnTileCache> columns;
    };
    static Cache& sharedCache() { static Cache cache; return cache; }
public:
    static void configureColumns(const std::shared_ptr<Storage>& storage, std::size_t memoryBudget) {
        if (!storage || !storage->hasPackedTileSource()) return;
        auto& cache = sharedCache();
        const std::lock_guard<std::mutex> lock(cache.mutex);
        if (cache.columnOwner == storage && cache.columns) return;
        const auto available = QStorageInfo(EvoViewerTemp::directory().absolutePath()).bytesAvailable();
        const auto raw = storage->rows() * storage->columns() * storage->elementSize();
        const auto disk = std::min<std::uint64_t>({raw, 6'000'000'000ULL, static_cast<std::uint64_t>(memoryBudget) * 32,
            available > 0 ? static_cast<std::uint64_t>(available) / 3 : 0});
        if (!disk) return;
        cache.columns = std::make_shared<ColumnTileCache>(storage->descriptors().size(), storage->columnTileSize(), storage->elementSize(), disk);
        cache.columnOwner = storage;
    }
    static void clearColumns() {
        auto& cache = sharedCache(); const std::lock_guard<std::mutex> lock(cache.mutex);
        cache.columns.reset(); cache.columnOwner.reset();
    }
    static std::shared_ptr<ColumnTileCache> columnsFor(const std::shared_ptr<Storage>& storage) {
        auto& cache = sharedCache(); const std::lock_guard<std::mutex> lock(cache.mutex);
        return cache.columnOwner == storage ? cache.columns : nullptr;
    }
    static std::atomic<std::size_t>& budget() { static std::atomic<std::size_t> bytes{64ULL << 20}; return bytes; }
    static void setBudget(std::size_t bytes) { budget().store(bytes); }
    class Lease {
    public:
        explicit Lease(std::shared_ptr<Entry> entry) : _entry(std::move(entry)) {}
        explicit operator bool() const { return _entry && !_entry->bytes.isEmpty(); }
        const void* data() const { return _entry->bytes.constData(); }
        std::uint64_t rowOffset() const { return _entry->descriptor.rowOffset; }
        std::uint64_t columnOffset() const { return _entry->descriptor.columnOffset; }
        std::uint64_t rows() const { return _entry->descriptor.rows; }
        std::uint64_t columns() const { return _entry->descriptor.columns; }
        std::size_t elementSize() const { return _entry->owner->elementSize(); }
    private:
        std::shared_ptr<Entry> _entry;
    };
    static bool supportsSubset(const std::shared_ptr<Storage>& storage) {
        if (storage->hasPackedTileSource()) return true;
        for (const auto& descriptor : storage->descriptors())
            if (descriptor.residentBytes.isEmpty() && descriptor.blob.isEmpty()) return false;
        return true;
    }
    static QFile* readerFor(const std::shared_ptr<ColumnTileCache>& cache, ColumnTileCache::Reader* reader) {
        if (!reader) return nullptr;
        if (reader->owner != cache) { reader->file.close(); reader->ahead.clear(); reader->aheadOffset = -1; reader->dictionary.clear(); reader->dictionaryTile = static_cast<std::size_t>(-1); reader->owner = cache; reader->file.setFileName(cache->fileName()); }
        return &reader->file;
    }
    static Lease readStripe(Points* points, const std::shared_ptr<Storage>& storage, std::uint64_t tileRow, std::uint64_t column,
        ColumnTileCache::Reader* reader = nullptr, bool singleColumn = false) {
        const auto tileColumns = (storage->columns() + storage->columnTileSize() - 1) / storage->columnTileSize();
        const auto index = static_cast<std::size_t>(tileRow * tileColumns + column / storage->columnTileSize());
        const auto& descriptor = storage->descriptors().at(index);
        if (const auto cache = columnsFor(storage)) {
            XSC_HOT_TRACE("gene.block-cache.read");
            const auto local = column - descriptor.columnOffset;
            readerFor(cache, reader);
            auto bytes = reader ? cache->stripe(index, descriptor.rows, descriptor.columns, local, *reader, singleColumn)
                : cache->stripe(index, descriptor.rows, descriptor.columns, local);
            if (!bytes.isEmpty()) {
                auto entry = std::make_shared<Entry>(); entry->owner = storage; entry->descriptor = descriptor;
                const auto first = singleColumn && reader ? local : local / ColumnTileCache::stripeWidth * ColumnTileCache::stripeWidth;
                entry->descriptor.columnOffset = descriptor.columnOffset + first;
                entry->descriptor.columns = singleColumn && reader ? 1 :
                    std::min<std::uint64_t>(ColumnTileCache::stripeWidth, descriptor.columns - first);
                entry->bytes = std::move(bytes); entry->state = Entry::State::Ready;
                XSC_HOT_TRACE("gene.block-cache.hit");
                return Lease(entry);
            }
        }
        XSC_TIMING_VALUE("gene.block-cache.miss", 1);
        return read(points, storage, tileRow, column / storage->columnTileSize());
    }
    static Lease readColumn(Points* points, const std::shared_ptr<Storage>& storage, std::uint64_t tileRow, std::uint64_t column,
        ColumnTileCache::Reader* reader = nullptr) {
        return readStripe(points, storage, tileRow, column, reader, true);
    }
    static Lease read(Points* points, const std::shared_ptr<Storage>& storage, std::uint64_t tileRow, std::uint64_t tileColumn) {
        XSC_TRACE("packed.tile.acquire");
        const auto tileColumns = (storage->columns() + storage->columnTileSize() - 1) / storage->columnTileSize();
        const auto index = static_cast<std::size_t>(tileRow * tileColumns + tileColumn);
        const auto& descriptor = storage->descriptors().at(index);
        const QString key = QString::number(reinterpret_cast<quintptr>(storage.get()), 16) + QChar(':') + QString::number(index);
        auto& cache = sharedCache();
        std::shared_ptr<Entry> entry;
        {
            std::unique_lock<std::mutex> lock(cache.mutex);
            entry = cache.entries.value(key);
            if (entry) {
                entry->changed.wait(lock, [&] { return entry->state != Entry::State::Loading; });
                if (entry->error) std::rethrow_exception(entry->error);
                lock.unlock();
                if (const auto columns = columnsFor(storage))
                    columns->offer(index, descriptor.rows, descriptor.columns, entry->bytes.constData());
                return Lease(entry);
            }
            entry = std::make_shared<Entry>(); entry->owner = storage; entry->descriptor = descriptor;
            cache.entries.insert(key, entry);
        }
        try {
            const auto expected = descriptor.rows * descriptor.columns * storage->elementSize();
            if (!descriptor.residentBytes.isEmpty()) entry->bytes = descriptor.residentBytes;
            else if (storage->hasPackedTileSource() && descriptor.packedBlock.uncompressedSize) {
                const auto source = storage->packedTileSourcePaths().at(static_cast<int>(descriptor.superTileIndex));
                thread_local QHash<QString, mv::util::SharedCodec> codecs;
                auto& codec = codecs[storage->packedTileSourceCodec()];
                if (!codec) codec = mv::util::codecRegistry().createCodec(nullptr, storage->packedTileSourceCodec());
                std::optional<QByteArray> encoded;
                if (QFileInfo(source).isAbsolute() && QFileInfo(source).isFile()) {
                    QFile file(source);
                    if (!file.open(QIODevice::ReadOnly) || !file.seek(static_cast<qint64>(descriptor.packedBlock.offset)))
                        throw std::runtime_error("Cannot read packed tile file");
                    encoded = file.read(static_cast<qint64>(descriptor.packedBlock.compressedSize));
                }
                else if (const auto project = mv::projects().getCurrentProject())
                    encoded = StoredZipRanges::read(project->getFilePath(), source,
                        descriptor.packedBlock.offset, descriptor.packedBlock.compressedSize);
                if (encoded) {
                    if (static_cast<std::uint64_t>(encoded->size()) != descriptor.packedBlock.compressedSize)
                        throw std::runtime_error("Incomplete packed tile frame");
                    XSC_TRACE("packed.tile.decode");
                    // decodeTo owns/releases its decompression context. Avoid
                    // the legacy allocating decode() path on every tile.
                    entry->bytes.resize(static_cast<qsizetype>(expected));
                    codec->decodeTo(*encoded, entry->bytes.data(), expected);
                }
                else {
                    XSC_TRACE("packed.tile.legacy-archive-fallback");
                    mv::util::PackedBlobFileReader reader(source, codec);
                    entry->bytes = reader.read(descriptor.packedBlock);
                }
            }
            else if (!descriptor.blob.isEmpty()) entry->bytes = mv::util::bytesFromBlobVariantMap(descriptor.blob);
            else {
                // Public range API for generated full datasets with no packed source.
                entry->bytes.resize(static_cast<qsizetype>(expected));
                std::size_t first = 0, offset = 0;
                points->visitRange(descriptor.rowOffset, descriptor.rows, descriptor.columnOffset, descriptor.columns,
                    [&](const PointDataAccessView& view) {
                        for (std::size_t row = 0; row < view.rows; ++row)
                            std::memcpy(entry->bytes.data() + ((first + row) * descriptor.columns + offset) * storage->elementSize(),
                                view.rowData(row), view.columns * storage->elementSize());
                        offset += view.columns;
                        if (offset == descriptor.columns) { first += view.rows; offset = 0; }
                    });
                if (first != descriptor.rows || offset) throw std::runtime_error("Incomplete generated tile");
            }
            if (static_cast<std::uint64_t>(entry->bytes.size()) != expected) throw std::runtime_error("Decoded tile size mismatch");
            if (const auto columns = columnsFor(storage)) {
                XSC_TRACE("gene.block-cache.build");
                columns->offer(index, descriptor.rows, descriptor.columns, entry->bytes.constData());
            }
            {
                const std::lock_guard<std::mutex> lock(cache.mutex);
                entry->state = Entry::State::Ready;
                cache.bytes += entry->bytes.size(); cache.order.push_back(key);
                while (cache.bytes > budget().load() && cache.order.size() > 1) {
                    const auto old = cache.entries.take(cache.order.front()); cache.order.pop_front();
                    if (old) cache.bytes -= old->bytes.size();
                }
            }
            entry->changed.notify_all();
            return Lease(entry);
        }
        catch (...) {
            { const std::lock_guard<std::mutex> lock(cache.mutex); entry->error = std::current_exception(); entry->state = Entry::State::Failed; cache.entries.remove(key); }
            entry->changed.notify_all(); throw;
        }
    }
};
