#pragma once

#include <cstddef>
#include <cstdint>

namespace PointStreaming {

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
