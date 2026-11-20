#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace ChartNumerics {

template<class Value>
double linearQuantile(const std::vector<Value>& sorted, double fraction)
{
    if (sorted.empty() || !std::isfinite(fraction))
        return std::numeric_limits<double>::quiet_NaN();
    const double index = std::clamp(fraction, 0.0, 1.0) * (sorted.size() - 1);
    const auto low = static_cast<size_t>(index);
    const auto high = std::min(low + 1, sorted.size() - 1);
    return static_cast<double>(sorted[low])
        + (static_cast<double>(sorted[high]) - sorted[low]) * (index - low);
}

struct PrincipalComponents {
    std::array<std::vector<double>, 2> scores;
    std::array<double, 2> varianceFraction{};
    bool valid = false;
};

// Columns contain centered, standardized gene expression. Decompose the
// symmetric species Gram matrix so a starting vector cannot miss a component.
inline PrincipalComponents principalComponents(const std::vector<std::vector<double>>& columns, size_t species)
{
    PrincipalComponents result;
    if (columns.empty() || species < 2) return result;

    std::vector<std::vector<double>> gram(species, std::vector<double>(species));
    for (const auto& column : columns) {
        if (column.size() != species
            || !std::all_of(column.begin(), column.end(), [](double value) { return std::isfinite(value); }))
            return result;
        for (size_t i = 0; i < species; ++i)
            for (size_t j = i; j < species; ++j)
                gram[i][j] += column[i] * column[j];
    }
    double trace = 0.0;
    for (size_t i = 0; i < species; ++i) {
        trace += gram[i][i];
        for (size_t j = i + 1; j < species; ++j)
            gram[j][i] = gram[i][j];
    }
    if (!(trace > 0.0) || !std::isfinite(trace)) return result;

    std::vector<std::vector<double>> vectors(species, std::vector<double>(species));
    for (size_t i = 0; i < species; ++i) vectors[i][i] = 1.0;
    const double tolerance = std::numeric_limits<double>::epsilon() * trace * 8;
    bool converged = false;
    for (int sweep = 0; sweep < 80; ++sweep) {
        converged = true;
        for (size_t p = 0; p < species; ++p) {
            for (size_t q = p + 1; q < species; ++q) {
                const double offDiagonal = gram[p][q];
                if (std::abs(offDiagonal) <= tolerance) continue;
                converged = false;
                const double tau = (gram[q][q] - gram[p][p]) / (2 * offDiagonal);
                const double t = std::copysign(1.0 / (std::abs(tau) + std::hypot(1.0, tau)), tau);
                const double cosine = 1.0 / std::hypot(1.0, t);
                const double sine = t * cosine;
                gram[p][p] -= t * offDiagonal;
                gram[q][q] += t * offDiagonal;
                gram[q][p] = gram[p][q] = 0.0;
                for (size_t i = 0; i < species; ++i) {
                    if (i != p && i != q) {
                        const double a = gram[i][p], b = gram[i][q];
                        gram[p][i] = gram[i][p] = cosine * a - sine * b;
                        gram[q][i] = gram[i][q] = sine * a + cosine * b;
                    }
                    const double a = vectors[i][p], b = vectors[i][q];
                    vectors[i][p] = cosine * a - sine * b;
                    vectors[i][q] = sine * a + cosine * b;
                }
            }
        }
        if (converged) break;
    }
    if (!converged) return result;

    std::vector<size_t> order(species);
    std::iota(order.begin(), order.end(), 0);
    std::partial_sort(order.begin(), order.begin() + 2, order.end(),
        [&](size_t a, size_t b) { return gram[a][a] > gram[b][b]; });
    for (size_t k = 0; k < 2; ++k) {
        const auto component = order[k];
        const double eigenvalue = gram[component][component] > tolerance ? gram[component][component] : 0.0;
        result.varianceFraction[k] = eigenvalue / trace;
        const double scale = std::sqrt(eigenvalue);
        for (size_t i = 0; i < species; ++i) result.scores[k].push_back(scale * vectors[i][component]);
    }
    result.valid = true;
    return result;
}

} // namespace ChartNumerics
