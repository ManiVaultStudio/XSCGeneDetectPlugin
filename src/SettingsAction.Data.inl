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
    //startCodeTimer("createModelFromData");
    QStandardItemModel* model = new QStandardItemModel();
    _initColumnNames = { "ID", "Species \nAppearance", "Gene Appearance Species Names", "Statistics", "Trait Correlation" };
    model->setColumnCount(_initColumnNames.size());
    model->setRowCount(static_cast<int>(_totalGeneList.size()));
    model->setHorizontalHeaderLabels(_initColumnNames);

    QStringList headers = _initColumnNames;
    _hiddenShowncolumns.setOptions(headers);
    _hiddenShowncolumns.setSelectedOptions({ headers[0], headers[1], headers[4] });

    // Pre-index the nested species map once so row construction does not scan
    // every species for every gene.
    QHash<QString, QVector<QPair<QString, Stats>>> geneToSpeciesStats;
    geneToSpeciesStats.reserve(static_cast<int>(_totalGeneList.size()));
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

    for (int rowIndex = 0; rowIndex < static_cast<int>(_totalGeneList.size()); ++rowIndex) {
        const auto& gene = _totalGeneList[rowIndex];
        const auto statisticsValuesForSpecies = geneToSpeciesStats.value(gene);
        const auto rankcounter = geneToSpeciesRank.value(gene);

        model->setItem(rowIndex, 0, new QStandardItem(gene)); // ID(string) should sort by string

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
        auto* countItem = new QStandardItem(); // Gene Appearances (int) should sort by int
        countItem->setData(count, Qt::DisplayRole);
        countItem->setData(count, Qt::UserRole); // Use Qt::UserRole or another custom role for sorting by integer
        model->setItem(rowIndex, 1, countItem);

        model->setItem(rowIndex, 2, new QStandardItem(speciesGeneAppearancesComb)); // Gene Appearance Species Names (string) should sort by string

        QString formattedStatistics;
        formattedStatistics.reserve(statisticsValuesForSpecies.size() * 96);
        for (const auto& speciesStats : statisticsValuesForSpecies) {
            const auto& species = speciesStats.first;
            const auto& stats = speciesStats.second;
            formattedStatistics += QString("Species: %1, Rank: %2, AbundanceTop: %3,  AbundanceMiddle: %4,  CountAbundanceNumerator: %5, MeanSelected: %6, CountSelected: %7, MeanNotSelected: %8, CountNotSelected: %9;\n")
                .arg(species)
                .arg(rankcounter.value(species))
                .arg(stats.abundanceTop)
                .arg(stats.abundanceMiddle)
                .arg(stats.countAbundanceNumerator)
                .arg(stats.meanSelected, 0, 'f', 2)
                .arg(stats.countSelected)
                .arg(stats.meanNonSelected, 0, 'f', 2)
                .arg(stats.countNonSelected);
        }
        model->setItem(rowIndex, 3, new QStandardItem(formattedStatistics)); // Statistics (string) should sort by string

        // Retrieve pre-calculated Trait Correlation from _geneTraitValuesComputed
        float correlation = 0.0f;
        auto itCorr = _geneTraitValuesComputed.find(gene);
        if (itCorr != _geneTraitValuesComputed.end()) {
            correlation = itCorr->second;
        }
        else {
            // Uncomment to debug key mismatches in your Application Output
            // qDebug() << "Gene not found in _geneTraitValuesComputed:" << gene;
        }

        auto* correlationItem = new QStandardItem();
        // Numeric DisplayRole so sorting is numeric; the bar delegate formats the text and draws the bar
        correlationItem->setData(static_cast<double>(correlation), Qt::DisplayRole);
        correlationItem->setData(static_cast<double>(correlation), Qt::UserRole);
        model->setItem(rowIndex, 4, correlationItem); // Trait Correlation (float)
    }

    //stopCodeTimer("createModelFromData");

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

    // The gene table is recognised by its header, not by its column count: it grew a fifth
    // "Trait Correlation" column, so a hard coded "== 4" check no longer matches.
    bool isGeneTableModel(const QAbstractItemModel* model)
    {
        return model && model->columnCount() >= 4 && model->headerData(3, Qt::Horizontal).toString() == "Statistics";
    }

    // The correlation column (if present) holds a double in DisplayRole.
    QString correlationText(const QAbstractItemModel* model, int row)
    {
        if (!model || model->columnCount() < 5) {
            return QString();
        }
        bool ok = false;
        const double value = model->data(model->index(row, 4), Qt::UserRole).toDouble(&ok);
        return ok ? QString::number(value, 'f', 3) : model->data(model->index(row, 4)).toString();
    }

} // namespace

void SettingsAction::exportTableViewToCSVForGenes(QTableView* tableView) {
    if (!tableView) {
        qWarning() << "TableView is null.";
        return;
    }

    QAbstractItemModel* model = tableView->model();
    if (!model) {
        qWarning() << "TableView model is null.";
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(nullptr, "Save CSV", "", "CSV Files (*.csv);;All Files (*)");
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

    if (isGeneTableModel(model))
    {
        const bool hasCorrelation = model->columnCount() >= 5;
        stream << "ID,Species Appearance,Gene Appearance Species Names";
        if (hasCorrelation) {
            stream << ",Trait Correlation";
        }
        stream << "\n";

        for (int row = 0; row < model->rowCount(); ++row) {
            stream << csvEscape(model->data(model->index(row, 0)).toString()) << ","
                << csvEscape(model->data(model->index(row, 1)).toString()) << ","
                << csvEscape(model->data(model->index(row, 2)).toString());
            if (hasCorrelation) {
                stream << "," << correlationText(model, row);
            }
            stream << "\n";
        }
    }
    else
    {
        // Write header
        for (int col = 0; col < model->columnCount(); ++col) {
            if (col > 0) {
                stream << ",";
            }
            stream << model->headerData(col, Qt::Horizontal).toString();
        }
        stream << "\n";

        // Write data
        for (int row = 0; row < model->rowCount(); ++row) {
            for (int col = 0; col < model->columnCount(); ++col) {
                if (col > 0) {
                    stream << ",";
                }
                stream << model->data(model->index(row, col)).toString();
            }
            stream << "\n";
        }
    }


    file.close();
}

QString computeMapFromStatistics(QString geneName, QStringList geneAppearanceSpeciesNamesList, QStringList statisticsList, const QString& traitCorrelation = QString())
{
    QString finalString = "";

    for (int i = 0; i < statisticsList.size(); i++)
    {
        QString tempString = statisticsList[i];
        QStringList pairs = tempString.split(", ");

        QString speciesName = "";
        QString rank = "";
        QString abundanceTop = "";
        QString abundanceMiddle = "";
        QString countAbundanceNumerator = "";
        QString meanSelected = "";
        QString countSelected = "";
        QString meanNotSelected = "";
        QString countNotSelected = "";

        for (const QString& pair : pairs) {
            QStringList keyValue = pair.split(": ");
            if (keyValue.size() == 2) {
                QString key = keyValue[0].trimmed();
                QString value = keyValue[1].trimmed();

                if (key == "Species") {
                    speciesName = value;
                }
                else if (key == "Rank") {
                    rank = value;
                }
                else if (key == "AbundanceTop") {
                    abundanceTop = value;
                }
                else if (key == "AbundanceMiddle") {
                    abundanceMiddle = value;
                }
                else if (key == "CountAbundanceNumerator") {
                    countAbundanceNumerator = value;
                }
                else if (key == "MeanSelected") {
                    meanSelected = value;
                }
                else if (key == "CountSelected") {
                    countSelected = value;
                }
                else if (key == "MeanNotSelected") {
                    meanNotSelected = value;
                }
                else if (key == "CountNotSelected") {
                    countNotSelected = value;
                }
            }
        }

        finalString += csvEscape(geneName);
        if (geneAppearanceSpeciesNamesList.contains(speciesName)) {
            finalString += ", True";
        }
        else {
            finalString += ", False";
        }
        finalString += ", " + csvEscape(speciesName);
        finalString += ", " + QString::number(meanSelected.toFloat() - meanNotSelected.toFloat());
        finalString += ", " + rank;
        finalString += ", " + abundanceTop;
        finalString += ", " + abundanceMiddle;
        finalString += ", " + countSelected;
        finalString += ", " + meanSelected;
        finalString += ", " + countNotSelected;
        finalString += ", " + meanNotSelected;
        if (!traitCorrelation.isNull()) {
            finalString += ", " + traitCorrelation;
        }
        if (i < statisticsList.size() - 1) {
            finalString += "\n";
        }
    }

    return finalString;
}


void SettingsAction::exportTableViewToCSVPerGene(QTableView* tableView) {
    if (!tableView) {
        qWarning() << "TableView is null.";
        return;
    }

    QAbstractItemModel* model = tableView->model();
    if (!model) {
        qWarning() << "TableView model is null.";
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(nullptr, "Save CSV", "", "CSV Files (*.csv);;All Files (*)");
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

    // Find the row index of the selected gene for column 0
    int geneRowIndex = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->data(model->index(row, 0)).toString() == _selectedGene.getString()) {
            geneRowIndex = row;
            break;
        }
    }


    qDebug() << "Gene row index: " << geneRowIndex;

    // If the gene row index is found, write the matching row

    if (isGeneTableModel(model)) {
        //ID,Species Appearance,Gene Appearance Species Names,Statistics
        QString ergicString = "Fraction in " + _topSelectedHierarchyStatus.getString();
        QStringList headers = { "Gene", "Species Appearance", "Species", "Mean Gene Differential Expression", "Gene Appearance Rank", "Fraction in Neuronal", ergicString ,"Count of Selected", "Mean Gene Expression of Selected", "Count of Non Selected","Mean Gene Expression of Non Selected" };
        if (model->columnCount() >= 5) {
            headers << "Trait Correlation";
        }
        QString headerNames = headers.join(",");
        stream << headerNames;
        stream << "\n";

        if (geneRowIndex != -1)
        {
            QString geneName = model->data(model->index(geneRowIndex, 0)).toString();
            QString speciesAppearance = model->data(model->index(geneRowIndex, 1)).toString();
            QString geneAppearanceSpeciesNames = model->data(model->index(geneRowIndex, 2)).toString();
            QStringList geneAppearanceSpeciesNamesList = geneAppearanceSpeciesNames.split(";");
            QString statistics = model->data(model->index(geneRowIndex, 3)).toString();
            QStringList statisticsList = statistics.split("\n");
            if (!statisticsList.isEmpty() && statisticsList.last().isEmpty()) {
                statisticsList.removeLast();
            }
            QString finalString = computeMapFromStatistics(geneName, geneAppearanceSpeciesNamesList, statisticsList, correlationText(model, geneRowIndex));
            stream << finalString;

        }
        else
        {
            for (int row = 0; row < model->rowCount(); ++row) {
                QString geneName = model->data(model->index(row, 0)).toString();
                QString speciesAppearance = model->data(model->index(row, 1)).toString();
                QString geneAppearanceSpeciesNames = model->data(model->index(row, 2)).toString();
                QStringList geneAppearanceSpeciesNamesList = geneAppearanceSpeciesNames.split(";");
                QString statistics = model->data(model->index(row, 3)).toString();
                QStringList statisticsList = statistics.split("\n");
                if (!statisticsList.isEmpty() && statisticsList.last().isEmpty()) {
                    statisticsList.removeLast();
                }
                QString finalString = computeMapFromStatistics(geneName, geneAppearanceSpeciesNamesList, statisticsList, correlationText(model, row));
                stream << finalString;
                if (row < model->rowCount() - 1) {
                    stream << "\n";
                }
            }
        }


    }
    else
    {
        // Write header
        for (int col = 0; col < model->columnCount(); ++col) {
            if (col > 0) {
                stream << ",";
            }
            QString headerVal = model->headerData(col, Qt::Horizontal).toString();
            // Remove all occurrences of "\n" from the headerVal
            headerVal.replace("\n", "");
            stream << headerVal;
        }
        stream << "\n";



        for (int row = 0; row < model->rowCount(); ++row) {
            for (int col = 0; col < model->columnCount(); ++col) {
                if (col > 0) {
                    stream << ",";
                }
                stream << model->data(model->index(row, col)).toString();
            }
            stream << "\n";
        }
    }

    file.close();
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

