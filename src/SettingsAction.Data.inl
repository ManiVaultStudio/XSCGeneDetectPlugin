void SettingsAction::clearTemporaryDatasetHandles()
{
    _selectedPointsTSNEDataset = Dataset<Points>();
    _selectedPointsDataset = Dataset<Points>();
    _selectedPointsEmbeddingDataset = Dataset<Points>();
    _filteredUMAPDatasetPoints = Dataset<Points>();
    _filteredUMAPDatasetColors = Dataset<Points>();
    _filteredUMAPDatasetClusters = Dataset<Points>();
    _tsneDatasetExpressionColors = Dataset<Points>();
    _geneSimilarityPoints = Dataset<Points>();

    _tsneDatasetSpeciesColors = Dataset<Clusters>();
    _tsneDatasetClusterColors = Dataset<Clusters>();
    _geneSimilarityClusterColoring = Dataset<Clusters>();
}

int SettingsAction::speciesGeneListLength(const QString& species) const
{
    const auto it = _clusterNameToGeneNameToExpressionValue.find(species);
    return it != _clusterNameToGeneNameToExpressionValue.end() ? static_cast<int>(it->second.size()) : 0;
}

void SettingsAction::clearUpdateDatasetHandles()
{
    // The datasets that every update replaces (group 10): selection subset, embedding, expression colors, species and cluster colors.
    // (The handles of the filtered UMAP datasets, the gene similarity datasets and the gene table t-SNE dataset are NOT reset here.)
    _selectedPointsDataset = Dataset<Points>();
    _selectedPointsEmbeddingDataset = Dataset<Points>();
    _selectedPointsTSNEDataset = Dataset<Points>();
    _tsneDatasetExpressionColors = Dataset<Points>();
    _tsneDatasetSpeciesColors = Dataset<Clusters>();
    _tsneDatasetClusterColors = Dataset<Clusters>();
}

void SettingsAction::removeTemporaryDatasets()
{
    // Group 10 (selection subset, embeddings, colors) is replaced by every update: the handles are reset and the datasets removed.
    clearUpdateDatasetHandles();
    removeDatasets(10);

    // Groups 20 (filtered UMAP datasets) and 30 (gene similarity datasets) are deliberately NOT removed and their handles are kept:
    // the update re-uses them. They are filled again when they are needed (the filtered UMAP datasets on every gene click, the
    // gene similarity datasets on every update), so nothing stale is shown. Resetting their handles used to create a new set on
    // every update, which is what piled up datasets. Removing them instead is NOT safe: the Scatterplot view keeps handles to
    // them in its coloring actions, and the next time it switched its color dataset it released a handle to a dataset that no
    // longer existed (access violation in DimensionPickerAction::setPointsDataset).
}

void SettingsAction::removeDatasets(int groupId)
{
    auto allDatasets = mv::data().getAllDatasets();

    // id -> dataset pointer (NO COPYING)
    QHash<QString, Dataset<DatasetImpl>> idToDataset;
    idToDataset.reserve(allDatasets.size());

    for (const auto& ds : allDatasets) {
        if (ds->getGroupIndex() == groupId) {
            idToDataset.insert(ds->getId(), ds);
        }
    }

    // Cache depth (memoization)
    QHash<QString, int> depthCache;
    depthCache.reserve(idToDataset.size());

    std::function<int(const QString&)> depthOf =
        [&](const QString& id) -> int
        {
            auto it = depthCache.find(id);
            if (it != depthCache.end())
                return it.value();

            int depth = 0;

            auto ds = idToDataset.value(id);
            auto parent = ds->getParent();

            if (parent.isValid()) {
                QString parentId = parent->getId();

                if (idToDataset.contains(parentId)) {
                    depth = 1 + depthOf(parentId);
                }
            }

            depthCache.insert(id, depth);
            return depth;
        };

    // Build list of ids
    QVector<QString> ids;
    ids.reserve(idToDataset.size());

    for (auto it = idToDataset.begin(); it != idToDataset.end(); ++it) {
        ids.push_back(it.key());
    }

    // Compute all depths (O(N))
    for (const auto& id : ids) {
        depthOf(id);
    }

    // Sort deepest first (critical step)
    std::sort(ids.begin(), ids.end(),
        [&](const QString& a, const QString& b) {
            return depthCache[a] > depthCache[b];
        });

    // Delete in correct order
    for (const auto& id : ids) {
        auto ds = idToDataset.value(id);
        if (ds.isValid()) {
            //qDebug() << "Deleting:" << ds->getId() << "with name:" << ds->getGuiName()
                //<< "depth:" << depthCache[id];

            mv::data().removeDataset(ds);
        }
    }
}
QVariant SettingsAction::createModelFromData(const std::map<QString, std::map<QString, Stats>>& map, const std::map<QString, std::vector<QString>>& geneCounter, const std::map<QString, std::vector<std::pair<QString, int>>>& rankingMap, const int& n) {

    if (map.empty() || _totalGeneList.empty()) {
        return QVariant();
    }

    // ---- 1. Model skeleton: one row per gene of the dataset ------------------------------------------------
    QStandardItemModel* model = new QStandardItemModel();

    _initColumnNames = { "ID", "Species \nAppearance", "Gene Appearance Species Names", "Statistics", "Trait\nCorrelation", "Weighted\nAppearance", "Best\nPosition", "Mean\nDifferential", "Positive\nSpecies (%)", "Mean\nExpression" }; // two lines so the headers fit the narrow bar columns
    const int geneCount = static_cast<int>(_totalGeneList.size());
    model->setColumnCount(_initColumnNames.size());
    model->setRowCount(geneCount);
    model->setHorizontalHeaderLabels(_initColumnNames);

    // column chooser: by default only ID, species count and trait correlation are visible
    QStringList headers = _initColumnNames;
    _hiddenShowncolumns.setOptions(headers);
    _hiddenShowncolumns.setSelectedOptions({ headers[0], headers[1], headers[kWeightedAppearanceColumn], headers[kTraitCorrelationColumn] });

    model->setHeaderData(0, Qt::Horizontal, "Gene. Hover a gene for a summary of its statistics across the species.", Qt::ToolTipRole);
    model->setHeaderData(1, Qt::Horizontal, "Number of species in which the gene is a top-N gene (all appearances count the same).\nSee Weighted Appearance for a version that weighs the positions.", Qt::ToolTipRole);
    model->setHeaderData(2, Qt::Horizontal, "The species in which the gene is a top-N gene. Hover a cell for the position of the gene in each of them.", Qt::ToolTipRole);
    model->setHeaderData(3, Qt::Horizontal, "Statistics of the gene in every species (one line per species). Hover a cell for a readable table.", Qt::ToolTipRole);
    model->setHeaderData(kBestPositionColumn, Qt::Horizontal, "Best (smallest) position of the gene in the top-N ordering of any species: 1 = the strongest gene of a species.\nShown through the column chooser.", Qt::ToolTipRole);
    model->setHeaderData(kMeanDifferentialColumn, Qt::Horizontal, "Mean differential expression over ALL species: mean of (mean expression in the selected cells - mean expression in the other cells).\nShown through the column chooser.", Qt::ToolTipRole);
    model->setHeaderData(kPositiveSpeciesColumn, Qt::Horizontal, "Percentage of species in which the gene is higher in the selected cells than in the other cells (direction consistency).\nShown through the column chooser.", Qt::ToolTipRole);
    model->setHeaderData(kMeanExpressionColumn, Qt::Horizontal, "Mean expression in the selected cells, averaged over the species.\nShown through the column chooser.", Qt::ToolTipRole);
    model->setHeaderData(kTraitCorrelationColumn, Qt::Horizontal, "Correlation of the gene's expression with the trait across species (see the Trait correlation group).\nHover a cell for the statistics.", Qt::ToolTipRole);
    model->setHeaderData(kWeightedAppearanceColumn, Qt::Horizontal,
        "Weighted appearance: the number of species in which the gene is a top-N gene, but every appearance is weighted by its position:\n"
        "position 1 counts 1, position N counts 1/N, divided by the number of species. 1 = position 1 in every species.\n"
        "Hover a cell for the best positions and the significance of the rank consistency across species.", Qt::ToolTipRole);

    // ---- 2. Lookup tables: gene -> (species, statistics) and gene -> (species -> rank) ------------------------
    QHash<QString, QVector<QPair<QString, Stats>>> geneToSpeciesStats;
    geneToSpeciesStats.reserve(geneCount);
    for (const auto& [speciesName, innerMap] : map) {
        for (const auto& [geneName, stats] : innerMap) {
            geneToSpeciesStats[geneName].append(qMakePair(speciesName, stats));
        }
    }

    QHash<QString, QHash<QString, int>> geneToSpeciesRank;
    geneToSpeciesRank.reserve(static_cast<int>(rankingMap.size()));
    for (const auto& [geneName, speciesRanks] : rankingMap) {
        auto& rankMap = geneToSpeciesRank[geneName];
        rankMap.reserve(static_cast<int>(speciesRanks.size()));
        for (const auto& [speciesName, rank] : speciesRanks) {
            rankMap.insert(speciesName, rank);
        }
    }

    // ---- 3. "Statistics" text of every gene (parallel) ------------------------------------------------------
    // Formatting ~20k genes x species strings is the most expensive part of building the model. The lookup tables
    // are only read here, and every worker writes distinct elements of statisticsText -> no locking required.
    // The text format is parsed again by convertToStatisticsMap() in the plugin - keep both in sync.
    std::vector<QString> statisticsText(static_cast<size_t>(geneCount));

    // Weighted appearance and rank consistency (computed in the same pass). Rank, as stored by findTopNGenesPerCluster, counts
    // from the other end for the "Negative" type, so the position inside the species' top-N ordering is derived from it.
    _weightedTopN = n;
    _weightedNegative = (_typeofTopNGenes.getCurrentText() == "Negative");
    const bool negativeType = _weightedNegative;
    const int topN = n;
    std::unordered_map<QString, int> speciesListLength; // genes ranked in every species
    for (const auto& [speciesName, innerMap] : map) {
        speciesListLength[speciesName] = static_cast<int>(innerMap.size());
    }
    const double speciesTotal = static_cast<double>(map.size());
    std::vector<double> weightedAppearance(static_cast<size_t>(geneCount), 0.0);
    std::vector<float> rankConsistencyP(static_cast<size_t>(geneCount), 1.0f);
    std::vector<QString> bestPositionsText(static_cast<size_t>(geneCount));
    std::vector<int> bestPosition(static_cast<size_t>(geneCount), 0);               // 0: the gene has no position
    std::vector<double> meanDifferential(static_cast<size_t>(geneCount), 0.0);      // over all species
    std::vector<double> positiveSpeciesPercent(static_cast<size_t>(geneCount), 0.0);
    std::vector<double> meanExpression(static_cast<size_t>(geneCount), 0.0);
    auto geneRanges = splitGeneRanges(static_cast<size_t>(geneCount), static_cast<size_t>(std::max(1, QThread::idealThreadCount())) * 4);
    QtConcurrent::blockingMap(geneRanges, [&](GeneRange& range) {
        for (size_t row = range.begin; row < range.end; ++row) {
            const QString& gene = _totalGeneList[row];

            const auto statsIt = geneToSpeciesStats.constFind(gene);
            if (statsIt == geneToSpeciesStats.constEnd()) {
                continue;
            }
            const auto rankIt = geneToSpeciesRank.constFind(gene);

            // position of the gene in the top-N ordering of every species
            double weightSum = 0.0;
            std::vector<double> normalizedRanks;
            std::vector<std::pair<int, QString>> positions;
            normalizedRanks.reserve(static_cast<size_t>(statsIt->size()));
            double differenceSum = 0.0, expressionSum = 0.0;
            int positiveSpecies = 0;
            for (const auto& speciesStats : *statsIt) {
                const double difference = static_cast<double>(speciesStats.second.meanSelected) - static_cast<double>(speciesStats.second.meanNonSelected);
                differenceSum += difference;
                expressionSum += static_cast<double>(speciesStats.second.meanSelected);
                positiveSpecies += difference > 0.0 ? 1 : 0;

                const int storedRank = (rankIt != geneToSpeciesRank.constEnd()) ? rankIt->value(speciesStats.first) : 0;
                const auto lengthIt = speciesListLength.find(speciesStats.first);
                if (storedRank < 1 || lengthIt == speciesListLength.end()) {
                    continue;
                }
                const int listLength = lengthIt->second;
                const int position = negativeType ? listLength - storedRank + 1 : storedRank;
                if (position < 1 || position > listLength) {
                    continue;
                }
                normalizedRanks.push_back(static_cast<double>(position) / static_cast<double>(listLength));
                positions.emplace_back(position, speciesStats.first);
                if (speciesStats.second.meanSelected > 0) { // same condition as the species appearance
                    weightSum += rankWeight(position, topN);
                }
            }
            weightedAppearance[row] = speciesTotal > 0.0 ? weightSum / speciesTotal : 0.0;
            rankConsistencyP[row] = static_cast<float>(robustRankAggregationPValue(normalizedRanks));

            const double speciesSeen = static_cast<double>(statsIt->size());
            meanDifferential[row] = speciesSeen > 0.0 ? differenceSum / speciesSeen : 0.0;
            meanExpression[row] = speciesSeen > 0.0 ? expressionSum / speciesSeen : 0.0;
            positiveSpeciesPercent[row] = GeneStats::percent(positiveSpecies, speciesSeen);

            std::sort(positions.begin(), positions.end());
            bestPosition[row] = positions.empty() ? 0 : positions.front().first;
            QStringList best;
            for (size_t k = 0; k < positions.size() && k < 5; ++k) {
                QString name = positions[k].second;
                name.replace("_", " ");
                best << QString("%1 (%2)").arg(name).arg(positions[k].first);
            }
            bestPositionsText[row] = best.join(", ");

            QString formattedStatistics;
            formattedStatistics.reserve(statsIt->size() * 96);
            for (const auto& speciesStats : *statsIt) {
                const auto& species = speciesStats.first;
                const auto& stats = speciesStats.second;
                const int rank = (rankIt != geneToSpeciesRank.constEnd()) ? rankIt->value(species) : 0;

                formattedStatistics += QString("Species: %1, Rank: %2, AbundanceTop: %3,  AbundanceMiddle: %4,  CountAbundanceNumerator: %5, MeanSelected: %6, CountSelected: %7, MeanNotSelected: %8, CountNotSelected: %9;\n")
                    .arg(species)
                    .arg(rank)
                    .arg(stats.abundanceTop)
                    .arg(stats.abundanceMiddle)
                    .arg(stats.countAbundanceNumerator)
                    .arg(stats.meanSelected, 0, 'f', 4)
                    .arg(stats.countSelected)
                    .arg(stats.meanNonSelected, 0, 'f', 4)
                    .arg(stats.countNonSelected);
            }
            statisticsText[row] = std::move(formattedStatistics);
        }
        });

    // FDR of the rank consistency over all genes
    const std::vector<float> rankConsistencyFdr = benjaminiHochberg(rankConsistencyP);

    // ---- 4. Fill the model (serial: QStandardItemModel is not thread safe) -----------------------------------
    for (int rowIndex = 0; rowIndex < geneCount; ++rowIndex) {
        const auto& gene = _totalGeneList[rowIndex];

        // Column 0: gene id (sorts as text)
        model->setItem(rowIndex, 0, new QStandardItem(gene));

        // Columns 1 and 2: in how many species / in which species the gene is a top-N gene
        QString speciesGeneAppearancesComb;
        int count = 0;
        if (auto it = geneCounter.find(gene); it != geneCounter.end()) {
            const auto& speciesDetails = it->second;
            count = static_cast<int>(speciesDetails.size());
            QStringList speciesNames;
            for (const auto& speciesDetail : speciesDetails) {
                speciesNames << speciesDetail;
            }
            speciesGeneAppearancesComb = speciesNames.join(";");
        }

        auto* countItem = new QStandardItem(); // sorts as integer
        countItem->setData(count, Qt::DisplayRole);
        countItem->setData(count, Qt::UserRole);
        model->setItem(rowIndex, 1, countItem);
        model->setItem(rowIndex, 2, new QStandardItem(speciesGeneAppearancesComb));

        // Column 3: per-species statistics text (see step 3)
        model->setItem(rowIndex, 3, new QStandardItem(statisticsText[static_cast<size_t>(rowIndex)]));

        // Column 4: trait correlation. A numeric DisplayRole keeps sorting numeric; the bar delegate formats the text
        // and draws the bar. (Values arrive/refresh later through publishTraitCorrelationToModel().)
        const auto itCorr = _geneTraitValuesComputed.find(gene);
        const auto itP = _geneTraitPValues.find(gene);
        const auto itQ = _geneTraitFdrValues.find(gene);
        const auto itE = _geneTraitExtras.find(gene);
        auto* correlationItem = new QStandardItem();
        setTraitCorrelationItemData(correlationItem,
            itCorr != _geneTraitValuesComputed.end() ? static_cast<double>(itCorr->second) : 0.0,
            itP != _geneTraitPValues.end() ? static_cast<double>(itP->second) : 1.0,
            itQ != _geneTraitFdrValues.end() ? static_cast<double>(itQ->second) : 1.0,
            itE != _geneTraitExtras.end() ? itE->second : GeneTraitExtras(),
            _traitSampleSize, _traitMethodNote, _traitSignificanceBasis);
        if (itCorr == _geneTraitValuesComputed.end() && _traitSampleSize > 0) {
            correlationItem->setData("Not part of the trait analysis: the trait correlation is computed for the listed top-N genes only\n(setting Trait Correlation Genes -> All genes (slower) includes every gene).", Qt::ToolTipRole);
        }
        model->setItem(rowIndex, kTraitCorrelationColumn, correlationItem);

        // Column 5: weighted appearance (numeric DisplayRole for sorting; a bar delegate draws it)
        const double weighted = weightedAppearance[static_cast<size_t>(rowIndex)];
        auto* weightedItem = new QStandardItem();
        weightedItem->setData(weighted, Qt::DisplayRole);
        weightedItem->setData(weighted, Qt::UserRole);
        weightedItem->setData(static_cast<double>(rankConsistencyP[static_cast<size_t>(rowIndex)]), kWeightedPValueRole);
        weightedItem->setData(static_cast<double>(rankConsistencyFdr[static_cast<size_t>(rowIndex)]), kWeightedFdrRole);
        weightedItem->setData(QString("Weighted appearance = %1\n"
            "(every species where the gene is a top-N gene counts (N - position + 1) / N, divided by the %2 species;\n"
            "1 = position 1 in all species)\n"
            "Best positions: %3\n"
            "Rank consistency across species (robust rank aggregation): p = %4, FDR = %5")
            .arg(weighted, 0, 'f', 3).arg(static_cast<int>(speciesTotal))
            .arg(bestPositionsText[static_cast<size_t>(rowIndex)].isEmpty() ? QString("none") : bestPositionsText[static_cast<size_t>(rowIndex)])
            .arg(static_cast<double>(rankConsistencyP[static_cast<size_t>(rowIndex)]), 0, 'g', 3)
            .arg(static_cast<double>(rankConsistencyFdr[static_cast<size_t>(rowIndex)]), 0, 'g', 3), Qt::ToolTipRole);
        model->setItem(rowIndex, kWeightedAppearanceColumn, weightedItem);

        // Columns 6-9: further statistics (numeric roles keep sorting numeric; the tooltips are built on demand when hovering)
        const auto numberItem = [](const QVariant& value) {
            auto* item = new QStandardItem();
            item->setData(value, Qt::DisplayRole);
            item->setData(value, Qt::UserRole);
            return item;
            };
        const int best = bestPosition[static_cast<size_t>(rowIndex)];
        model->setItem(rowIndex, kBestPositionColumn, numberItem(best > 0 ? QVariant(best) : QVariant()));
        model->setItem(rowIndex, kMeanDifferentialColumn, numberItem(meanDifferential[static_cast<size_t>(rowIndex)]));
        model->setItem(rowIndex, kPositiveSpeciesColumn, numberItem(positiveSpeciesPercent[static_cast<size_t>(rowIndex)]));
        model->setItem(rowIndex, kMeanExpressionColumn, numberItem(meanExpression[static_cast<size_t>(rowIndex)]));
    }

    return QVariant::fromValue(model);
}

void SettingsAction::createClusterPositionMap()
{
    _clusterPositionMap;
}
QStringList SettingsAction::getSystemModeColor() {
    // Get the application palette
    QPalette palette = QApplication::palette();

    // Check the color of the window text to determine if the system is in dark mode or light mode
    // Assuming dark mode has lighter text (e.g., white) and light mode has darker text (e.g., black)
    if (palette.color(QPalette::WindowText).lightness() < 128) {
        // Light mode
        return { "#FFFFFF","#000000" }; // White
    }
    else {
        // Dark mode
        return { "#000000","#FFFFFF" }; // Black
    }
}


// ============================================================================================================
//  CSV export
//
//  Two tables can be saved:
//   - "Save Gene Table"               : one row per gene of the gene table (rows and order as shown, i.e. filtered and sorted)
//   - "Save Species Statistics Table" : one row per gene and species (the statistics shown in the right-hand table)
//  Both can start with the analysis parameters as '#' comment lines, and both contain the trait statistics only when a
//  trait correlation was actually computed (otherwise the columns would hold placeholder zeros that look like results).
// ============================================================================================================

namespace {

    // A field is quoted when it contains a separator, a quote or a line break.
    QString csvEscape(const QString& value)
    {
        if (value.contains(',') || value.contains('"') || value.contains('\n') || value.contains('\r')) {
            QString escaped = value;
            escaped.replace('"', "\"\"");
            return '"' + escaped + '"';
        }
        return value;
    }

    QString csvLine(const QStringList& fields)
    {
        QStringList escaped;
        escaped.reserve(fields.size());
        for (const QString& field : fields) {
            escaped << csvEscape(field);
        }
        return escaped.join(",");
    }

    // The gene table is recognised by its header, not by its column count.
    bool isGeneTableModel(const QAbstractItemModel* model)
    {
        return model && model->columnCount() >= 4 && model->headerData(3, Qt::Horizontal).toString() == "Statistics";
    }

    // Text that is safe inside a file name.
    QString fileNamePart(const QString& text)
    {
        QString part = text;
        part.replace(QRegularExpression("[^A-Za-z0-9_.-]+"), "_");
        return part.left(60);
    }

    // Ask where to save; ".csv" is added when no extension was typed. Empty string: cancelled.
    QString askForCsvPath(const QString& suggestedName)
    {
        QString filePath = QFileDialog::getSaveFileName(nullptr, "Save CSV", suggestedName, "CSV Files (*.csv);;All Files (*)");
        if (!filePath.isEmpty() && QFileInfo(filePath).suffix().isEmpty()) {
            filePath += ".csv";
        }
        return filePath;
    }

    // Same rule as the on-screen table: several middle clusters -> redundant column "Neuronal", one -> its name, none -> "Abundance".
    QString middleFractionName(const QStringList& middleSet, bool& redundant)
    {
        if (middleSet.size() > 1) {
            redundant = true;
            return "Neuronal";
        }
        if (middleSet.size() == 1) {
            redundant = false;
            return middleSet.first();
        }
        redundant = true;
        return "Abundance";
    }

    double percentOfCounts(double numerator, double denominator)
    {
        return denominator != 0.0 ? numerator / denominator * 100.0 : 0.0;
    }

    // Table of an unknown layout (e.g. an empty model): written as it is.
    void writeGenericTable(const QAbstractItemModel* model, QTextStream& stream)
    {
        QStringList header;
        for (int col = 0; col < model->columnCount(); ++col) {
            QString name = model->headerData(col, Qt::Horizontal).toString();
            name.replace("\n", " ");
            header << name;
        }
        stream << csvLine(header) << "\n";
        for (int row = 0; row < model->rowCount(); ++row) {
            QStringList fields;
            for (int col = 0; col < model->columnCount(); ++col) {
                fields << model->data(model->index(row, col)).toString();
            }
            stream << csvLine(fields) << "\n";
        }
    }

    /**
     * Parse the "Statistics" text of a gene (one "Species: ..., Rank: ..., ..." line per species) into CSV rows,
     * one per species. Fractions are numerator / denominator in percent, like in the on-screen table.
     */
    QStringList speciesStatisticsRows(const QString& gene, const QStringList& topNSpecies, const QStringList& statisticsLines,
        const QStringList& explorerSelection, bool withMiddleFraction, const QStringList& traitValues,
        const std::function<double(const QString&, int, double)>& rankWeightOf,
        const std::function<int(const QString&)>& listLengthOf, bool negativeType)
    {
        QStringList rows;
        for (const QString& line : statisticsLines) {
            if (line.trimmed().isEmpty()) {
                continue;
            }

            QMap<QString, QString> values;
            for (const QString& pair : line.split(", ")) {
                const QStringList keyValue = pair.split(": ");
                if (keyValue.size() == 2) {
                    QString value = keyValue[1].trimmed();
                    if (value.endsWith(';')) {
                        value.chop(1); // every statistics line ends with ';'
                    }
                    values.insert(keyValue[0].trimmed(), value);
                }
            }

            const QString species = values.value("Species");
            const double meanSelected = values.value("MeanSelected").toDouble();
            const double meanNotSelected = values.value("MeanNotSelected").toDouble();
            const double numerator = values.value("CountAbundanceNumerator").toDouble();

            QStringList fields;
            fields << gene
                << (topNSpecies.contains(species) ? "True" : "False")
                << species
                << (explorerSelection.contains(species) ? "True" : "False")
                << QString::number(meanSelected - meanNotSelected, 'f', 4)
                << values.value("Rank")
                << QString::number(rankWeightOf(species, values.value("Rank").toInt(), meanSelected), 'f', 4);
            {
                // rank percentile: where the gene stands among all genes of the species (small = strong)
                const int listLength = listLengthOf(species);
                const int position = GeneStats::positionFromRank(values.value("Rank").toInt(), listLength, negativeType);
                fields << (position > 0 ? QString::number(GeneStats::percent(position, listLength), 'f', 3) : QString());

                double log2Fold = 0.0;
                fields << (GeneStats::log2FoldChange(meanSelected, meanNotSelected, log2Fold) ? QString::number(log2Fold, 'f', 4) : QString());

                const double selectedCells = values.value("CountSelected").toDouble();
                fields << QString::number(GeneStats::percent(selectedCells, selectedCells + values.value("CountNotSelected").toDouble()), 'f', 2);
            }
            fields << QString::number(percentOfCounts(numerator, values.value("AbundanceTop").toDouble()), 'f', 2);
            if (withMiddleFraction) {
                fields << QString::number(percentOfCounts(numerator, values.value("AbundanceMiddle").toDouble()), 'f', 2);
            }
            fields << values.value("CountSelected")
                << QString::number(meanSelected, 'f', 4)
                << values.value("CountNotSelected")
                << QString::number(meanNotSelected, 'f', 4);
            fields << traitValues;
            rows << csvLine(fields);
        }
        return rows;
    }

} // namespace

bool SettingsAction::traitColumnsAvailable(const QAbstractItemModel* model) const
{
    return model && model->columnCount() > kTraitCorrelationColumn && _traitSampleSize > 0;
}

QStringList SettingsAction::traitExportHeaders() const
{
    // p-value, FDR and confidence interval always describe the correlation across the compared species
    const QString suffix = _traitContributionShown ? " (overall correlation)" : "";

    QStringList headers;
    headers << (_traitValueLabel.isEmpty() ? QString("Trait correlation") : _traitValueLabel);
    if (_traitContributionShown) {
        headers << "Trait overall correlation (all species)";
    }
    headers << "Trait p-value" + suffix
        << "Trait FDR (Benjamini-Hochberg)" + suffix
        << "Trait phylogenetic correlation"
        << "Trait phylogenetic p-value"
        << "Trait phylogenetic FDR"
        << QString("Trait significant (FDR < %2, %1)").arg(_traitSignificanceBasis.startsWith("phylogenetically") ? "phylogenetically corrected" : "chosen method, ancestry not corrected").arg(significanceLevel())
        << "Trait N species"
        << "Trait CI low" + suffix
        << "Trait CI high" + suffix
        << "Trait leave-one-out min" + suffix
        << "Trait leave-one-out max" + suffix;
    return headers;
}

QStringList SettingsAction::traitExportValues(const QAbstractItemModel* model, int row) const
{
    const QModelIndex index = model->index(row, kTraitCorrelationColumn);
    const QVariantList stats = index.data(kTraitStatsRole).toList();

    // stats: N species, CI low, CI high, leave-one-out min, max, overall correlation (unavailable values are left empty)
    const auto optionalNumber = [&stats](int position, int precision) {
        if (position >= stats.size()) {
            return QString();
        }
        const double value = stats.at(position).toDouble();
        return std::isfinite(value) ? QString::number(value, 'f', precision) : QString();
        };

    const double fdr = index.data(kTraitFdrRole).toDouble();
    const double significanceFdr = index.data(kTraitSignificanceFdrRole).toDouble();

    // number with significant digits (p-values) for stats position `position`
    const auto optionalSignificant = [&stats](int position) {
        if (position >= stats.size()) {
            return QString();
        }
        const double value = stats.at(position).toDouble();
        return std::isfinite(value) ? QString::number(value, 'g', 4) : QString();
        };

    QStringList values;
    values << QString::number(index.data(Qt::UserRole).toDouble(), 'f', 4);
    if (_traitContributionShown) {
        values << optionalNumber(5, 4);
    }
    values << QString::number(index.data(kTraitPValueRole).toDouble(), 'g', 4)
        << QString::number(fdr, 'g', 4)
        << optionalNumber(6, 4)
        << optionalSignificant(7)
        << optionalSignificant(8)
        << (significanceFdr < significanceLevel() ? "True" : "False")
        << optionalNumber(0, 0)
        << optionalNumber(1, 4)
        << optionalNumber(2, 4)
        << optionalNumber(3, 4)
        << optionalNumber(4, 4);
    return values;
}

QStringList SettingsAction::buildExportParameterLines(const QString& tableTitle, int rowCount, bool withTraitColumns, int significantRows)
{
    QStringList lines;
    lines << "XSC Gene Detect - " + tableTitle;
    lines << "Created: " + QDateTime::currentDateTime().toString(Qt::ISODate);

    const auto mainDataset = _mainPointsDataset.getCurrentDataset();
    if (mainDataset.isValid()) {
        lines << "Dataset: " + mainDataset->getGuiName();
    }
    lines << QString("Selected cells: %1").arg(_selectedIndicesFromStorage.size());

    QStringList cellTypes = _selectedClusterNames;
    if (cellTypes.isEmpty() && !_selectedClusterName.isEmpty()) {
        cellTypes << _selectedClusterName;
    }
    lines << "Cell types: " + (cellTypes.isEmpty() ? QString("none") : cellTypes.join(", "));
    lines << QString("Top N genes per species: %1 (%2)").arg(_topNGenesFilter.getValue()).arg(_typeofTopNGenes.getCurrentText());

    const QStringList explorerSelection = _speciesExplorerInMap.getSelectedOptions();
    lines << "Species selected in the species explorer: " + (explorerSelection.isEmpty() ? QString("none") : explorerSelection.join(", "));

    const QString searchText = _searchBox ? _searchBox->text() : QString();
    if (!searchText.isEmpty()) {
        lines << "Gene search: " + searchText;
    }

    if (withTraitColumns) {
        lines << "Trait: " + _reverseSearchTraitSelectionString.getString();
        // the note is multi-line: method, number of species, what the value means
        lines << "Trait statistics: " + _traitMethodNote.split("\n", Qt::SkipEmptyParts).join(" | ");
        lines << QString("Cell type pooling: %1; minimum cells per species: %2").arg(_traitPoolingMode.getCurrentText()).arg(_traitMinCells.getValue());
        lines << "Trait column: " + (_traitValueLabel.isEmpty() ? QString("Trait correlation") : _traitValueLabel);
        if (_traitSignificantOnly.isChecked()) {
            lines << QString("Only genes with a significant trait correlation (FDR < %1) are listed").arg(significanceLevel());
        }
    }

    lines << QString("Rows: %1").arg(rowCount);
    if (withTraitColumns && significantRows >= 0) {
        lines << QString("Rows with FDR < %1: %2").arg(significanceLevel()).arg(significantRows);
    }

    for (QString& line : lines) {
        line = "# " + line;
    }
    return lines;
}

void SettingsAction::exportTableViewToCSVForGenes(QTableView* tableView) {
    if (!tableView || !tableView->model()) {
        qWarning() << "No table to export.";
        return;
    }
    QAbstractItemModel* model = tableView->model();

    // suggested name: the content of the table
    QString suggestedName = "gene_table";
    if (!_reverseSearchTraitSelectionString.getString().isEmpty() && _traitSampleSize > 0) {
        suggestedName += "_" + fileNamePart(_reverseSearchTraitSelectionString.getString());
    }
    if (!_selectedClusterName.isEmpty()) {
        suggestedName += "_" + fileNamePart(_selectedClusterName);
    }

    const QString filePath = askForCsvPath(suggestedName + ".csv");
    if (filePath.isEmpty()) {
        qWarning() << "No file selected for saving.";
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not open file for writing: " << filePath;
        return;
    }
    QTextStream stream(&file);

    if (!isGeneTableModel(model)) {
        writeGenericTable(model, stream);
        return;
    }

    const bool withTrait = traitColumnsAvailable(model);

    // the rows exactly as shown: filtered (top-N genes, search, significant only) and sorted like the view
    QStringList rows;
    int significantRows = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        QStringList fields;
        const QModelIndex weightedIndex = model->index(row, kWeightedAppearanceColumn);
        fields << model->data(model->index(row, 0)).toString()
            << model->data(model->index(row, 1)).toString()
            << model->data(model->index(row, 2)).toString()
            << QString::number(weightedIndex.data(Qt::UserRole).toDouble(), 'f', 4)
            << QString::number(weightedIndex.data(kWeightedPValueRole).toDouble(), 'g', 4)
            << QString::number(weightedIndex.data(kWeightedFdrRole).toDouble(), 'g', 4)
            << model->data(model->index(row, kBestPositionColumn), Qt::UserRole).toString()
            << QString::number(model->data(model->index(row, kMeanDifferentialColumn), Qt::UserRole).toDouble(), 'f', 4)
            << QString::number(model->data(model->index(row, kPositiveSpeciesColumn), Qt::UserRole).toDouble(), 'f', 1)
            << QString::number(model->data(model->index(row, kMeanExpressionColumn), Qt::UserRole).toDouble(), 'f', 4);
        if (withTrait) {
            const QStringList traitValues = traitExportValues(model, row);
            fields << traitValues;
            if (model->index(row, kTraitCorrelationColumn).data(kTraitSignificanceFdrRole).toDouble() < significanceLevel()) {
                ++significantRows;
            }
        }
        rows << csvLine(fields);
    }

    if (_saveIncludeParameters.isChecked()) {
        for (const QString& line : buildExportParameterLines("gene table", static_cast<int>(rows.size()), withTrait, significantRows)) {
            stream << line << "\n";
        }
    }

    QStringList header = { "Gene", "Species Appearance", "Gene Appearance Species Names", "Weighted Appearance",
                           "Rank consistency p-value (robust rank aggregation)", "Rank consistency FDR",
                           "Best position in a species", "Mean differential expression (all species)",
                           "Species with positive differential (%)", "Mean expression in selected cells (mean over species)" };
    if (withTrait) {
        header << traitExportHeaders();
    }
    stream << csvLine(header) << "\n";
    for (const QString& row : rows) {
        stream << row << "\n";
    }
}

void SettingsAction::exportTraitMatrixToCSV(QTableView* tableView) {
    if (!tableView || !tableView->model()) {
        qWarning() << "No table to export.";
        return;
    }
    if (_traitMatrix.empty()) {
        QMessageBox::information(nullptr, "Trait matrix", "There is no trait correlation yet.\nChoose a trait and cell type(s) first.");
        return;
    }

    // genes: the ones listed in the gene table (filtered and sorted like the view) that are part of the matrix
    QStringList genes;
    std::vector<size_t> columns;
    QAbstractItemModel* model = tableView->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QString gene = model->data(model->index(row, 0)).toString();
        const auto it = _traitMatrix.geneIndex.find(gene);
        if (it != _traitMatrix.geneIndex.end()) {
            genes << gene;
            columns.push_back(it->second);
        }
    }

    QString suggestedName = "trait_matrix_" + fileNamePart(_traitMatrix.traitName);
    if (!_selectedClusterName.isEmpty()) {
        suggestedName += "_" + fileNamePart(_selectedClusterName);
    }
    const QString filePath = askForCsvPath(suggestedName + ".csv");
    if (filePath.isEmpty()) {
        qWarning() << "No file selected for saving.";
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not open file for writing: " << filePath;
        return;
    }
    QTextStream stream(&file);

    if (_saveIncludeParameters.isChecked()) {
        QStringList lines = buildExportParameterLines("trait matrix (one row per species, one column per gene: mean expression)", static_cast<int>(_traitMatrix.species.size()), true, -1);
        lines << QString("# Genes: %1 (the genes listed in the gene table)").arg(genes.size());
        for (const QString& line : lines) {
            stream << line << "\n";
        }
    }

    QStringList header = { "Species", "Trait value (" + _traitMatrix.traitName + ")", "Selected in species explorer" };
    header << genes;
    stream << csvLine(header) << "\n";

    for (size_t species = 0; species < _traitMatrix.species.size(); ++species) {
        QStringList fields;
        fields << _traitMatrix.species[species]
            << QString::number(_traitMatrix.trait[species], 'g', 8)
            << (_traitMatrix.selected[species] ? "True" : "False");
        for (const size_t column : columns) {
            fields << QString::number(static_cast<double>(_traitMatrix.expression[species][column]), 'g', 6);
        }
        stream << csvLine(fields) << "\n";
    }
}

void SettingsAction::exportTableViewToCSVPerGene(QTableView* tableView) {
    if (!tableView || !tableView->model()) {
        qWarning() << "No table to export.";
        return;
    }
    QAbstractItemModel* model = tableView->model();

    // The selected gene (if it is listed) -> only its species; otherwise every listed gene
    const QString selectedGene = _selectedGene.getString();
    int geneRowIndex = -1;
    if (!selectedGene.isEmpty()) {
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->data(model->index(row, 0)).toString() == selectedGene) {
                geneRowIndex = row;
                break;
            }
        }
    }

    QString suggestedName = geneRowIndex != -1 ? "species_statistics_" + fileNamePart(selectedGene) : "species_statistics_all_genes";
    if (!_selectedClusterName.isEmpty()) {
        suggestedName += "_" + fileNamePart(_selectedClusterName);
    }

    const QString filePath = askForCsvPath(suggestedName + ".csv");
    if (filePath.isEmpty()) {
        qWarning() << "No file selected for saving.";
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not open file for writing: " << filePath;
        return;
    }
    QTextStream stream(&file);

    if (!isGeneTableModel(model)) {
        writeGenericTable(model, stream);
        return;
    }

    const bool withTrait = traitColumnsAvailable(model);
    bool middleRedundant = false;
    const QString middleName = middleFractionName(_currentHierarchyItemsMiddleForTable, middleRedundant);
    const QStringList explorerSelection = _speciesExplorerInMap.getSelectedOptions();

    // weight of the gene in a species: position 1 -> 1 ... position N -> 1/N (see rankWeight); 0 when it is no top-N gene there
    const int weightTopN = _weightedTopN;
    const bool weightNegative = _weightedNegative;
    const std::function<double(const QString&, int, double)> rankWeightOf = [this, weightTopN, weightNegative](const QString& species, int storedRank, double meanSelected) {
        const auto it = _clusterNameToGeneNameToExpressionValue.find(species);
        if (it == _clusterNameToGeneNameToExpressionValue.end() || storedRank < 1 || weightTopN <= 0 || meanSelected <= 0.0) {
            return 0.0;
        }
        const int listLength = static_cast<int>(it->second.size());
        return rankWeight(weightNegative ? listLength - storedRank + 1 : storedRank, weightTopN);
        };

    QStringList rows;
    int significantRows = 0;
    const int firstRow = geneRowIndex != -1 ? geneRowIndex : 0;
    const int lastRow = geneRowIndex != -1 ? geneRowIndex : model->rowCount() - 1;
    for (int row = firstRow; row <= lastRow; ++row) {
        const QString gene = model->data(model->index(row, 0)).toString();
        const QStringList topNSpecies = model->data(model->index(row, 2)).toString().split(";", Qt::SkipEmptyParts);
        const QStringList statisticsLines = model->data(model->index(row, 3)).toString().split("\n", Qt::SkipEmptyParts);
        const QStringList traitValues = withTrait ? traitExportValues(model, row) : QStringList();
        if (withTrait && model->index(row, kTraitCorrelationColumn).data(kTraitSignificanceFdrRole).toDouble() < significanceLevel()) {
            ++significantRows;
        }
        rows << speciesStatisticsRows(gene, topNSpecies, statisticsLines, explorerSelection, !middleRedundant, traitValues, rankWeightOf,
            [this](const QString& species) { return speciesGeneListLength(species); }, weightNegative);
    }

    if (_saveIncludeParameters.isChecked()) {
        const QString title = geneRowIndex != -1 ? "species statistics of gene " + selectedGene : "species statistics of all listed genes";
        for (const QString& line : buildExportParameterLines(title, static_cast<int>(rows.size()), withTrait, significantRows)) {
            stream << line << "\n";
        }
    }

    QStringList header = { "Gene", "Top-N gene in species", "Species", "Species selected in explorer", "Mean Gene Differential Expression",
                           "Gene Appearance Rank", "Rank weight in species", "Rank percentile (% of genes, small = strong)",
                           "Log2 fold change (selected / other cells)", "Selected cells of the species (%)", "Fraction in Neuronal (%)" };
    if (!middleRedundant) {
        header << "Fraction in " + middleName + " (%)";
    }
    header << "Count of Selected" << "Mean Gene Expression of Selected" << "Count of Non Selected" << "Mean Gene Expression of Non Selected";
    if (withTrait) {
        header << traitExportHeaders();
    }
    stream << csvLine(header) << "\n";
    for (const QString& row : rows) {
        stream << row << "\n";
    }
}

void SettingsAction::populatePointDataConcurrently(QString datasetId, const std::vector<float>& pointVector, int numPoints, int numDimensions, std::vector<QString> dimensionNames)
{
    (void)QtConcurrent::run([this, datasetId, pointVector, numPoints, numDimensions, dimensionNames]() {
        auto pointDataset = mv::data().getDataset<Points>(datasetId);

        if (pointDataset.isValid())
        {
            pointDataset->setSelectionIndices({});
            if (!pointVector.empty() && numPoints > 0 && numDimensions > 0) {
                pointDataset->setData(pointVector.data(), numPoints, numDimensions);
                pointDataset->setDimensionNames(dimensionNames);
                mv::events().notifyDatasetDataChanged(pointDataset);
            }
        }
        });
}
void SettingsAction::enableActions()
{
    //_startComputationTriggerAction.setDisabled(false);
    _topNGenesFilter.setDisabled(false);
    _typeofTopNGenes.setDisabled(false);
    _clusterCountSortingType.setDisabled(false);
    _scatterplotReembedColorOption.setDisabled(false);
    _applyLogTransformation.setDisabled(false);
    _toggleScatterplotSelection.setDisabled(false);
    _usePreComputedTSNE.setDisabled(false);
    _tsnePerplexity.setDisabled(false);
    _performGeneTableTsnePerplexity.setDisabled(false);
    _performGeneTableTsneKnn.setDisabled(false);
    _performGeneTableTsneDistance.setDisabled(false);
    _performGeneTableTsneTrigger.setDisabled(false);
    _computeTreesToDisplayFromHierarchy.setDisabled(false);
    _referenceTreeDataset.setDisabled(false);
    _mainPointsDataset.setDisabled(false);
    _embeddingDataset.setDisabled(false);
    _speciesNamesDataset.setDisabled(false);
    _bottomClusterNamesDataset.setDisabled(false);
    _middleClusterNamesDataset.setDisabled(false);
    _topClusterNamesDataset.setDisabled(false);
    _speciesExplorerInMap.setDisabled(false);
    _topHierarchyClusterNamesFrequencyInclusionList.setDisabled(false);
    _speciesExplorerInMapTrigger.setDisabled(false);
    _saveGeneTable.setDisabled(false);
    _saveSpeciesTable.setDisabled(false);
    _revertRowSelectionChangesToInitial.setDisabled(false);
    _scatterplotEmbeddingPointsUMAPOption.setDisabled(false);
    _selectedSpeciesVals.setDisabled(false);
    _clusterOrderHierarchy.setDisabled(false);
    _rightClickedCluster.setDisabled(false);
    _topSelectedHierarchyStatus.setDisabled(false);
    _clearRightClickedCluster.setDisabled(false);
    _statusColorAction.setDisabled(false);
    _searchBox->setDisabled(false);
    enableDisableButtonsAutomatically();
    if (_statusColorAction.getString() == "C")
    {
        _startComputationTriggerAction.setDisabled(true);
    }
    else
    {
        _startComputationTriggerAction.setDisabled(false);
    }
    _toggleScatterplotSelection.setChecked(true);
    QApplication::processEvents();
}
void SettingsAction::disableActions()
{
    _statusColorAction.setString("R");
    _clearRightClickedCluster.trigger();
    _startComputationTriggerAction.setDisabled(true);
    _topNGenesFilter.setDisabled(true);
    _typeofTopNGenes.setDisabled(true);
    _clusterCountSortingType.setDisabled(true);
    _scatterplotReembedColorOption.setDisabled(true);
    _removeRowSelection.setDisabled(true);
    _speciesExplorerInMapTrigger.setDisabled(true);
    _saveGeneTable.setDisabled(true);
    _saveSpeciesTable.setDisabled(true);
    _usePreComputedTSNE.setDisabled(true);
    _applyLogTransformation.setDisabled(true);
    _speciesExplorerInMap.setDisabled(true);
    _revertRowSelectionChangesToInitial.setDisabled(true);
    _toggleScatterplotSelection.setDisabled(true);
    _tsnePerplexity.setDisabled(true);
    _performGeneTableTsnePerplexity.setDisabled(true);
    _performGeneTableTsneKnn.setDisabled(true);
    _performGeneTableTsneDistance.setDisabled(true);
    _performGeneTableTsneTrigger.setDisabled(true);
    _computeTreesToDisplayFromHierarchy.setDisabled(true);
    _referenceTreeDataset.setDisabled(true);
    _mainPointsDataset.setDisabled(true);
    _embeddingDataset.setDisabled(true);
    _speciesNamesDataset.setDisabled(true);
    _bottomClusterNamesDataset.setDisabled(true);
    _middleClusterNamesDataset.setDisabled(true);
    _topClusterNamesDataset.setDisabled(true);
    _scatterplotEmbeddingPointsUMAPOption.setDisabled(true);
    _topHierarchyClusterNamesFrequencyInclusionList.setDisabled(true);
    _selectedSpeciesVals.setDisabled(true);
    _clusterOrderHierarchy.setDisabled(true);
    _rightClickedCluster.setDisabled(true);
    _topSelectedHierarchyStatus.setDisabled(true);
    _clearRightClickedCluster.setDisabled(true);
    _statusColorAction.setDisabled(true);
    _searchBox->setDisabled(true);
    QApplication::processEvents();
}

void SettingsAction::enableDisableButtonsAutomatically()
{

    bool optionsActionHasOptions = !_speciesExplorerInMap.getOptions().isEmpty();
    bool stringActionHasOptions = !_selectedSpeciesVals.getString().isEmpty();

    bool bothListsEqual = false;
    Q_UNUSED(bothListsEqual); // kept for the (currently disabled) button-state logic below
    if (optionsActionHasOptions && stringActionHasOptions) {
        QStringList temp = _selectedSpeciesVals.getString().split(" @%$,$%@ ");
        QStringList species = _speciesExplorerInMap.getSelectedOptions();

        std::sort(temp.begin(), temp.end());
        std::sort(species.begin(), species.end());
        bothListsEqual = (temp == species);
    }
    _revertRowSelectionChangesToInitial.setDisabled(false);
    _speciesExplorerInMapTrigger.setDisabled(false);
    //if (!stringActionHasOptions)
    //{
    //    _revertRowSelectionChangesToInitial.setDisabled(true);
    //}
    //else
    //{
    //    if (!optionsActionHasOptions)
    //    {

    //        _revertRowSelectionChangesToInitial.setDisabled(false);
    //    }
    //    else
    //    {
    //        if (bothListsEqual)
    //        {

    //            _revertRowSelectionChangesToInitial.setDisabled(true);
    //        }
    //        else
    //        {

    //            _revertRowSelectionChangesToInitial.setDisabled(false);
    //        }
    //    }
    //}



    //if (!optionsActionHasOptions)
    //{
    //    _speciesExplorerInMapTrigger.setDisabled(true);

    //}
    //else 
    //{
    //    _speciesExplorerInMapTrigger.setDisabled(false);
    //}



}


void SettingsAction::populatePointData(QString& datasetId, std::vector<float>& pointVector, int& numPoints, int& numDimensions, std::vector<QString>& dimensionNames)
{
    auto pointDataset = mv::data().getDataset<Points>(datasetId);

    if (pointDataset.isValid())
    {
        pointDataset->setSelectionIndices({});
        if (pointVector.size() > 0 && numPoints > 0 && numDimensions > 0) {
            pointDataset->setData(pointVector.data(), numPoints, numDimensions);
            pointDataset->setDimensionNames(dimensionNames);
            mv::events().notifyDatasetDataChanged(pointDataset);
        }

    }
}
void SettingsAction::populateClusterData(QString& datasetId, std::map<QString, std::pair<QColor, std::vector<int>>>& clusterMap)
{

    auto colorDataset = mv::data().getDataset<Clusters>(datasetId);
    if (colorDataset.isValid())
    {
        colorDataset->getClusters() = QVector<Cluster>();
        for (const auto& pair : clusterMap)
        {
            QString clusterName = pair.first;
            std::pair<QColor, std::vector<int>> value = pair.second;
            QColor clusterColor = value.first;
            std::vector<std::uint32_t> clusterIndices(value.second.begin(), value.second.end());

            if (clusterIndices.size() > 0)
            {
                Cluster clusterValue;
                clusterValue.setName(clusterName);
                clusterValue.setColor(clusterColor);
                clusterValue.setIndices(clusterIndices);
                colorDataset->addCluster(clusterValue);
            }
        }

        mv::events().notifyDatasetDataChanged(colorDataset);
    }


}

void SettingsAction::clearTableSelection(QTableView* tableView) {
    if (tableView && tableView->selectionModel()) {
        // Clear the current selection
        tableView->clearSelection();

        // Temporarily disable the selection mode to remove highlight
        QAbstractItemView::SelectionMode oldMode = tableView->selectionMode();
        tableView->setSelectionMode(QAbstractItemView::NoSelection);

        // Clear the current index
        tableView->selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);

        // Restore the original selection mode
        tableView->setSelectionMode(oldMode);

        // Update the view to ensure changes are reflected
        tableView->update();
    }
    else {
        qDebug() << "TableView or its selection model is null";
    }
}

// ============================================================================================================
//  Species explorer selection
// ============================================================================================================

void SettingsAction::setSpeciesExplorerSelection(const QStringList& species)
{
    QStringList current = _speciesExplorerInMap.getSelectedOptions();
    QStringList wanted = species;
    current.sort();
    wanted.sort();
    if (current == wanted) {
        return; // nothing to do: avoids a redundant selectedOptionsChanged -> table recolor -> correlation refresh
    }
    _speciesExplorerInMap.setSelectedOptions(species);
}

QStringList SettingsAction::resolveExplorerSelectionForGene(const QStringList& geneSpecies)
{
    // Clicking a gene always selects the species in which that gene appears (empty entries come from genes without species).
    QStringList species = geneSpecies;
    species.removeAll(QString());
    return species;
}

void SettingsAction::removeSelectionTableRows(QStringList* selectedLeaves)
{
    //check if _selectionDetailsTable is valid
    if (_selectionDetailsTable == nullptr || selectedLeaves == nullptr) {
        return;
    }

    clearTableSelection(_selectionDetailsTable);

    QAbstractItemModel* model = _selectionDetailsTable->model();

    //check if model is valid
    if (model == nullptr) {
        return;
    }

    //auto colorValues = getSystemModeColor();
    //auto systemColor = colorValues[0];
    //auto valuesColor = colorValues[1];

    // Iterate through all rows
    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex index = model->index(row, 0); // Assuming species name is in column 0
        QString species = model->data(index, Qt::UserRole).toString();

        // Check if the species is one of the selected species
        if (selectedLeaves->contains(species)) {
            for (int col = 0; col < model->columnCount(); ++col) {
                QModelIndex cellIndex = model->index(row, col);
                _selectionDetailsTable->model()->setData(cellIndex, QBrush(QColor("#00A2ED")), Qt::BackgroundRole);
                _selectionDetailsTable->model()->setData(cellIndex, QBrush(QColor("#000000")), Qt::ForegroundRole);
            }
        }
        else
        {
            //remove existing color from rows
            for (int col = 0; col < model->columnCount(); ++col) {
                QModelIndex cellIndex = model->index(row, col);
                _selectionDetailsTable->model()->setData(cellIndex, QBrush(QColor("#FFFFFF")), Qt::BackgroundRole);
                _selectionDetailsTable->model()->setData(cellIndex, QBrush(QColor("#000000")), Qt::ForegroundRole);
            }
        }
    }

}

