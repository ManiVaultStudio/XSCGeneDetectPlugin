#pragma once

#include "XSCDebug.h"

// Streaming access to the main (cell x gene) point dataset.
//
// Semantics (from the point-data implementation and PointsViewWidget::displayQuery):
//  - valueAt() indexes a view relative to the view.
//  - a request can be delivered as several views per row block (column segments, e.g. tiles). The views of one row block
//    arrive in column order; the column offset of a view is the sum of the widths of the views before it in that block.
//  - a view does not carry its absolute column position, so the offset is tracked here, as the widget does.
//
// Access patterns:
//  - reduceRows / aggregateColumns: rows are read in ascending order, in blocks; contiguous runs use FetchMode::Range2D,
//    scattered rows use FetchMode::RowList. No buffer for the whole selection is allocated.
//  - fetchColumnValues: one column for a set of rows, FetchMode::ColumnsForRows.

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
#include <thread>
#include <utility>
#include <vector>

class Points;
using PointDataAccessVisitor = std::function<void(const PointDataAccessView&)>;

/**
 * Value at (row, column) of a view delivered by the point-data API.
 * row and column are relative to the view. Same element decoding as PointsViewWidget.cpp.
 */
inline float valueAt(const PointDataAccessView& view, const std::uint64_t row, const std::uint64_t column)
{
    const auto* bytes = view.rowData(row) + column * view.elementSizeBytes;

    switch (view.elementTypeIndex)
    {
        case 0:
            return *reinterpret_cast<const float*>(bytes);
        case 1:
            return static_cast<float>(*reinterpret_cast<const biovault::bfloat16_t*>(bytes));
        case 2:
            return static_cast<float>(*reinterpret_cast<const std::int32_t*>(bytes));
        case 3:
            return static_cast<float>(*reinterpret_cast<const std::uint32_t*>(bytes));
        case 4:
            return static_cast<float>(*reinterpret_cast<const std::int16_t*>(bytes));
        case 5:
            return static_cast<float>(*reinterpret_cast<const std::uint16_t*>(bytes));
        case 6:
            return static_cast<float>(*reinterpret_cast<const std::int8_t*>(bytes));
        case 7:
            return static_cast<float>(*reinterpret_cast<const std::uint8_t*>(bytes));
        default:
            return 0.0f;
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
        case FetchMode::SingleRow:
            visitor(points->getRow(rowId));
            break;
        case FetchMode::RowRange:
            points->visitRows(firstRow, rowCount, visitor);
            break;
        case FetchMode::RowList:
            points->visitRows(rowIds, visitor);
            break;
        case FetchMode::SingleColumn:
            points->visitColumns(columnId, 1, visitor);
            break;
        case FetchMode::ColumnRange:
            points->visitColumns(firstColumn, columnCount, visitor);
            break;
        case FetchMode::ColumnList:
            points->visitColumns(columnIds, visitor);
            break;
        case FetchMode::ColumnsForRows:
            points->visitColumns(firstColumn, columnCount, rowIds, visitor);
            break;
        case FetchMode::Range2D:
            points->visitRange(firstRow, rowCount, firstColumn, columnCount, visitor);
            break;
    }
}

namespace PointStreaming {

/** Rows per block. Smaller selections are read by fewer threads, because thread start-up would dominate. */
inline constexpr std::size_t kMinRowsPerBlock = 4096;

/** A block is read as one span (Range2D) when the span is at most this many times the number of selected rows. */
inline constexpr std::size_t kMaxSpanFactor = 2;

/** Average run length from which contiguous runs are read with Range2D instead of a row list. */
inline constexpr std::size_t kMinRowsPerRun = 32;

/**
 * Number of genes that can be read: the smaller of the gene names and the data width.
 * Gene lists must never index past the columns of the dataset.
 */
inline std::size_t usableGeneCount(std::size_t namedGenes, std::uint64_t dataDimensions)
{
    return std::min<std::size_t>(namedGenes, static_cast<std::size_t>(dataDimensions));
}

/** Splits ascending row ids into contiguous runs: each run is {first row, number of rows}. */
inline std::vector<std::pair<std::uint64_t, std::uint64_t>> contiguousRuns(const std::vector<std::uint64_t>& ascendingRows)
{
    std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
    for (std::size_t k = 0; k < ascendingRows.size();) {
        std::size_t end = k + 1;
        while (end < ascendingRows.size() && ascendingRows[end] == ascendingRows[end - 1] + 1)
            ++end;
        runs.emplace_back(ascendingRows[k], static_cast<std::uint64_t>(end - k));
        k = end;
    }
    return runs;
}

/**
 * The requested columns that fall inside one column segment (a view), with their index inside the view.
 * Cached while the segment does not change, so the mapping is computed once per segment layout, not per row.
 */
struct SegmentColumns {
    std::uint64_t start = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t width = 0;
    std::vector<std::uint64_t> relative; /** column index inside the view */
    std::vector<std::size_t>   slot;     /** index of the column in the requested column list */

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
    }
};

namespace detail {

template <typename T>
inline float toFloat(const T value) { return static_cast<float>(value); }

template <typename T>
inline void accumulateTypedSegment(const std::uint8_t* bytes, const std::uint32_t elementSizeBytes, const SegmentColumns& segment, double* sums, std::uint64_t* nonZero)
{
    const std::size_t count = segment.relative.size();
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

/**
 * Adds the values of the requested columns that lie in the view's segment to \p sums (indexed by requested column),
 * and counts non-zero values in \p nonZero when it is not null. The element type is decoded once per row.
 */
inline void accumulateSegment(const PointDataAccessView& view, const std::uint64_t row, const SegmentColumns& segment, double* sums, std::uint64_t* nonZero)
{
    if (segment.relative.empty())
        return;

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(view.rowData(row));
    const auto size = view.elementSizeBytes;

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
 * Reads the rows in \p rowIds in parallel blocks and reduces them into one state.
 *
 * - columnCount                                    number of columns requested, starting at column 0
 * - make()                                         creates an empty state (one per block)
 * - visit(state, rowPosition, view, row, offset)   called for every row of every column segment. rowPosition indexes
 *                                                  rowIds, row indexes the view, offset is the absolute column
 *                                                  position of the view's first column.
 * - merge(into, from)                              combines two states
 *
 * Rows are read in ascending order (sequential access), visit() still receives the position in rowIds.
 * Blocks are merged in block order, so the result is deterministic for a fixed thread count.
 * Throws std::out_of_range / std::runtime_error when the delivered views do not match the request.
 */
template <typename RowContainer, typename Make, typename Visit, typename Merge>
inline auto reduceRows(
    Points* points,
    const RowContainer& rowIds,
    int threads,
    std::uint64_t columnCount,
    Make&& make,
    Visit&& visit,
    Merge&& merge)
    -> decltype(make())
{
    using State = decltype(make());

    XSC_TRACE("PointStreaming::reduceRows");

    if (!points || rowIds.empty() || columnCount == 0)
        return make();

    const std::uint64_t dimensions = points->getNumDimensions();

    if (columnCount > dimensions) {
        throw std::out_of_range(
            "PointStreaming: requested columns exceed the number "
            "of dimensions of the dataset");
    }

    // Target the logical size of each 2D row block.
    // Four bytes per element is a conservative estimate for the
    // supported numeric types. The actual element size is available
    // in each delivered view.
    constexpr std::size_t kTargetReadBytes = 128ULL * 1024 * 1024;
    constexpr std::size_t kEstimatedElementBytes = 4;

    const std::size_t rowCount = rowIds.size();

    const std::size_t columns =
        static_cast<std::size_t>(columnCount);

    const std::size_t bytesPerRow =
        columns > std::numeric_limits<std::size_t>::max()
        / kEstimatedElementBytes
        ? std::numeric_limits<std::size_t>::max()
        : std::max<std::size_t>(
            1,
            columns * kEstimatedElementBytes);

    const std::size_t rowsPerBlock =
        std::max<std::size_t>(
            1,
            kTargetReadBytes / bytesPerRow);

    // Use enough blocks to keep each Range2D request close to the
    // target size. The worker count is limited separately below.
    const std::size_t blockCount =
        rowCount / rowsPerBlock +
        (rowCount % rowsPerBlock != 0 ? 1 : 0);

    const std::size_t workerCount =
        std::min<std::size_t>(
            blockCount,
            static_cast<std::size_t>(std::max(1, threads)));

    XSC_LOG(
        "reduceRows: rows",
        rowCount,
        "columns",
        columnCount,
        "dataset dimensions",
        dimensions,
        "target bytes",
        kTargetReadBytes,
        "estimated bytes per row",
        bytesPerRow,
        "rows per block",
        rowsPerBlock,
        "blocks",
        blockCount,
        "workers",
        workerCount);

    // Preserve the original position of every row ID, including
    // duplicates, so visit() receives the same rowPosition as before.
    std::vector<std::uint64_t> requested(
        rowIds.begin(), rowIds.end());

    std::vector<std::size_t> order(rowCount);
    std::iota(order.begin(), order.end(), std::size_t{ 0 });

    std::vector<std::uint64_t> rows;

    if (std::is_sorted(requested.begin(), requested.end())) {
        rows = std::move(requested);
    }
    else {
        std::sort(
            order.begin(),
            order.end(),
            [&requested](std::size_t a, std::size_t b) {
                return requested[a] < requested[b];
            });

        rows.resize(rowCount);

        for (std::size_t i = 0; i < rowCount; ++i)
            rows[i] = requested[order[i]];
    }

    // One independent reduction state per block. Blocks can execute
    // concurrently, and states are merged in ascending block order.
    std::vector<State> states;
    states.reserve(blockCount);

    for (std::size_t b = 0; b < blockCount; ++b)
        states.push_back(make());

    std::vector<std::exception_ptr> errors(blockCount);

    auto readBlock = [&](std::size_t b) {
        try {
            const std::size_t begin = b * rowsPerBlock;

            const std::size_t end =
                begin + std::min(rowsPerBlock, rowCount - begin);

            const std::vector<std::uint64_t> block(
                rows.begin() + begin,
                rows.begin() + end);

            if (block.empty())
                return;

            XSC_TRACE("PointStreaming::read block");

            auto deliverWith =
                [&](std::size_t& rowBase,
                    std::uint64_t& offset,
                    const std::uint64_t totalWidth) {
                        return PointDataAccessVisitor(
                            [&, totalWidth](
                                const PointDataAccessView& view) {
                                    if (view.columns == 0)
                                        return;

                                    if (offset + view.columns > totalWidth) {
                                        throw std::out_of_range(
                                            "PointStreaming: a delivered view "
                                            "exceeds the requested column range");
                                    }

                                    for (std::uint64_t row = 0;
                                        row < view.rows;
                                        ++row) {
                                        const std::size_t sorted =
                                            rowBase +
                                            static_cast<std::size_t>(row);

                                        if (sorted >= end) {
                                            throw std::out_of_range(
                                                "PointStreaming: more rows "
                                                "delivered than requested");
                                        }

                                        visit(
                                            states[b],
                                            order[sorted],
                                            view,
                                            row,
                                            offset);
                                    }

                                    offset += view.columns;

                                    // A complete column range finishes this
                                    // row batch. The next view batch begins
                                    // at column zero.
                                    if (offset == totalWidth) {
                                        offset = 0;
                                        rowBase +=
                                            static_cast<std::size_t>(view.rows);
                                    }
                            });
                };

            const std::uint64_t spanFirst = block.front();

            const std::uint64_t spanCount =
                block.back() - spanFirst + 1;

            const auto runs = contiguousRuns(block);

            // Strategy 1: one compact contiguous row span.
            if (spanCount <= kMaxSpanFactor * block.size()) {
                XSC_LOG(
                    "block",
                    b,
                    "path: span",
                    "rows",
                    block.size(),
                    "span",
                    spanCount,
                    "first",
                    spanFirst);

                const std::size_t spanSize =
                    static_cast<std::size_t>(spanCount);

                // Map each row in the requested span to its positions
                // in this block. This also handles duplicate row IDs.
                std::vector<std::size_t> rowOffsets(
                    spanSize + 1, 0);

                for (std::size_t i = 0; i < block.size(); ++i) {
                    const std::size_t relative =
                        static_cast<std::size_t>(
                            block[i] - spanFirst);

                    ++rowOffsets[relative + 1];
                }

                for (std::size_t i = 1;
                    i < rowOffsets.size();
                    ++i) {
                    rowOffsets[i] += rowOffsets[i - 1];
                }

                std::size_t blockPtr = begin;
                std::uint64_t blockFirst = spanFirst;
                std::uint64_t offset = 0;

                fetchPointData(
                    points,
                    FetchMode::Range2D,
                    PointDataAccessVisitor(
                        [&](const PointDataAccessView& view) {
                            if (view.columns == 0)
                                return;

                            if (offset + view.columns > columnCount) {
                                throw std::out_of_range(
                                    "PointStreaming: a delivered view "
                                    "exceeds the requested column range");
                            }

                            const bool lastSegment =
                                offset + view.columns == columnCount;

                            if (blockFirst < spanFirst) {
                                throw std::runtime_error(
                                    "PointStreaming: invalid span row offset");
                            }

                            const std::uint64_t baseOffset =
                                blockFirst - spanFirst;

                            if (baseOffset > spanCount ||
                                view.rows > spanCount - baseOffset) {
                                throw std::out_of_range(
                                    "PointStreaming: delivered rows exceed "
                                    "the requested span");
                            }

                            for (std::uint64_t r = 0;
                                r < view.rows;
                                ++r) {
                                const std::size_t relative =
                                    static_cast<std::size_t>(
                                        baseOffset + r);

                                const std::size_t first =
                                    rowOffsets[relative];

                                const std::size_t last =
                                    rowOffsets[relative + 1];

                                for (std::size_t q = first;
                                    q < last;
                                    ++q) {
                                    visit(
                                        states[b],
                                        order[begin + q],
                                        view,
                                        r,
                                        offset);
                                }
                            }

                            if (lastSegment) {
                                const std::uint64_t next =
                                    blockFirst + view.rows;

                                while (blockPtr < end &&
                                    rows[blockPtr] < next) {
                                    ++blockPtr;
                                }

                                blockFirst = next;
                                offset = 0;
                            }
                            else {
                                offset += view.columns;
                            }
                        }),
                    0,
                    spanFirst,
                    spanCount,
                    {},
                    0,
                    0,
                    columnCount);

                if (offset != 0 || blockPtr != end) {
                    throw std::runtime_error(
                        "PointStreaming: streamed views do not "
                        "cover the requested rows");
                }
            }

            // Strategy 2: sufficiently long contiguous runs.
            else if (
                !runs.empty() &&
                block.size() / runs.size() >= kMinRowsPerRun) {
                XSC_LOG(
                    "block",
                    b,
                    "path: runs",
                    "rows",
                    block.size(),
                    "runs",
                    runs.size());

                std::size_t cursor = begin;

                for (const auto& run : runs) {
                    std::size_t rowBase = cursor;
                    std::uint64_t offset = 0;

                    fetchPointData(
                        points,
                        FetchMode::Range2D,
                        deliverWith(rowBase, offset, columnCount),
                        0,
                        run.first,
                        run.second,
                        {},
                        0,
                        0,
                        columnCount);

                    if (offset != 0 ||
                        rowBase != cursor + run.second) {
                        throw std::runtime_error(
                            "PointStreaming: streamed views do not "
                            "cover the requested rows");
                    }

                    cursor +=
                        static_cast<std::size_t>(run.second);
                }
            }

            // Strategy 3: scattered rows.
            else {
                XSC_LOG(
                    "block",
                    b,
                    "path: row list",
                    "rows",
                    block.size(),
                    "span",
                    spanCount);

                std::size_t rowBase = begin;
                std::uint64_t offset = 0;

                fetchPointData(
                    points,
                    FetchMode::RowList,
                    deliverWith(rowBase, offset, dimensions),
                    0,
                    0,
                    0,
                    block,
                    0,
                    0,
                    0);

                if (offset != 0 || rowBase != end) {
                    throw std::runtime_error(
                        "PointStreaming: streamed views do not "
                        "cover the requested rows");
                }
            }
        }
        catch (...) {
            errors[b] = std::current_exception();
        }
        };

    // Schedule blocks dynamically, but never run more than workerCount
    // blocks concurrently. This avoids creating one thread per block.
    std::atomic<std::size_t> nextBlock{ 0 };

    auto worker = [&]() {
        while (true) {
            const std::size_t b =
                nextBlock.fetch_add(1, std::memory_order_relaxed);

            if (b >= blockCount)
                break;

            readBlock(b);
        }
        };

    std::vector<std::thread> workers;
    workers.reserve(workerCount > 0 ? workerCount - 1 : 0);

    try {
        for (std::size_t i = 1; i < workerCount; ++i)
            workers.emplace_back(worker);
    }
    catch (...) {
        // Ensure already-created threads finish before propagating
        // a thread creation error.
        worker();

        for (auto& thread : workers) {
            if (thread.joinable())
                thread.join();
        }

        throw;
    }

    worker();

    for (auto& thread : workers)
        thread.join();

    for (const auto& error : errors) {
        if (error)
            std::rethrow_exception(error);
    }

    for (std::size_t b = 1; b < blockCount; ++b)
        merge(states[0], states[b]);

    XSC_LOG("reduceRows: finished");

    return std::move(states[0]);
}

/** Per-column aggregates over a set of rows, accumulated in double precision. */
struct ColumnAggregates {
    std::vector<double>        sums;     /** sum of the values of each requested column */
    std::vector<std::uint64_t> nonZero;  /** number of rows with a value != 0 for each requested column */
    std::uint64_t              rows = 0; /** number of rows that were read */
};


/** Per-column aggregates over selected rows, accumulated in double precision. */
template <typename RowContainer>
inline ColumnAggregates aggregateColumns(
    Points* points,
    const RowContainer& rowIds,
    const std::vector<int>& columns,
    int threads = 1)
{
    XSC_TRACE("PointStreaming::aggregateColumns");

    ColumnAggregates result;
    result.sums.assign(columns.size(), 0.0);
    result.nonZero.assign(columns.size(), 0);

    if (!points || rowIds.empty() || columns.empty())
        return result;

    const std::uint64_t dimensions = points->getNumDimensions();

    // Validate before converting signed column IDs to unsigned IDs.
    for (const int column : columns) {
        if (column < 0 ||
            static_cast<std::uint64_t>(column) >= dimensions) {
            throw std::out_of_range(
                "PointStreaming: requested column is out of range");
        }
    }

    const std::vector<std::uint64_t> rows(
        rowIds.begin(), rowIds.end());

    const std::vector<std::uint64_t> columnIds(
        columns.begin(), columns.end());

    // Build sorted unique column IDs. Keep columnIds unchanged so
    // SegmentColumns can preserve original output positions and duplicates.
    std::vector<std::uint64_t> uniqueColumns = columnIds;

    std::sort(uniqueColumns.begin(), uniqueColumns.end());

    uniqueColumns.erase(
        std::unique(uniqueColumns.begin(), uniqueColumns.end()),
        uniqueColumns.end());

    // Split only at gaps between requested columns. No tile dimensions
    // or assumptions about the storage layout are required.
    const auto runs = contiguousRuns(uniqueColumns);

    struct SegmentState {
        SegmentColumns columns;
    };

    SegmentState segmentState;

    // Fetch only the selected rows and the requested column ranges.
    for (const auto& run : runs) {
        const std::uint64_t runStart = run.first;
        const std::uint64_t runWidth = run.second;

        std::size_t rowBase = 0;
        std::uint64_t offset = 0;

        fetchPointData(
            points,
            FetchMode::ColumnsForRows,
            PointDataAccessVisitor(
                [&](const PointDataAccessView& view) {
                    if (view.columns == 0)
                        return;

                    if (offset + view.columns > runWidth) {
                        throw std::out_of_range(
                            "PointStreaming: delivered view exceeds "
                            "the requested column run");
                    }

                    // Convert the view's run-relative offset to an
                    // absolute dataset column position.
                    const std::uint64_t absoluteStart =
                        runStart + offset;

                    segmentState.columns.update(
                        columnIds,
                        absoluteStart,
                        view.columns);

                    // The API is expected to deliver rows in the order
                    // supplied in rows, as fetchColumnValues assumes.
                    for (std::uint64_t row = 0;
                        row < view.rows;
                        ++row) {
                        if (rowBase + row >= rows.size()) {
                            throw std::out_of_range(
                                "PointStreaming: more rows delivered "
                                "than requested");
                        }

                        accumulateSegment(
                            view,
                            row,
                            segmentState.columns,
                            result.sums.data(),
                            result.nonZero.data());
                    }

                    offset += view.columns;

                    // The next row block starts at the beginning of
                    // this column run again.
                    if (offset == runWidth) {
                        offset = 0;
                        rowBase += view.rows;
                    }
                }),
            0,              // rowId
            0,              // firstRow
            0,              // rowCount
            rows,           // selected rows
            0,              // columnId
            runStart,       // firstColumn
            runWidth);      // columnCount

        if (offset != 0 || rowBase != rows.size()) {
            throw std::runtime_error(
                "PointStreaming: streamed views do not cover "
                "the requested rows and columns");
        }
    }

    result.rows = rows.size();

    return result;
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
                values[position] = valueAt(view, row, 0); // the view holds only this column: index 0
        }
    }, 0, 0, 0, rows, 0, columnId, 1);

    if (position != values.size())
        throw std::runtime_error("PointStreaming: streamed row count does not match the requested rows");

    return values;
}

} // namespace PointStreaming
