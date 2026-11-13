#pragma once

// ============================================================================
//  Streaming access to the main (cell x gene) point dataset.
//
//  Semantics (point-data implementation and PointsViewWidget::displayQuery):
//   - valueAt() indexes a view relative to the view.
//   - a request can arrive as several views per row batch (column segments, e.g. tiles).
//     The segments of one row batch arrive in column order; the column offset of a segment
//     is the sum of the widths of the segments before it in that batch.
//   - a view does not carry its absolute column position, so the offset is tracked here.
//
//  Access paths for a block of rows (rows are sorted, duplicates allowed):
//   - span:     one Range2D read over the block's row span, unselected rows are skipped
//   - runs:     one Range2D read per contiguous run of rows
//   - row list: one RowList read (sparse blocks)
//  Column requests that cover at least a quarter of their range are read as full rows;
//  sparse column requests read only the requested column runs (ColumnsForRows).
//
//  Parallelism: blocks are assigned to workers statically (worker w takes blocks w, w+W, ...),
//  each worker owns one reduction state, states are merged in worker order. The result is
//  deterministic for a fixed number of workers.
// ============================================================================

#include "XSCDebug.h"
#include "PointReadRanges.h"
#include "PackedTileAccess.h"
#include "ExpressionScanResources.h"

#include <PointData/PointData.h>
#include <PointData/storage/tiled/TiledPointsStorageBackend.h>
#include <PointData/storage/tiled/PointDataTiledStorage.h>
#include <biovault_bfloat16/biovault_bfloat16.h>
#include <util/BlobCodec.h>

#include <QDebug>
#include <QString>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class Points;
using PointDataAccessVisitor = std::function<void(const PointDataAccessView&)>;

/** Value at (row, column) of a view delivered by the point-data API. Row and column are relative to the view. */
inline float valueAt(const PointDataAccessView& view, const std::uint64_t row, const std::uint64_t column)
{
    const auto* bytes = view.rowData(row) + column * view.elementSizeBytes;

    switch (view.elementTypeIndex)
    {
        case 0: return *reinterpret_cast<const float*>(bytes);
        case 1: return static_cast<float>(*reinterpret_cast<const biovault::bfloat16_t*>(bytes));
        case 2: return static_cast<float>(*reinterpret_cast<const std::int32_t*>(bytes));
        case 3: return static_cast<float>(*reinterpret_cast<const std::uint32_t*>(bytes));
        case 4: return static_cast<float>(*reinterpret_cast<const std::int16_t*>(bytes));
        case 5: return static_cast<float>(*reinterpret_cast<const std::uint16_t*>(bytes));
        case 6: return static_cast<float>(*reinterpret_cast<const std::int8_t*>(bytes));
        case 7: return static_cast<float>(*reinterpret_cast<const std::uint8_t*>(bytes));
        default: return 0.0f;
    }
}

template <typename ValueType>
constexpr std::uint32_t elementTypeIndex()
{
    if constexpr (std::is_same_v<ValueType, float>) return 0;
    else if constexpr (std::is_same_v<ValueType, biovault::bfloat16_t>) return 1;
    else if constexpr (std::is_same_v<ValueType, std::int32_t>) return 2;
    else if constexpr (std::is_same_v<ValueType, std::uint32_t>) return 3;
    else if constexpr (std::is_same_v<ValueType, std::int16_t>) return 4;
    else if constexpr (std::is_same_v<ValueType, std::uint16_t>) return 5;
    else if constexpr (std::is_same_v<ValueType, std::int8_t>) return 6;
    else if constexpr (std::is_same_v<ValueType, std::uint8_t>) return 7;
    else return std::numeric_limits<std::uint32_t>::max();
}

enum class FetchMode {
    SingleRow,
    RowRange,
    RowList,
    SingleColumn,
    ColumnRange,
    ColumnList,
    ColumnsForRows,
    Range2D
};

inline void fetchPointData(
    Points* points,
    FetchMode mode,
    const PointDataAccessVisitor& visitor,
    std::uint64_t rowId = 0,
    std::uint64_t firstRow = 0,
    std::uint64_t rowCount = 0,
    const std::vector<std::uint64_t>& rowIds = {},
    std::uint64_t columnId = 0,
    std::uint64_t firstColumn = 0,
    std::uint64_t columnCount = 0,
    const std::vector<std::uint64_t>& columnIds = {})
{
    if (!points)
        return;

    switch (mode)
    {
        case FetchMode::SingleRow:       visitor(points->getRow(rowId)); break;
        case FetchMode::RowRange:        points->visitRows(firstRow, rowCount, visitor); break;
        case FetchMode::RowList:         points->visitRows(rowIds, visitor); break;
        case FetchMode::SingleColumn:    points->visitColumns(columnId, 1, visitor); break;
        case FetchMode::ColumnRange:     points->visitColumns(firstColumn, columnCount, visitor); break;
        case FetchMode::ColumnList:      points->visitColumns(columnIds, visitor); break;
        case FetchMode::ColumnsForRows:  points->visitColumns(firstColumn, columnCount, rowIds, visitor); break;
        case FetchMode::Range2D:         points->visitRange(firstRow, rowCount, firstColumn, columnCount, visitor); break;
    }
}

namespace PointStreamingDetail {

inline std::shared_ptr<mv::pointdata::tiled::PointDataTiledStorage> tiledStorage(const Points* points)
{
    if (!points || points->isProxy())
        return {};

    const auto* backend = points->getStorageBackend();
    const auto* tiledBackend = dynamic_cast<const mv::pointdata::storage::TiledPointsStorageBackend*>(backend);
    return tiledBackend ? tiledBackend->getStorage() : nullptr;
}

struct DenseVisitProbe {
    template <typename Iterator>
    void operator()(Iterator, Iterator) const;
};

template <typename PointType, typename = void>
struct HasDirectDenseAccess : std::false_type {};

template <typename PointType>
struct HasDirectDenseAccess<PointType, std::void_t<
    decltype(std::declval<const PointType&>().isProxy()),
    decltype(std::declval<const PointType&>().isFull()),
    decltype(std::declval<const PointType&>().getStorageBackendType()),
    decltype(std::declval<const PointType&>().constVisitFromBeginToEnd(DenseVisitProbe{})),
    typename PointType::StorageBackendType>> : std::true_type {};

template <typename View, typename = void>
struct HasRowStrideBytes : std::false_type {};

template <typename View>
struct HasRowStrideBytes<View, std::void_t<decltype(std::declval<View&>().rowStrideBytes)>> : std::true_type {};

template <typename View, typename = void>
struct HasLifetimeToken : std::false_type {};

template <typename View>
struct HasLifetimeToken<View, std::void_t<decltype(std::declval<View&>().lifetime)>> : std::true_type {};

template <typename View>
void setNonOwningRangeView(View& view, const void* data, const std::uint64_t firstRow,
                           const std::uint64_t rowCount, const std::uint64_t columns,
                           const std::uint64_t dimensions, const std::uint32_t elementType,
                           const std::uint32_t elementSize)
{
    const std::uint64_t strideBytes = dimensions * elementSize;
    view.data = reinterpret_cast<decltype(view.data)>(static_cast<const char*>(data) + firstRow * strideBytes);
    view.rows = rowCount;
    view.columns = columns;
    view.elementTypeIndex = elementType;
    view.elementSizeBytes = elementSize;
    if constexpr (HasRowStrideBytes<View>::value)
        view.rowStrideBytes = strideBytes;
    else
        view.stride = dimensions;
    if constexpr (HasLifetimeToken<View>::value)
        view.lifetime.reset();
}

template <typename PointType, typename Work, std::enable_if_t<HasDirectDenseAccess<PointType>::value, int> = 0>
bool withDensePointData(const PointType* points, Work&& work, bool allowSubset = false)
{
    if (points->isProxy() || (!allowSubset && !points->isFull()) ||
        points->getStorageBackendType() != PointType::StorageBackendType::Dense)
        return false;

    points->constVisitFromBeginToEnd([&](const auto begin, const auto end) {
        using ValueType = typename std::decay_t<decltype(*begin)>;
        work(begin == end ? nullptr : static_cast<const void*>(std::addressof(*begin)),
            elementTypeIndex<ValueType>(), static_cast<std::uint32_t>(sizeof(ValueType)), allowSubset ? points->getNumRawPoints() : points->getNumPoints());
    });
    return true;
}

template <typename PointType, typename Work, std::enable_if_t<!HasDirectDenseAccess<PointType>::value, int> = 0>
bool withDensePointData(const PointType*, Work&&, bool = false)
{
    return false;
}

} // namespace PointStreamingDetail

namespace PointStreaming {

/** Target size of one row-block read (bytes, at 4 bytes per value). */
inline constexpr std::size_t kTargetReadBytes = 64ULL * 1024 * 1024;

/** Every worker gets at least this many blocks, so the work is shared even for moderate selections. */
inline constexpr std::size_t kBlocksPerWorker = 4;

/** Smallest block, in rows. */
inline constexpr std::size_t kMinRowsPerBlock = 256;

/** A block is read as one span when the span is at most this many times the number of selected rows. */
inline constexpr std::size_t kMaxSpanFactor = 2;

/** Average run length from which contiguous runs are read with Range2D instead of a row list. */
inline constexpr std::size_t kMinRowsPerRun = 32;

/** Number of genes that can be read: the smaller of the gene names and the data width. */
inline std::size_t usableGeneCount(std::size_t namedGenes, std::uint64_t dataDimensions)
{
    return std::min<std::size_t>(namedGenes, static_cast<std::size_t>(dataDimensions));
}

/** Splits ascending values into maximal runs of consecutive ids: {first, count}. */
inline std::vector<std::pair<std::uint64_t, std::uint64_t>> contiguousRuns(const std::vector<std::uint64_t>& ascending)
{
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;

    for (std::size_t k = 0; k < ascending.size();) {
        std::size_t end = k + 1;

        while (end < ascending.size() && ascending[end] == ascending[end - 1] + 1)
            ++end;

        runs.emplace_back(ascending[k], static_cast<std::uint64_t>(end - k));
        k = end;
    }

    return runs;
}

/**
 * The requested columns that lie in one column segment (a view), with their index inside the view.
 * Recomputed only when the segment layout changes.
 */
struct SegmentColumns {
    std::uint64_t start = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t width = 0;

    std::vector<std::uint64_t> relative; /** column index inside the view */
    std::vector<std::size_t>   slot;     /** index of the column in the requested list */

    /** Cache the requested column layout so tiled segments need not rescan every requested column. */
    const std::uint64_t* cachedColumnsData = nullptr;
    std::size_t cachedColumnsSize = 0;
    bool columnsSorted = false;

    /** True when relative[k] = firstRelative + k and slot[k] = firstSlot + k (one straight run). */
    bool          contiguous    = false;
    std::uint64_t firstRelative = 0;
    std::size_t   firstSlot     = 0;

    void update(const std::vector<std::uint64_t>& columns, const std::uint64_t segmentStart, const std::uint64_t segmentWidth)
    {
        if (segmentStart == start && segmentWidth == width)
            return;

        start = segmentStart;
        width = segmentWidth;
        relative.clear();
        slot.clear();

        if (cachedColumnsData != columns.data() || cachedColumnsSize != columns.size()) {
            cachedColumnsData = columns.data();
            cachedColumnsSize = columns.size();
            columnsSorted = std::is_sorted(columns.begin(), columns.end());
        }

        if (columnsSorted) {
            const auto first = std::lower_bound(columns.begin(), columns.end(), segmentStart);
            const std::uint64_t segmentEnd = segmentStart + segmentWidth;
            for (auto column = first; column != columns.end() && *column < segmentEnd; ++column) {
                relative.push_back(*column - segmentStart);
                slot.push_back(static_cast<std::size_t>(column - columns.begin()));
            }
        }
        else {
            // Preserve support for arbitrary unsorted column lists.
            for (std::size_t k = 0; k < columns.size(); ++k) {
                if (columns[k] >= segmentStart && columns[k] < segmentStart + segmentWidth) {
                    relative.push_back(columns[k] - segmentStart);
                    slot.push_back(k);
                }
            }
        }

        contiguous = !relative.empty();

        for (std::size_t k = 1; contiguous && k < relative.size(); ++k) {
            if (relative[k] != relative[0] + k || slot[k] != slot[0] + k)
                contiguous = false;
        }

        firstRelative = relative.empty() ? 0 : relative[0];
        firstSlot     = slot.empty() ? 0 : slot[0];
    }
};

/**
 * Determines the number of workers.
 *
 * threads == 0: automatically detect hardware concurrency.
 * threads > 0: use the requested number, subject to workload limits.
 */
inline std::size_t workerCount(
    const int threads,
    const std::size_t rowCount)
{
    const unsigned int hardwareThreads =
        std::thread::hardware_concurrency();

    // hardware_concurrency() can return 0.
    const std::size_t availableThreads =
        hardwareThreads > 0 ? hardwareThreads : 1;

    const std::size_t requestedThreads =
        threads > 0
        ? static_cast<std::size_t>(threads)
        : availableThreads;

    // Avoid creating workers that have too little work.
    const std::size_t usefulThreads =
        std::max<std::size_t>(
            1, (rowCount + kMinRowsPerBlock * kBlocksPerWorker - 1) /
            (kMinRowsPerBlock * kBlocksPerWorker));

    XSC_LOG(
        "PointStreaming: hardware threads",
        hardwareThreads,
        "requested threads",
        threads,
        "rows",
        rowCount,
        "selected workers",
        std::min<std::size_t>(
            256,
            std::min(requestedThreads, usefulThreads)));

    // Keep the existing safety limit.
    return std::min<std::size_t>(
        256,
        std::min(requestedThreads, usefulThreads));
}

namespace detail {

template <typename T>
inline float toFloat(const T value) { return static_cast<float>(value); }

template <typename T>
inline void accumulateTypedSegment(const std::uint8_t* bytes, const std::uint32_t elementSizeBytes, const SegmentColumns& segment, double* sums, std::uint64_t* nonZero)
{
    const std::size_t count = segment.relative.size();

    if (segment.contiguous) {
        // Straight run: stride sizeof(T), no index arrays, no branch in the value loop.
        const std::uint8_t* base = bytes + segment.firstRelative * sizeof(T);
        double* s = sums + segment.firstSlot;

        if (nonZero) {
            std::uint64_t* z = nonZero + segment.firstSlot;

            for (std::size_t k = 0; k < count; ++k) {
                T raw;
                std::memcpy(&raw, base + k * sizeof(T), sizeof(T));
                const float value = toFloat(raw);
                s[k] += value;
                z[k] += (value != 0.0f) ? 1u : 0u;
            }
        }
        else {
            for (std::size_t k = 0; k < count; ++k) {
                T raw;
                std::memcpy(&raw, base + k * sizeof(T), sizeof(T));
                s[k] += toFloat(raw);
            }
        }

        return;
    }

    for (std::size_t k = 0; k < count; ++k) {
        T raw;
        std::memcpy(&raw, bytes + segment.relative[k] * elementSizeBytes, sizeof(T));
        const float value = toFloat(raw);
        const std::size_t s = segment.slot[k];
        sums[s] += value;

        if (nonZero && value != 0.0f)
            nonZero[s] += 1;
    }
}

} // namespace detail

/** Adds the requested columns of one row of a segment to \p sums (by requested slot) and counts non-zeros when given. */
inline void accumulateSegment(const PointDataAccessView& view, const std::uint64_t row, const SegmentColumns& segment, double* sums, std::uint64_t* nonZero)
{
    if (segment.relative.empty())
        return;

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(view.rowData(row));
    const auto size   = view.elementSizeBytes;

    switch (view.elementTypeIndex)
    {
        case 0: return detail::accumulateTypedSegment<float>(bytes, size, segment, sums, nonZero);
        case 1: return detail::accumulateTypedSegment<biovault::bfloat16_t>(bytes, size, segment, sums, nonZero);
        case 2: return detail::accumulateTypedSegment<std::int32_t>(bytes, size, segment, sums, nonZero);
        case 3: return detail::accumulateTypedSegment<std::uint32_t>(bytes, size, segment, sums, nonZero);
        case 4: return detail::accumulateTypedSegment<std::int16_t>(bytes, size, segment, sums, nonZero);
        case 5: return detail::accumulateTypedSegment<std::uint16_t>(bytes, size, segment, sums, nonZero);
        case 6: return detail::accumulateTypedSegment<std::int8_t>(bytes, size, segment, sums, nonZero);
        case 7: return detail::accumulateTypedSegment<std::uint8_t>(bytes, size, segment, sums, nonZero);
        default: return;
    }
}

/**
 * Runs body(block, state) for every block 0..blockCount-1. Worker w (one state per worker) handles the blocks
 * w, w + W, w + 2W, ... Worker 0 runs on the calling thread. Exceptions are rethrown after all workers finished.
 */
template <typename State, typename Body>
inline void forEachBlockParallel(std::vector<State>& states, const std::size_t blockCount, Body&& body)
{
    const std::size_t workers = states.size();
    std::vector<std::exception_ptr> errors(workers);

    auto work = [&](const std::size_t w) {
        XSC_TRACE("stream.worker");
        try {
            for (std::size_t b = w; b < blockCount; b += workers)
                body(b, states[w]);
        }
        catch (...) {
            errors[w] = std::current_exception();
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(workers > 0 ? workers - 1 : 0);

    try {
        for (std::size_t w = 1; w < workers; ++w)
            pool.emplace_back(work, w);
    }
    catch (const std::system_error&) {
        // Thread creation failed: finish the threads that exist, then report the failure.
        for (auto& thread : pool)
            thread.join();
        throw;
    }

    work(0);

    for (auto& thread : pool)
        thread.join();

    for (const auto& error : errors) {
        if (error)
            std::rethrow_exception(error);
    }
}

/**
 * Reads the rows in \p rowIds in blocks and reduces them into one state per worker.
 *
 *  - columnCount                           number of columns requested, starting at column 0
 *  - make()                                creates an empty state
 *  - visit(state, rowPosition, view, row, offset)
 *                                          called for every row of every column segment. rowPosition indexes
 *                                          rowIds, row indexes the view, offset is the absolute column position of
 *                                          the view's first column.
 *  - merge(into, from)                     combines two states
 *  - progress(completed, total)             optional serialized callback after each block;
 *                                          may run on a worker thread, so must not touch UI objects
 *
 * Throws std::out_of_range / std::runtime_error when the delivered views do not match the request.
 */
template <typename RowContainer, typename Make, typename Visit, typename Merge>
inline auto reduceRows(Points* points, const RowContainer& rowIds, int threads, std::uint64_t columnCount,
                       Make&& make, Visit&& visit, Merge&& merge,
                       const std::function<void(std::size_t, std::size_t)>& progress = {})
    -> decltype(make())
{
    using State = decltype(make());

    XSC_TRACE("PointStreaming::reduceRows");

    if (!points || rowIds.empty() || columnCount == 0)
        return make();

    const std::uint64_t dimensions = points->getNumDimensions();

    if (columnCount > dimensions)
        throw std::out_of_range("PointStreaming: requested columns exceed the number of dimensions of the dataset");

    // Most callers already pass sorted row IDs. Reuse that storage instead of
    // copying millions of 32-bit selection IDs into a second 64-bit array.
    // For unsorted input, keep the sorted working copy and the source-position
    // map needed to preserve the visitor's rowPosition contract.
    std::vector<std::size_t>   order;
    std::vector<std::uint64_t> rows;
    bool alreadySorted = true;
    bool hasPrevious = false;
    std::uint64_t previousRow = 0;
    for (const auto rawRow : rowIds) {
        using RowId = std::decay_t<decltype(rawRow)>;
        if constexpr (std::is_signed_v<RowId>) {
            if (rawRow < 0)
                throw std::out_of_range("PointStreaming: requested row ID is out of range");
        }
        const auto row = static_cast<std::uint64_t>(rawRow);
        if (hasPrevious && row < previousRow)
            alreadySorted = false;
        previousRow = row;
        hasPrevious = true;
    }

    if (!alreadySorted) {
        std::vector<std::uint64_t> requested(rowIds.begin(), rowIds.end());
        order.resize(requested.size());
        std::iota(order.begin(), order.end(), std::size_t{ 0 });
        std::sort(order.begin(), order.end(), [&requested](std::size_t a, std::size_t b) { return requested[a] < requested[b]; });

        rows.resize(order.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            rows[i] = requested[order[i]];
    }

    const std::size_t rowCount = rowIds.size();
    const auto sortedRowAt = [&](const std::size_t position) -> std::uint64_t {
        return alreadySorted ? static_cast<std::uint64_t>(rowIds[position]) : rows[position];
    };

    // Measure the actual row-block work, including tile access and the caller's
    // reduction callback. maxConcurrentJobs makes overlap visible in the app log.
    std::atomic<std::size_t> activeJobs { 0 };
    std::atomic<std::size_t> maxConcurrentJobs { 0 };
    const auto runMeasuredJob = [&](auto&& job) {
        const auto active = activeJobs.fetch_add(1, std::memory_order_relaxed) + 1;
        auto observedMaximum = maxConcurrentJobs.load(std::memory_order_relaxed);
        while (active > observedMaximum && !maxConcurrentJobs.compare_exchange_weak(observedMaximum, active, std::memory_order_relaxed))
        {
        }

        try {
            job();
        }
        catch (...) {
            activeJobs.fetch_sub(1, std::memory_order_relaxed);
            throw;
        }

        activeJobs.fetch_sub(1, std::memory_order_relaxed);
    };

    // Read tiled data in physical tile order. Reading all genes of each cell
    // cycles through column tiles and can evict them before the next cell.
    // One job owns all selected cells in a tile row; it finishes one column
    // tile for those cells before moving to the next tile. This also prevents
    // different workers from repeatedly loading the same tile row.
    const auto tiledStorage = !points->isProxy() && points->isFull() ? PointStreamingDetail::tiledStorage(points) : nullptr;
    if (tiledStorage) {
        if (tiledStorage->rowTileSize() == 0 || tiledStorage->columnTileSize() == 0)
            throw std::runtime_error("PointStreaming: invalid tile geometry");
        if (sortedRowAt(rowCount - 1) >= tiledStorage->rows())
            throw std::out_of_range("PointStreaming: requested row ID is out of range");

        std::vector<std::pair<std::size_t, std::size_t>> jobs;
        for (std::size_t first = 0; first < rowCount;) {
            const auto end = tileReadEnd(first, rowCount, tiledStorage->rowTileSize(), sortedRowAt);
            jobs.emplace_back(first, end);
            first = end;
        }
        validateTileJobs(jobs, rowCount, tiledStorage->rowTileSize(), tiledStorage->rows(), sortedRowAt);
        XSC_TIMING_VALUE("stream.physical-tile-rows", (tiledStorage->rows() + tiledStorage->rowTileSize() - 1) / tiledStorage->rowTileSize());
        XSC_TIMING_VALUE("stream.tile-row-height", tiledStorage->rowTileSize());
        XSC_TIMING_VALUE("stream.requested-workers", threads);
        const auto workers = std::min(workerCount(threads, rowCount), jobs.size());
        const auto cacheHitsBefore = tiledStorage->cacheHits();
        const auto cacheMissesBefore = tiledStorage->cacheMisses();
        const auto cacheEvictionsBefore = tiledStorage->cacheEvictions();
        const auto codecTelemetryBefore = mv::util::BlobCodec::getFileRangeTelemetry();
        std::vector<State> states;
        states.reserve(workers);
        for (std::size_t w = 0; w < workers; ++w)
            states.push_back(make());

        XSC_TIMING_VALUE("stream.workers", workers);
        XSC_TIMING_VALUE("stream.tile-row-jobs", jobs.size());
        XSC_TIMING_VALUE("stream.rows", rowCount);
        std::mutex progressMutex;
        std::size_t completed = 0;
        const auto scanStart = std::chrono::steady_clock::now();
        forEachBlockParallel(states, jobs.size(), [&](std::size_t job, State& state) {
            runMeasuredJob([&] {
                XSC_TRACE("stream.tile-row-block");
                const auto [begin, end] = jobs[job];
                const auto type = static_cast<std::uint32_t>(Points::elementTypeSpecifier(points->getElementTypeName()));
                for (std::uint64_t column = 0; column < columnCount;) {
                    const auto tile = [&]() {
                        XSC_TRACE("stream.tile.acquire");
                        return PackedTileAccess::read(points, tiledStorage, sortedRowAt(begin) / tiledStorage->rowTileSize(), column / tiledStorage->columnTileSize());
                    }();
                    if (!tile || tile.columns() == 0) throw std::runtime_error("PointStreaming: missing tile");
                    PointDataAccessView view{};
                    PointStreamingDetail::setNonOwningRangeView(view, tile.data(), 0, tile.rows(),
                        tile.columns(), tile.columns(), type, static_cast<std::uint32_t>(tile.elementSize()));
                    view.columns = std::min<std::uint64_t>(tile.columns(), columnCount - column);
                    // One lease per tile, rather than a cache lookup, mutex and shared
                    // allocation for every selected cell and column segment.
                    for (auto position = begin; position < end; ++position) {
                        const auto row = sortedRowAt(position);
                        if (row < tile.rowOffset() || row >= tile.rowOffset() + tile.rows())
                            throw std::runtime_error("PointStreaming: tile does not cover requested row");
                        visit(state, order.empty() ? position : order[position], view, row - tile.rowOffset(), column);
                    }
                    column += view.columns;
                }
            });
            if (progress) {
                const std::lock_guard<std::mutex> lock(progressMutex);
                progress(++completed, jobs.size());
            }
        });
        const auto elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - scanStart).count();
        const auto codecTelemetryAfter = mv::util::BlobCodec::getFileRangeTelemetry();
        const auto cacheHits = tiledStorage->cacheHits() - cacheHitsBefore;
        const auto cacheMisses = tiledStorage->cacheMisses() - cacheMissesBefore;
        const auto cacheEvictions = tiledStorage->cacheEvictions() - cacheEvictionsBefore;
        const auto residentCacheMiB = static_cast<double>(tiledStorage->cacheSizeInBytes()) / (1024.0 * 1024.0);
        const auto codecReadCalls = codecTelemetryAfter.calls - codecTelemetryBefore.calls;
        const auto codecEncodedBytes = codecTelemetryAfter.encodedBytes - codecTelemetryBefore.encodedBytes;
        const auto codecReadMilliseconds = (codecTelemetryAfter.readNanoseconds - codecTelemetryBefore.readNanoseconds) / 1000000;
        const auto codecDecodeMilliseconds = (codecTelemetryAfter.decodeNanoseconds - codecTelemetryBefore.decodeNanoseconds) / 1000000;
        qInfo().noquote() << QStringLiteral("[PointStreaming] tiled row scan: rows=%1 jobs=%2 workers=%3 max-concurrent=%4 wall-ms=%5 cache-hits=%6 cache-misses=%7 cache-evictions=%8 cache-resident-MiB=%9 codec=%10 packed-reads=%11 encoded-MiB=%12 read-worker-ms=%13 decode-worker-ms=%14")
            .arg(rowCount).arg(jobs.size()).arg(workers).arg(maxConcurrentJobs.load(std::memory_order_relaxed)).arg(elapsedMilliseconds)
            .arg(cacheHits).arg(cacheMisses).arg(cacheEvictions).arg(residentCacheMiB, 0, 'f', 1).arg(tiledStorage->packedTileSourceCodec()).arg(codecReadCalls)
            .arg(static_cast<double>(codecEncodedBytes) / (1024.0 * 1024.0), 0, 'f', 1)
            .arg(codecReadMilliseconds).arg(codecDecodeMilliseconds);
        for (std::size_t w = 1; w < workers; ++w)
            merge(states[0], states[w]);
        return std::move(states[0]);
    }

    // Workers and blocks. Every worker gets several blocks, so small selections still use all workers.
    const std::size_t workers = workerCount(threads, rowCount);

    const std::size_t bytesPerRow = static_cast<std::size_t>(columnCount) * sizeof(float);
    std::size_t rowsPerBlock = std::max<std::size_t>(1, kTargetReadBytes / std::max<std::size_t>(1, bytesPerRow));
    rowsPerBlock = std::min(rowsPerBlock, std::max<std::size_t>(kMinRowsPerBlock, (rowCount + workers * kBlocksPerWorker - 1) / (workers * kBlocksPerWorker)));
    const std::size_t blockCount = (rowCount + rowsPerBlock - 1) / rowsPerBlock;

    XSC_LOG("reduceRows: rows", rowCount, "columns", columnCount, "dimensions", dimensions,
            "workers", workers, "blocks", blockCount, "rows per block", rowsPerBlock);

    std::vector<State> states;
    states.reserve(workers);

    for (std::size_t w = 0; w < workers; ++w)
        states.push_back(make());

    auto readBlock = [&](const std::size_t b, State& state, const void* denseData = nullptr,
                         const std::uint32_t denseElementType = 0, const std::uint32_t denseElementSize = 0) {
        const std::size_t begin = b * rowsPerBlock;
        const std::size_t end   = std::min(rowCount, begin + rowsPerBlock);

        // Distinct rows of the block with the positions (in rows) that refer to each of them.
        struct RowGroup { std::uint64_t row; std::size_t first; std::size_t last; };
        std::vector<RowGroup> groups;

        for (std::size_t i = begin; i < end;) {
            std::size_t j = i + 1;
            while (j < end && sortedRowAt(j) == sortedRowAt(i))
                ++j;
            groups.push_back({ sortedRowAt(i), i, j });
            i = j;
        }

        const std::uint64_t spanFirst = groups.front().row;
        const std::uint64_t spanCount = groups.back().row - spanFirst + 1;

        XSC_TRACE("PointStreaming::read block");

        // Cursor over one fetch: the first absolute row of the current row batch and the column offset of the next view.
        // A row batch is complete when its last column segment arrives (offset == totalWidth).
        auto deliverFor = [&](const bool rowList, std::size_t& batchGroup, std::uint64_t& batchFirst, std::uint64_t& offset, const std::uint64_t totalWidth) {
            return PointDataAccessVisitor([&, rowList, totalWidth](const PointDataAccessView& view) {
                if (view.columns == 0)
                    return;

                if (offset + view.columns > totalWidth)
                    throw std::out_of_range("PointStreaming: a delivered view exceeds the requested column range");

                const bool lastSegment = offset + view.columns == totalWidth;

                if (rowList) {
                    // One row per batch: the batch is the group at batchGroup.
                    for (std::uint64_t r = 0; r < view.rows; ++r) {
                        if (batchGroup + r >= groups.size())
                            throw std::out_of_range("PointStreaming: more rows delivered than requested");

                        const RowGroup& g = groups[batchGroup + r];

                        for (std::size_t q = g.first; q < g.last; ++q)
                            visit(state, order.empty() ? q : order[q], view, r, offset);
                    }

                    if (lastSegment) {
                        batchGroup += static_cast<std::size_t>(view.rows);
                        offset = 0;
                    }
                    else {
                        offset += view.columns;
                    }

                    return;
                }

                // Span or run: the absolute row of view row r is batchFirst + r. Every segment of the batch looks up
                // its rows from the start of the batch; the batch pointer advances only when the batch is complete.
                std::size_t g = batchGroup;

                for (std::uint64_t r = 0; r < view.rows; ++r) {
                    const std::uint64_t absolute = batchFirst + r;

                    while (g < groups.size() && groups[g].row < absolute)
                        ++g;

                    if (g < groups.size() && groups[g].row == absolute) {
                        const RowGroup& rg = groups[g];

                        for (std::size_t q = rg.first; q < rg.last; ++q)
                            visit(state, order.empty() ? q : order[q], view, r, offset);
                    }
                }

                if (lastSegment) {
                    const std::uint64_t next = batchFirst + view.rows;

                    while (batchGroup < groups.size() && groups[batchGroup].row < next)
                        ++batchGroup;

                    batchFirst = next;
                    offset = 0;
                }
                else {
                    offset += view.columns;
                }
            });
        };

        // Dense point storage is already one contiguous row-major buffer. Build
        // a non-owning view for each fetched span while constVisitFromBeginToEnd
        // keeps the dataset locked. This avoids one temporary byte vector and
        // shared allocation per row in PointData::visitDenseRange.
        auto readRange = [&](const std::uint64_t firstRow, const std::uint64_t count,
                             const PointDataAccessVisitor& receiver) {
            if (!denseData) {
                fetchPointData(points, FetchMode::Range2D, receiver,
                    0, firstRow, count, {}, 0, 0, columnCount);
                return;
            }

            PointDataAccessView view{};
            PointStreamingDetail::setNonOwningRangeView(view, denseData, firstRow, count, columnCount,
                dimensions, denseElementType, denseElementSize);
            receiver(view);
        };

        // Runs are built from the distinct rows: a duplicated row must not split a run and be read twice.
        std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
        runs.reserve(groups.size());
        for (std::size_t first = 0; first < groups.size();) {
            std::size_t end = first + 1;
            while (end < groups.size() && groups[end].row == groups[end - 1].row + 1)
                ++end;
            runs.emplace_back(groups[first].row, static_cast<std::uint64_t>(end - first));
            first = end;
        }

        if (spanCount <= kMaxSpanFactor * (end - begin)) {
            // Span: one Range2D read over the whole row span of the block.
            XSC_LOG("block", b, "path: span", "rows", end - begin, "span", spanCount, "first", spanFirst);

            std::size_t groupIndex = 0;
            std::uint64_t batchFirst = spanFirst;
            std::uint64_t offset = 0;

            readRange(spanFirst, spanCount, deliverFor(false, groupIndex, batchFirst, offset, columnCount));

            if (offset != 0 || batchFirst != spanFirst + spanCount)
                throw std::runtime_error("PointStreaming: streamed views do not cover the requested rows");
        }
        else if (!runs.empty() && groups.size() / runs.size() >= kMinRowsPerRun) {
            // Long runs: one Range2D read per run. Groups of a run are consecutive in the block.
            XSC_LOG("block", b, "path: runs", "rows", end - begin, "runs", runs.size());

            std::size_t groupIndex = 0;

            for (const auto& run : runs) {
                while (groupIndex < groups.size() && groups[groupIndex].row < run.first)
                    ++groupIndex;

                std::uint64_t batchFirst = run.first;
                std::uint64_t offset = 0;

                readRange(run.first, run.second, deliverFor(false, groupIndex, batchFirst, offset, columnCount));

                if (offset != 0 || batchFirst != run.first + run.second)
                    throw std::runtime_error("PointStreaming: streamed views do not cover the requested rows");
            }
        }
        else {
            // Coalesce only nearby sparse rows. A byte limit alone can read
            // nearly the entire dataset for a scattered selection; also cap
            // the number of fetched rows relative to the requested rows.
            const std::uint64_t maxSpan = std::max<std::uint64_t>(1,
                kTargetReadBytes / (std::max<std::uint64_t>(1, columnCount) * sizeof(float)));
            std::size_t groupIndex = 0;
            while (groupIndex < groups.size()) {
                const std::uint64_t first = groups[groupIndex].row;
                const std::size_t next = boundedReadEnd(groupIndex, groups.size(), maxSpan, kMaxSpanFactor,
                    [&](std::size_t index) { return groups[index].row; });
                const std::uint64_t count = groups[next-1].row - first + 1;
                std::uint64_t batchFirst = first, offset = 0;
                readRange(first, count, deliverFor(false, groupIndex, batchFirst, offset, columnCount));
                if (offset != 0 || batchFirst != first + count || groupIndex != next)
                    throw std::runtime_error("PointStreaming: streamed range does not cover the requested rows");
            }
        }
    };

    std::mutex progressMutex;
    std::size_t completedBlocks = 0;
    const auto scanStart = std::chrono::steady_clock::now();
    const auto processBlocks = [&](const void* denseData, const std::uint32_t elementType,
                                   const std::uint32_t elementSize, const std::uint64_t pointCount) {
        if (denseData) {
            for (std::size_t i = 0; i < rowCount; ++i) {
                if (sortedRowAt(i) >= pointCount)
                    throw std::out_of_range("PointStreaming: requested row ID is out of range");
            }
        }
        forEachBlockParallel(states, blockCount, [&](const std::size_t b, State& state) {
            runMeasuredJob([&] {
                readBlock(b, state, denseData, elementType, elementSize);
            });
            if (progress) {
                const std::lock_guard<std::mutex> lock(progressMutex);
                progress(++completedBlocks, blockCount);
            }
        });
    };

    if (!PointStreamingDetail::withDensePointData(points, processBlocks))
        processBlocks(nullptr, 0, 0, 0);

    const auto elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - scanStart).count();
    qInfo().noquote() << QStringLiteral("[PointStreaming] row scan: rows=%1 blocks=%2 workers=%3 max-concurrent=%4 wall-ms=%5")
        .arg(rowCount).arg(blockCount).arg(workers).arg(maxConcurrentJobs.load(std::memory_order_relaxed)).arg(elapsedMilliseconds);

    for (std::size_t w = 1; w < workers; ++w)
        merge(states[0], states[w]);

    XSC_LOG("reduceRows: finished");

    return std::move(states[0]);
}

/** Per-column aggregates over a set of rows, accumulated in double precision. */
struct ColumnAggregates {
    std::vector<double>        sums;     /** sum of the values of each requested column */
    std::vector<std::uint64_t> nonZero;  /** number of rows with a value != 0 for each requested column */
    std::uint64_t              rows = 0; /** number of rows that were read */
};

/** Sums, detection counts and row count of \p columns over \p rowIds, in one streamed pass. */
template <typename RowContainer>
inline ColumnAggregates aggregateColumns(Points* points, const RowContainer& rowIds, const std::vector<int>& columns, int threads = 0)
{
    XSC_TRACE("PointStreaming::aggregateColumns");

    ColumnAggregates result;
    result.sums.assign(columns.size(), 0.0);
    result.nonZero.assign(columns.size(), 0);

    if (!points || rowIds.empty() || columns.empty())
        return result;

    const std::uint64_t dimensions = points->getNumDimensions();

    for (const int column : columns) {
        if (column < 0 || static_cast<std::uint64_t>(column) >= dimensions)
            throw std::out_of_range("PointStreaming: requested column is out of range");
    }

    const std::vector<std::uint64_t> columnIds(columns.begin(), columns.end());

    std::vector<std::uint64_t> uniqueColumns = columnIds;
    std::sort(uniqueColumns.begin(), uniqueColumns.end());
    uniqueColumns.erase(std::unique(uniqueColumns.begin(), uniqueColumns.end()), uniqueColumns.end());

    const std::uint64_t spanWidth = uniqueColumns.back() + 1;

    struct State {
        ColumnAggregates aggregates;
        SegmentColumns   segment;
    };

    auto makeState = [&]() {
        State state;
        state.aggregates.sums.assign(columns.size(), 0.0);
        state.aggregates.nonZero.assign(columns.size(), 0);
        return state;
    };

    auto mergeState = [](State& into, State& from) {
        for (std::size_t k = 0; k < into.aggregates.sums.size(); ++k) {
            into.aggregates.sums[k] += from.aggregates.sums[k];
            into.aggregates.nonZero[k] += from.aggregates.nonZero[k];
        }
        into.aggregates.rows += from.aggregates.rows;
    };

    // Dense request: full rows, parallel blocks.
    if (uniqueColumns.size() >= 32 || uniqueColumns.size() * 4 >= spanWidth) {
        State state = reduceRows(points, rowIds, threads, spanWidth, makeState,
            [&](State& s, std::size_t, const PointDataAccessView& view, std::uint64_t row, std::uint64_t offset) {
                s.segment.update(columnIds, offset, view.columns);
                accumulateSegment(view, row, s.segment, s.aggregates.sums.data(), s.aggregates.nonZero.data());
                if (offset == 0)
                    ++s.aggregates.rows; // count each row once, on its first column segment
            },
            mergeState);

        return std::move(state.aggregates);
    }

    // Sparse request: read only the requested column runs, for blocks of rows in parallel.
    const std::vector<std::uint64_t> rows(rowIds.begin(), rowIds.end());
    const auto runs = contiguousRuns(uniqueColumns);

    const std::size_t workers = workerCount(threads, rows.size());
    const std::size_t rowsPerBlock = std::max<std::size_t>(kMinRowsPerBlock,
        (rows.size() + workers * kBlocksPerWorker - 1) / (workers * kBlocksPerWorker));
    const std::size_t blockCount = (rows.size() + rowsPerBlock - 1) / rowsPerBlock;

    std::vector<State> states;
    states.reserve(workers);
    for (std::size_t w = 0; w < workers; ++w)
        states.push_back(makeState());

    forEachBlockParallel(states, blockCount, [&](const std::size_t b, State& state) {
        const std::size_t begin = b * rowsPerBlock;
        const std::size_t end   = std::min(rows.size(), begin + rowsPerBlock);
        const std::vector<std::uint64_t> block(rows.begin() + begin, rows.begin() + end);

        for (const auto& run : runs) {
            std::size_t rowPosition = 0; // index of the row batch within the block
            std::uint64_t offset = 0;

            fetchPointData(points, FetchMode::ColumnsForRows, [&](const PointDataAccessView& view) {
                if (view.columns == 0)
                    return;

                if (offset + view.columns > run.second)
                    throw std::out_of_range("PointStreaming: delivered view exceeds the requested column run");

                const std::uint64_t absoluteStart = run.first + offset;
                state.segment.update(columnIds, absoluteStart, view.columns);

                for (std::uint64_t r = 0; r < view.rows; ++r) {
                    if (rowPosition + r >= block.size())
                        throw std::out_of_range("PointStreaming: more rows delivered than requested");

                    accumulateSegment(view, r, state.segment, state.aggregates.sums.data(), state.aggregates.nonZero.data());
                }

                offset += view.columns;

                if (offset == run.second) {
                    offset = 0;
                    rowPosition += view.rows;
                }
            }, 0, 0, 0, block, 0, run.first, run.second);

            if (offset != 0 || rowPosition != block.size())
                throw std::runtime_error("PointStreaming: streamed views do not cover the requested rows and columns");
        }

        state.aggregates.rows += block.size();
    });

    for (std::size_t w = 1; w < workers; ++w)
        mergeState(states[0], states[w]);

    return std::move(states[0].aggregates);
}

/** Per-group sums from one shared scan; overlapping and duplicate memberships are retained. */
struct GroupAggregates {
    std::vector<std::vector<double>> sums;
    std::vector<std::vector<std::uint64_t>> nonZero;
    std::vector<std::uint64_t> counts;
};

template <typename Rows, typename GroupAt>
inline GroupAggregates aggregateSparseDisjointRows(Points* points, const Rows& rows, const std::size_t groupCount,
    GroupAt&& groupAt, const std::vector<int>& columns, const int threads,
    const std::function<void(std::size_t, std::size_t)>& progress = {})
{
    GroupAggregates empty;
    empty.sums.assign(groupCount, std::vector<double>(columns.size(), 0.0));
    empty.counts.assign(groupCount, 0);
    if (!points || rows.empty() || columns.empty())
        return empty;

    const std::vector<std::uint64_t> columnIds(columns.begin(), columns.end());
    std::vector<std::uint64_t> uniqueColumns = columnIds;
    std::sort(uniqueColumns.begin(), uniqueColumns.end());
    uniqueColumns.erase(std::unique(uniqueColumns.begin(), uniqueColumns.end()), uniqueColumns.end());
    const auto runs = contiguousRuns(uniqueColumns);
    const std::size_t workers = workerCount(threads, rows.size());
    const std::size_t rowsPerBlock = std::max<std::size_t>(kMinRowsPerBlock,
        (rows.size() + workers * kBlocksPerWorker - 1) / (workers * kBlocksPerWorker));
    const std::size_t blockCount = (rows.size() + rowsPerBlock - 1) / rowsPerBlock;

    struct State { GroupAggregates result; SegmentColumns segment; };
    std::vector<State> states;
    states.reserve(workers);
    for (std::size_t worker = 0; worker < workers; ++worker) {
        State state;
        state.result = empty;
        states.push_back(std::move(state));
    }

    std::mutex progressMutex;
    std::size_t completed = 0;
    forEachBlockParallel(states, blockCount, [&](const std::size_t block, State& state) {
        const std::size_t begin = block * rowsPerBlock;
        const std::size_t end = std::min(rows.size(), begin + rowsPerBlock);

        for (std::size_t runIndex = 0; runIndex < runs.size(); ++runIndex) {
            const auto& run = runs[runIndex];
            const std::uint64_t maxSpan = std::max<std::uint64_t>(1,
                kTargetReadBytes / (std::max<std::uint64_t>(1, run.second) * sizeof(float)));

            for (std::size_t rowBegin = begin; rowBegin < end;) {
                if (progress) { const std::lock_guard<std::mutex> lock(progressMutex); progress(completed, blockCount); }
                const std::uint64_t firstRow = static_cast<std::uint64_t>(rows[rowBegin]);
                std::size_t rowEnd = rowBegin + 1;
                while (rowEnd < end && static_cast<std::uint64_t>(rows[rowEnd]) - firstRow < maxSpan)
                    ++rowEnd;
                const std::uint64_t rowCount = static_cast<std::uint64_t>(rows[rowEnd - 1]) - firstRow + 1;

                std::size_t nextGroup = rowBegin;
                std::uint64_t batchFirst = firstRow;
                std::uint64_t columnOffset = 0;
                fetchPointData(points, FetchMode::Range2D, [&](const PointDataAccessView& view) {
                    if (view.columns == 0)
                        return;
                    if (columnOffset + view.columns > run.second)
                        throw std::out_of_range("PointStreaming: delivered view exceeds the requested gene run");

                    const bool lastColumnSegment = columnOffset + view.columns == run.second;
                    std::size_t group = nextGroup;
                    for (std::uint64_t row = 0; row < view.rows; ++row) {
                        const std::uint64_t absoluteRow = batchFirst + row;
                        while (group < rowEnd && static_cast<std::uint64_t>(rows[group]) < absoluteRow)
                            ++group;
                        if (group < rowEnd && static_cast<std::uint64_t>(rows[group]) == absoluteRow) {
                            state.segment.update(columnIds, run.first + columnOffset, view.columns);
                            const std::size_t destinationGroup = groupAt(group);
                            accumulateSegment(view, row, state.segment, state.result.sums[destinationGroup].data(), nullptr);
                            if (runIndex == 0 && columnOffset == 0)
                                ++state.result.counts[destinationGroup];
                        }
                    }

                    if (lastColumnSegment) {
                        const std::uint64_t nextRow = batchFirst + view.rows;
                        while (nextGroup < rowEnd && static_cast<std::uint64_t>(rows[nextGroup]) < nextRow)
                            ++nextGroup;
                        batchFirst = nextRow;
                        columnOffset = 0;
                    }
                    else {
                        columnOffset += view.columns;
                    }
                }, 0, firstRow, rowCount, {}, 0, run.first, run.second);

                if (columnOffset != 0 || batchFirst != firstRow + rowCount || nextGroup != rowEnd)
                    throw std::runtime_error("PointStreaming: sparse grouped read did not cover the requested rows and columns");
                rowBegin = rowEnd;
            }
        }
        if (progress) { const std::lock_guard<std::mutex> lock(progressMutex); progress(++completed, blockCount); }
    });

    for (std::size_t worker = 1; worker < workers; ++worker) {
        for (std::size_t group = 0; group < states[0].result.sums.size(); ++group) {
            states[0].result.counts[group] += states[worker].result.counts[group];
            for (std::size_t gene = 0; gene < states[0].result.sums[group].size(); ++gene)
                states[0].result.sums[group][gene] += states[worker].result.sums[group][gene];
        }
    }
    return std::move(states[0].result);
}

template <typename Rows, typename GroupAt>
inline GroupAggregates aggregateDisjointRows(Points* points, const Rows& rows, const std::size_t groupCount,
    GroupAt&& groupAt, const std::vector<int>& columns, int threads, const std::size_t budgetBytes,
    const std::size_t mappingBytes, const bool includeDetection,
    const std::function<void(std::size_t, std::size_t)>& progress = {})
{
    GroupAggregates empty;
    empty.sums.assign(groupCount, std::vector<double>(columns.size(), 0.0));
    if (includeDetection)
        empty.nonZero.assign(groupCount, std::vector<std::uint64_t>(columns.size(), 0));
    empty.counts.assign(groupCount, 0);
    if (!points || rows.empty() || columns.empty())
        return empty;

    const std::vector<std::uint64_t> columnIds(columns.begin(), columns.end());
    for (const int column : columns)
        if (column < 0 || static_cast<std::uint64_t>(column) >= points->getNumDimensions())
            throw std::out_of_range("PointStreaming: grouped column out of range");
    const std::uint64_t width = *std::max_element(columnIds.begin(), columnIds.end()) + 1;
    const std::size_t bytesPerAggregate = sizeof(double) + (includeDetection ? sizeof(std::uint64_t) : 0);
    const std::size_t stateBytes = groupCount * columns.size() * bytesPerAggregate + kTargetReadBytes;
    const int requestedThreads = threads;
    threads = ExpressionScanResources::workersForMemory(threads, budgetBytes, mappingBytes, stateBytes);

    std::vector<std::uint64_t> uniqueColumns = columnIds;
    std::sort(uniqueColumns.begin(), uniqueColumns.end());
    uniqueColumns.erase(std::unique(uniqueColumns.begin(), uniqueColumns.end()), uniqueColumns.end());
    const std::uint64_t spanWidth = uniqueColumns.back() + 1;
    const bool sparseColumns = uniqueColumns.size() <= 256 && uniqueColumns.size() * 4 < spanWidth;
    const bool denseStorage = PointStreamingDetail::withDensePointData(points,
        [](const void*, std::uint32_t, std::uint32_t, std::uint64_t) {});
    // Sparse API reads repeatedly decompress/stage the same row-major tile
    // for each gene run. Physical tiles must use the single-pass path below.
    const auto tiledStorage = !points->isProxy() && points->isFull() ? PointStreamingDetail::tiledStorage(points) : nullptr;
    const bool physicalTiles = static_cast<bool>(tiledStorage);
    if (groupedReadPath(sparseColumns, denseStorage, physicalTiles, includeDetection) == GroupedReadPath::SparseRanges)
        return aggregateSparseDisjointRows(points, rows, groupCount, std::forward<GroupAt>(groupAt), columns, threads, progress);

    struct State { GroupAggregates result; SegmentColumns segment; std::unique_ptr<ColumnTileCache::Reader> reader; };
    auto makeState = [&]() { return State{ empty, {}, std::make_unique<ColumnTileCache::Reader>() }; };
    auto mergeState = [](State& into, State& from) {
        for (std::size_t group = 0; group < into.result.sums.size(); ++group) {
            into.result.counts[group] += from.result.counts[group];
            for (std::size_t gene = 0; gene < into.result.sums[group].size(); ++gene) {
                into.result.sums[group][gene] += from.result.sums[group][gene];
                if (!into.result.nonZero.empty())
                    into.result.nonZero[group][gene] += from.result.nonZero[group][gene];
            }
        }
    };

    if (physicalTiles && std::is_sorted(rows.begin(), rows.end())) {
        const auto storage = tiledStorage;
        const auto cache = PackedTileAccess::columnsFor(storage);
        const bool sparseRows = rows.size() <= storage->rows() / 8;
        const bool narrow = uniqueColumns.size() * 4 < storage->columns();
        if (cache && ((narrow && cache->enabled()) || (includeDetection && sparseRows && cache->complete()))) {
            XSC_TRACE("analysis.gene-blocks.aggregate");
            std::vector<std::pair<std::size_t, std::size_t>> jobs;
            const auto rowAt = [&](std::size_t i) { return static_cast<std::uint64_t>(rows[i]); };
            for (std::size_t first = 0; first < rows.size();) {
                const auto end = tileReadEnd(first, rows.size(), storage->rowTileSize(), rowAt);
                jobs.emplace_back(first, end); first = end;
            }
            validateTileJobs(jobs, rows.size(), storage->rowTileSize(), storage->rows(), rowAt);
            std::vector<State> states;
            const auto tileBytes = std::min<std::uint64_t>(storage->rows(), storage->rowTileSize())
                * std::min<std::uint64_t>(storage->columns(), storage->columnTileSize()) * storage->elementSize();
            // Allow for the decoded tile, its encoded input, and column-cache scratch.
            const auto tileWorkerBytes = groupCount * columns.size() * bytesPerAggregate
                + 2 * tileBytes + 4 * ExpressionScanResources::MiB;
            const auto tileWorkers = ExpressionScanResources::workersForMemory(requestedThreads,
                budgetBytes, mappingBytes, tileWorkerBytes);
            const auto workers = std::min<std::size_t>(tileWorkers, jobs.size());
            qInfo() << "[Expression scan]" << workers << "workers for" << jobs.size() << "tile blocks, scratch budget"
                    << budgetBytes / ExpressionScanResources::MiB << "MiB";
            for (std::size_t w = 0; w < workers; ++w) states.push_back(makeState());
            std::mutex progressMutex;
            std::size_t completed = 0;
            // A contiguous cache read pays off when the request touches most
            // stripes. Sparse requests retain their per-stripe reads.
            const auto stripeWidth = ColumnTileCache::stripeWidth;
            const auto stripeCount = (storage->columns() + stripeWidth - 1) / stripeWidth;
            std::size_t requestedStripes = 0;
            auto previousStripe = stripeCount;
            for (const auto column : uniqueColumns) {
                const auto stripe = column / stripeWidth;
                if (stripe != previousStripe) ++requestedStripes;
                previousStripe = stripe;
            }
            const bool readAhead = storage->columnTileSize() >= storage->columns() &&
                requestedStripes >= 8 && requestedStripes * 4 >= stripeCount * 3;
            forEachBlockParallel(states, jobs.size(), [&](std::size_t job, State& state) {
                const auto [begin, end] = jobs[job];
                const auto tileRow = rowAt(begin) / storage->rowTileSize();
                auto& selectedRows = state.reader->selectedRows;
                selectedRows.clear();
                if (end - begin <= storage->rowTileSize() / 4) {
                    for (auto position = begin; position < end; ++position)
                        selectedRows.push_back(rowAt(position) % storage->rowTileSize());
                }
                if (readAhead) {
                    PackedTileAccess::readerFor(cache, state.reader.get());
                    cache->prefetch(tileRow, storage->columns(), *state.reader);
                }
                std::size_t blocksRead = 0;
                for (std::size_t next = 0; next < uniqueColumns.size();) {
                    // Bound cancellation latency without serializing every stripe read.
                    if (progress && blocksRead++ % 8 == 0) {
                        const std::lock_guard<std::mutex> lock(progressMutex);
                        progress(completed, jobs.size());
                    }
                    const auto tile = uniqueColumns.size() == 1
                        ? PackedTileAccess::readColumn(points, storage, tileRow, uniqueColumns[next], state.reader.get())
                        : PackedTileAccess::readStripe(points, storage, tileRow, uniqueColumns[next], state.reader.get());
                    if (!tile || !tile.columns()) throw std::runtime_error("Missing analysis gene block");
                    PointDataAccessView view{};
                    const auto type = static_cast<std::uint32_t>(Points::elementTypeSpecifier(points->getElementTypeName()));
                    PointStreamingDetail::setNonOwningRangeView(view, tile.data(), 0, tile.rows(), tile.columns(), tile.columns(), type,
                        static_cast<std::uint32_t>(tile.elementSize()));
                    state.segment.update(columnIds, tile.columnOffset(), view.columns);
                    for (std::size_t position = begin; position < end; ++position) {
                        const auto group = groupAt(position);
                        accumulateSegment(view, rowAt(position) - tile.rowOffset(), state.segment, state.result.sums[group].data(),
                            includeDetection ? state.result.nonZero[group].data() : nullptr);
                        if (next == 0) ++state.result.counts[group];
                    }
                    const auto limit = tile.columnOffset() + tile.columns();
                    if (uniqueColumns[next] < tile.columnOffset() || uniqueColumns[next] >= limit)
                        throw std::runtime_error("Analysis gene block does not cover its requested column");
                    do { ++next; } while (next < uniqueColumns.size() && uniqueColumns[next] < limit);
                }
                if (progress) { const std::lock_guard<std::mutex> lock(progressMutex); progress(++completed, jobs.size()); }
            });
            for (std::size_t w = 1; w < states.size(); ++w) mergeState(states[0], states[w]);
            return std::move(states[0].result);
        }
    }

    auto result = reduceRows(points, rows, threads, width, makeState,
        [&](State& state, const std::size_t position, const PointDataAccessView& view,
            const std::uint64_t row, const std::uint64_t offset) {
            state.segment.update(columnIds, offset, view.columns);
            const std::size_t group = groupAt(position);
            accumulateSegment(view, row, state.segment, state.result.sums[group].data(),
                includeDetection ? state.result.nonZero[group].data() : nullptr);
            if (offset == 0)
                ++state.result.counts[group];
        }, mergeState, progress);
    return std::move(result.result);
}

/** Conservative peak memory estimate for the row-to-group index used by grouped aggregation. */
inline std::size_t groupedIndexMemoryBytes(const std::vector<std::vector<std::uint64_t>>& groups)
{
    std::uint64_t span = 0;
    std::size_t memberships = 0;
    std::size_t largestGroup = 0;
    for (const auto& group : groups) {
        memberships += group.size();
        largestGroup = std::max(largestGroup, group.size());
        for (const auto row : group)
            span = std::max(span, row + 1);
    }
    // counts + offsets by row, group memberships, unique sorted row ids, caller row lists,
    // and the largest temporary int row list used while building those groups.
    return static_cast<std::size_t>(span + 1) * 2 * sizeof(std::size_t)
        + memberships * (sizeof(std::size_t) + 2 * sizeof(std::uint64_t))
        + largestGroup * sizeof(int);
}

inline GroupAggregates aggregateGroupedColumns(Points* points,
    const std::vector<std::vector<std::uint64_t>>& groups, const std::vector<int>& columns,
    int threads, std::size_t budgetBytes, bool includeDetection = false,
    const std::function<void(std::size_t, std::size_t)>& progress = {}) {
    GroupAggregates empty;
    empty.sums.assign(groups.size(), std::vector<double>(columns.size(), 0.0));
    if (includeDetection)
        empty.nonZero.assign(groups.size(), std::vector<std::uint64_t>(columns.size(), 0));
    empty.counts.assign(groups.size(), 0);
    if (!points || columns.empty()) return empty;
    std::size_t span = 0, membershipsCount = 0;
    for (const auto& group : groups) for (const auto row : group) {
        if (row >= std::numeric_limits<std::size_t>::max())
            throw std::overflow_error("PointStreaming: grouped row index exceeds addressable memory");
        if (membershipsCount == std::numeric_limits<std::size_t>::max())
            throw std::overflow_error("PointStreaming: grouped membership count overflow");
        span = std::max(span, static_cast<std::size_t>(row) + 1);
        ++membershipsCount;
    }
    if (span == 0) return empty;

    // Species/cell-type groups are normally disjoint. Store one compact group
    // ID per used point rather than two size_t tables and a membership array.
    // The overlap-aware path below remains in place for shared or duplicated rows.
    if (span <= std::numeric_limits<std::uint32_t>::max() && groups.size() < std::numeric_limits<std::uint32_t>::max()) {
        constexpr std::uint32_t noGroup = std::numeric_limits<std::uint32_t>::max();
        std::vector<std::uint32_t> groupByRow(span, noGroup);
        bool disjoint = true;
        for (std::size_t groupIndex = 0; groupIndex < groups.size() && disjoint; ++groupIndex) {
            for (const auto row : groups[groupIndex]) {
                auto& groupForRow = groupByRow[static_cast<std::size_t>(row)];
                if (groupForRow != noGroup) {
                    disjoint = false;
                    break;
                }
                groupForRow = static_cast<std::uint32_t>(groupIndex);
            }
        }

        if (disjoint) {
            std::vector<std::uint32_t> rows;
            rows.reserve(membershipsCount);
            for (std::size_t row = 0; row < groupByRow.size(); ++row)
                if (groupByRow[row] != noGroup)
                    rows.push_back(static_cast<std::uint32_t>(row));

            const std::size_t mappingBytes = groupByRow.size() * sizeof(std::uint32_t) + rows.size() * sizeof(std::uint32_t);
            return aggregateDisjointRows(points, rows, groups.size(),
                [&](const std::size_t position) { return groupByRow[rows[position]]; },
                columns, threads, budgetBytes, mappingBytes, includeDetection, progress);
        }
    }

    std::vector<std::size_t> counts(span, 0), offsets(span+1, 0);
    for (const auto& group : groups) for (auto row : group) ++counts[row];
    for (std::size_t row=0; row<span; ++row) offsets[row+1]=offsets[row]+counts[row];
    std::vector<std::size_t> members(membershipsCount);
    for (std::size_t group=0; group<groups.size(); ++group)
        for (auto row:groups[group]) members[offsets[row]+--counts[row]]=group;
    std::vector<std::uint64_t> rows;
    rows.reserve(std::min(span,membershipsCount));
    for (std::size_t row=0; row<span; ++row) if (offsets[row]!=offsets[row+1]) rows.push_back(row);
    counts.clear(); counts.shrink_to_fit();
    const std::vector<std::uint64_t> columnIds(columns.begin(),columns.end());
    for (int column:columns) if(column<0 || static_cast<std::uint64_t>(column)>=points->getNumDimensions()) throw std::out_of_range("PointStreaming: grouped column out of range");
    const std::uint64_t width=*std::max_element(columnIds.begin(),columnIds.end())+1;
    const std::size_t mappingBytes=groupedIndexMemoryBytes(groups);
    const std::size_t bytesPerAggregate = sizeof(double) + (includeDetection ? sizeof(std::uint64_t) : 0);
    const std::size_t stateBytes=groups.size()*columns.size()*bytesPerAggregate+kTargetReadBytes;
    threads = ExpressionScanResources::workersForMemory(threads, budgetBytes, mappingBytes, stateBytes);
    struct State { GroupAggregates result; SegmentColumns segment; };
    auto result=reduceRows(points,rows,threads,width,[&](){return State{empty,{}};},
        [&](State& state,std::size_t position,const PointDataAccessView& view,std::uint64_t row,std::uint64_t offset) {
            state.segment.update(columnIds,offset,view.columns);
            const std::size_t global=static_cast<std::size_t>(rows[position]);
            for(std::size_t m=offsets[global];m<offsets[global+1];++m) {
                const std::size_t group=members[m];
                accumulateSegment(view,row,state.segment,state.result.sums[group].data(),
                    includeDetection ? state.result.nonZero[group].data() : nullptr);
                if(offset==0) ++state.result.counts[group];
            }
        },[](State& into,State& from){
            for(std::size_t group=0;group<into.result.sums.size();++group){
                into.result.counts[group]+=from.result.counts[group];
                for(std::size_t g=0;g<into.result.sums[group].size();++g) {
                    into.result.sums[group][g]+=from.result.sums[group][g];
                    if (!into.result.nonZero.empty())
                        into.result.nonZero[group][g]+=from.result.nonZero[group][g];
                }
            }
        }, progress);
    return std::move(result.result);
}

/** Values of one column for \p rowIds, in the order of \p rowIds. Uses the column path of the API. */
template <typename RowContainer>
inline std::vector<float> fetchColumnValues(Points* points, int column, const RowContainer& rowIds,
    const std::function<bool()>& cancelled = {}, const std::function<void(std::size_t, std::size_t)>& progress = {}, int threads = 1)
{
    XSC_TRACE("PointStreaming::fetchColumnValues");

    std::vector<float> values(rowIds.size(), 0.0f);

    if (!points || values.empty())
        return values;

    const auto dimensions = points->getNumDimensions();
    if (column < 0 || static_cast<std::uint64_t>(column) >= dimensions)
        throw std::out_of_range("PointStreaming: requested column is out of range");

    // A full dense dataset is already resident as one typed row-major buffer.
    // Read the requested gene directly instead of asking visitColumns to create
    // one temporary view for every selected point.
    const bool readDirectly = PointStreamingDetail::withDensePointData(points,
        [&](const void* data, const std::uint32_t elementType, const std::uint32_t elementSize, const std::uint64_t pointCount) {
            PointDataAccessView rowView{};
            for (std::size_t i = 0; i < rowIds.size(); ++i) {
                if ((i % 4096) == 0 && cancelled && cancelled()) throw std::runtime_error("Cancelled");
                using RowId = std::decay_t<decltype(rowIds[i])>;
                if constexpr (std::is_signed_v<RowId>) {
                    if (rowIds[i] < 0)
                        throw std::out_of_range("PointStreaming: requested row ID is out of range");
                }
                const auto localRow = static_cast<std::uint64_t>(rowIds[i]);
                const auto row = logicalToSourceRow(points->isFull(), localRow, points->getNumPoints(), points->indices, pointCount);
                if (row >= pointCount)
                    throw std::out_of_range("PointStreaming: requested row ID is out of range");
                PointStreamingDetail::setNonOwningRangeView(rowView, data,
                    row, 1, dimensions, dimensions, elementType, elementSize);
                values[i] = valueAt(rowView, 0, static_cast<std::uint64_t>(column));
                if (progress && (i % 4096 == 0 || i + 1 == rowIds.size())) progress(i + 1, rowIds.size());
            }
        }, true);
    if (readDirectly)
        return values;

    if (!points->isProxy()) {
        if (const auto storage = PointStreamingDetail::tiledStorage(points); storage && (points->isFull() || PackedTileAccess::supportsSubset(storage))) {
            std::vector<std::pair<std::uint64_t, std::size_t>> requested;
            requested.reserve(rowIds.size());
            for (std::size_t i = 0; i < rowIds.size(); ++i) {
                const auto local = static_cast<std::uint64_t>(rowIds[i]);
                if (local >= points->getNumPoints()) throw std::out_of_range("PointStreaming: requested row is out of range");
                const auto global = points->isFull() ? local : static_cast<std::uint64_t>(points->indices.at(static_cast<std::size_t>(local)));
                requested.emplace_back(global, i);
            }
            const auto less = [](const auto& left, const auto& right) { return left.first < right.first; };
            if (!std::is_sorted(requested.begin(), requested.end(), less)) std::stable_sort(requested.begin(), requested.end(), less);
            const auto type = static_cast<std::uint32_t>(Points::elementTypeSpecifier(points->getElementTypeName()));
            std::vector<std::pair<std::size_t, std::size_t>> jobs;
            for (std::size_t first = 0; first < requested.size();) {
                auto end = first + 1;
                const auto tileRow = requested[first].first / storage->rowTileSize();
                while (end < requested.size() && requested[end].first / storage->rowTileSize() == tileRow) ++end;
                jobs.emplace_back(first, end);
                first = end;
            }
            std::vector<std::unique_ptr<ColumnTileCache::Reader>> states;
            const auto workers = std::min<std::size_t>(jobs.size(), static_cast<std::size_t>(std::max(1, threads)));
            for (std::size_t worker = 0; worker < workers; ++worker) states.push_back(std::make_unique<ColumnTileCache::Reader>());
            std::mutex progressMutex;
            std::size_t completed = 0;
            forEachBlockParallel(states, jobs.size(), [&](std::size_t job, std::unique_ptr<ColumnTileCache::Reader>& reader) {
                auto first = jobs[job].first;
                if (cancelled && cancelled()) throw std::runtime_error("Cancelled");
                const auto tile = PackedTileAccess::readColumn(points, storage, requested[first].first / storage->rowTileSize(),
                    static_cast<std::uint64_t>(column), reader.get());
                if (!tile || tile.columns() == 0) throw std::runtime_error("PointStreaming: missing column tile");
                PointDataAccessView view{};
                PointStreamingDetail::setNonOwningRangeView(view, tile.data(), 0, tile.rows(), tile.columns(),
                    tile.columns(), type, static_cast<std::uint32_t>(tile.elementSize()));
                const auto end = tile.rowOffset() + tile.rows();
                do {
                    if (requested[first].first < tile.rowOffset()) throw std::runtime_error("PointStreaming: invalid tile row");
                    values[requested[first].second] = valueAt(view, requested[first].first - tile.rowOffset(),
                        static_cast<std::uint64_t>(column) - tile.columnOffset());
                    ++first;
                } while (first < jobs[job].second && requested[first].first < end);
                if (first != jobs[job].second) throw std::runtime_error("PointStreaming: incomplete column tile");
                if (progress) {
                    const std::lock_guard<std::mutex> lock(progressMutex);
                    completed += jobs[job].second - jobs[job].first;
                    progress(completed, requested.size());
                }
            });
            return values;
        }
    }
    const std::uint64_t columnId = static_cast<std::uint64_t>(column);
    std::size_t position = 0;
    const auto read = [&](const PointDataAccessView& view) {
        if (cancelled && cancelled()) throw std::runtime_error("Cancelled");
        if (view.rows > values.size() - position)
            throw std::runtime_error("PointStreaming: too many rows in the requested gene range");
        for (std::uint64_t row = 0; row < view.rows; ++row)
            values[position++] = valueAt(view, row, 0);
    };
    for (std::size_t begin = 0; begin < rowIds.size();) {
        if (cancelled && cancelled()) throw std::runtime_error("Cancelled");
        const auto first = static_cast<std::uint64_t>(rowIds[begin]);
        if (first >= points->getNumPoints())
            throw std::out_of_range("PointStreaming: requested row ID is out of range");
        auto end = begin + 1;
        while (end < rowIds.size() && end - begin < 4096 &&
            static_cast<std::uint64_t>(rowIds[end]) == first + (end - begin))
            ++end;
        points->visitRange(first, end - begin, columnId, 1, read);
        if (position != end)
            throw std::runtime_error("PointStreaming: incomplete gene range");
        if (progress) progress(position, rowIds.size());
        begin = end;
    }

    return values;
}

} // namespace PointStreaming
