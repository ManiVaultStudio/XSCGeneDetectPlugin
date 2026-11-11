#pragma once
#include "EvoViewerTemp.h"
#include "XSCDebug.h"
#include "RowValueCodes.h"
#include <QTemporaryFile>
#include <QByteArray>
#include <QString>
#include <QDebug>
#include <zstd.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>
#include <memory>

// Lossless, bounded interaction layout, populated only from tiles that are
// already decoded for real work. One file, independently published gene blocks.
// A click decompresses 64 columns instead of thousands of unrelated columns.
class ColumnTileCache {
    struct Block { qint64 offset = -1; std::uint32_t size = 0; std::uint8_t encoding = 0; }; // zstd / all-zero / row codes
public:
    struct Reader {
        std::vector<std::size_t> selectedRows;
        std::shared_ptr<ColumnTileCache> owner;
        QFile file; // closes before owner, so Windows can remove the cache file
        QByteArray ahead;
        qint64 aheadOffset = -1;
        std::size_t dictionaryTile = static_cast<std::size_t>(-1);
        QByteArray dictionary;
    };
    static constexpr std::size_t stripeWidth = 64;
    ColumnTileCache(std::size_t tiles, std::size_t tileColumns, std::size_t elementSize, std::uint64_t diskBudget)
        : _stripes((tileColumns + stripeWidth - 1) / stripeWidth), _elementSize(elementSize), _diskBudget(diskBudget) {
        if (!_stripes || !elementSize || tiles > (128ULL << 20) / sizeof(Block) / _stripes) return;
        if (!_file.open()) return;
        _blocks.resize(tiles * _stripes);
        _tiles.resize(tiles, 0);
        _dictionaries.resize(tiles);
        _dictionarySizes.resize(tiles, 0);
        _enabled = true;
    }
    bool enabled() const { const std::lock_guard<std::mutex> lock(_mutex); return _enabled; }
    QString fileName() const { return _file.fileName(); }
    std::uint64_t bytes() const { const std::lock_guard<std::mutex> lock(_mutex); return _bytes; }
    QString summary() const {
        const std::lock_guard<std::mutex> lock(_mutex);
        return QStringLiteral("%1/%2 tiles prepared; %3 use lossless row dictionaries; %4 MB written; maximum %5 MB%6")
            .arg(_preparedTiles).arg(_tiles.size()).arg(_codedTiles).arg(_bytes / 1000000).arg(_diskBudget / 1000000)
            .arg(_full ? QStringLiteral(" (limit reached; missing blocks use source)") : QString());
    }
    void offer(std::size_t tile, std::size_t rows, std::size_t columns, const char* data) {
        try { offerImpl(tile, rows, columns, data); }
        catch (const std::exception& error) {
            disable(); qWarning() << "[Gene cache] Optional gene-block packing disabled:" << error.what();
        }
    }
private:
    void offerImpl(std::size_t tile, std::size_t rows, std::size_t columns, const char* data) {
        {
            const std::lock_guard<std::mutex> lock(_mutex);
            if (!_enabled || _full || tile >= _tiles.size() || _tiles[tile]) return;
            _tiles[tile] = 1; // one producer; other completed tiles remain readable
        }
        struct Context { ZSTD_CCtx* value = ZSTD_createCCtx(); ~Context() { ZSTD_freeCCtx(value); } };
        thread_local Context context;
        if (!context.value) { disable(); return; }
        QByteArray packed;
        std::vector<Block> blocks;
        blocks.reserve(_stripes);
        Block dictionaryBlock;
        std::uint32_t dictionarySize = 0;
        bool dictionaryUsed = false;
        {
        XSC_TRACE("gene.block-cache.pack");
        auto coded = RowValueCodes::encode(data, rows, columns, _elementSize);
        const bool useCodes = coded.valid && (coded.allZero || coded.codes.size() + coded.dictionary.size() < rows * columns * _elementSize * 3 / 4);
        dictionaryUsed = useCodes;
        const char* packingData = useCodes ? coded.codes.constData() : data;
        const auto packingSize = useCodes ? 1 : _elementSize;
        packed.reserve(static_cast<qsizetype>(rows * columns * packingSize + coded.dictionary.size() + _stripes * 1024));
        if (useCodes && !coded.allZero) {
            QByteArray encoded(ZSTD_compressBound(coded.dictionary.size()), Qt::Uninitialized);
            const auto size = ZSTD_compressCCtx(context.value, encoded.data(), encoded.size(), coded.dictionary.constData(), coded.dictionary.size(), -3);
            if (ZSTD_isError(size)) { disable(); return; }
            dictionaryBlock = {0, static_cast<std::uint32_t>(size), 0};
            dictionarySize = static_cast<std::uint32_t>(coded.dictionary.size());
            packed.append(encoded.constData(), size);
        }
        QByteArray layout(static_cast<qsizetype>(rows * columns * packingSize), Qt::Uninitialized);
        char* layoutBytes = layout.data();
        // A short row band keeps source pages hot while filling every stripe.
        // The old stripe-first loop revisited a 16 MB tile with a wide stride
        // for every block, thrashing translation/data caches during Update.
        for (std::size_t band = 0; band < rows; band += 16)
            for (std::size_t first = 0; first < columns; first += stripeWidth) {
                const auto width = std::min(stripeWidth, columns - first);
                const auto end = std::min(rows, band + 16);
                if (useCodes) {
                    for (std::size_t column = 0; column < width; ++column)
                        for (std::size_t row = band; row < end; ++row)
                            layoutBytes[(first + column) * rows + row] = packingData[row * columns + first + column];
                } else {
                    for (std::size_t row = band; row < end; ++row)
                        std::memcpy(layoutBytes + (first * rows + row * width) * packingSize,
                            packingData + (row * columns + first) * packingSize, width * packingSize);
                }
            }
        QByteArray encoded(ZSTD_compressBound(rows * std::min(stripeWidth, columns) * packingSize + 1), Qt::Uninitialized);
        for (std::size_t first = 0; first < columns; first += stripeWidth) {
            const auto width = std::min(stripeWidth, columns - first);
            const auto sizeBytes = rows * width * packingSize;
            const auto source = layout.constData() + first * rows * packingSize;
            std::size_t encodedSize = 0;
            std::uint8_t encoding = 0;
            const auto compact = useCodes && !coded.allZero
                ? RowValueCodes::pack(source, sizeBytes, !coded.hasRawRows) : QByteArray();
            const bool zero = useCodes ? compact.isEmpty() :
                std::all_of(source, source + sizeBytes, [](char value) { return value == 0; });
            if (zero && !coded.hasRawRows) encoding = 2;
            else {
                const auto input = useCodes ? compact.constData() : source;
                const auto inputSize = useCodes ? static_cast<std::size_t>(compact.size()) : sizeBytes;
                encoding = useCodes ? 6 : 0;
                const auto size = ZSTD_compressCCtx(context.value, encoded.data(), encoded.size(), input, inputSize, useCodes ? 1 : -3);
                if (ZSTD_isError(size)) { disable(); return; }
                encodedSize = size;
            }
            blocks.push_back({packed.size(), static_cast<std::uint32_t>(encodedSize), encoding});
            if (encodedSize) packed.append(encoded.constData(), static_cast<qsizetype>(encodedSize));
        }
        }
        // One append per physical tile instead of hundreds of thousands of
        // seek/flush/write cycles while Update prepares the interaction layout.
        std::unique_lock<std::mutex> lock(_mutex, std::defer_lock);
        { XSC_TRACE("gene.block-cache.writer-wait"); lock.lock(); }
        XSC_TRACE("gene.block-cache.write");
        if (!_enabled || _full) return;
        if (static_cast<std::uint64_t>(packed.size()) > _diskBudget - _bytes) {
            _full = true;
            qWarning() << "[Gene cache] Disk budget reached at" << _bytes << "bytes; unprepared blocks will use the source reader.";
            return;
        }
        if ((_file.pos() != static_cast<qint64>(_bytes) && !_file.seek(static_cast<qint64>(_bytes))) || _file.write(packed) != packed.size() || !_file.flush()) {
            _enabled = false; return;
        }
        for (std::size_t stripe = 0; stripe < blocks.size(); ++stripe) {
            blocks[stripe].offset += static_cast<qint64>(_bytes);
            _blocks.at(tile * _stripes + stripe) = blocks[stripe];
        }
        if (dictionaryBlock.offset >= 0) dictionaryBlock.offset += static_cast<qint64>(_bytes);
        _dictionaries[tile] = dictionaryBlock; _dictionarySizes[tile] = dictionarySize;
        _bytes += packed.size();
        ++_preparedTiles; _codedTiles += dictionaryUsed;
        XSC_TIMING_VALUE("gene.block-cache.raw-bytes", rows * columns * _elementSize);
        XSC_TIMING_VALUE("gene.block-cache.encoded-bytes", packed.size());
    }
public:
    bool prefetch(std::size_t tile, std::size_t columns, Reader& reader, std::size_t maxBytes = 2 * 1024 * 1024) {
        reader.ahead.clear();
        reader.aheadOffset = -1;
        qint64 first, length;
        {
            const std::lock_guard<std::mutex> lock(_mutex);
            if (!_enabled || tile >= _tiles.size() || !columns) return false;
            const auto count = (columns + stripeWidth - 1) / stripeWidth;
            if (count > _stripes) return false;
            const auto& begin = _blocks[tile * _stripes];
            const auto& end = _blocks[tile * _stripes + count - 1];
            if (begin.offset < 0 || end.offset < 0) return false;
            first = begin.offset; length = end.offset + end.size - first;
            if (_dictionaries[tile].offset >= 0) { length += first - _dictionaries[tile].offset; first = _dictionaries[tile].offset; }
        }
        if (length <= 0 || static_cast<std::uint64_t>(length) > maxBytes) return false;
        if (!reader.file.isOpen()) { reader.file.setFileName(fileName()); if (!reader.file.open(QIODevice::ReadOnly)) return false; }
        if (!reader.file.seek(first)) return false;
        reader.ahead = reader.file.read(length); reader.aheadOffset = first;
        return reader.ahead.size() == length;
    }
    QByteArray stripe(std::size_t tile, std::size_t rows, std::size_t columns, std::size_t column, Reader& reader, bool singleColumn = false) {
        Block block;
        {
            const std::lock_guard<std::mutex> lock(_mutex);
            if (!_enabled || tile >= _tiles.size() || column >= columns) return {};
            block = _blocks.at(tile * _stripes + column / stripeWidth);
        }
        if (block.offset < 0) return {};
        const auto first = column / stripeWidth * stripeWidth;
        const auto width = std::min(stripeWidth, columns - first);
        const auto decode = [&](const char* data) {
            const auto dictionary = block.encoding >= 4 ? loadDictionary(tile, &reader) : QByteArray();
            if (!singleColumn)
                return decodeBlock(data, block.size, rows * width * _elementSize, block.encoding, rows, dictionary, first, reader.selectedRows);
            QByteArray result(static_cast<qsizetype>(rows * _elementSize), Qt::Uninitialized);
            if (block.encoding == 6) {
                const auto size = ZSTD_getFrameContentSize(data, block.size);
                if (size > rows * width + 1) return QByteArray();
                QByteArray packed(static_cast<qsizetype>(size), Qt::Uninitialized);
                if (!decodeInto(data, block.size, size, packed.data())) return QByteArray();
                const auto codes = RowValueCodes::unpack(packed, rows * width);
                if (codes.isEmpty() || !RowValueCodes::decode(
                    codes.mid((column - first) * rows, rows), dictionary, rows, 1, result.data(), true, column))
                    return QByteArray();
                return result;
            }
            const auto stripe = decodeBlock(data, block.size, rows * width * _elementSize, block.encoding, rows, dictionary, first);
            if (stripe.isEmpty()) return QByteArray();
            for (std::size_t row = 0; row < rows; ++row)
                std::memcpy(result.data() + row * _elementSize,
                    stripe.constData() + (row * width + column - first) * _elementSize, _elementSize);
            return result;
        };
        const auto relative = block.offset - reader.aheadOffset;
        if (reader.aheadOffset >= 0 && relative >= 0 && relative + block.size <= reader.ahead.size()) {
            return decode(reader.ahead.constData() + relative);
        }
        if (!reader.file.isOpen()) { reader.file.setFileName(fileName()); if (!reader.file.open(QIODevice::ReadOnly)) return {}; }
        if (!reader.file.seek(block.offset)) return {};
        const auto encoded = reader.file.read(block.size);
        if (encoded.size() != block.size) return {};
        return decode(encoded.constData());
    }
    QByteArray stripe(std::size_t tile, std::size_t rows, std::size_t columns, std::size_t column, QFile* reader = nullptr) {
        if (column >= columns) return {};
        QByteArray encoded;
        Block block;
        {
            const std::lock_guard<std::mutex> lock(_mutex);
            if (!_enabled || tile >= _tiles.size()) return {};
            block = _blocks.at(tile * _stripes + column / stripeWidth);
            if (block.offset < 0) return {};
            if (!reader) {
                if (!_file.seek(block.offset)) return {};
                encoded = _file.read(block.size);
                if (encoded.size() != block.size) return {};
            }
        }
        if (reader) {
            if (!reader->isOpen()) { reader->setFileName(_file.fileName()); if (!reader->open(QIODevice::ReadOnly)) return {}; }
            if (!reader->seek(block.offset)) return {};
            encoded = reader->read(block.size);
            if (encoded.size() != block.size) return {};
        }
        const auto first = column / stripeWidth * stripeWidth;
        const auto width = std::min(stripeWidth, columns - first);
        const auto dictionary = block.encoding >= 4 ? loadDictionary(tile) : QByteArray();
        auto decoded = decodeBlock(encoded.constData(), encoded.size(), rows * width * _elementSize, block.encoding, rows, dictionary, first);
        if (decoded.isEmpty()) { disable(); return {}; }
        return decoded;
    }
    QByteArray tile(std::size_t tile, std::size_t rows, std::size_t columns, QFile* reader = nullptr) {
        std::vector<Block> blocks;
        QByteArray encoded;
        qint64 first = 0, length = 0;
        {
            const std::lock_guard<std::mutex> lock(_mutex);
            if (!_enabled || tile >= _tiles.size() || !columns) return {};
            const auto count = (columns + stripeWidth - 1) / stripeWidth;
            if (count > _stripes) return {};
            blocks.assign(_blocks.begin() + tile * _stripes, _blocks.begin() + tile * _stripes + count);
            for (const auto& block : blocks) if (block.offset < 0) return {};
            first = blocks.front().offset;
            length = blocks.back().offset + blocks.back().size - first;
            if (!reader) { if (!_file.seek(first)) return {}; encoded = _file.read(length); }
        }
        if (reader) {
            if (!reader->isOpen()) { reader->setFileName(_file.fileName()); if (!reader->open(QIODevice::ReadOnly)) return {}; }
            if (!reader->seek(first)) return {};
            encoded = reader->read(length);
        }
        if (encoded.size() != length) return {};
        QByteArray layout(static_cast<qsizetype>(rows * columns * _elementSize), Qt::Uninitialized);
        const auto dictionary = loadDictionary(tile);
        for (std::size_t stripe = 0; stripe < blocks.size(); ++stripe) {
            const auto column = stripe * stripeWidth, width = std::min(stripeWidth, columns - column);
            const auto& block = blocks[stripe];
            const auto offset = block.offset - first;
            if (offset < 0 || offset + block.size > encoded.size()) { disable(); return {}; }
            if (!decodeBlockInto(encoded.constData() + offset, block.size, rows * width * _elementSize,
                layout.data() + column * rows * _elementSize, block.encoding, rows, dictionary, column)) { disable(); return {}; }
        }
        QByteArray result(static_cast<qsizetype>(rows * columns * _elementSize), Qt::Uninitialized);
        for (std::size_t band = 0; band < rows; band += 16)
            for (std::size_t column = 0; column < columns; column += stripeWidth) {
                const auto width = std::min(stripeWidth, columns - column);
                for (std::size_t row = band; row < std::min(rows, band + 16); ++row)
                    std::memcpy(result.data() + (row * columns + column) * _elementSize,
                        layout.constData() + (column * rows + row * width) * _elementSize, width * _elementSize);
            }
        return result;
    }
    QByteArray column(std::size_t tile, std::size_t rows, std::size_t columns, std::size_t column, QFile* reader = nullptr) {
        auto decoded = stripe(tile, rows, columns, column, reader);
        if (decoded.isEmpty()) return {};
        const auto first = column / stripeWidth * stripeWidth;
        const auto width = std::min(stripeWidth, columns - first);
        QByteArray result(static_cast<qsizetype>(rows * _elementSize), Qt::Uninitialized);
        for (std::size_t row = 0; row < rows; ++row)
            std::memcpy(result.data() + row * _elementSize,
                decoded.constData() + (row * width + column - first) * _elementSize, _elementSize);
        return result;
    }
    bool full() const { const std::lock_guard<std::mutex> lock(_mutex); return _full; }
    bool complete() const {
        const std::lock_guard<std::mutex> lock(_mutex);
        return _enabled && _preparedTiles == _tiles.size();
    }
private:
    QByteArray loadDictionary(std::size_t tile, Reader* reader = nullptr) {
        if (reader && reader->dictionaryTile == tile) return reader->dictionary;
        Block block; std::uint32_t expected = 0; QByteArray encoded;
        {
            const std::lock_guard<std::mutex> lock(_mutex);
            if (tile >= _dictionaries.size()) return {};
            block = _dictionaries[tile]; expected = _dictionarySizes[tile];
            if (block.offset < 0 || !expected) return {};
            if (!reader) { if (!_file.seek(block.offset)) return {}; encoded = _file.read(block.size); }
        }
        if (reader) {
            const auto offset = block.offset - reader->aheadOffset;
            if (reader->aheadOffset >= 0 && offset >= 0 && offset + block.size <= reader->ahead.size())
                encoded = QByteArray::fromRawData(reader->ahead.constData() + offset, block.size);
            else {
                if (!reader->file.isOpen()) { reader->file.setFileName(fileName()); if (!reader->file.open(QIODevice::ReadOnly)) return {}; }
                if (!reader->file.seek(block.offset)) return {};
                encoded = reader->file.read(block.size);
            }
        }
        if (encoded.size() != block.size) return {};
        QByteArray dictionary(expected, Qt::Uninitialized);
        if (!decodeInto(encoded.constData(), encoded.size(), expected, dictionary.data())) return {};
        if (reader) { reader->dictionary = dictionary; reader->dictionaryTile = tile; }
        return dictionary;
    }
    template<class T>
    static bool decodeSparseTyped(const char* source, std::size_t bytes, std::size_t expected, char* output) {
        const auto count = expected / sizeof(T), maskBytes = (count + 7) / 8;
        if (bytes < maskBytes) return false;
        std::memset(output, 0, expected);
        std::size_t position = maskBytes;
        for (std::size_t group = 0; group < maskBytes; ++group) {
            const auto bits = static_cast<unsigned char>(source[group]);
            if (!bits) continue;
            for (std::size_t bit = 0; bit < 8; ++bit) if (bits & (1U << bit)) {
                const auto index = group * 8 + bit;
                if (index >= count || position + sizeof(T) > bytes) return false;
                std::memcpy(output + index * sizeof(T), source + position, sizeof(T)); position += sizeof(T);
            }
        }
        return position == bytes;
    }
    bool decodeBlockInto(const char* source, std::size_t bytes, std::size_t expected, char* output, std::uint8_t encoding,
        std::size_t rows, const QByteArray& dictionary, std::size_t columnBase, const std::vector<std::size_t>& selectedRows = {}) const {
        if (encoding == 0) return decodeInto(source, bytes, expected, output);
        if (encoding == 2) { std::memset(output, 0, expected); return bytes == 0; }
        if (encoding == 4 || encoding == 5 || encoding == 6) {
            if (_elementSize != 4 || !rows || expected % (rows * 4)) return false;
            const auto raw = encoding >= 5 ? ZSTD_getFrameContentSize(source, bytes) : expected / 4;
            if (raw == ZSTD_CONTENTSIZE_ERROR || raw == ZSTD_CONTENTSIZE_UNKNOWN || raw > expected / 4 * 2 + 1) return false;
            QByteArray codes(raw, Qt::Uninitialized);
            if (!decodeInto(source, bytes, codes.size(), codes.data())) return false;
            if (encoding >= 5) codes = RowValueCodes::unpack(codes, expected / 4);
            return RowValueCodes::decode(codes, dictionary, rows, expected / (rows * 4), output, encoding == 6, columnBase, selectedRows);
        }
        if (encoding != 1) return false;
        switch (_elementSize) {
        case 1: return decodeSparseTyped<std::uint8_t>(source, bytes, expected, output);
        case 2: return decodeSparseTyped<std::uint16_t>(source, bytes, expected, output);
        case 4: return decodeSparseTyped<std::uint32_t>(source, bytes, expected, output);
        case 8: return decodeSparseTyped<std::uint64_t>(source, bytes, expected, output);
        default: return false;
        }
    }
    QByteArray decodeBlock(const char* source, std::size_t bytes, std::size_t expected, std::uint8_t encoding,
        std::size_t rows, const QByteArray& dictionary, std::size_t columnBase, const std::vector<std::size_t>& selectedRows = {}) const {
        QByteArray result(static_cast<qsizetype>(expected), Qt::Uninitialized);
        return decodeBlockInto(source, bytes, expected, result.data(), encoding, rows, dictionary, columnBase, selectedRows) ? result : QByteArray();
    }
    static bool decodeInto(const char* data, std::size_t size, std::size_t expected, char* output) {
        struct Context { ZSTD_DCtx* value = ZSTD_createDCtx(); ~Context() { ZSTD_freeDCtx(value); } };
        thread_local Context context;
        if (!context.value) return false;
        const auto decoded = ZSTD_decompressDCtx(context.value, output, expected, data, size);
        return !ZSTD_isError(decoded) && decoded == expected;
    }
    void disable() { const std::lock_guard<std::mutex> lock(_mutex); _enabled = false; }
    const std::size_t _stripes, _elementSize;
    const std::uint64_t _diskBudget;
    mutable std::mutex _mutex;
    QTemporaryFile _file{EvoViewerTemp::fileTemplate(QStringLiteral("gene-blocks"))};
    std::vector<Block> _blocks;
    std::vector<Block> _dictionaries;
    std::vector<std::uint32_t> _dictionarySizes;
    std::vector<unsigned char> _tiles;
    std::uint64_t _bytes = 0;
    std::size_t _preparedTiles = 0, _codedTiles = 0;
    bool _enabled = false, _full = false;
};
