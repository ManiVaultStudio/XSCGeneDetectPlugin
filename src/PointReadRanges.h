#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <utility>

namespace PointStreaming {

enum class GroupedReadPath { PhysicalTiles, SparseRanges, DenseRows };
inline GroupedReadPath groupedReadPath(bool sparseColumns, bool eager, bool physicalTiles, bool detection) {
    if (physicalTiles) return GroupedReadPath::PhysicalTiles;
    return sparseColumns && !eager && !detection ? GroupedReadPath::SparseRanges : GroupedReadPath::DenseRows;
}

template<typename Indices>
std::uint64_t logicalToSourceRow(bool full, std::uint64_t row, std::uint64_t logicalCount,
    const Indices& indices, std::uint64_t rawCount)
{
    if (row >= logicalCount) throw std::out_of_range("Logical point row is out of bounds");
    const auto source = full ? row : static_cast<std::uint64_t>(indices.at(static_cast<std::size_t>(row)));
    if (source >= rawCount) throw std::out_of_range("Source point row is out of bounds");
    return source;
}

// Independent validation: job count must equal the physical tile rows actually
// touched by the sorted selection. Never assume the logged count is a constant.
template<typename RowAt>
void validateTileJobs(const std::vector<std::pair<std::size_t, std::size_t>>& jobs,
    std::size_t count, std::uint64_t tileRows, std::uint64_t totalRows, RowAt rowAt)
{
    if (tileRows == 0) throw std::invalid_argument("Zero tile-row height");
    std::size_t expected = 0;
    std::uint64_t previousRow = 0, previousTile = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto row = rowAt(i);
        if (row >= totalRows || (i && row < previousRow)) throw std::out_of_range("Invalid tile-job row order/range");
        const auto tile = row / tileRows;
        if (i == 0 || tile != previousTile) ++expected;
        previousRow = row; previousTile = tile;
    }
    if (jobs.size() != expected) throw std::runtime_error("Tile job count does not match selected tile rows");
    std::size_t covered = 0;
    for (const auto& job : jobs) {
        if (job.first != covered || job.second <= job.first || job.second > count ||
            rowAt(job.first) / tileRows != rowAt(job.second - 1) / tileRows)
            throw std::runtime_error("Tile jobs overlap, omit rows, or cross physical tile boundaries");
        covered = job.second;
    }
    if (covered != count) throw std::runtime_error("Tile jobs do not cover the selection");
}

// Keep every selected row from one physical tile row in the same job.
template <typename RowAt>
std::size_t tileReadEnd(std::size_t begin, std::size_t end, std::uint64_t tileRows, RowAt rowAt)
{
    const auto tileRow = rowAt(begin) / tileRows;
    std::size_t next = begin + 1;
    while (next < end && rowAt(next) / tileRows == tileRow)
        ++next;
    return next;
}

// Group sorted distinct rows without letting gaps turn a sparse selection into
// a full-data scan. Both the byte-derived row limit and read amplification apply.
template <typename RowAt>
std::size_t boundedReadEnd(std::size_t begin, std::size_t end, std::uint64_t maxRows,
                           std::uint64_t maxAmplification, RowAt rowAt)
{
    std::size_t next = begin + 1;
    while (next < end) {
        const auto span = rowAt(next) - rowAt(begin) + 1;
        if (span > maxRows || span > maxAmplification * (next - begin + 1))
            break;
        ++next;
    }
    return next;
}

} // namespace PointStreaming
