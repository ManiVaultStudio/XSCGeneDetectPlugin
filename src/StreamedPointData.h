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

#include <PointData/PointData.h>
#include <biovault_bfloat16/biovault_bfloat16.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
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

        for (std::size_t k = 0; k < columns.size(); ++k) {
            if (columns[k] >= segmentStart && columns[k] < segmentStart + segmentWidth) {
                relative.push_back(columns[k] - segmentStart);
                slot.push_back(k);
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
 *
 * Throws std::out_of_range / std::runtime_error when the delivered views do not match the request.
 */
template <typename RowContainer, typename Make, typename Visit, typename Merge>
inline auto reduceRows(Points* points, const RowContainer& rowIds, int threads, std::uint64_t columnCount,
                       Make&& make, Visit&& visit, Merge&& merge)
    -> decltype(make())
{
    using State = decltype(make());

    XSC_TRACE("PointStreaming::reduceRows");

    if (!points || rowIds.empty() || columnCount == 0)
        return make();

    const std::uint64_t dimensions = points->getNumDimensions();

    if (columnCount > dimensions)
        throw std::out_of_range("PointStreaming: requested columns exceed the number of dimensions of the dataset");

    // Sort the row IDs; order maps a sorted position back to the position in rowIds (empty when already sorted).
    std::vector<std::uint64_t> requested(rowIds.begin(), rowIds.end());
    std::vector<std::size_t>   order;
    std::vector<std::uint64_t> rows;

    if (std::is_sorted(requested.begin(), requested.end())) {
        rows = std::move(requested);
    }
    else {
        order.resize(requested.size());
        std::iota(order.begin(), order.end(), std::size_t{ 0 });
        std::sort(order.begin(), order.end(), [&requested](std::size_t a, std::size_t b) { return requested[a] < requested[b]; });

        rows.resize(order.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            rows[i] = requested[order[i]];
    }

    const std::size_t rowCount = rows.size();

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

    auto readBlock = [&](const std::size_t b, State& state) {
        const std::size_t begin = b * rowsPerBlock;
        const std::size_t end   = std::min(rowCount, begin + rowsPerBlock);

        // Distinct rows of the block with the positions (in rows) that refer to each of them.
        struct RowGroup { std::uint64_t row; std::size_t first; std::size_t last; };
        std::vector<RowGroup> groups;

        for (std::size_t i = begin; i < end;) {
            std::size_t j = i + 1;
            while (j < end && rows[j] == rows[i])
                ++j;
            groups.push_back({ rows[i], i, j });
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

        // Runs are built from the distinct rows: a duplicated row must not split a run and be read twice.
        std::vector<std::uint64_t> distinctRows;
        distinctRows.reserve(groups.size());
        for (const auto& g : groups)
            distinctRows.push_back(g.row);

        const auto runs = contiguousRuns(distinctRows);

        if (spanCount <= kMaxSpanFactor * (end - begin)) {
            // Span: one Range2D read over the whole row span of the block.
            XSC_LOG("block", b, "path: span", "rows", end - begin, "span", spanCount, "first", spanFirst);

            std::size_t groupIndex = 0;
            std::uint64_t batchFirst = spanFirst;
            std::uint64_t offset = 0;

            fetchPointData(points, FetchMode::Range2D, deliverFor(false, groupIndex, batchFirst, offset, columnCount),
                           0, spanFirst, spanCount, {}, 0, 0, columnCount);

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

                fetchPointData(points, FetchMode::Range2D, deliverFor(false, groupIndex, batchFirst, offset, columnCount),
                               0, run.first, run.second, {}, 0, 0, columnCount);

                if (offset != 0 || batchFirst != run.first + run.second)
                    throw std::runtime_error("PointStreaming: streamed views do not cover the requested rows");
            }
        }
        else {
            // Sparse: one row-list read for the block; each row is delivered in full width.
            XSC_LOG("block", b, "path: row list", "rows", end - begin, "groups", groups.size());

            std::size_t groupIndex = 0;
            std::uint64_t batchFirst = 0;
            std::uint64_t offset = 0;
            const std::vector<std::uint64_t> distinct = [&] {
                std::vector<std::uint64_t> ids;
                ids.reserve(groups.size());
                for (const auto& g : groups)
                    ids.push_back(g.row);
                return ids;
            }();

            fetchPointData(points, FetchMode::RowList, deliverFor(true, groupIndex, batchFirst, offset, dimensions),
                           0, 0, 0, distinct, 0, 0, 0);

            if (offset != 0 || groupIndex != groups.size())
                throw std::runtime_error("PointStreaming: streamed views do not cover the requested rows");
        }
    };

    forEachBlockParallel(states, blockCount, [&](const std::size_t b, State& state) {
        readBlock(b, state);
    });

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
    if (uniqueColumns.size() * 4 >= spanWidth) {
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

/** Values of one column for \p rowIds, in the order of \p rowIds. Uses the column path of the API. */
template <typename RowContainer>
inline std::vector<float> fetchColumnValues(Points* points, int column, const RowContainer& rowIds)
{
    XSC_TRACE("PointStreaming::fetchColumnValues");

    std::vector<float> values(rowIds.size(), 0.0f);

    if (!points || values.empty())
        return values;

    const std::vector<std::uint64_t> rows(rowIds.begin(), rowIds.end());
    const std::uint64_t columnId = static_cast<std::uint64_t>(column);
    std::size_t position = 0;

    fetchPointData(points, FetchMode::ColumnsForRows, [&](const PointDataAccessView& view) {
        for (std::uint64_t row = 0; row < view.rows; ++row, ++position) {
            if (position < values.size())
                values[position] = valueAt(view, row, 0); // the view holds only this column
        }
    }, 0, 0, 0, rows, 0, columnId, 1);

    if (position != values.size())
        throw std::runtime_error("PointStreaming: streamed row count does not match the requested rows");

    return values;
}

} // namespace PointStreaming
