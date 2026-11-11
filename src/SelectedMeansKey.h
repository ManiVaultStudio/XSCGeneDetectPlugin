#pragma once

#include <QString>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Identify cells together with their species; selection storage order is irrelevant.
template<class Rows, class Species>
QString selectedMeansKey(const QString& datasetId, const Rows& rows, const Species& species,
    std::size_t geneCount, std::size_t speciesCount)
{
    if (rows.size() != species.size()) throw std::invalid_argument("Selection/species size mismatch");
    std::vector<std::pair<std::uint64_t, int>> membership;
    membership.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i)
        membership.emplace_back(static_cast<std::uint64_t>(rows[i]), species[i]);
    if (!std::is_sorted(membership.begin(), membership.end())) std::sort(membership.begin(), membership.end());
    std::uint64_t hash = 1469598103934665603ull;
    const auto add = [&hash](std::uint64_t value) {
        hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    };
    for (const auto& item : membership) { add(item.first); add(static_cast<std::uint32_t>(item.second)); }
    return datasetId + QLatin1Char('|') + QString::number(hash, 16) + QLatin1Char('|')
        + QString::number(geneCount) + QLatin1Char('|') + QString::number(speciesCount)
        + QLatin1Char('|') + QString::number(rows.size());
}
