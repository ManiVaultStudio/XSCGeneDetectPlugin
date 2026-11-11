#include "../src/PointReadRanges.h"
#include <numeric>
#include <iostream>

auto jobsFor(const std::vector<unsigned>& rows, unsigned height, unsigned total)
{
    std::vector<std::pair<std::size_t, std::size_t>> jobs;
    const auto at = [&](std::size_t i) { return rows[i]; };
    for (std::size_t first = 0; first < rows.size();) {
        const auto end = PointStreaming::tileReadEnd(first, rows.size(), height, at);
        jobs.emplace_back(first, end); first = end;
    }
    PointStreaming::validateTileJobs(jobs, rows.size(), height, total, at);
    return jobs;
}
int main()
{
    using Path = PointStreaming::GroupedReadPath;
    if (PointStreaming::groupedReadPath(true, false, true, false) != Path::PhysicalTiles)
        throw std::runtime_error("Sparse trait requests on tiled data must use one physical scan, not repeated archive range reads");
    if (PointStreaming::groupedReadPath(true, false, false, true) != Path::DenseRows)
        throw std::runtime_error("DE detection counts must not use the sums-only sparse reader");
    if (PointStreaming::groupedReadPath(true, false, false, false) != Path::SparseRanges)
        throw std::runtime_error("Non-tiled sparse compatibility path must remain available");
    const std::vector<unsigned> subset{2,0,2};
    const std::vector<float> raw{10,20,30};
    const std::vector<unsigned> requested{0,2,1};
    std::vector<float> mapped;
    for (const auto row : requested) mapped.push_back(raw[PointStreaming::logicalToSourceRow(false, row, 3, subset, 3)]);
    if (mapped != std::vector<float>({30,30,10})) throw std::runtime_error("Dense subset values must preserve logical order and duplicates");
    std::vector<unsigned> rows(2326579);
    std::iota(rows.begin(), rows.end(), 0);
    const auto full = jobsFor(rows, 284, rows.size());
    if (full.size() != 8193 || full.back().second - full.back().first != 51)
        throw std::runtime_error("Wrong job count or final boundary tile for the reported dataset");
    if (jobsFor({0,0,283,284,2326578}, 284, rows.size()).size() != 3)
        throw std::runtime_error("Sparse selections must only schedule touched tile rows");
    if (jobsFor(rows, 8192, rows.size()).size() != 285)
        throw std::runtime_error("Changed geometry must change job count");
    auto broken = full; broken.pop_back();
    bool rejected = false;
    try { PointStreaming::validateTileJobs(broken, rows.size(), 284, rows.size(), [&](std::size_t i) { return rows[i]; }); }
    catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Missing jobs must be rejected");
    std::cout << "PASS: reported 8193 jobs, boundary rows, sparse/duplicate rows, changed geometry, invalid schedules\n";
}
