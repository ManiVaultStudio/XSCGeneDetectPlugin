#include "../src/ChartNumerics.h"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool near(double actual, double expected)
{
    return std::abs(actual - expected) < 1e-10;
}
}

int main()
{
    try {
        using ChartNumerics::linearQuantile;
        const std::vector<float> values{ 0, 2, 4, 10 };
        check(near(linearQuantile(values, 0.25), 1.5), "First quartile must interpolate");
        check(near(linearQuantile(values, 0.5), 3.0), "Even sample median must average middle observations");
        check(near(linearQuantile(values, 0.75), 5.5), "Third quartile must interpolate");
        check(std::isnan(linearQuantile(std::vector<float>{}, 0.5)), "Empty sample must be unavailable");

        // The largest row norm is in the smaller component's support.
        // Power iteration seeded with that row used to reverse the components.
        const std::vector<double> u{ 0.5, -0.5, 0.5, -0.5, 0, 0 };
        const std::vector<double> v{ 0, 0, 0, 0, std::sqrt(0.5), -std::sqrt(0.5) };
        auto first = u, second = v;
        for (auto& value : first) value *= std::sqrt(5.0);
        for (auto& value : second) value *= 2.0;
        const auto components = ChartNumerics::principalComponents({ first, second }, 6);
        check(components.valid, "PCA must converge on the known rank-two example");
        check(near(components.varianceFraction[0], 5.0 / 9.0), "Largest component must be first");
        check(near(components.varianceFraction[1], 4.0 / 9.0), "Second component must retain its variance");
        check(near(std::abs(std::inner_product(components.scores[0].begin(), components.scores[0].end(), u.begin(), 0.0)),
            std::sqrt(5.0)), "First scores must align with the leading eigenvector");
        check(near(std::inner_product(components.scores[0].begin(), components.scores[0].end(), components.scores[1].begin(), 0.0), 0.0),
            "Component scores must be orthogonal");
        const auto rankOne = ChartNumerics::principalComponents({ { -1.0, 0.0, 1.0 } }, 3);
        check(rankOne.valid && near(rankOne.varianceFraction[0], 1) && near(rankOne.varianceFraction[1], 0),
            "A rank-one matrix must explain all variance on PC1");
        check(!ChartNumerics::principalComponents({ { 0.0, 0.0 } }, 2).valid,
            "Zero variation must be unavailable");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
