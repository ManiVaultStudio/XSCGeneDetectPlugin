#include "SettingsAction.h"
#include "XSCGeneDetectPlugin.h"
#include<string>  
#include <QFileDialog>
#include <QFileInfo>
#include <QMap>
#include <QDateTime>
#include <QRegularExpression>
#include <QTextStream>
#include <QPageLayout>
#include <QWebEngineView>
#include <XSCTreeData/XSCTreeData.h>
#include <numeric>   // for std::reduce
//#include "lib/Distance/annoylib.h"
//#include "lib/Distance/kissrandom.h"
#include <QtConcurrent>
//#include "lib/JSONnlohmann/json.hpp"
//#include "lib/Clustering/fastcluster.h"
//#include "lib/NewickComparator/newick_comparator.h" //https://github.com/MaciejSurowiec/Maximum_agreement_subtree_problem
#include <sstream>
#include <stack>
#include <algorithm> // for std::find
#include <mutex>
#include <unordered_set>
#include <iostream>
#include <map>
#include <string>
#include <chrono>
#include <cmath> // Include for std::log
#include <climits>
#include <random>
#include <QMutex>
#include <QThread>
#include <QThreadPool>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QMutexLocker>
#include <limits>
#include <vector>
#include <thread>
#include <unordered_map>
#include <QApplication>
#include <QPalette>
#ifdef _WIN32
#include <execution>
#endif
using namespace mv;
using namespace mv::gui;

std::map<std::string, std::chrono::high_resolution_clock::time_point> timers;

// Function to sort by count (descending)
bool sortByCount(const ClusterOrderContainer& a, const ClusterOrderContainer& b) {
    return a.count > b.count;
}

// Function to sort by name (ascending)
bool sortByName(const ClusterOrderContainer& a, const ClusterOrderContainer& b) {
    return a.name < b.name;
}

// Prepare a map for custom list sorting
std::unordered_map<QString, int> prepareCustomSortMap(const std::vector<QString>& customOrder) {
    std::unordered_map<QString, int> sortOrderMap;
    for (size_t i = 0; i < customOrder.size(); ++i) {
        sortOrderMap[customOrder[i]] = static_cast<int>(i);
    }
    return sortOrderMap;
}

// Function to sort by custom list
bool sortByCustomList(const ClusterOrderContainer& a, const ClusterOrderContainer& b,
    const std::unordered_map<QString, int>& sortOrderMap) {
    auto posA = sortOrderMap.find(a.name);
    auto posB = sortOrderMap.find(b.name);
    int indexA = (posA != sortOrderMap.end()) ? posA->second : INT_MAX;
    int indexB = (posB != sortOrderMap.end()) ? posB->second : INT_MAX;
    return indexA < indexB;
}

Stats combineStatisticsSingle(const StatisticsSingle& selected, const StatisticsSingle& nonSelected, const int topAbundance, const int middleAbundance, const int countAbundanceNumerator/*, const StatisticsSingle& allSelected*/) {
    Stats combinedStats;
    combinedStats.meanSelected = selected.meanVal;
    combinedStats.countSelected = selected.countVal;

    //combinedStats.meanAll = allSelected.meanVal;
    //combinedStats.countAll = allSelected.countVal;

    combinedStats.meanNonSelected = nonSelected.meanVal;
    combinedStats.countNonSelected = nonSelected.countVal;

    combinedStats.abundanceTop = topAbundance;
    combinedStats.abundanceMiddle = middleAbundance;
    combinedStats.countAbundanceNumerator = countAbundanceNumerator;
    return combinedStats;
}

StatisticsSingle calculateStatistics(const std::vector<float>& data) {
    const int count = static_cast<int>(data.size());
    if (count == 0) {
        return { 0.0f, 0 }; // Return early if data is empty to avoid division by zero
    }

    float sum = 0.0;
#ifdef _WIN32
    sum = std::reduce(std::execution::par, data.begin(), data.end(), 0.0f);
#else
    sum = std::reduce(data.begin(), data.end(), 0.0f);
#endif
    //sum= std::accumulate(v.begin(), v.end(), 0.0);
    float mean = std::round((sum / count) * 100.0f) / 100.0f;

    return { mean, count };
}
float calculateMean(const std::vector<float>& v) {
    if (v.empty())
        return 0.0f;


#ifdef _WIN32
    float sum = std::reduce(std::execution::par, v.begin(), v.end(), 0.0f);
#else
    float sum = std::reduce(v.begin(), v.end(), 0.0f);
#endif
    //float sum= std::accumulate(v.begin(), v.end(), 0.0);


    return sum / static_cast<float>(v.size());

}


void startCodeTimer(const std::string& message) {
    timers[message] = std::chrono::high_resolution_clock::now();
    std::cout << "Timer started for: " << message << std::endl;
}

void stopCodeTimer(const std::string& message) {
    auto search = timers.find(message);
    if (search != timers.end()) {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - search->second;
        std::cout << "Timer stopped for: " << message << ". Elapsed time: " << elapsed.count() << "s\n";
        timers.erase(search); // Remove the timer from the map
    }
    else {
        std::cout << "Timer not found for: " << message << ". Ensure the timer was started with the exact same message string." << std::endl;
    }
}


/*
float calculateMeanLogTransformed(const std::vector<float>& v) {
    if (v.empty())
        return 0.0f;

    // Use a lambda to filter and log-transform positive values only
    auto logTransform = [](float value) -> float {
        return value > 0.0f ? std::log(value) : 0.0f;
        };

    // Count how many positive values are present
    size_t positiveCount = std::count_if(v.begin(), v.end(), [](float value) { return value > 0.0f; });

    // If there are no positive values, return 0 to avoid division by zero
    if (positiveCount == 0)
        return 0.0f;



    #ifdef _WIN32
    float sum = std::transform_reduce(std::execution::par, v.begin(), v.end(), 0.0f, std::plus<>(), logTransform);
    #else
    float sum = std::transform_reduce(v.begin(), v.end(), 0.0f, std::plus<>(), logTransform);
    #endif

    return sum / static_cast<float>(positiveCount);
}
*/
/*
std::string jsonToNewick(const nlohmann::json& node, const std::vector<QString>& species) {
    std::string newick;
    if (node.contains("children")) {
        newick += "(";
        for (const auto& child : node["children"]) {
            newick += jsonToNewick(child, species) + ",";
        }
        newick = newick.substr(0, newick.size() - 1);  // Remove trailing comma
        newick += ")";
    }
    if (node.contains("name")) {
        std::string nodeName = node["name"].get<std::string>();
        auto it = std::find_if(species.begin(), species.end(), [&nodeName](const QString& str) {
            return str.compare(QString::fromStdString(nodeName)) == 0;
            });
        if (it != species.end()) {
            newick += std::to_string(std::distance(species.begin(), it) + 1);  // Indices start from 1
        }
    }
    return newick;
}
*/
bool areSameIgnoreOrder(const QStringList& list1, const QStringList& list2) {
    if (list1.size() != list2.size()) {
        return false;
    }

    QHash<QString, int> list1Counts;
    for (const auto& item : list1) {
        ++list1Counts[item];
    }

    for (const auto& item : list2) {
        if (!list1Counts.contains(item) || --list1Counts[item] < 0) {
            return false;
        }
    }

    return true;
}



int findIndex(const std::vector<std::seed_seq::result_type>& vec, int value) {
    auto it = std::find(vec.begin(), vec.end(), value);
    return (it != vec.end()) ? static_cast<int>(std::distance(vec.begin(), it)) : -1;
}

namespace {

    constexpr int kGeneChunkSize = 256;

    std::unordered_set<QString> toStringSet(const QStringList& values) {
        std::unordered_set<QString> result;
        result.reserve(values.size());
        for (const auto& value : values) {
            result.insert(value);
        }
        return result;
    }

    template <typename LeftContainer, typename RightContainer>
    std::vector<int> intersectSortedIndicesToInt(const LeftContainer& leftIndices, const RightContainer& rightIndices) {
        std::vector<int> result;
        result.reserve(std::min(leftIndices.size(), rightIndices.size()));

        auto leftIt = leftIndices.begin();
        auto rightIt = rightIndices.begin();

        while (leftIt != leftIndices.end() && rightIt != rightIndices.end()) {
            const int leftValue = static_cast<int>(*leftIt);
            const int rightValue = static_cast<int>(*rightIt);

            if (leftValue < rightValue) {
                ++leftIt;
            }
            else if (rightValue < leftValue) {
                ++rightIt;
            }
            else {
                result.push_back(leftValue);
                ++leftIt;
                ++rightIt;
            }
        }

        return result;
    }

    template <typename PointIndexContainer, typename Callback>
    void computeChunkedGeneMeans(const mv::Dataset<Points>& pointDataset, const std::vector<int>& allGeneIndices, const PointIndexContainer& pointIndices, Callback&& callback) {
        if (!pointDataset.isValid() || allGeneIndices.empty() || pointIndices.empty()) {
            return;
        }

        // Large selections are faster when we fetch a block of genes once and
        // aggregate the point-major buffer locally instead of calling into the
        // dataset backend once per gene.
        std::vector<int> geneChunkIndices;
        geneChunkIndices.reserve(std::min<int>(kGeneChunkSize, static_cast<int>(allGeneIndices.size())));

        std::vector<float> chunkData;
        std::vector<float> chunkMeans;

        const int pointCount = static_cast<int>(pointIndices.size());

        for (int start = 0; start < static_cast<int>(allGeneIndices.size()); start += kGeneChunkSize) {
            const int chunkSize = std::min(kGeneChunkSize, static_cast<int>(allGeneIndices.size()) - start);

            geneChunkIndices.assign(allGeneIndices.begin() + start, allGeneIndices.begin() + start + chunkSize);
            chunkData.assign(static_cast<size_t>(pointCount) * static_cast<size_t>(chunkSize), 0.0f);
            chunkMeans.assign(chunkSize, 0.0f);

            pointDataset->populateDataForDimensions(chunkData, geneChunkIndices, pointIndices);

            for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
                const int rowOffset = pointIndex * chunkSize;
                for (int geneOffset = 0; geneOffset < chunkSize; ++geneOffset) {
                    chunkMeans[geneOffset] += chunkData[rowOffset + geneOffset];
                }
            }

            const float inversePointCount = 1.0f / static_cast<float>(pointCount);
            for (float& meanValue : chunkMeans) {
                meanValue *= inversePointCount;
            }

            callback(start, geneChunkIndices, chunkMeans);
        }
    }

    /**
     * Number of worker threads that keeps the temporary expression buffers within a fixed memory budget.
     * Each worker reading a gene chunk holds (points * kGeneChunkSize * 4 bytes), so very large cell sets must not
     * be processed by every core at once.
     * @param pointsPerWorker number of cells a single worker reads at a time
     */
    inline int memorySafeThreadCount(size_t pointsPerWorker) {
        constexpr double kBudgetBytes = 1024.0 * 1024.0 * 1024.0; // ~1 GiB of scratch buffers
        const double bytesPerWorker = static_cast<double>(std::max<size_t>(pointsPerWorker, 1)) * kGeneChunkSize * sizeof(float);
        const int idealThreads = std::max(1, QThread::idealThreadCount());
        const int budgetThreads = static_cast<int>(std::max(1.0, kBudgetBytes / bytesPerWorker));
        return std::max(1, std::min(idealThreads, budgetThreads));
    }

    /**
     * Fill the trait correlation cell of the gene table: correlation (display + sorting), p-value, FDR, the other statistics
     * and a tooltip that explains how far the number can be trusted.
     */
    inline void setTraitCorrelationItemData(QStandardItem* item, double value, double p, double q, const GeneTraitExtras& extras, int sampleSize, const QString& methodNote) {
        item->setData(value, Qt::DisplayRole);
        item->setData(value, Qt::UserRole);
        item->setData(p, kTraitPValueRole);
        item->setData(q, kTraitFdrRole);
        item->setData(QVariantList{ sampleSize, extras.ciLow, extras.ciHigh, extras.looMin, extras.looMax, extras.overall }, kTraitStatsRole);

        QString tooltip = methodNote.isEmpty() ? QString("Trait correlation") : methodNote;
        tooltip += QString("\nValue shown = %1").arg(value, 0, 'f', 3);
        if (std::isfinite(extras.overall)) {
            tooltip += QString("\nOverall correlation of all species = %1").arg(extras.overall, 0, 'f', 3);
        }
        if (std::isfinite(extras.ciLow) && std::isfinite(extras.ciHigh)) {
            tooltip += QString("\n95% confidence interval of the correlation: [%1, %2]").arg(extras.ciLow, 0, 'f', 2).arg(extras.ciHigh, 0, 'f', 2);
        }
        tooltip += QString("\np-value = %1\nFDR (Benjamini-Hochberg) = %2").arg(p, 0, 'g', 3).arg(q, 0, 'g', 3);
        if (std::isfinite(extras.looMin) && std::isfinite(extras.looMax)) {
            tooltip += QString("\nLeaving one species out: [%1, %2]").arg(extras.looMin, 0, 'f', 2).arg(extras.looMax, 0, 'f', 2);
            if (extras.looMin < 0.0f && extras.looMax > 0.0f) {
                tooltip += "\nWarning: the sign changes when a single species is left out";
            }
        }
        if (sampleSize > 0 && sampleSize < 5) {
            tooltip += QString("\nWarning: only %1 species - interpret with care").arg(sampleSize);
        }
        item->setData(tooltip, Qt::ToolTipRole);
    }

    // ----------------------------------------------------------------------------------------------------------
    //  Trait correlation kernel (plain C++, no Qt types so it can be unit tested in isolation)
    // ----------------------------------------------------------------------------------------------------------

    /** Minimum number of species (with trait values) in the comparison: the standardisation needs a spread of trait values. */
    constexpr size_t kMinSpeciesForTraitCorrelation = 2;

    /** Half-open range [begin, end) of gene indices processed by one worker. */
    struct GeneRange {
        size_t begin = 0;
        size_t end = 0;
    };

    /** Split [0, count) into at most \p parts contiguous, non-empty ranges. */
    inline std::vector<GeneRange> splitGeneRanges(size_t count, size_t parts) {
        std::vector<GeneRange> ranges;
        if (count == 0 || parts == 0) {
            return ranges;
        }
        parts = std::min(parts, count);
        const size_t base = count / parts;
        const size_t remainder = count % parts;
        size_t begin = 0;
        for (size_t part = 0; part < parts; ++part) {
            const size_t length = base + (part < remainder ? 1 : 0);
            ranges.push_back({ begin, begin + length });
            begin += length;
        }
        return ranges;
    }

    /**
     * Pearson correlation between the expression of every gene in \p range (one value per species) and the trait.
     * @param speciesGeneMeans one entry per species, each holding the mean expression of every gene
     * @param traitValues      trait value per species (same order as speciesGeneMeans)
     * @param correlations     output, one value per gene; only the entries inside \p range are written
     * Genes or traits without variance get a correlation of 0 (instead of NaN).
     */
    inline void computePearsonCorrelations(const std::vector<const std::vector<float>*>& speciesGeneMeans,
        const std::vector<double>& traitValues,
        const GeneRange& range,
        std::vector<float>& correlations) {
        const size_t speciesCount = traitValues.size();
        if (speciesCount == 0 || speciesGeneMeans.size() != speciesCount) {
            return;
        }

        double traitMean = 0.0;
        for (const double value : traitValues) {
            traitMean += value;
        }
        traitMean /= static_cast<double>(speciesCount);

        double traitVariance = 0.0;
        for (const double value : traitValues) {
            traitVariance += (value - traitMean) * (value - traitMean);
        }

        for (size_t gene = range.begin; gene < range.end; ++gene) {
            double geneMean = 0.0;
            for (size_t species = 0; species < speciesCount; ++species) {
                geneMean += static_cast<double>((*speciesGeneMeans[species])[gene]);
            }
            geneMean /= static_cast<double>(speciesCount);

            double covariance = 0.0;
            double geneVariance = 0.0;
            for (size_t species = 0; species < speciesCount; ++species) {
                const double geneDelta = static_cast<double>((*speciesGeneMeans[species])[gene]) - geneMean;
                covariance += geneDelta * (traitValues[species] - traitMean);
                geneVariance += geneDelta * geneDelta;
            }

            const double denominator = std::sqrt(geneVariance * traitVariance);
            double correlation = denominator > 0.0 ? covariance / denominator : 0.0;
            correlation = std::max(-1.0, std::min(1.0, correlation));
            correlations[gene] = static_cast<float>(correlation);
        }
    }

    /** How the means of several selected cell types are combined into one value per species. */
    enum class TraitPooling {
        PoolCells,           /** all cells of the selected cell types pooled: cell types with more cells weigh more */
        EqualCellTypeWeight  /** every selected cell type (with cells in the species) weighs the same */
    };

    /** Mean expression of one cell type in one species: number of cells and the mean of every gene (null/empty when no cells). */
    struct CellTypeMeans {
        size_t cellCount = 0;
        const std::vector<float>* means = nullptr;
    };

    /**
     * Combine the per cell type means of ONE species into one mean per gene for the genes in \p range.
     *  PoolCells           : sum(n_c * mean_c) / sum(n_c)   == mean over the pooled cells (exact, no cell re-reading needed)
     *  EqualCellTypeWeight : mean of mean_c over the cell types that have cells
     * Species without any cell get 0.
     */
    inline void combineCellTypeMeans(const std::vector<CellTypeMeans>& parts, TraitPooling mode, const GeneRange& range, std::vector<float>& out) {
        double totalCells = 0.0;
        size_t typesWithCells = 0;
        for (const auto& part : parts) {
            if (part.cellCount > 0 && part.means) {
                totalCells += static_cast<double>(part.cellCount);
                ++typesWithCells;
            }
        }

        for (size_t gene = range.begin; gene < range.end; ++gene) {
            if (typesWithCells == 0) {
                out[gene] = 0.0f;
                continue;
            }
            double sum = 0.0;
            for (const auto& part : parts) {
                if (part.cellCount == 0 || !part.means) {
                    continue;
                }
                const double weight = (mode == TraitPooling::PoolCells) ? static_cast<double>(part.cellCount) : 1.0;
                sum += weight * static_cast<double>((*part.means)[gene]);
            }
            const double divisor = (mode == TraitPooling::PoolCells) ? totalCells : static_cast<double>(typesWithCells);
            out[gene] = static_cast<float>(sum / divisor);
        }
    }

    /** Continued fraction of the incomplete beta function (modified Lentz). */
    inline double incompleteBetaContinuedFraction(double a, double b, double x) {
        constexpr int kMaxIterations = 400;
        constexpr double kEpsilon = 3.0e-16;
        constexpr double kTiny = 1.0e-300;

        const double qab = a + b;
        const double qap = a + 1.0;
        const double qam = a - 1.0;
        double c = 1.0;
        double d = 1.0 - qab * x / qap;
        if (std::fabs(d) < kTiny) d = kTiny;
        d = 1.0 / d;
        double h = d;
        for (int m = 1; m <= kMaxIterations; ++m) {
            const int m2 = 2 * m;
            double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
            d = 1.0 + aa * d;
            if (std::fabs(d) < kTiny) d = kTiny;
            c = 1.0 + aa / c;
            if (std::fabs(c) < kTiny) c = kTiny;
            d = 1.0 / d;
            h *= d * c;
            aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
            d = 1.0 + aa * d;
            if (std::fabs(d) < kTiny) d = kTiny;
            c = 1.0 + aa / c;
            if (std::fabs(c) < kTiny) c = kTiny;
            d = 1.0 / d;
            const double delta = d * c;
            h *= delta;
            if (std::fabs(delta - 1.0) < kEpsilon) {
                break;
            }
        }
        return h;
    }

    /** Regularised incomplete beta function I_x(a, b). */
    inline double regularizedIncompleteBeta(double a, double b, double x) {
        if (x <= 0.0) return 0.0;
        if (x >= 1.0) return 1.0;
        const double front = std::exp(std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b) + a * std::log(x) + b * std::log1p(-x));
        if (x < (a + 1.0) / (a + b + 2.0)) {
            return front * incompleteBetaContinuedFraction(a, b, x) / a;
        }
        return 1.0 - front * incompleteBetaContinuedFraction(b, a, 1.0 - x) / b;
    }

    /**
     * Two-sided p-value of a Pearson correlation \p r computed from \p sampleCount species
     * (Student t with sampleCount - 2 degrees of freedom). Fewer than 3 species carry no significance information -> 1.
     */
    inline double pearsonPValue(double r, size_t sampleCount) {
        if (sampleCount < 3) {
            return 1.0;
        }
        r = std::max(-1.0, std::min(1.0, r));
        if (std::fabs(r) >= 1.0) {
            return 0.0;
        }
        const double degreesOfFreedom = static_cast<double>(sampleCount) - 2.0;
        const double tSquared = r * r * degreesOfFreedom / (1.0 - r * r);
        const double p = regularizedIncompleteBeta(degreesOfFreedom / 2.0, 0.5, degreesOfFreedom / (degreesOfFreedom + tSquared));
        return std::max(0.0, std::min(1.0, p));
    }

    /** Benjamini-Hochberg false discovery rate (q-values) for a vector of p-values. */
    inline std::vector<float> benjaminiHochberg(const std::vector<float>& pValues) {
        const size_t count = pValues.size();
        std::vector<float> qValues(count, 1.0f);
        if (count == 0) {
            return qValues;
        }

        std::vector<size_t> order(count);
        for (size_t i = 0; i < count; ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&pValues](size_t a, size_t b) { return pValues[a] < pValues[b]; });

        // walk from the largest p-value down, keeping the running minimum of p * m / rank
        double runningMinimum = 1.0;
        for (size_t rank = count; rank >= 1; --rank) {
            const size_t index = order[rank - 1];
            const double adjusted = static_cast<double>(pValues[index]) * static_cast<double>(count) / static_cast<double>(rank);
            runningMinimum = std::min(runningMinimum, adjusted);
            qValues[index] = static_cast<float>(std::min(1.0, runningMinimum));
        }
        return qValues;
    }

    // ----------------------------------------------------------------------------------------------------------
    //  Correlation methods, confidence interval, phylogenetic independent contrasts, leave-one-species-out
    // ----------------------------------------------------------------------------------------------------------

    /** Correlation method used for the gene/trait statistics. */
    enum class TraitMethod {
        Pearson,       /** linear correlation of the mean expression with the trait */
        Spearman,      /** rank correlation: robust to outliers and to skewed expression distributions */
        Phylogenetic   /** Pearson on phylogenetically independent contrasts: corrects for the shared ancestry of the species */
    };

    /** Average ranks (ties share the mean rank), 1-based. */
    inline std::vector<double> averageRanks(const std::vector<double>& values) {
        const size_t count = values.size();
        std::vector<size_t> order(count);
        for (size_t i = 0; i < count; ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&values](size_t a, size_t b) { return values[a] < values[b]; });

        std::vector<double> ranks(count, 0.0);
        size_t start = 0;
        while (start < count) {
            size_t end = start;
            while (end + 1 < count && values[order[end + 1]] == values[order[start]]) {
                ++end;
            }
            const double averageRank = (static_cast<double>(start) + static_cast<double>(end)) / 2.0 + 1.0;
            for (size_t k = start; k <= end; ++k) {
                ranks[order[k]] = averageRank;
            }
            start = end + 1;
        }
        return ranks;
    }

    /** Plain Pearson correlation of two equally long vectors (0 when one of them has no variance). */
    inline double pearsonOfVectors(const std::vector<double>& a, const std::vector<double>& b) {
        const size_t count = a.size();
        if (count < 2 || b.size() != count) {
            return 0.0;
        }
        double meanA = 0.0, meanB = 0.0;
        for (size_t i = 0; i < count; ++i) {
            meanA += a[i];
            meanB += b[i];
        }
        meanA /= static_cast<double>(count);
        meanB /= static_cast<double>(count);
        double covariance = 0.0, varianceA = 0.0, varianceB = 0.0;
        for (size_t i = 0; i < count; ++i) {
            covariance += (a[i] - meanA) * (b[i] - meanB);
            varianceA += (a[i] - meanA) * (a[i] - meanA);
            varianceB += (b[i] - meanB) * (b[i] - meanB);
        }
        const double denominator = std::sqrt(varianceA * varianceB);
        return denominator > 0.0 ? std::max(-1.0, std::min(1.0, covariance / denominator)) : 0.0;
    }

    /** Correlation of two vectors through the origin (contrasts have no intercept): sum(ab) / sqrt(sum(a^2) sum(b^2)). */
    inline double correlationThroughOrigin(const std::vector<double>& a, const std::vector<double>& b) {
        double sumAB = 0.0, sumAA = 0.0, sumBB = 0.0;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
            sumAB += a[i] * b[i];
            sumAA += a[i] * a[i];
            sumBB += b[i] * b[i];
        }
        const double denominator = std::sqrt(sumAA * sumBB);
        return denominator > 0.0 ? std::max(-1.0, std::min(1.0, sumAB / denominator)) : 0.0;
    }

    /** 95% confidence interval of a correlation (Fisher z transformation). NaN for fewer than 4 observations. */
    inline void fisherConfidenceInterval(double r, size_t sampleCount, double& low, double& high) {
        constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
        if (sampleCount < 4) {
            low = kNaN;
            high = kNaN;
            return;
        }
        const double clamped = std::max(-0.999999, std::min(0.999999, r));
        const double z = std::atanh(clamped);
        const double standardError = 1.0 / std::sqrt(static_cast<double>(sampleCount) - 3.0);
        constexpr double kZ975 = 1.959963984540054;
        low = std::tanh(z - kZ975 * standardError);
        high = std::tanh(z + kZ975 * standardError);
    }

    // ---- phylogenetic independent contrasts (Felsenstein 1985) ----

    /** Branches shorter than this are treated as this long: zero-length branches would divide by zero. */
    constexpr double kPicMinBranchLength = 1.0e-9;

    /** Node of the species tree. Leaves carry the index of their species (-1: leaf that is not part of the comparison). */
    struct PicNode {
        int leafIndex = -1;
        std::vector<int> children;
        double branchLength = 0.0;
    };

    /** One contrast: combines two (partial) subtrees. All factors are precomputed so that applying it is cheap. */
    struct PicOp {
        int left = 0;            /** slot of the left subtree value */
        int right = 0;           /** slot of the right subtree value */
        int out = 0;             /** slot that receives the weighted ancestral value */
        double scale = 1.0;      /** 1 / sqrt(vLeft + vRight): standardises the contrast */
        double leftWeight = 0.5; /** vRight / (vLeft + vRight) */
        double rightWeight = 0.5;/** vLeft / (vLeft + vRight) */
    };

    /** The contrasts of a tree restricted to a set of species. Depends only on the tree and the species, not on the data. */
    struct PicPlan {
        std::vector<PicOp> ops;          /** contrasts in post-order */
        std::vector<int> leafSlots;      /** per species: value slot, -1 when the species is not in the plan */
        int slotCount = 0;
        size_t speciesInPlan = 0;
    };

    /** Builds a plan for the species flagged in \p keepSpecies (one flag per species). The tree is pruned to those species. */
    inline PicPlan buildPicPlan(const std::vector<PicNode>& nodes, int root, const std::vector<char>& keepSpecies) {
        PicPlan plan;
        plan.leafSlots.assign(keepSpecies.size(), -1);

        struct Result { int slot = -1; double length = 0.0; };

        struct Builder {
            const std::vector<PicNode>& nodes;
            const std::vector<char>& keep;
            PicPlan& plan;

            Result build(int index) {
                const PicNode& node = nodes[index];
                if (node.children.empty()) {
                    if (node.leafIndex >= 0 && static_cast<size_t>(node.leafIndex) < keep.size() && keep[node.leafIndex]) {
                        plan.leafSlots[node.leafIndex] = plan.slotCount;
                        ++plan.speciesInPlan;
                        return { plan.slotCount++, node.branchLength };
                    }
                    return {};
                }

                Result accumulated;
                for (const int child : node.children) {
                    const Result part = build(child);
                    if (part.slot < 0) {
                        continue; // nothing of this subtree is kept
                    }
                    if (accumulated.slot < 0) {
                        accumulated = part;
                        continue;
                    }
                    // a node with more than two kept children is resolved one pair after the other
                    const double vLeft = std::max(accumulated.length, kPicMinBranchLength);
                    const double vRight = std::max(part.length, kPicMinBranchLength);
                    PicOp op;
                    op.left = accumulated.slot;
                    op.right = part.slot;
                    op.out = plan.slotCount++;
                    op.scale = 1.0 / std::sqrt(vLeft + vRight);
                    op.leftWeight = vRight / (vLeft + vRight);
                    op.rightWeight = vLeft / (vLeft + vRight);
                    plan.ops.push_back(op);
                    accumulated.slot = op.out;
                    accumulated.length = vLeft * vRight / (vLeft + vRight); // extension of the ancestral branch
                }
                if (accumulated.slot >= 0) {
                    accumulated.length += node.branchLength; // a single kept child simply inherits the branch
                }
                return accumulated;
            }
        };

        Builder builder{ nodes, keepSpecies, plan };
        builder.build(root);
        return plan;
    }

    /** Standardised contrasts of \p speciesValues (one value per species) for a plan. \p scratch is resized as needed. */
    inline void computePicContrasts(const PicPlan& plan, const std::vector<double>& speciesValues, std::vector<double>& scratch, std::vector<double>& contrasts) {
        scratch.assign(static_cast<size_t>(plan.slotCount), 0.0);
        for (size_t species = 0; species < plan.leafSlots.size() && species < speciesValues.size(); ++species) {
            if (plan.leafSlots[species] >= 0) {
                scratch[static_cast<size_t>(plan.leafSlots[species])] = speciesValues[species];
            }
        }
        contrasts.resize(plan.ops.size());
        for (size_t k = 0; k < plan.ops.size(); ++k) {
            const PicOp& op = plan.ops[k];
            const double left = scratch[static_cast<size_t>(op.left)];
            const double right = scratch[static_cast<size_t>(op.right)];
            contrasts[k] = (left - right) * op.scale;
            scratch[static_cast<size_t>(op.out)] = left * op.leftWeight + right * op.rightWeight;
        }
    }

    // ---- which species are compared and which are "selected" ----

    /** What the displayed value refers to. */
    enum class TraitScope {
        SelectedContribution,  /** the trend is defined by all species; the value is the contribution of the selected species to it */
        AcrossSelected         /** the correlation is computed across the selected species only */
    };

    /** A correlation across fewer species than this is meaningless (two points always give +-1). */
    constexpr size_t kMinSpeciesAcrossSelection = 3;

    struct SpeciesSelectionPlan {
        std::vector<size_t> population;   /** species (indices into the usable species) that enter the correlation */
        std::vector<size_t> selected;     /** species (indices into the usable species) whose contribution is reported; empty = none */
        bool contribution = false;        /** the displayed value is the contribution score, not the correlation */
        bool selectionTooSmall = false;   /** "across selected species" was requested with fewer than 3 species: all species are used */
    };

    /**
     * Decide how a species selection is handled.
     *  contribution scope : population = all usable species, selected = the picked ones (none picked -> everybody: the value is
     *                       then exactly the overall correlation). Works for one species up to all of them.
     *  across selected    : population = the picked species when at least 3 are picked, otherwise all species.
     *  phylogenetic method: contributions are not defined for contrasts, so it always behaves like "across selected".
     * @param usableCount number of species with a trait value and enough cells
     * @param picked      indices (into the usable species) of the species picked in the species explorer
     */
    inline SpeciesSelectionPlan planSpeciesSelection(size_t usableCount, const std::vector<size_t>& picked, TraitScope scope, TraitMethod method) {
        SpeciesSelectionPlan plan;
        std::vector<size_t> everybody(usableCount);
        for (size_t i = 0; i < usableCount; ++i) {
            everybody[i] = i;
        }

        if (scope == TraitScope::SelectedContribution && method != TraitMethod::Phylogenetic) {
            plan.population = everybody;
            plan.selected = picked.empty() ? everybody : picked;
            plan.contribution = true;
            return plan;
        }

        if (picked.size() >= kMinSpeciesAcrossSelection) {
            plan.population = picked;
        }
        else {
            plan.population = everybody;
            plan.selectionTooSmall = !picked.empty();
        }
        return plan;
    }

    // ---- statistics of all genes ----

    /** Everything that does not depend on the gene; prepared once per computation. Index 0 = all species, 1 + k = without species k. */
    struct TraitStatisticsContext {
        TraitMethod method = TraitMethod::Pearson;
        size_t speciesCount = 0;
        std::vector<std::vector<double>> subsetTrait;      /** Pearson/Spearman: trait (ranked for Spearman) of the species that remain */
        std::vector<PicPlan> plans;                        /** Phylogenetic: contrast plans */
        std::vector<std::vector<double>> traitContrasts;   /** Phylogenetic: contrasts of the trait per plan */
        std::vector<size_t> selected;                      /** species whose contribution is reported (empty: no contribution score) */
        double traitMean = 0.0;                            /** mean / standard deviation of the (ranked) trait over all species */
        double traitStdDev = 0.0;
    };

    /** Statistics of one gene. NaN marks values that cannot be computed (e.g. a confidence interval for < 4 species). */
    struct GeneTraitStatistics {
        float correlation = 0.0f;                 /** correlation across the species of the comparison */
        float selectionScore = std::numeric_limits<float>::quiet_NaN(); /** contribution of the selected species (see below) */
        float pValue = 1.0f;
        float ciLow = std::numeric_limits<float>::quiet_NaN();
        float ciHigh = std::numeric_limits<float>::quiet_NaN();
        float leaveOneOutMin = std::numeric_limits<float>::quiet_NaN();
        float leaveOneOutMax = std::numeric_limits<float>::quiet_NaN();
    };

    /**
     * Prepare the gene independent part of the statistics.
     * @param traitValues one trait value per species
     * @param picNodes    species tree (only used for TraitMethod::Phylogenetic; may be empty otherwise)
     * @param picRoot     index of the root in \p picNodes
     */
    inline TraitStatisticsContext prepareTraitStatisticsContext(TraitMethod method, const std::vector<double>& traitValues, const std::vector<PicNode>& picNodes, int picRoot, const std::vector<size_t>& selected = {}) {
        TraitStatisticsContext context;
        context.method = method;
        context.speciesCount = traitValues.size();
        context.selected = selected;
        const size_t speciesCount = traitValues.size();

        if (method == TraitMethod::Phylogenetic) {
            std::vector<double> scratch;
            for (size_t omit = 0; omit <= speciesCount; ++omit) {          // omit == 0 : keep everybody
                std::vector<char> keep(speciesCount, 1);
                if (omit > 0) {
                    keep[omit - 1] = 0;
                }
                context.plans.push_back(buildPicPlan(picNodes, picRoot, keep));
                std::vector<double> contrasts;
                computePicContrasts(context.plans.back(), traitValues, scratch, contrasts);
                context.traitContrasts.push_back(std::move(contrasts));
            }
        }
        else {
            for (size_t omit = 0; omit <= speciesCount; ++omit) {
                std::vector<double> subset;
                subset.reserve(speciesCount);
                for (size_t i = 0; i < speciesCount; ++i) {
                    if (omit == 0 || i != omit - 1) {
                        subset.push_back(traitValues[i]);
                    }
                }
                context.subsetTrait.push_back(method == TraitMethod::Spearman ? averageRanks(subset) : std::move(subset));
            }

            // mean and standard deviation of the trait over ALL species: the reference for the contribution score
            const std::vector<double>& fullTrait = context.subsetTrait[0];
            for (const double value : fullTrait) {
                context.traitMean += value;
            }
            context.traitMean /= static_cast<double>(speciesCount);
            double variance = 0.0;
            for (const double value : fullTrait) {
                variance += (value - context.traitMean) * (value - context.traitMean);
            }
            context.traitStdDev = std::sqrt(variance / static_cast<double>(speciesCount));
        }
        return context;
    }

    /**
     * Correlation, p-value, 95% confidence interval and leave-one-species-out range of every gene in \p range.
     * The leave-one-out range shows how much a single species drives the result: if it is wide or changes sign, the
     * correlation rests on one species and should not be trusted.
     */
    inline void computeTraitStatistics(const std::vector<const std::vector<float>*>& speciesGeneMeans,
        const TraitStatisticsContext& context,
        const GeneRange& range,
        std::vector<GeneTraitStatistics>& out) {
        const size_t speciesCount = context.speciesCount;
        if (speciesCount == 0 || speciesGeneMeans.size() != speciesCount) {
            return;
        }

        std::vector<double> geneValues(speciesCount), subset, scratch, contrasts;
        subset.reserve(speciesCount);

        // a gene without any variation across the species has no correlation. (Without this check the contrasts of such a
        // gene are pure rounding noise and would give an arbitrary correlation.)
        bool constantGene = false;

        // correlation of the current gene when species `omit - 1` is left out (omit == 0: nobody)
        const auto correlationFor = [&](size_t omit) -> double {
            if (constantGene) {
                return 0.0;
            }
            if (context.method == TraitMethod::Phylogenetic) {
                const PicPlan& plan = context.plans[omit];
                if (plan.ops.size() < 2) {
                    return 0.0;
                }
                computePicContrasts(plan, geneValues, scratch, contrasts);
                return correlationThroughOrigin(contrasts, context.traitContrasts[omit]);
            }
            subset.clear();
            for (size_t i = 0; i < speciesCount; ++i) {
                if (omit == 0 || i != omit - 1) {
                    subset.push_back(geneValues[i]);
                }
            }
            if (context.method == TraitMethod::Spearman) {
                subset = averageRanks(subset);
            }
            return pearsonOfVectors(subset, context.subsetTrait[omit]);
            };

        // number of species that really enter the statistics (the tree may lack some species)
        const size_t effectiveCount = (context.method == TraitMethod::Phylogenetic) ? context.plans[0].speciesInPlan : speciesCount;

        for (size_t gene = range.begin; gene < range.end; ++gene) {
            for (size_t species = 0; species < speciesCount; ++species) {
                geneValues[species] = static_cast<double>((*speciesGeneMeans[species])[gene]);
            }

            const auto [lowest, highest] = std::minmax_element(geneValues.begin(), geneValues.end());
            constantGene = (*highest - *lowest) <= 1.0e-9 * (1.0 + std::fabs(*highest));

            GeneTraitStatistics& result = out[gene];
            const double r = correlationFor(0);
            result.correlation = static_cast<float>(r);
            result.pValue = static_cast<float>(pearsonPValue(r, effectiveCount));

            // Contribution of the selected species to the overall correlation: the mean over the selected species of
            //     z(expression of the gene) * z(trait)
            // with both z-scores taken against ALL species. It is defined for any number of selected species (also one) and equals
            // the correlation itself when every species is selected. It is not bounded by 1: a selection of the most extreme species
            // can exceed it. (Not available for phylogenetic contrasts.)
            if (!context.selected.empty() && context.method != TraitMethod::Phylogenetic) {
                double score = 0.0;
                if (!constantGene && context.traitStdDev > 0.0) {
                    const std::vector<double> ranked = (context.method == TraitMethod::Spearman) ? averageRanks(geneValues) : geneValues;
                    double geneMean = 0.0;
                    for (const double value : ranked) {
                        geneMean += value;
                    }
                    geneMean /= static_cast<double>(speciesCount);
                    double geneVariance = 0.0;
                    for (const double value : ranked) {
                        geneVariance += (value - geneMean) * (value - geneMean);
                    }
                    const double geneStdDev = std::sqrt(geneVariance / static_cast<double>(speciesCount));
                    if (geneStdDev > 0.0) {
                        const std::vector<double>& fullTrait = context.subsetTrait[0];
                        for (const size_t species : context.selected) {
                            score += ((ranked[species] - geneMean) / geneStdDev) * ((fullTrait[species] - context.traitMean) / context.traitStdDev);
                        }
                        score /= static_cast<double>(context.selected.size());
                    }
                }
                result.selectionScore = static_cast<float>(score); // not clamped: picking the most extreme species can exceed 1
            }

            double low = 0.0, high = 0.0;
            fisherConfidenceInterval(r, effectiveCount, low, high);
            result.ciLow = static_cast<float>(low);
            result.ciHigh = static_cast<float>(high);

            if (effectiveCount >= 4) {
                double minimum = std::numeric_limits<double>::max();
                double maximum = std::numeric_limits<double>::lowest();
                for (size_t omit = 1; omit <= speciesCount; ++omit) {
                    if (context.method == TraitMethod::Phylogenetic && context.plans[omit].speciesInPlan == effectiveCount) {
                        continue; // the left-out species is not in the tree: nothing changes
                    }
                    const double looR = correlationFor(omit);
                    minimum = std::min(minimum, looR);
                    maximum = std::max(maximum, looR);
                }
                if (minimum <= maximum) {
                    result.leaveOneOutMin = static_cast<float>(minimum);
                    result.leaveOneOutMax = static_cast<float>(maximum);
                }
            }
        }
    }

} // namespace

// Split implementation by responsibility to keep navigation manageable while
// preserving the original SettingsAction behavior and method signatures.

#include "SettingsAction.Ui.inl"

#include "SettingsAction.Analysis.inl"

#include "SettingsAction.Data.inl"

#include "SettingsAction.Tree.inl"

#include "SettingsAction.Serialization.inl"
