#include "SettingsAction.h"
#include "XSCGeneDetectPlugin.h"
#include<string>  
#include <QFileDialog>
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

    /**
     * Trait/expression agreement score of every gene in \p range for a SET of species.
     *
     * Both the gene expression and the trait are standardised against ALL species (population z-scores), because a single
     * trait value has no spread of its own. The score of a gene is the mean over the selected species of
     *      z(expression of the gene in the species) * z(trait of the species)
     * so a gene that is high where the trait is high (and low where it is low) scores close to +1, the opposite close to -1.
     * With every species selected this is exactly the Pearson correlation; with a single species it shows how strongly that
     * species follows the overall gene/trait trend. The value is clamped to [-1, 1]; genes or traits without variance score 0.
     *
     * @param speciesGeneMeans mean expression of every gene, one entry per species (all species of the comparison)
     * @param traitValues      trait value per species (same order)
     * @param selected         indices (into the two vectors above) of the selected species; must not be empty
     */
    inline void computeSelectionTraitScores(const std::vector<const std::vector<float>*>& speciesGeneMeans,
        const std::vector<double>& traitValues,
        const std::vector<size_t>& selected,
        const GeneRange& range,
        std::vector<float>& scores) {
        const size_t speciesCount = traitValues.size();
        if (speciesCount == 0 || selected.empty() || speciesGeneMeans.size() != speciesCount) {
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
        traitVariance /= static_cast<double>(speciesCount);
        const double traitStdDev = std::sqrt(traitVariance);

        for (size_t gene = range.begin; gene < range.end; ++gene) {
            double geneMean = 0.0;
            for (size_t species = 0; species < speciesCount; ++species) {
                geneMean += static_cast<double>((*speciesGeneMeans[species])[gene]);
            }
            geneMean /= static_cast<double>(speciesCount);

            double geneVariance = 0.0;
            for (size_t species = 0; species < speciesCount; ++species) {
                const double delta = static_cast<double>((*speciesGeneMeans[species])[gene]) - geneMean;
                geneVariance += delta * delta;
            }
            geneVariance /= static_cast<double>(speciesCount);
            const double geneStdDev = std::sqrt(geneVariance);

            if (geneStdDev <= 0.0 || traitStdDev <= 0.0) {
                scores[gene] = 0.0f;
                continue;
            }

            double sum = 0.0;
            for (const size_t species : selected) {
                const double geneZ = (static_cast<double>((*speciesGeneMeans[species])[gene]) - geneMean) / geneStdDev;
                const double traitZ = (traitValues[species] - traitMean) / traitStdDev;
                sum += geneZ * traitZ;
            }
            const double score = sum / static_cast<double>(selected.size());
            scores[gene] = static_cast<float>(std::max(-1.0, std::min(1.0, score)));
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
