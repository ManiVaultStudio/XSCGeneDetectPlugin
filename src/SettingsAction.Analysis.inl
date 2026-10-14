/*
void SettingsAction::triggerTrippleHierarchyFrequencyChange()
{
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
    {
        return;
    }
    _clusterSpeciesFrequencyMap.clear();
    auto startTimer = std::chrono::high_resolution_clock::now();
    //qDebug() << "computeFrequencyMapForHierarchyItemsChange for all 3 levels Start";

    if (!_speciesNamesDataset.getCurrentDataset().isValid() || !_mainPointsDataset.getCurrentDataset().isValid() || !_topClusterNamesDataset.getCurrentDataset().isValid() || !_middleClusterNamesDataset.getCurrentDataset().isValid() || !_bottomClusterNamesDataset.getCurrentDataset().isValid()) {
        qDebug() << "Datasets are not valid";
        return;
    }

    auto speciesClusterDatasetFull = mv::data().getDataset<Clusters>(_speciesNamesDataset.getCurrentDataset().getDatasetId());
    auto mainPointDatasetFull = mv::data().getDataset<Points>(_mainPointsDataset.getCurrentDataset().getDatasetId());
    auto numOfPoints = mainPointDatasetFull->getNumPoints();
    std::vector<bool> topClusterNames(numOfPoints, true);
    std::vector<bool> middleClusterNames(numOfPoints, true);
    std::vector<bool> bottomClusterNames(numOfPoints, true);
    QStringList topInclusionList;
    QStringList middleInclusionList;
    QStringList bottomInclusionList;
    auto topClusterDataset = mv::data().getDataset<Clusters>(_topClusterNamesDataset.getCurrentDataset().getDatasetId());
    auto middleClusterDataset = mv::data().getDataset<Clusters>(_middleClusterNamesDataset.getCurrentDataset().getDatasetId());
    auto bottomClusterDataset = mv::data().getDataset<Clusters>(_bottomClusterNamesDataset.getCurrentDataset().getDatasetId());

    auto processTopClusters = [&]() {
        if (topClusterDataset.isValid())
        {
            for (const auto& cluster : topClusterDataset->getClusters())
            {
                if (!topInclusionList.contains(cluster.getName()))
                {
                    for (const auto& index : cluster.getIndices())
                    {
                        topClusterNames[index] = false;
                    }
                }
            }
        }
        };

    auto processMiddleClusters = [&]() {
        if (middleClusterDataset.isValid())
        {
            for (const auto& cluster : middleClusterDataset->getClusters())
            {
                if (!middleInclusionList.contains(cluster.getName()))
                {
                    for (const auto& index : cluster.getIndices())
                    {
                        middleClusterNames[index] = false;
                    }
                }
            }
        }
        };

    auto processBottomClusters = [&]() {
        if (bottomClusterDataset.isValid())
        {
            for (const auto& cluster : bottomClusterDataset->getClusters())
            {
                if (!bottomInclusionList.contains(cluster.getName()))
                {
                    for (const auto& index : cluster.getIndices())
                    {
                        bottomClusterNames[index] = false;
                    }
                }
            }
        }
        };

    // Run the three tasks in parallel
    QFuture<void> topFuture = QtConcurrent::run(processTopClusters);
    QFuture<void> middleFuture = QtConcurrent::run(processMiddleClusters);
    QFuture<void> bottomFuture = QtConcurrent::run(processBottomClusters);

    // Wait for all tasks to complete
    topFuture.waitForFinished();
    middleFuture.waitForFinished();
    bottomFuture.waitForFinished();

    if (speciesClusterDatasetFull.isValid() && mainPointDatasetFull.isValid())
    {
        auto speciesclusters = speciesClusterDatasetFull->getClusters();
        for (const auto& species : speciesclusters) {
            auto speciesIndices = species.getIndices();
            auto speciesName = species.getName();
            int topCount = std::count_if(speciesIndices.begin(), speciesIndices.end(), [&topClusterNames](int index) {
                return topClusterNames[index];
                });
            int middleCount = std::count_if(speciesIndices.begin(), speciesIndices.end(), [&middleClusterNames](int index) {
                return middleClusterNames[index];
                });
            int bottomCount = std::count_if(speciesIndices.begin(), speciesIndices.end(), [&bottomClusterNames](int index) {
                return bottomClusterNames[index];
                });

            _clusterSpeciesFrequencyMap[speciesName]["topCells"] = topCount;
            _clusterSpeciesFrequencyMap[speciesName]["middleCells"] = middleCount;
            _clusterSpeciesFrequencyMap[speciesName]["bottomCells"] = bottomCount;
        }
    }

    auto endTimer = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTimer - startTimer).count();
    qDebug() << "Time taken for computeFrequencyMapForHierarchyItemsChange for all 3 levels: " + QString::number(duration / 1000.0) + " s";
}
*/

QString SettingsAction::filterData(QString& key, const QString& category, const QJsonObject& data) {
    QJsonObject result;
    QJsonObject keyObject;
    for (auto it = data.begin(); it != data.end(); ++it) {
        QJsonObject subObject = it.value().toObject();
        if (subObject.contains(category)) {
            QJsonObject categoryObject = subObject.value(category).toObject();
            if (categoryObject.contains(key)) {
                keyObject.insert(it.key(), categoryObject.value(key)); // Directly insert the value
            }
        }
    }
    result.insert("type", key);
    result.insert("data", keyObject);
    QJsonDocument doc(result);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact)); // Use Compact to remove newlines
}
void SettingsAction::updateButtonTriggered()
{
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
    {
        qDebug() << "Map for hierarchy items change method stop for project load blocker is checked";
        return;
    }

    try {
        // _startComputationTriggerAction.setDisabled(true);
        startCodeTimer("UpdateGeneFilteringTrigger");
        //startCodeTimer("Part1");

        int groupIDDeletion = 10;
        int groupID1 = 10 * 2;
        int groupID2 = 10 * 3;
        clearTemporaryDatasetHandles();
        removeDatasets(groupIDDeletion);
        auto pointsDataset = _mainPointsDataset.getCurrentDataset();
        auto embeddingDataset = _embeddingDataset.getCurrentDataset();
        auto speciesDataset = _speciesNamesDataset.getCurrentDataset();
        auto clusterDataset = _bottomClusterNamesDataset.getCurrentDataset();
        auto referenceTreeDataset = _referenceTreeDataset.getCurrentDataset();
        _selectedSpeciesVals.setString("");
        _geneNamesConnection.setString("");
        bool isValid = false;

        QString referenceTreedatasetId = "";
        //stopCodeTimer("Part1");
        //startCodeTimer("Part2");
        if (!pointsDataset.isValid() || !embeddingDataset.isValid() || !speciesDataset.isValid() || !clusterDataset.isValid() || !referenceTreeDataset.isValid())
        {
            qDebug() << "No datasets selected";
            //_startComputationTriggerAction.setDisabled(false);
            return;
        }
        if (pointsDataset->getSelectionIndices().size() < 1)
        {
            qDebug() << "No points selected";
            //_startComputationTriggerAction.setDisabled(false);
            return;
        }
        /*if (_selectedPointsTSNEDataset.isValid())
        {
            _selectedPointsTSNEDataset->setSelectionIndices({});
        }*/
        //stopCodeTimer("Part2");
        //startCodeTimer("Part3");
        _clusterNameToGeneNameToExpressionValue.clear();
        referenceTreedatasetId = referenceTreeDataset->getId();
        isValid = speciesDataset->getParent() == pointsDataset && clusterDataset->getParent() == pointsDataset && embeddingDataset->getParent() == pointsDataset;
        if (!isValid)
        {
            qDebug() << "Datasets are not valid";
            //_startComputationTriggerAction.setDisabled(false);
            return;
        }
        _selectedIndicesFromStorage.clear();
        _selectedIndicesFromStorage = pointsDataset->getSelectionIndices();
        markSelectionChanged(); // cached trait-correlation means refer to the previous selection

        auto embeddingDatasetRaw = mv::data().getDataset<Points>(embeddingDataset->getId());
        auto pointsDatasetRaw = mv::data().getDataset<Points>(pointsDataset->getId());
        auto pointsDatasetallColumnNameList = pointsDatasetRaw->getDimensionNames();
        auto embeddingDatasetallColumnNameList = embeddingDatasetRaw->getDimensionNames();
        //stopCodeTimer("Part3");
        //startCodeTimer("Part4");
        std::vector<int> embeddingDatasetColumnIndices(embeddingDatasetallColumnNameList.size());
        std::iota(embeddingDatasetColumnIndices.begin(), embeddingDatasetColumnIndices.end(), 0);

        std::vector<int> pointsDatasetallColumnIndices(pointsDatasetallColumnNameList.size());
        std::iota(pointsDatasetallColumnIndices.begin(), pointsDatasetallColumnIndices.end(), 0);
        //stopCodeTimer("Part4");
        {

            if (_selectedIndicesFromStorage.size() > 0 && embeddingDatasetColumnIndices.size() > 0)
            {
                //startCodeTimer("Part5");
                auto speciesDatasetRaw = mv::data().getDataset<Clusters>(speciesDataset->getId());
                auto clusterDatasetRaw = mv::data().getDataset<Clusters>(clusterDataset->getId());
                auto clusterDatasetName = clusterDatasetRaw->getGuiName();
                auto clustersValuesAll = clusterDatasetRaw->getClusters();
                auto speciesValuesAll = speciesDatasetRaw->getClusters();

                std::map<QString, std::pair<QColor, std::vector<int>>> selectedClustersMap;
                std::map<QString, std::pair<QColor, std::vector<int>>> selectedSpeciesMap;
                //stopCodeTimer("Part5");
                if (!speciesValuesAll.empty() && !clustersValuesAll.empty())
                {

                    //if (_selectedPointsTSNEDataset.isValid())
                    //{
                        //auto datasetIDLowRem = _selectedPointsTSNEDataset.getDatasetId();
                        //mv::events().notifyDatasetAboutToBeRemoved(_selectedPointsTSNEDataset);
                        //mv::data().removeDataset(_selectedPointsTSNEDataset);
                        //mv::events().notifyDatasetRemoved(datasetIDLowRem, PointType);
                    //}

                   // _selectedPointsDataset = Dataset<Points>();
                    //_selectedPointsEmbeddingDataset = Dataset<Points>();
                    //startCodeTimer("Part6.1");
                    /*if (!_selectedPointsDataset.isValid())
                    {
                        _selectedPointsDataset = mv::data().createDataset("Points", "SelectedPointsDataset");
                        _selectedPointsDataset->setGroupIndex(10);
                        mv::events().notifyDatasetAdded(_selectedPointsDataset);

                    }*/

                    pointsDatasetRaw->setSelectionIndices(_selectedIndicesFromStorage);
                    _selectedPointsDataset = pointsDatasetRaw->createSubsetFromSelection("SelectedPointsDataset");
                    _selectedPointsDataset->setGroupIndex(groupIDDeletion);

                    if (!_tsneDatasetExpressionColors.isValid())
                    {
                        _tsneDatasetExpressionColors = mv::data().createDataset("Points", "TSNEDatasetExpressionColors", _selectedPointsDataset);
                        _tsneDatasetExpressionColors->setGroupIndex(groupIDDeletion);
                        mv::events().notifyDatasetAdded(_tsneDatasetExpressionColors);

                    }

                    embeddingDatasetRaw->setSelectionIndices(_selectedIndicesFromStorage);
                    _selectedPointsEmbeddingDataset = embeddingDatasetRaw->createSubsetFromSelection("TSNEDataset", _selectedPointsDataset);
                    _selectedPointsEmbeddingDataset->setGroupIndex(groupIDDeletion);


                    if (!_tsneDatasetSpeciesColors.isValid())
                    {
                        _tsneDatasetSpeciesColors = mv::data().createDataset("Cluster", "TSNEDatasetSpeciesColors", _selectedPointsDataset);
                        _tsneDatasetSpeciesColors->setGroupIndex(groupIDDeletion);
                        mv::events().notifyDatasetAdded(_tsneDatasetSpeciesColors);
                    }

                    if (!_tsneDatasetClusterColors.isValid())
                    {
                        _tsneDatasetClusterColors = mv::data().createDataset("Cluster", "TSNEDatasetClusterColors", _selectedPointsDataset);
                        _tsneDatasetClusterColors->setGroupIndex(groupIDDeletion);
                        mv::events().notifyDatasetAdded(_tsneDatasetClusterColors);
                    }


                    if (!_filteredUMAPDatasetPoints.isValid())
                    {
                        _filteredUMAPDatasetPoints = mv::data().createDataset("Points", "Filtered UMAP Dataset Points");
                        _filteredUMAPDatasetPoints->setGroupIndex(groupID1);
                        mv::events().notifyDatasetAdded(_filteredUMAPDatasetPoints);
                        if (!_filteredUMAPDatasetColors.isValid())
                        {
                            //need to delete

                        }
                        if (!_filteredUMAPDatasetClusters.isValid())
                        {
                            //need to delete

                        }
                        _filteredUMAPDatasetColors = mv::data().createDataset("Points", "Filtered UMAP Dataset Colors", _filteredUMAPDatasetPoints);
                        _filteredUMAPDatasetColors->setGroupIndex(groupID1);
                        mv::events().notifyDatasetAdded(_filteredUMAPDatasetColors);

                        _filteredUMAPDatasetClusters = mv::data().createDataset("Cluster", "Filtered UMAP Dataset Clusters", _filteredUMAPDatasetPoints);
                        _filteredUMAPDatasetClusters->setGroupIndex(groupID1);
                        mv::events().notifyDatasetAdded(_filteredUMAPDatasetClusters);

                    }



                    /*if (!_selectedPointsEmbeddingDataset.isValid())
                    {
                        _selectedPointsEmbeddingDataset = mv::data().createDataset("Points", "TSNEDataset", _selectedPointsDataset);
                        _selectedPointsEmbeddingDataset->setGroupIndex(10);
                        mv::events().notifyDatasetAdded(_selectedPointsEmbeddingDataset);

                    }*/


                    if (!_geneSimilarityPoints.isValid())
                    {
                        _geneSimilarityPoints = mv::data().createDataset("Points", "GeneSimilarityPoints");
                        _geneSimilarityPoints->setGroupIndex(groupID2);
                        mv::events().notifyDatasetAdded(_geneSimilarityPoints);
                    }
                    if (!_geneSimilarityClusterColoring.isValid())
                    {
                        _geneSimilarityClusterColoring = mv::data().createDataset("Cluster", "GeneSimilarityClusterColoring", _geneSimilarityPoints);
                        _geneSimilarityClusterColoring->setGroupIndex(groupID2);
                        mv::events().notifyDatasetAdded(_geneSimilarityClusterColoring);

                    }
                    //_geneSimilarityClusters.clear();
                    //stopCodeTimer("Part6.1");
                    if (_selectedPointsDataset.isValid() && _selectedPointsEmbeddingDataset.isValid() && _tsneDatasetSpeciesColors.isValid() && _tsneDatasetClusterColors.isValid() && _geneSimilarityPoints.isValid() && _geneSimilarityClusterColoring.isValid())
                    {
                        //startCodeTimer("Part6.2");
                        //_tsneDatasetSpeciesColors->getClusters() = QVector<Cluster>();
                        //events().notifyDatasetDataChanged(_tsneDatasetSpeciesColors);
                        //_tsneDatasetClusterColors->getClusters() = QVector<Cluster>();
                        //events().notifyDatasetDataChanged(_tsneDatasetClusterColors);
                        _geneSimilarityClusterColoring->getClusters() = QVector<Cluster>();
                        events().notifyDatasetDataChanged(_geneSimilarityClusterColoring);
                        //stopCodeTimer("Part6.2");
                        //startCodeTimer("Part7");
                        //startCodeTimer("Part7.1");
                        int selectedIndicesFromStorageSize = static_cast<int>(_selectedIndicesFromStorage.size());
                        int pointsDatasetColumnsSize = static_cast<int>(pointsDatasetallColumnIndices.size());
                        int embeddingDatasetColumnsSize = static_cast<int>(embeddingDatasetColumnIndices.size());
                        //QString datasetIdEmb = _selectedPointsDataset->getId();
                        //QString datasetId = _selectedPointsEmbeddingDataset->getId();
                        int dimofDatasetExp = 1;
                        std::vector<QString> dimensionNamesExp = { "Expression" };
                        QString datasetIdExp = _tsneDatasetExpressionColors->getId();
                        //stopCodeTimer("Part7.1");
                        //startCodeTimer("Part7.2");

                        // Define result containers outside the lambda functions to ensure they are accessible later
                        //std::vector<float> resultContainerForSelectedPoints(selectedIndicesFromStorageSize * pointsDatasetColumnsSize);
                        //std::vector<float> resultContainerForSelectedEmbeddingPoints(selectedIndicesFromStorageSize * embeddingDatasetColumnsSize);
                        std::vector<float> resultContainerColorPoints(selectedIndicesFromStorageSize, -1.0f);

                        //first thread start
                        //auto future1 = std::async(std::launch::async, [&]() {
                            //pointsDatasetRaw->populateDataForDimensions(resultContainerForSelectedPoints, pointsDatasetallColumnIndices, _selectedIndicesFromStorage);
                           // });

                        //second thread start
                       // auto future2 = std::async(std::launch::async, [&]() {
                            //embeddingDatasetRaw->populateDataForDimensions(resultContainerForSelectedEmbeddingPoints, embeddingDatasetColumnIndices, _selectedIndicesFromStorage);
                           // });


                        // Wait for all futures to complete before proceeding
                        //future1.wait();
                        //future2.wait();


                        //startCodeTimer("Part7.2.1");
                        //needs to wait for future1 finish only
                        //populatePointData(datasetIdEmb, resultContainerForSelectedPoints, selectedIndicesFromStorageSize, pointsDatasetColumnsSize, pointsDatasetallColumnNameList);
                        //stopCodeTimer("Part7.2.1");

                        //startCodeTimer("Part7.2.2");
                        //needs to wait for future2 finish only
                        //populatePointData(datasetId, resultContainerForSelectedEmbeddingPoints, selectedIndicesFromStorageSize, embeddingDatasetColumnsSize, embeddingDatasetallColumnNameList);
                        //stopCodeTimer("Part7.2.2");

                        //startCodeTimer("Part7.2.3");
                        //needs to wait for future3 finish only
                        populatePointData(datasetIdExp, resultContainerColorPoints, selectedIndicesFromStorageSize, dimofDatasetExp, dimensionNamesExp);
                        //stopCodeTimer("Part7.2.3");

                        //stopCodeTimer("Part7.2");


                        //stopCodeTimer("Part7");
                        //startCodeTimer("Part8");
                        if (_selectedPointsTSNEDataset.isValid())
                        {
                            auto runningAction = dynamic_cast<TriggerAction*>(_selectedPointsTSNEDataset->findChildByPath("TSNE/TsneComputationAction/Running"));

                            if (runningAction)
                            {

                                if (runningAction->isChecked())
                                {
                                    auto stopAction = dynamic_cast<TriggerAction*>(_selectedPointsTSNEDataset->findChildByPath("TSNE/TsneComputationAction/Stop"));
                                    if (stopAction)
                                    {
                                        stopAction->trigger();
                                        QApplication::processEvents();
                                    }
                                }

                            }
                        }
                        //stopCodeTimer("Part8");
                        //startCodeTimer("Part9");
                        if (!_performGeneTableTsneAction.isChecked())
                        {


                            mv::plugin::AnalysisPlugin* analysisPlugin = nullptr;
                            bool usePreTSNE = _usePreComputedTSNE.isChecked();

                            auto scatterplotModificationsLowDimUMAP = [this]() {
                                if (_selectedPointsTSNEDataset.isValid()) {
                                    auto scatterplotViewFactory = mv::plugins().getPluginFactory("Scatterplot View");
                                    mv::gui::DatasetPickerAction* colorDatasetPickerAction = nullptr;
                                    mv::gui::DatasetPickerAction* pointDatasetPickerAction = nullptr;
                                    mv::gui::ViewPluginSamplerAction* samplerActionAction = nullptr;
                                    if (scatterplotViewFactory) {
                                        for (auto plugin : mv::plugins().getPluginsByFactory(scatterplotViewFactory)) {
                                            if (plugin->getGuiName() == "Scatterplot Cell Selection Overview") {
                                                pointDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Position"));
                                                if (pointDatasetPickerAction) {
                                                    pointDatasetPickerAction->setCurrentText("");

                                                    pointDatasetPickerAction->setCurrentDataset(_selectedPointsTSNEDataset);

                                                    colorDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Color"));
                                                    if (colorDatasetPickerAction)
                                                    {
                                                        colorDatasetPickerAction->setCurrentText("");



                                                        auto selectedColorType = _scatterplotReembedColorOption.getCurrentText();
                                                        if (selectedColorType != "")
                                                        {
                                                            if (selectedColorType == "Cluster")
                                                            {
                                                                if (_bottomClusterNamesDataset.getCurrentDataset().isValid())
                                                                {
                                                                    colorDatasetPickerAction->setCurrentDataset(_bottomClusterNamesDataset.getCurrentDataset());

                                                                    auto legendViewFactory = mv::plugins().getPluginFactory("ChartLegend View");
                                                                    if (legendViewFactory)
                                                                    {
                                                                        for (auto legendPlugin : mv::plugins().getPluginsByFactory(legendViewFactory))
                                                                        {
                                                                            if (legendPlugin->getGuiName() == "Legend View")
                                                                            {
                                                                                //legendPlugin->printChildren();
                                                                                auto legendDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Cluster dataset"));
                                                                                if (legendDatasetPickerAction)
                                                                                {
                                                                                    legendDatasetPickerAction->setCurrentDataset(_bottomClusterNamesDataset.getCurrentDataset());
                                                                                }
                                                                                auto chartTitle = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Chart Title"));
                                                                                if (chartTitle)
                                                                                {
                                                                                    chartTitle->setString("Cell types");
                                                                                }
                                                                                /*
                                                                                auto selectionColor = dynamic_cast<ColorAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Selection color"));
                                                                                if (selectionColor)
                                                                                {
                                                                                    selectionColor->setColor(QColor(53, 126, 199));
                                                                                }
                                                                                auto selectionStringDelimiter = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Delimiter"));
                                                                                if (selectionStringDelimiter)
                                                                                {
                                                                                    selectionStringDelimiter->setString(",");
                                                                                }
                                                                                auto selectionClustersString = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Cluster Selection string"));
                                                                                if (selectionClustersString)
                                                                                {
                                                                                    selectionClustersString->setString(""); //TODO
                                                                                }
                                                                                */
                                                                            }
                                                                        }
                                                                    }


                                                                }
                                                            }
                                                            else if (selectedColorType == "Species")
                                                            {
                                                                if (_speciesNamesDataset.getCurrentDataset().isValid())
                                                                {
                                                                    colorDatasetPickerAction->setCurrentDataset(_speciesNamesDataset.getCurrentDataset());
                                                                }
                                                            }
                                                            else if (selectedColorType == "Expression")
                                                            {
                                                                if (_tsneDatasetExpressionColors.isValid())
                                                                {
                                                                    colorDatasetPickerAction->setCurrentDataset(_tsneDatasetExpressionColors);
                                                                }
                                                            }



                                                        }
                                                    }

                                                    samplerActionAction = plugin->findChildByPath<mv::gui::ViewPluginSamplerAction>("Sampler");

                                                    if (samplerActionAction)
                                                    {
                                                        samplerActionAction->setHtmlViewGeneratorFunction([this](const ViewPluginSamplerAction::SampleContext& toolTipContext) -> QString {
                                                            QString clusterDatasetId = _speciesNamesDataset.getCurrentDataset().getDatasetId();
                                                            return generateTooltip(toolTipContext, clusterDatasetId, true, "GlobalPointIndices");
                                                            });
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                };
                            //stopCodeTimer("Part9");
                            if (!usePreTSNE)
                            {
                                //startCodeTimer("Part10");
                                analysisPlugin = mv::plugins().requestPlugin<AnalysisPlugin>("tSNE Analysis", { _selectedPointsEmbeddingDataset });
                                if (!analysisPlugin) {
                                    qDebug() << "Could not find create TSNE Analysis";
                                    return;
                                }
                                _selectedPointsTSNEDataset = analysisPlugin->getOutputDataset();
                                _selectedPointsTSNEDataset->setGroupIndex(groupIDDeletion);
                                if (_selectedPointsTSNEDataset.isValid())
                                {

                                    int perplexity = std::min(static_cast<int>(_selectedIndicesFromStorage.size()), _tsnePerplexity.getValue());
                                    if (perplexity < 5)
                                    {
                                        qDebug() << "Perplexity is less than 5";
                                        //_startComputationTriggerAction.setDisabled(false);
                                        return;
                                    }
                                    if (perplexity != _tsnePerplexity.getValue())
                                    {
                                        _tsnePerplexity.setValue(perplexity);
                                    }

                                    auto perplexityAction = dynamic_cast<IntegralAction*>(_selectedPointsTSNEDataset->findChildByPath("TSNE/Perplexity"));
                                    if (perplexityAction)
                                    {
                                        qDebug() << "Perplexity: Found";
                                        perplexityAction->setValue(perplexity);
                                    }
                                    else
                                    {
                                        qDebug() << "Perplexity: Not Found";
                                    }

                                    scatterplotModificationsLowDimUMAP();

                                    auto startAction = dynamic_cast<TriggerAction*>(_selectedPointsTSNEDataset->findChildByPath("TSNE/TsneComputationAction/Start"));
                                    if (startAction) {

                                        startAction->trigger();

                                        analysisPlugin->getOutputDataset()->setSelectionIndices({});
                                    }

                                }
                                //stopCodeTimer("Part10");
                            }
                            else
                            {
                                //startCodeTimer("Part11");
                                auto umapDataset = _scatterplotEmbeddingPointsUMAPOption.getCurrentDataset();

                                if (umapDataset.isValid())
                                {


                                    _selectedPointsTSNEDataset = mv::data().createDerivedDataset<Points>("SelectedPointsTSNEDataset", _selectedPointsEmbeddingDataset, _selectedPointsEmbeddingDataset);
                                    _selectedPointsTSNEDataset->setGroupIndex(groupIDDeletion);
                                    mv::events().notifyDatasetAdded(_selectedPointsTSNEDataset);

                                    auto umapDatasetRaw = mv::data().getDataset<Points>(umapDataset->getId());
                                    auto dimNames = umapDatasetRaw->getDimensionNames();
                                    int preComputedEmbeddingColumnsSize = umapDatasetRaw->getNumDimensions();
                                    std::vector<float> resultContainerPreComputedUMAP(selectedIndicesFromStorageSize * preComputedEmbeddingColumnsSize);
                                    std::vector<int> preComputedEmbeddingColumnIndices(preComputedEmbeddingColumnsSize);

                                    std::iota(preComputedEmbeddingColumnIndices.begin(), preComputedEmbeddingColumnIndices.end(), 0);

                                    umapDatasetRaw->populateDataForDimensions(resultContainerPreComputedUMAP, preComputedEmbeddingColumnIndices, _selectedIndicesFromStorage);

                                    QString datasetId = _selectedPointsTSNEDataset->getId();
                                    populatePointData(datasetId, resultContainerPreComputedUMAP, selectedIndicesFromStorageSize, preComputedEmbeddingColumnsSize, dimNames);

                                    if (_selectedPointsTSNEDataset.isValid())
                                    {
                                        scatterplotModificationsLowDimUMAP();
                                    }
                                }
                                else
                                {
                                    qDebug() << "UMAP Dataset not valid";
                                }


                                //stopCodeTimer("Part11");



                            }
                        }


                    }
                    else
                    {
                        qDebug() << "Datasets are not valid";
                    }
                    std::sort(_selectedIndicesFromStorage.begin(), _selectedIndicesFromStorage.end());

                    std::unordered_map<int, int> selectedIndexLookup;
                    selectedIndexLookup.reserve(_selectedIndicesFromStorage.size());
                    for (int selectedPosition = 0; selectedPosition < static_cast<int>(_selectedIndicesFromStorage.size()); ++selectedPosition) {
                        selectedIndexLookup.emplace(static_cast<int>(_selectedIndicesFromStorage[selectedPosition]), selectedPosition);
                    }

                    //startCodeTimer("Part12");
                    //startCodeTimer("Part12.1");
                    QFuture<void> futureClusterCVals = QtConcurrent::run([&]() {
                        for (auto& clusters : clustersValuesAll) {
                            const auto clusterIndices = clusters.getIndices();
                            auto clusterName = clusters.getName();
                            auto clusterColor = clusters.getColor();
                            std::vector<int> filteredIndices;
                            filteredIndices.reserve(clusterIndices.size());

                            for (const auto index : clusterIndices) {
                                auto selectedIndexIt = selectedIndexLookup.find(static_cast<int>(index));
                                if (selectedIndexIt != selectedIndexLookup.end()) {
                                    filteredIndices.push_back(selectedIndexIt->second);
                                }
                            }

                            selectedClustersMap[clusterName] = { clusterColor, std::move(filteredIndices) };
                        }
                        });
                    QFuture<void> futureSpeciesCVals = QtConcurrent::run([&]() {
                        for (auto& clusters : speciesValuesAll) {
                            const auto clusterIndices = clusters.getIndices();
                            auto clusterName = clusters.getName();
                            auto clusterColor = clusters.getColor();
                            std::vector<int> filteredIndices;
                            filteredIndices.reserve(clusterIndices.size());

                            for (const auto index : clusterIndices) {
                                auto selectedIndexIt = selectedIndexLookup.find(static_cast<int>(index));
                                if (selectedIndexIt != selectedIndexLookup.end()) {
                                    filteredIndices.push_back(selectedIndexIt->second);
                                }
                            }

                            selectedSpeciesMap[clusterName] = { clusterColor, std::move(filteredIndices) };
                        }
                        });
                    futureClusterCVals.waitForFinished(); // Wait for the concurrent task to complete
                    futureSpeciesCVals.waitForFinished();
                    //stopCodeTimer("Part12.1");



                    //startCodeTimer("Part12.2");
                    //_currentHierarchyItemsTopForTable.clear();
                    //_currentHierarchyItemsMiddleForTable.clear();
                    QStringList inclusionList = _topHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
                    const auto inclusionSet = toStringSet(inclusionList);
                    _currentHierarchyItemsMiddleForTable = QStringList{};
                    if (_topSelectedHierarchyStatus.getString() != "")
                    {

                        QStringList list = _topSelectedHierarchyStatus.getString().split(" @%$,$%@ ");
                        for (const auto& item : list) {

                            if (inclusionSet.find(item) != inclusionSet.end())
                            {
                                _currentHierarchyItemsMiddleForTable.push_back(item);
                            }

                        }
                    }
                    else
                    {
                        _currentHierarchyItemsMiddleForTable = QStringList{};
                    }


                    //_currentHierarchyItemsMiddleForTable = QSet<QString>{};
                    //
                    /*for (const auto& [key, clusterIndicesMap] : _topHierarchyClusterMap)
                    {

                        for (const auto& index : _selectedIndicesFromStorage)
                        {
                            if (clusterIndicesMap.at(index))
                            {

                                if (inclusionList.contains(key))
                                {
                                    _currentHierarchyItemsMiddleForTable.insert(key);
                                }
                                break;
                            }
                        }
                    }*/
                    //qDebug() << "Middle Hierarchy Items: " << _currentHierarchyItemsMiddleForTable;

                    //


                    // The expensive part of the update path is computing
                    // selected means for every gene across all affected species.
                    // We therefore separate cheap per-species metadata from the
                    // chunked matrix scan so the selected subset is read only once.
                    struct SpeciesComputationMeta {
                        QString name;
                        QColor color;
                        int allCellCount = 0;
                        int selectedCellCount = 0;
                        int nonSelectedCellsCount = 0;
                        int abundanceTop = 0;
                        int abundanceMiddle = 0;
                        int countAbundanceNumerator = 0;
                    };

                    std::vector<SpeciesComputationMeta> speciesComputationMeta;
                    speciesComputationMeta.reserve(speciesValuesAll.size());

                    const int selectedIndicesFromStorageSize = static_cast<int>(_selectedIndicesFromStorage.size());
                    std::vector<int> selectedPointSpeciesIndex(selectedIndicesFromStorageSize, -1);
                    std::vector<std::vector<int>> selectedGlobalIndicesPerSpecies;
                    selectedGlobalIndicesPerSpecies.resize(speciesValuesAll.size());

                    for (int speciesIndex = 0; speciesIndex < static_cast<int>(speciesValuesAll.size()); ++speciesIndex) {
                        const auto& species = speciesValuesAll[speciesIndex];
                        SpeciesComputationMeta meta;
                        meta.name = species.getName();
                        meta.color = species.getColor();
                        meta.allCellCount = static_cast<int>(species.getIndices().size());
                        speciesComputationMeta.push_back(meta);
                    }

                    for (int speciesIndex = 0; speciesIndex < static_cast<int>(speciesValuesAll.size()); ++speciesIndex) {
                        const auto& species = speciesValuesAll[speciesIndex];
                        for (const auto globalIndex : species.getIndices()) {
                            auto selectedIndexIt = selectedIndexLookup.find(static_cast<int>(globalIndex));
                            if (selectedIndexIt != selectedIndexLookup.end()) {
                                const int selectedPosition = selectedIndexIt->second;
                                selectedPointSpeciesIndex[selectedPosition] = speciesIndex;
                                selectedGlobalIndicesPerSpecies[speciesIndex].push_back(static_cast<int>(globalIndex));
                            }
                        }
                    }

                    for (int speciesIndex = 0; speciesIndex < static_cast<int>(speciesComputationMeta.size()); ++speciesIndex) {
                        auto& meta = speciesComputationMeta[speciesIndex];
                        meta.selectedCellCount = static_cast<int>(selectedGlobalIndicesPerSpecies[speciesIndex].size());
                        meta.nonSelectedCellsCount = meta.allCellCount - meta.selectedCellCount;
                    }

                    for (int speciesIndex = 0; speciesIndex < static_cast<int>(speciesValuesAll.size()); ++speciesIndex) {
                        const auto speciesIndices = speciesValuesAll[speciesIndex].getIndices();
                        auto& meta = speciesComputationMeta[speciesIndex];

                        for (const auto& cluster : _topHierarchyClusterMap) {
                            if (inclusionSet.find(cluster.first) == inclusionSet.end()) {
                                continue;
                            }

                            bool clusterPresent = false;
                            const auto& currentInclusionClusterMap = cluster.second;
                            int clusterSize = 0;

                            for (const auto speciesPointIndex : speciesIndices) {
                                if (currentInclusionClusterMap[speciesPointIndex]) {
                                    clusterSize++;
                                }
                            }

                            for (const auto selectedGlobalIndex : selectedGlobalIndicesPerSpecies[speciesIndex]) {
                                if (currentInclusionClusterMap[selectedGlobalIndex]) {
                                    meta.countAbundanceNumerator++;
                                    clusterPresent = true;
                                }
                            }

                            meta.abundanceTop += clusterSize;
                            if (clusterPresent) {
                                meta.abundanceMiddle += clusterSize;
                            }
                        }
                    }

                    _clusterNameToGeneNameToExpressionValue.clear();
                    _selectedSpeciesCellCountMap.clear();

                    for (const auto& meta : speciesComputationMeta) {
                        _clusterNameToGeneNameToExpressionValue[meta.name] = {};
                        auto& countInfo = _selectedSpeciesCellCountMap[meta.name];
                        countInfo.color = meta.color;
                        countInfo.selectedCellsCount = meta.selectedCellCount;
                        countInfo.nonSelectedCellsCount = meta.nonSelectedCellsCount;
                        countInfo.abundanceMiddle = meta.abundanceMiddle;
                        countInfo.abundanceTop = meta.abundanceTop;
                        countInfo.countAbundanceNumerator = meta.countAbundanceNumerator;
                    }

                    const int speciesCount = static_cast<int>(speciesComputationMeta.size());

                    if (_selectedIndicesFromStorage.empty()) {
                        // Preserve the legacy behavior for an empty selection:
                        // selected mean becomes 0 and the non-selected mean falls
                        // back to the species-wide baseline.
                        for (int speciesIndex = 0; speciesIndex < speciesCount; ++speciesIndex) {
                            const auto& meta = speciesComputationMeta[speciesIndex];
                            const auto speciesMeanMapIt = _clusterGeneMeanExpressionMap.find(meta.name);
                            if (speciesMeanMapIt == _clusterGeneMeanExpressionMap.end()) {
                                continue;
                            }

                            auto& speciesExpressionMap = _clusterNameToGeneNameToExpressionValue[meta.name];
                            for (int geneIndex = 0; geneIndex < static_cast<int>(pointsDatasetallColumnNameList.size()); ++geneIndex) {
                                const auto& geneName = pointsDatasetallColumnNameList[geneIndex];
                                float allCellMean = 0.0f;
                                if (const auto geneMeanIt = speciesMeanMapIt->second.find(geneName); geneMeanIt != speciesMeanMapIt->second.end()) {
                                    allCellMean = geneMeanIt->second.second;
                                }

                                Stats valueStats;
                                valueStats.abundanceMiddle = meta.abundanceMiddle;
                                valueStats.abundanceTop = meta.abundanceTop;
                                valueStats.countSelected = 0;
                                valueStats.countNonSelected = meta.allCellCount;
                                valueStats.meanSelected = 0.0f;
                                valueStats.meanNonSelected = allCellMean;
                                valueStats.color = meta.color;
                                valueStats.countAbundanceNumerator = meta.countAbundanceNumerator;
                                speciesExpressionMap[geneName] = valueStats;
                            }
                        }
                    }
                    else {
                        const int totalGeneCount = static_cast<int>(pointsDatasetallColumnIndices.size());

                        // ---- Stage 1 (parallel over gene chunks): mean expression of the SELECTED cells, per species ----
                        // A worker reads one gene chunk for all selected cells once and fans the rows out to the species
                        // sums. Workers write disjoint gene columns of speciesSelectedMeans -> no locking required.
                        // The chunk is narrowed for huge selections so that (threads x buffer) stays within the memory budget.
                        const int jobChunkSize = std::max(8, std::min(kGeneChunkSize, static_cast<int>(8000000 / std::max(1, selectedIndicesFromStorageSize))));
                        struct GeneChunkJob { int start; int size; };
                        std::vector<GeneChunkJob> chunkJobs;
                        for (int jobStart = 0; jobStart < totalGeneCount; jobStart += jobChunkSize) {
                            chunkJobs.push_back({ jobStart, std::min(jobChunkSize, totalGeneCount - jobStart) });
                        }

                        std::vector<std::vector<float>> speciesSelectedMeans(speciesCount, std::vector<float>(totalGeneCount, 0.0f));

                        QThreadPool statisticsPool;
                        statisticsPool.setMaxThreadCount(memorySafeThreadCount(static_cast<size_t>(selectedIndicesFromStorageSize) * jobChunkSize / kGeneChunkSize));
                        QtConcurrent::blockingMap(&statisticsPool, chunkJobs, [&](GeneChunkJob& job) {
                            std::vector<int> geneChunkIndices(pointsDatasetallColumnIndices.begin() + job.start, pointsDatasetallColumnIndices.begin() + job.start + job.size);
                            std::vector<float> selectedChunkData(static_cast<size_t>(selectedIndicesFromStorageSize) * static_cast<size_t>(job.size), 0.0f);
                            pointsDatasetRaw->populateDataForDimensions(selectedChunkData, geneChunkIndices, _selectedIndicesFromStorage);

                            std::vector<std::vector<double>> speciesChunkSums(speciesCount, std::vector<double>(job.size, 0.0));
                            for (int selectedRowIndex = 0; selectedRowIndex < selectedIndicesFromStorageSize; ++selectedRowIndex) {
                                const int speciesIndex = selectedPointSpeciesIndex[selectedRowIndex];
                                if (speciesIndex < 0) {
                                    continue;
                                }
                                const size_t rowOffset = static_cast<size_t>(selectedRowIndex) * static_cast<size_t>(job.size);
                                auto& currentSpeciesChunkSums = speciesChunkSums[speciesIndex];
                                for (int geneOffset = 0; geneOffset < job.size; ++geneOffset) {
                                    currentSpeciesChunkSums[geneOffset] += selectedChunkData[rowOffset + geneOffset];
                                }
                            }

                            for (int speciesIndex = 0; speciesIndex < speciesCount; ++speciesIndex) {
                                const int selectedCount = speciesComputationMeta[speciesIndex].selectedCellCount;
                                if (selectedCount <= 0) {
                                    continue;
                                }
                                for (int geneOffset = 0; geneOffset < job.size; ++geneOffset) {
                                    speciesSelectedMeans[speciesIndex][job.start + geneOffset] =
                                        static_cast<float>(speciesChunkSums[speciesIndex][geneOffset] / static_cast<double>(selectedCount));
                                }
                            }
                            });

                        // ---- Stage 2 (parallel over species): build the per-gene statistics maps ----
                        // The outer map entries already exist (created above), so every worker only touches its own inner map.
                        struct SpeciesStatsJob { int speciesIndex; std::map<QString, Stats>* target; };
                        std::vector<SpeciesStatsJob> speciesJobs;
                        speciesJobs.reserve(speciesCount);
                        for (int speciesIndex = 0; speciesIndex < speciesCount; ++speciesIndex) {
                            const auto& meta = speciesComputationMeta[speciesIndex];
                            if (_clusterGeneMeanExpressionMap.find(meta.name) == _clusterGeneMeanExpressionMap.end()) {
                                continue; // no species-wide baseline available -> species is skipped (legacy behavior)
                            }
                            speciesJobs.push_back({ speciesIndex, &_clusterNameToGeneNameToExpressionValue[meta.name] });
                        }

                        QtConcurrent::blockingMap(speciesJobs, [&](SpeciesStatsJob& job) {
                            const auto& meta = speciesComputationMeta[job.speciesIndex];
                            const auto& baselineMeans = _clusterGeneMeanExpressionMap.at(meta.name);
                            const auto& selectedMeans = speciesSelectedMeans[job.speciesIndex];

                            for (int geneIndex = 0; geneIndex < totalGeneCount; ++geneIndex) {
                                const auto& geneName = pointsDatasetallColumnNameList[geneIndex];

                                float allCellMean = 0.0f;
                                if (const auto geneMeanIt = baselineMeans.find(geneName); geneMeanIt != baselineMeans.end()) {
                                    allCellMean = geneMeanIt->second.second;
                                }

                                const float selectedCellsMean = (meta.selectedCellCount > 0) ? selectedMeans[geneIndex] : 0.0f;

                                // mean of the NON-selected cells = (species total - selected total) / non-selected count
                                float nonSelectedMean = allCellMean;
                                if (meta.selectedCellCount > 0 && meta.nonSelectedCellsCount > 0) {
                                    const float allCellTotal = allCellMean * meta.allCellCount;
                                    nonSelectedMean = (allCellTotal - (selectedCellsMean * meta.selectedCellCount)) / meta.nonSelectedCellsCount;
                                }

                                Stats valueStats;
                                valueStats.abundanceMiddle = meta.abundanceMiddle;
                                valueStats.abundanceTop = meta.abundanceTop;
                                valueStats.countSelected = meta.selectedCellCount;
                                valueStats.countNonSelected = meta.nonSelectedCellsCount;
                                valueStats.meanSelected = selectedCellsMean;
                                valueStats.meanNonSelected = nonSelectedMean;
                                valueStats.color = meta.color;
                                valueStats.countAbundanceNumerator = meta.countAbundanceNumerator;
                                (*job.target)[geneName] = valueStats;
                            }
                            });
                    }

                    //stopCodeTimer("Part12.2");

                    auto clusterColorDatasetId = _tsneDatasetClusterColors->getId();
                    auto speciesColorDatasetId = _tsneDatasetSpeciesColors->getId();
                    //startCodeTimer("Part12.3");
                    populateClusterData(speciesColorDatasetId, selectedSpeciesMap);
                    //stopCodeTimer("Part12.3");
                    //startCodeTimer("Part12.4");
                    populateClusterData(clusterColorDatasetId, selectedClustersMap);
                    //stopCodeTimer("Part12.4");
                    //stopCodeTimer("Part12");
                    updateClusterInfoStatusBar();
                    /*
                    QLayoutItem* layoutItem;
                    while ((layoutItem = _selectedCellClusterInfoStatusBar->takeAt(0)) != nullptr) {
                        delete layoutItem->widget();
                        delete layoutItem;
                    }
                    if (_tsneDatasetClusterColors.isValid())
                    {

                        auto clusterValues = _tsneDatasetClusterColors->getClusters();
                        if (!clusterValues.empty())
                        {
                            //startCodeTimer("Part13");

                            //QLayoutItem* layoutItem;
                            //while ((layoutItem = _selectedCellClusterInfoStatusBar->takeAt(0)) != nullptr) {
                            //    delete layoutItem->widget();
                            //    delete layoutItem;
                            //}

                            // Create a description label
                            auto descriptionLabel = new QLabel("Selected Cell Counts per " + clusterDatasetName + " :");
                            // Optionally, set a stylesheet for the description label for styling
                            descriptionLabel->setStyleSheet("QLabel { font-weight: bold; padding: 2px; }");
                            // Add the description label to the layout
                            _selectedCellClusterInfoStatusBar->addWidget(descriptionLabel);


                            std::vector<ClusterOrderContainer> orderedClustersSet;

                            for (const auto& cluster : clusterValues) {
                                ClusterOrderContainer temp{
                                    cluster.getIndices().size(),
                                    cluster.getColor(),
                                    cluster.getName()
                                };
                                orderedClustersSet.push_back(std::move(temp));
                            }

                            const auto& currentText = _clusterCountSortingType.getCurrentText();
                            if (currentText == "Name") {
                                std::sort(orderedClustersSet.begin(), orderedClustersSet.end(), sortByName);
                            }
                            else if (currentText == "Hierarchy View" && !_customOrderClustersFromHierarchy.empty()) {
                                if (_customOrderClustersFromHierarchyMap.empty()) {
                                    _customOrderClustersFromHierarchyMap = prepareCustomSortMap(_customOrderClustersFromHierarchy);
                                }
                                std::sort(orderedClustersSet.begin(), orderedClustersSet.end(), [&](const ClusterOrderContainer& a, const ClusterOrderContainer& b) {
                                    return sortByCustomList(a, b, _customOrderClustersFromHierarchyMap);
                                    });
                            }
                            else {
                                std::sort(orderedClustersSet.begin(), orderedClustersSet.end(), sortByCount);
                                if (currentText != "Count") {
                                    _clusterCountSortingType.setCurrentText("Count");
                                }
                            }

                            for (const auto& clustersFromSet : orderedClustersSet)
                            {
                                auto clusterLabel = new QLabel(QString("%1: %2").arg(clustersFromSet.name).arg(clustersFromSet.count));
                                QColor textColor = clustersFromSet.color.lightness() > 127 ? Qt::black : Qt::white;
                                clusterLabel->setStyleSheet(QString("QLabel { color: %1; background-color: %2; padding: 2px; border: 0.5px solid %3; }")
                                    .arg(textColor.name()).arg(clustersFromSet.color.name(QColor::HexArgb)).arg(textColor.name()));
                                _selectedCellClusterInfoStatusBar->addWidget(clusterLabel);
                            }





                            //for (auto cluster : clusterValues) {
                            //    auto clusterName = cluster.getName();
                            //    auto clusterIndicesSize = cluster.getIndices().size();
                            //    auto clusterColor = cluster.getColor(); // Assuming getColor() returns a QColor

                            //    // Calculate luminance
                            //    qreal luminance = 0.299 * clusterColor.redF() + 0.587 * clusterColor.greenF() + 0.114 * clusterColor.blueF();

                            //    // Choose text color based on luminance
                            //    QString textColor = (luminance > 0.5) ? "black" : "white";

                            //    // Convert QColor to hex string for stylesheet
                            //    QString backgroundColor = clusterColor.name(QColor::HexArgb);

                            //    auto clusterLabel = new QLabel(QString("%1: %2").arg(clusterName).arg(clusterIndicesSize));
                            //    // Add text color and background color to clusterLabel with padding and border for better styling
                            //    clusterLabel->setStyleSheet(QString("QLabel { color: %1; background-color: %2; padding: 2px; border: 0.5px solid %3; }")
                            //        .arg(textColor).arg(backgroundColor).arg(textColor));
                            //    _selectedCellClusterInfoStatusBar->addWidget(clusterLabel);
                            //}


                        }

                    }
                    */
                    //the next line should only execute if all above are finished


                    //startCodeTimer("Part14");
                    findTopNGenesPerCluster();
                    //stopCodeTimer("Part14");




                }

                else
                {
                    qDebug() << "Species or Clusters are empty";
                }


            }

            else
            {
                qDebug() << "No points selected or no dimensions present";
            }

            _removeRowSelection.trigger();
            _removeRowSelection.setEnabled(false);
            //enableDisableButtonsAutomatically();

        }
        stopCodeTimer("UpdateGeneFilteringTrigger");
        //_startComputationTriggerAction.setDisabled(false);
    }
    catch (const std::exception& e) {
        qDebug() << "An exception occurred in coputation: " << e.what();
        _statusColorAction.setString("E");
    }
    catch (...) {
        qDebug() << "An unknown exception occurred in coputation";
        _statusColorAction.setString("E");
    }
}

void SettingsAction::updateClusterInfoStatusBar()
{
    QLayoutItem* layoutItem;
    while ((layoutItem = _selectedCellClusterInfoStatusBar->takeAt(0)) != nullptr) {
        delete layoutItem->widget();
        delete layoutItem;
    }
    if (_tsneDatasetClusterColors.isValid() && _bottomClusterNamesDataset.getCurrentDataset().isValid())
    {
        auto clusterDatasetName = _bottomClusterNamesDataset.getCurrentDataset()->getGuiName();
        auto clusterValues = _tsneDatasetClusterColors->getClusters();
        if (!clusterValues.empty())
        {
            //startCodeTimer("Part13");

            /*QLayoutItem* layoutItem;
            while ((layoutItem = _selectedCellClusterInfoStatusBar->takeAt(0)) != nullptr) {
                delete layoutItem->widget();
                delete layoutItem;
            }*/

            // Create a description label
            auto descriptionLabel = new QLabel("Cell counts per " + clusterDatasetName + ", sorted by " + _clusterCountSortingType.getCurrentText() + ":");

            // Optionally, set a stylesheet for the description label for styling
            descriptionLabel->setStyleSheet("QLabel { font-weight: bold; padding: 2px; }");
            // Add the description label to the layout
            _selectedCellClusterInfoStatusBar->addWidget(descriptionLabel);


            std::vector<ClusterOrderContainer> orderedClustersSet;

            for (const auto& cluster : clusterValues) {
                ClusterOrderContainer temp{
                    static_cast<int>(cluster.getIndices().size()),
                    cluster.getColor(),
                    cluster.getName()
                };
                orderedClustersSet.push_back(std::move(temp));
            }

            const auto& currentText = _clusterCountSortingType.getCurrentText();
            if (currentText == "Name") {
                std::sort(orderedClustersSet.begin(), orderedClustersSet.end(), sortByName);
            }
            else if (currentText == "Hierarchy View" && !_customOrderClustersFromHierarchy.empty()) {
                if (_customOrderClustersFromHierarchyMap.empty()) {
                    _customOrderClustersFromHierarchyMap = prepareCustomSortMap(_customOrderClustersFromHierarchy);
                }
                std::sort(orderedClustersSet.begin(), orderedClustersSet.end(), [&](const ClusterOrderContainer& a, const ClusterOrderContainer& b) {
                    return sortByCustomList(a, b, _customOrderClustersFromHierarchyMap);
                    });
            }
            else {
                std::sort(orderedClustersSet.begin(), orderedClustersSet.end(), sortByCount);
                if (currentText != "Count") {
                    _clusterCountSortingType.setCurrentText("Count");
                }
            }
            // Clear or rebuild the status bar, and reset the stale pointer first
            _selectedClusterLabel = nullptr;
            _clusterLabelsByName.clear();

            QString selectedClustersString = "";
            bool foundPrevious = false;
            ClickableLabel* firstLabel = nullptr;
            QString firstName = "";

            for (const auto& clustersFromSet : orderedClustersSet)
            {
                auto clusterLabel = new ClickableLabel();
                QString labelText = QString("%1: %2").arg(clustersFromSet.name).arg(clustersFromSet.count);
                clusterLabel->setText(labelText);
                selectedClustersString = selectedClustersString + clustersFromSet.name + ",";

                // Save reference to the very first item as a fallback
                if (!firstLabel)
                {
                    firstLabel = clusterLabel;
                    firstName = clustersFromSet.name;
                }

                QColor textColor = clustersFromSet.color.lightness() > 127 ? Qt::black : Qt::white;

                QString normalStyle = QString("ClickableLabel { color: %1; background-color: %2; padding: 2px; border: 0.5px solid %3; }")
                    .arg(textColor.name()).arg(clustersFromSet.color.name(QColor::HexArgb)).arg(textColor.name());

                QString selectedStyle = QString("ClickableLabel { color: %1; background-color: %2; padding: 2px; border: 2px solid #00A3EE; }")
                    .arg(textColor.name()).arg(clustersFromSet.color.name(QColor::HexArgb));

                clusterLabel->setProperty("normalStyle", normalStyle);
                clusterLabel->setProperty("selectedStyle", selectedStyle);

                // Select if name matches previous selection, or if it's completely empty on initial run
                _clusterLabelsByName.insert(clustersFromSet.name, clusterLabel);

                // Keep the previous selection (one or several cell types); on the very first run select the first one
                if (_selectedClusterNames.isEmpty() && !_selectedClusterName.isEmpty())
                {
                    _selectedClusterNames << _selectedClusterName;
                }

                bool shouldSelect = false;
                if (_selectedClusterNames.isEmpty() && !foundPrevious && firstLabel == clusterLabel)
                {
                    shouldSelect = true;
                    foundPrevious = true;
                }
                else if (_selectedClusterNames.contains(clustersFromSet.name))
                {
                    shouldSelect = true;
                    foundPrevious = true;
                }

                if (shouldSelect)
                {
                    clusterLabel->setStyleSheet(selectedStyle);
                    _selectedClusterLabel = clusterLabel;
                    _selectedClusterName = clustersFromSet.name;
                    if (!_selectedClusterNames.contains(clustersFromSet.name))
                    {
                        _selectedClusterNames << clustersFromSet.name;
                    }
                }
                else
                {
                    clusterLabel->setStyleSheet(normalStyle);
                }

                connect(clusterLabel, &ClickableLabel::clicked, this, [this, clusterLabel, clusterName = clustersFromSet.name]() {
                    Qt::MouseButtons buttons = QGuiApplication::mouseButtons();

                    if (buttons.testFlag(Qt::RightButton))
                    {
                        // Right-click changes sorting order; selection state stays preserved in _selectedClusterName
                        int current = _clusterCountSortingType.getCurrentIndex();
                        int newIndex;
                        if (current == 0)
                        {
                            newIndex = 1;
                        }
                        else if (current == 1)
                        {
                            if (!_customOrderClustersFromHierarchy.empty())
                            {
                                newIndex = 2;
                            }
                            else
                            {
                                newIndex = 0;
                            }
                        }
                        else
                        {
                            newIndex = 0;
                        }
                        _clusterCountSortingType.setCurrentIndex(newIndex);
                    }
                    else if (buttons.testFlag(Qt::LeftButton))
                    {
                        if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier))
                        {
                            // Shift + click: add / remove this cell type (at least one stays selected)
                            if (_selectedClusterNames.contains(clusterName))
                            {
                                if (_selectedClusterNames.size() == 1)
                                {
                                    return;
                                }
                                _selectedClusterNames.removeAll(clusterName);
                            }
                            else
                            {
                                _selectedClusterNames << clusterName;
                            }
                        }
                        else
                        {
                            // plain click: select only this cell type
                            if (_selectedClusterNames.size() == 1 && _selectedClusterNames.first() == clusterName)
                            {
                                return;
                            }
                            _selectedClusterNames = QStringList{ clusterName };
                        }

                        _selectedClusterName = _selectedClusterNames.last();
                        _selectedClusterLabel = _clusterLabelsByName.value(_selectedClusterName).data();
                        applyClusterLabelSelectionStyles();
                        updateTraitReverseSearchFeature();
                    }
                    });

                _selectedCellClusterInfoStatusBar->addWidget(clusterLabel);
            }

            // Drop selected cell types that are no longer present
            {
                QStringList presentNames;
                for (const QString& name : _selectedClusterNames)
                {
                    if (_clusterLabelsByName.contains(name))
                    {
                        presentNames << name;
                    }
                }
                _selectedClusterNames = presentNames;
                if (!presentNames.isEmpty())
                {
                    _selectedClusterName = presentNames.last();
                }
            }

            // Fallback: If previous selection was specified but is no longer present in the new set, select the first item instead
            if (!foundPrevious && !orderedClustersSet.empty() && firstLabel != nullptr)
            {
                firstLabel->setStyleSheet(firstLabel->property("selectedStyle").toString());
                _selectedClusterLabel = firstLabel;
                _selectedClusterName = firstName;
                _selectedClusterNames = QStringList{ firstName };
            }

            auto legendViewFactory = mv::plugins().getPluginFactory("ChartLegend View");
            if (legendViewFactory)
            {
                for (auto legendPlugin : mv::plugins().getPluginsByFactory(legendViewFactory))
                {
                    if (legendPlugin->getGuiName() == "Legend View")
                    {
                        auto selectionColor = dynamic_cast<ColorAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Selection color"));
                        if (selectionColor)
                        {
                            selectionColor->setColor(QColor(53, 126, 199));
                        }
                        auto selectionStringDelimiter = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Delimiter"));
                        if (selectionStringDelimiter)
                        {
                            selectionStringDelimiter->setString(",");
                        }
                        auto selectionClustersString = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Cluster Selection string"));
                        if (selectionClustersString)
                        {
                            selectionClustersString->setString(selectedClustersString);
                        }
                    }
                }
            }

            /*

            for (auto cluster : clusterValues) {
                auto clusterName = cluster.getName();
                auto clusterIndicesSize = cluster.getIndices().size();
                auto clusterColor = cluster.getColor(); // Assuming getColor() returns a QColor

                // Calculate luminance
                qreal luminance = 0.299 * clusterColor.redF() + 0.587 * clusterColor.greenF() + 0.114 * clusterColor.blueF();

                // Choose text color based on luminance
                QString textColor = (luminance > 0.5) ? "black" : "white";

                // Convert QColor to hex string for stylesheet
                QString backgroundColor = clusterColor.name(QColor::HexArgb);

                auto clusterLabel = new QLabel(QString("%1: %2").arg(clusterName).arg(clusterIndicesSize));
                // Add text color and background color to clusterLabel with padding and border for better styling
                clusterLabel->setStyleSheet(QString("QLabel { color: %1; background-color: %2; padding: 2px; border: 0.5px solid %3; }")
                    .arg(textColor).arg(backgroundColor).arg(textColor));
                _selectedCellClusterInfoStatusBar->addWidget(clusterLabel);
            }
            */

        }

    }
}


void SettingsAction::setModifiedTriggeredData(QVariant geneListTable)
{
    if (!geneListTable.isNull())
    {
        ////startCodeTimer("Part15");
        //_filteredGeneNamesVariant.setVariant(geneListTable);
        _listModel.setVariant(geneListTable);
        ////stopCodeTimer("Part15");

    }
    else
    {
        qDebug() << "QVariant empty";
    }
}

void createTreeInitial(QJsonObject& node, const std::map<QString, InitialStatistics>& utilityMap) {
    // Check if the "name" key exists in the current node
    if (node.contains("name")) {
        QString nodeName = node["name"].toString();
        auto it = utilityMap.find(nodeName);

        if (it != utilityMap.end()) {
            node["mean"] = std::round(it->second.meanVal * 100.0) / 100.0; // Round to 2 decimal places
            node["differential"] = std::round(it->second.differentialVal * 100.0) / 100.0; // Round to 2 decimal places


            float topAbundance = 0.0;
            if (it->second.abundanceTop != 0)
            {

                topAbundance = (static_cast<float>(it->second.countAbundanceNumerator) / static_cast<float>(it->second.abundanceTop)) * 100;
            }

            float middleAbundance = 0.0;
            if (it->second.abundanceMiddle != 0)
            {

                middleAbundance = (static_cast<float>(it->second.countAbundanceNumerator) / static_cast<float>(it->second.abundanceMiddle)) * 100;
            }


            node["abundanceTop"] = topAbundance;
            node["abundanceMiddle"] = middleAbundance;
            node["rank"] = it->second.rankVal;
            node["gene"] = it->second.geneName;
        }
    }

    // If the node has "children", recursively update them as well
    if (node.contains("children")) {
        QJsonArray children = node["children"].toArray();
        for (int i = 0; i < children.size(); ++i) {
            QJsonObject child = children[i].toObject();
            createTreeInitial(child, utilityMap); // Recursive call
            children[i] = child; // Update the modified object back into the array
        }
        node["children"] = children; // Update the modified array back into the parent JSON object
    }
}


void SettingsAction::precomputeTreesFromHierarchy()
{
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
    {
        return;
    }
    _precomputedTreesFromTheHierarchy.clear();
    auto start = std::chrono::high_resolution_clock::now();
    qDebug() << "Computing precomputeTreesFromHierarchy";

    if (!_speciesNamesDataset.getCurrentDataset().isValid() || !_mainPointsDataset.getCurrentDataset().isValid() || !_topClusterNamesDataset.getCurrentDataset().isValid() || !_middleClusterNamesDataset.getCurrentDataset().isValid() || !_bottomClusterNamesDataset.getCurrentDataset().isValid() || !_referenceTreeDataset.getCurrentDataset().isValid()) {
        qDebug() << "Datasets are not valid";
        return;
    }
    auto speciesNamesDataset = mv::data().getDataset<Clusters>(_speciesNamesDataset.getCurrentDataset().getDatasetId());
    auto mainPointsDataset = mv::data().getDataset<Points>(_mainPointsDataset.getCurrentDataset().getDatasetId());
    auto topClusterNamesDataset = mv::data().getDataset<Clusters>(_topClusterNamesDataset.getCurrentDataset().getDatasetId());
    auto middleClusterNamesDataset = mv::data().getDataset<Clusters>(_middleClusterNamesDataset.getCurrentDataset().getDatasetId());
    auto bottomClusterNamesDataset = mv::data().getDataset<Clusters>(_bottomClusterNamesDataset.getCurrentDataset().getDatasetId());

    auto referenceTreeDataset = mv::data().getDataset<XSCTree>(_referenceTreeDataset.getCurrentDataset().getDatasetId());
    QJsonObject speciesDataJson = referenceTreeDataset->getTreeData();
    QStringList speciesNamesVerify = referenceTreeDataset->getTreeLeafNames();
    if (speciesDataJson.isEmpty() || speciesNamesVerify.isEmpty())
    {
        qDebug() << "Reference tree data is empty";
        return;
    }


    if (speciesNamesDataset.isValid() && mainPointsDataset.isValid() && topClusterNamesDataset.isValid() && middleClusterNamesDataset.isValid() && bottomClusterNamesDataset.isValid())
    {
        auto speciesClusters = speciesNamesDataset->getClusters();

        QStringList speciesClusterNames;
        speciesClusterNames.reserve(speciesClusters.size());
        for (const auto& speciesCluster : speciesClusters) {
            speciesClusterNames.push_back(speciesCluster.getName());
        }
        if (!areSameIgnoreOrder(speciesNamesVerify, speciesClusterNames))
        {
            qDebug() << "Species names do not match";
            return;
        }


        auto mainPointDimensionNames = mainPointsDataset->getDimensionNames();
        std::vector<int> allGeneIndices(mainPointDimensionNames.size());
        std::iota(allGeneIndices.begin(), allGeneIndices.end(), 0);

        QVector<Cluster> topClusters = topClusterNamesDataset->getClusters();
        QVector<Cluster>  middleClusters = middleClusterNamesDataset->getClusters();
        QVector<Cluster>  bottomClusters = bottomClusterNamesDataset->getClusters();

        if (!mainPointDimensionNames.empty()) {
            std::map<QString, QVector<Cluster>> combinedClusters = {
                {"top", topClusters},
                {"middle", middleClusters},
                {"bottom", bottomClusters}
            };

            // Read the option once on the GUI thread instead of from every worker thread
            const QString optionValueText = _typeofTopNGenes.getCurrentText();

            // Abundance bookkeeping: number of cells of each species that sit in every included top cluster.
            // Computed once here so the (cluster x species) loop below only has to look at the cluster cells.
            std::map<QString, std::map<QString, int>> speciesTopClusterSizes;
            for (const auto& species : speciesClusters) {
                const auto speciesIndicesForSizes = species.getIndices();
                auto& sizesForSpecies = speciesTopClusterSizes[species.getName()];
                for (const auto& topEntry : _topHierarchyClusterMap) {
                    int clusterSize = 0;
                    for (const auto idx : speciesIndicesForSizes) {
                        if (static_cast<size_t>(idx) < topEntry.second.size() && topEntry.second[idx]) {
                            ++clusterSize;
                        }
                    }
                    sizesForSpecies[topEntry.first] = clusterSize;
                }
            }

            QMutex precomputedTreesMutex;

            QtConcurrent::blockingMap(combinedClusters, [&](const auto& pair) {
                const auto& hierarchyType = pair.first;
                const auto& clusters = pair.second;

                for (const auto& cluster : clusters) {
                    const auto& clusterName = cluster.getName();
                    auto clusterIndices = cluster.getIndices();
                    std::sort(clusterIndices.begin(), clusterIndices.end());
                    std::map<QString, std::map<QString, Stats>> topSpeciesToGeneExpressionMap;

                    for (const auto& species : speciesClusters) {
                        const auto& speciesName = species.getName();
                        auto speciesIndices = species.getIndices();
                        std::sort(speciesIndices.begin(), speciesIndices.end());

                        std::vector<int> commonPointsIndices = intersectSortedIndicesToInt(speciesIndices, clusterIndices);

                        if (commonPointsIndices.empty()) {
                            continue;
                        }

                        const auto speciesMeanMapIt = _clusterGeneMeanExpressionMap.find(speciesName);
                        if (speciesMeanMapIt == _clusterGeneMeanExpressionMap.end()) {
                            continue;
                        }
                        const auto& speciesMeanMap = speciesMeanMapIt->second;

                        std::vector<float> selectedGeneMeans(mainPointDimensionNames.size(), 0.0f);
                        computeChunkedGeneMeans(mainPointsDataset, allGeneIndices, commonPointsIndices, [&](int, const std::vector<int>& geneChunkIndices, const std::vector<float>& chunkMeans) {
                            for (int localGeneIndex = 0; localGeneIndex < static_cast<int>(geneChunkIndices.size()); ++localGeneIndex) {
                                selectedGeneMeans[geneChunkIndices[localGeneIndex]] = chunkMeans[localGeneIndex];
                            }
                            });

                        const int selectedCellCount = static_cast<int>(commonPointsIndices.size());

                        // Same abundance definition as the main table (see updateButtonTriggered):
                        //  top    = species cells inside the included top clusters
                        //  middle = species cells inside the included top clusters that contain this cluster's cells
                        //  numerator = this cluster's species cells inside the included top clusters
                        int topHierarchyCountValue = 0;
                        int middleHierarchyCountValue = 0;
                        int abundanceNumerator = 0;
                        const auto speciesSizesIt = speciesTopClusterSizes.find(speciesName);
                        for (const auto& topEntry : _topHierarchyClusterMap) {
                            int clusterSize = 0;
                            if (speciesSizesIt != speciesTopClusterSizes.end()) {
                                const auto sizeIt = speciesSizesIt->second.find(topEntry.first);
                                if (sizeIt != speciesSizesIt->second.end()) {
                                    clusterSize = sizeIt->second;
                                }
                            }
                            bool present = false;
                            for (const int idx : commonPointsIndices) {
                                if (static_cast<size_t>(idx) < topEntry.second.size() && topEntry.second[idx]) {
                                    ++abundanceNumerator;
                                    present = true;
                                }
                            }
                            topHierarchyCountValue += clusterSize;
                            if (present) {
                                middleHierarchyCountValue += clusterSize;
                            }
                        }

                        auto& geneStatsForSpecies = topSpeciesToGeneExpressionMap[speciesName];
                        for (int geneIndex = 0; geneIndex < static_cast<int>(mainPointDimensionNames.size()); ++geneIndex) {
                            const auto& geneName = mainPointDimensionNames[geneIndex];
                            const auto nonSelectionDetailsIt = speciesMeanMap.find(geneName);
                            if (nonSelectionDetailsIt == speciesMeanMap.end()) {
                                continue;
                            }

                            const int allCellCounts = nonSelectionDetailsIt->second.first;
                            const float allCellMean = nonSelectionDetailsIt->second.second;
                            const float selectedMean = selectedGeneMeans[geneIndex];
                            const float allCellTotal = allCellMean * allCellCounts;
                            const int nonSelectedCells = allCellCounts - selectedCellCount;
                            const float nonSelectedMean = (nonSelectedCells > 0) ? (allCellTotal - selectedMean * selectedCellCount) / nonSelectedCells : 0.0f;

                            StatisticsSingle calculateStatisticsShort = { selectedMean, selectedCellCount };
                            StatisticsSingle calculateStatisticsNot = { nonSelectedMean, nonSelectedCells };
                            geneStatsForSpecies[geneName] = combineStatisticsSingle(calculateStatisticsShort, calculateStatisticsNot, topHierarchyCountValue, middleHierarchyCountValue, abundanceNumerator);
                        }
                    }

                    enum class SelectionOption {
                        AbsoluteTopN,
                        PositiveTopN,
                        NegativeTopN
                    };

                    SelectionOption option = SelectionOption::AbsoluteTopN;
                    if (optionValueText == "Positive") {
                        option = SelectionOption::PositiveTopN;
                    }
                    else if (optionValueText == "Negative") {
                        option = SelectionOption::NegativeTopN;
                    }

                    std::map<QString, std::vector<std::pair<QString, int>>> rankingMap;

                    for (const auto& [speciesName, geneMap] : topSpeciesToGeneExpressionMap) {
                        std::vector<std::pair<QString, float>> geneExpressionVec;
                        geneExpressionVec.reserve(geneMap.size());
                        for (const auto& [geneName, stats] : geneMap) {
                            float differenceMeanValue = stats.meanSelected - stats.meanNonSelected;
                            geneExpressionVec.emplace_back(geneName, differenceMeanValue);
                        }

                        if (option == SelectionOption::AbsoluteTopN) {
                            std::sort(geneExpressionVec.begin(), geneExpressionVec.end(), [](const auto& a, const auto& b) {
                                return std::abs(a.second) > std::abs(b.second);
                                });
                        }
                        else {
                            std::sort(geneExpressionVec.begin(), geneExpressionVec.end(), [](const auto& a, const auto& b) {
                                return a.second > b.second;
                                });
                            if (option == SelectionOption::NegativeTopN) {
                                std::reverse(geneExpressionVec.begin(), geneExpressionVec.end());
                            }
                        }

                        for (int i = 0; i < geneExpressionVec.size(); ++i) {
                            int rank = (option == SelectionOption::NegativeTopN) ? geneExpressionVec.size() - i : i + 1;
                            rankingMap[geneExpressionVec[i].first].emplace_back(speciesName, rank);
                        }
                    }

                    // Results for this cluster are collected locally and merged under a lock once,
                    // because the three hierarchy levels are processed on different threads.
                    std::unordered_map<QString, QString> treesForCluster;
                    treesForCluster.reserve(rankingMap.size());

                    for (auto& [geneName, speciesRankVec] : rankingMap) {
                        std::map<QString, InitialStatistics> utilityMap;
                        for (const auto& [speciesName, rank] : speciesRankVec) {
                            const auto speciesStatsIt = topSpeciesToGeneExpressionMap.find(speciesName);
                            if (speciesStatsIt == topSpeciesToGeneExpressionMap.end()) {
                                continue;
                            }
                            const auto geneStatsIt = speciesStatsIt->second.find(geneName);
                            if (geneStatsIt == speciesStatsIt->second.end()) {
                                continue;
                            }
                            const Stats& geneStats = geneStatsIt->second;

                            InitialStatistics tempStats;
                            tempStats.rankVal = rank;
                            tempStats.geneName = geneName;
                            tempStats.meanVal = geneStats.meanSelected;
                            tempStats.differentialVal = geneStats.meanSelected - geneStats.meanNonSelected;
                            // createTreeInitial() turns these three counts into the percentages shown in the tree
                            tempStats.abundanceTop = geneStats.abundanceTop;
                            tempStats.abundanceMiddle = geneStats.abundanceMiddle;
                            tempStats.countAbundanceNumerator = geneStats.countAbundanceNumerator;

                            utilityMap[speciesName] = tempStats;
                        }

                        // Work on a copy: the reference tree is shared by all threads and all genes, and
                        // nodes that are missing from utilityMap must not keep the previous gene's values.
                        QJsonObject treeForGene = speciesDataJson;
                        createTreeInitial(treeForGene, utilityMap);

                        // store as a compact string (smaller than a QJsonObject) and parse it again on demand
                        treesForCluster[geneName] = QString::fromUtf8(QJsonDocument(treeForGene).toJson(QJsonDocument::Compact));
                    }

                    {
                        QMutexLocker locker(&precomputedTreesMutex);
                        _precomputedTreesFromTheHierarchy[hierarchyType][clusterName] = std::move(treesForCluster);
                    }
                }
                });

        }

        else
        {
            qDebug() << "Datasets are not valid";
            return;
        }

    }
    else
    {
        qDebug() << "Datasets are not valid";
        return;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    qDebug() << "Time taken for precomputeTreesFromHierarchy : " + QString::number(duration / 1000.0) + " s";
    //_popupMessageInit.hide();
    //_popupMessageTreeCreationCompletion->show();
    //QApplication::processEvents();
}


void SettingsAction::computeGeneMeanExpressionMap()
{
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
    {
        return;
    }


    _clusterGeneMeanExpressionMap.clear();
    auto start = std::chrono::high_resolution_clock::now();
    qDebug() << "Computing gene mean expression map";

    _clusterGeneMeanExpressionMap.clear();
    if (_speciesNamesDataset.getCurrentDataset().isValid() && _mainPointsDataset.getCurrentDataset().isValid()) {
        auto speciesClusterDatasetFull = mv::data().getDataset<Clusters>(_speciesNamesDataset.getCurrentDataset().getDatasetId());
        auto mainPointDatasetFull = mv::data().getDataset<Points>(_mainPointsDataset.getCurrentDataset().getDatasetId());
        if (speciesClusterDatasetFull.isValid() && mainPointDatasetFull.isValid()) {
            auto speciesclusters = speciesClusterDatasetFull->getClusters();
            auto mainPointDimensionNames = mainPointDatasetFull->getDimensionNames();
            std::vector<int> allGeneIndices(mainPointDimensionNames.size());
            std::iota(allGeneIndices.begin(), allGeneIndices.end(), 0);
            QMutex mapMutex;

            QtConcurrent::blockingMap(speciesclusters, [&](const auto& species) {
                auto speciesIndices = species.getIndices();
                auto speciesName = species.getName();
                std::unordered_map<QString, std::pair<int, float>> localGeneMeans;
                localGeneMeans.reserve(mainPointDimensionNames.size());

                computeChunkedGeneMeans(mainPointDatasetFull, allGeneIndices, speciesIndices, [&](int, const std::vector<int>& geneChunkIndices, const std::vector<float>& chunkMeans) {
                    for (int localGeneIndex = 0; localGeneIndex < static_cast<int>(geneChunkIndices.size()); ++localGeneIndex) {
                        const int geneIndex = geneChunkIndices[localGeneIndex];
                        localGeneMeans[mainPointDimensionNames[geneIndex]] = std::make_pair(static_cast<int>(speciesIndices.size()), chunkMeans[localGeneIndex]);
                    }
                    });

                QMutexLocker locker(&mapMutex);
                _clusterGeneMeanExpressionMap[speciesName] = std::move(localGeneMeans);
                });

            _meanMapComputed = true;
        }
    }


    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    qDebug() << "Time taken for computeGeneMeanExpressionMap : " + QString::number(duration / 1000.0) + " s";
}

void SettingsAction::computeHierarchyAppearanceVector()
{
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
    {
        return;
    }

    auto startTimer = std::chrono::high_resolution_clock::now();
    qDebug() << "computeHierarchyAppearanceVector Start";
    _topHierarchyClusterMap.clear();

    if (_mainPointsDataset.getCurrentDataset().isValid()) {
        auto fullMainDataset = mv::data().getDataset<Points>(_mainPointsDataset.getCurrentDataset().getDatasetId());
        auto numOfPoints = fullMainDataset->getNumPoints();
        Dataset<Clusters> clusterDataset;
        if (_topClusterNamesDataset.getCurrentDataset().isValid())
        {
            clusterDataset = mv::data().getDataset<Clusters>(_topClusterNamesDataset.getCurrentDataset().getDatasetId());
        }
        QStringList inclusionList = _topHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
        const auto inclusionSet = toStringSet(inclusionList);
        if (clusterDataset.isValid()) {
            auto clusters = clusterDataset->getClusters();
            if (!clusters.empty()) {

                for (const auto& cluster : clusters) {

                    auto clusterName = cluster.getName();
                    if (inclusionSet.find(clusterName) != inclusionSet.end())
                    {
                        std::vector<bool> clusterNamesAppearance(numOfPoints, false);
                        for (const auto& index : cluster.getIndices()) {
                            if (static_cast<size_t>(index) < clusterNamesAppearance.size()) {
                                clusterNamesAppearance[index] = true;
                            }
                        }
                        _topHierarchyClusterMap[clusterName] = clusterNamesAppearance;
                    }


                }
            }
        }

    }

    auto endTimer = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTimer - startTimer).count();
    qDebug() << "Time taken for computeHierarchyAppearanceVector : " + QString::number(duration / 1000.0) + " s";

}


void SettingsAction::computeFrequencyMapForHierarchyItemsChange(QString hierarchyType)
{
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked() || hierarchyType.isEmpty())
    {
        return;
    }

    auto startTimer = std::chrono::high_resolution_clock::now();
    qDebug() << "computeFrequencyMapForHierarchyItemsChange Start for " + hierarchyType;

    if (!_speciesNamesDataset.getCurrentDataset().isValid() || !_mainPointsDataset.getCurrentDataset().isValid()) {
        return;
    }

    auto speciesClusterDatasetFull = mv::data().getDataset<Clusters>(_speciesNamesDataset.getCurrentDataset().getDatasetId());
    auto mainPointDatasetFull = mv::data().getDataset<Points>(_mainPointsDataset.getCurrentDataset().getDatasetId());
    auto numOfPoints = mainPointDatasetFull->getNumPoints();
    std::vector<bool> clusterNames(numOfPoints, true);
    QStringList inclusionList;
    mv::Dataset<Clusters> clusterDataset;

    if (hierarchyType == "top" && _topClusterNamesDataset.getCurrentDataset().isValid())
    {
        inclusionList = _topHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
        clusterDataset = mv::data().getDataset<Clusters>(_topClusterNamesDataset.getCurrentDataset().getDatasetId());
    }
    /*
    else if (hierarchyType == "middle" && _middleClusterNamesDataset.getCurrentDataset().isValid())
    {
        inclusionList = _middleHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
        clusterDataset = mv::data().getDataset<Clusters>(_middleClusterNamesDataset.getCurrentDataset().getDatasetId());
    }
    else if (hierarchyType == "bottom" && _bottomClusterNamesDataset.getCurrentDataset().isValid())
    {
        inclusionList = _bottomHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
        clusterDataset = mv::data().getDataset<Clusters>(_bottomClusterNamesDataset.getCurrentDataset().getDatasetId());
    }
    */
    const auto inclusionSet = toStringSet(inclusionList);
    if (clusterDataset.isValid())
    {
        for (const auto& cluster : clusterDataset->getClusters())
        {
            if (inclusionSet.find(cluster.getName()) == inclusionSet.end())
            {
                for (const auto& index : cluster.getIndices())
                {
                    if (static_cast<size_t>(index) < clusterNames.size())
                    {
                        clusterNames[index] = false;
                    }
                }
            }
        }
    }

    if (speciesClusterDatasetFull.isValid() && mainPointDatasetFull.isValid())
    {
        auto speciesclusters = speciesClusterDatasetFull->getClusters();
        for (const auto& species : speciesclusters) {
            auto speciesIndices = species.getIndices();
            auto speciesName = species.getName();
            int count = static_cast<int>(std::count_if(speciesIndices.begin(), speciesIndices.end(), [&clusterNames](size_t index) {
                return index < clusterNames.size() && clusterNames[index];
                }));

            if (hierarchyType == "top")
            {
                _clusterSpeciesFrequencyMap[speciesName]["topCells"] = count;
            }
            else if (hierarchyType == "middle")
            {
                _clusterSpeciesFrequencyMap[speciesName]["middleCells"] = count;
            }
            else if (hierarchyType == "bottom")
            {
                _clusterSpeciesFrequencyMap[speciesName]["bottomCells"] = count;
            }
        }
    }

    auto endTimer = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTimer - startTimer).count();
    qDebug() << "Time taken for computeFrequencyMapForHierarchyItemsChange for " + hierarchyType + " : " + QString::number(duration / 1000.0) + " s";
}
/*
void SettingsAction::computeGeneMeanExpressionMapForHierarchyItemsChangeExperimental(QString hierarchyType)
{
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
    {
        return;
    }
    auto startTimer = std::chrono::high_resolution_clock::now();
    qDebug() << "computeGeneMeanExpressionMapForHierarchyItemsChange Experimental Start for " + hierarchyType;
    if (hierarchyType == "")
    {
        return;
    }


    if (_speciesNamesDataset.getCurrentDataset().isValid() && _mainPointsDataset.getCurrentDataset().isValid()) {

        auto speciesClusterDatasetFull = mv::data().getDataset<Clusters>(_speciesNamesDataset.getCurrentDataset().getDatasetId());
        auto mainPointDatasetFull = mv::data().getDataset<Points>(_mainPointsDataset.getCurrentDataset().getDatasetId());
        auto numOfPoints = mainPointDatasetFull->getNumPoints();
        std::vector<bool> clusterNames(numOfPoints, true);
        bool datasetCheck = false;
        QStringList inclusionList;
        if (hierarchyType == "top")
        {
            inclusionList = _topHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
            if (_topClusterNamesDataset.getCurrentDataset().isValid())
            {
                datasetCheck = true;
            }
        }
        else if (hierarchyType == "middle")
        {
            inclusionList = _middleHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
            if (_middleClusterNamesDataset.getCurrentDataset().isValid())
            {
                datasetCheck = true;
            }
        }
        else if (hierarchyType == "bottom")
        {
            inclusionList = _bottomHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
            if (_bottomClusterNamesDataset.getCurrentDataset().isValid())
            {
                datasetCheck = true;
            }
        }

        if (datasetCheck)
        {
            mv::Dataset<Clusters> clusterDataset;

            if (hierarchyType == "top")
            {
                clusterDataset = mv::data().getDataset<Clusters>(_topClusterNamesDataset.getCurrentDataset().getDatasetId());
            }
            else if (hierarchyType == "middle")
            {
                clusterDataset = mv::data().getDataset<Clusters>(_middleClusterNamesDataset.getCurrentDataset().getDatasetId());
            }
            else if (hierarchyType == "bottom")
            {
                clusterDataset = mv::data().getDataset<Clusters>(_bottomClusterNamesDataset.getCurrentDataset().getDatasetId());
            }

            for (auto cluster : clusterDataset->getClusters())
            {
                auto clusterIndices = cluster.getIndices();
                auto clusterName = cluster.getName();
                if (!inclusionList.contains(clusterName))
                {
                    for (auto index : clusterIndices)
                    {
                        clusterNames[index] = false;
                    }
                }

            }
        }



        if (speciesClusterDatasetFull.isValid() && mainPointDatasetFull.isValid())
        {
            auto speciesclusters = speciesClusterDatasetFull->getClusters();
            auto mainPointDimensionNames = mainPointDatasetFull->getDimensionNames();
            for (auto species : speciesclusters) {
                auto speciesIndices = species.getIndices();
                auto speciesName = species.getName();
                std::vector<int> indices;

                // Loop through all species indices to determine if they are in respective clusters
                for (int i = 0; i < speciesIndices.size(); ++i) {
                    // Check if the current species index is present in the cluster and only include those that are true
                    if (std::find(clusterNames.begin(), clusterNames.end(), speciesIndices[i]) != clusterNames.end()) {
                        indices.push_back(i);
                    }

                }

                for (int i = 0; i < mainPointDimensionNames.size(); i++) {
                    auto& geneName = mainPointDimensionNames[i];
                    auto geneIndex = { i };



                    std::vector<float> resultContainer(indices.size());
                    mainPointDatasetFull->populateDataForDimensions(resultContainer, geneIndex, indices);
                    float topMean = calculateMean(resultContainer);

                    if (hierarchyType == "top")
                    {
                        _clusterGeneMeanExpressionMap[speciesName][geneName]["topCells"] = std::make_pair(indices.size(), topMean);
                    }
                    else if (hierarchyType == "middle")
                    {
                        _clusterGeneMeanExpressionMap[speciesName][geneName]["middleCells"] = std::make_pair(indices.size(), topMean);
                    }
                    else if (hierarchyType == "bottom")
                    {
                        _clusterGeneMeanExpressionMap[speciesName][geneName]["bottomCells"] = std::make_pair(indices.size(), topMean);
                    }
                }

            }


        }
    }
    auto endTimer = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTimer - startTimer).count();
    qDebug() << "Time taken for computeGeneMeanExpressionMapForHierarchyItemsChangeExperimental for " + hierarchyType + " : " + QString::number(duration / 1000.0) + " s";

}
void SettingsAction::computeGeneMeanExpressionMapExperimental()
{
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
    {
        return;
    }
    auto start = std::chrono::high_resolution_clock::now();
    qDebug() << "Computing gene mean expression map";


    _clusterGeneMeanExpressionMap.clear();

    if (_speciesNamesDataset.getCurrentDataset().isValid() && _mainPointsDataset.getCurrentDataset().isValid()) {

        auto speciesClusterDatasetFull = mv::data().getDataset<Clusters>(_speciesNamesDataset.getCurrentDataset().getDatasetId());
        auto mainPointDatasetFull = mv::data().getDataset<Points>(_mainPointsDataset.getCurrentDataset().getDatasetId());
        auto numOfPoints = mainPointDatasetFull->getNumPoints();
        std::vector<bool> topClusterNames(numOfPoints, true);
        std::vector<bool> middleClusterNames(numOfPoints, true);
        std::vector<bool>  bottomClusterNames(numOfPoints, true);
        QStringList topInclusionList = _topHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
        QStringList middleInclusionList = _middleHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
        QStringList bottomInclusionList = _bottomHierarchyClusterNamesFrequencyInclusionList.getSelectedOptions();
        if (_topClusterNamesDataset.getCurrentDataset().isValid() && _middleClusterNamesDataset.getCurrentDataset().isValid() && _bottomClusterNamesDataset.getCurrentDataset().isValid())

        {
            auto topClusterDataset = mv::data().getDataset<Clusters>(_topClusterNamesDataset.getCurrentDataset().getDatasetId());
            auto middleClusterDataset = mv::data().getDataset<Clusters>(_middleClusterNamesDataset.getCurrentDataset().getDatasetId());
            auto bottomClusterDataset = mv::data().getDataset<Clusters>(_bottomClusterNamesDataset.getCurrentDataset().getDatasetId());

            auto processCluster = [&](const Clusters& dataset, std::vector<bool>& clusterNames) {
                for (const auto& cluster : dataset.getClusters()) {
                    auto clusterIndices = cluster.getIndices();
                    auto clusterName = cluster.getName();
                    if (!topInclusionList.contains(clusterName)) {
                        for (auto index : clusterIndices) {
                            if (index < clusterNames.size()) {
                                clusterNames[index] = false;
                            }
                        }
                    }
                }
                };

            QFuture<void> topFuture = QtConcurrent::run([&]() { processCluster(*topClusterDataset, topClusterNames); });
            QFuture<void> middleFuture = QtConcurrent::run([&]() { processCluster(*middleClusterDataset, middleClusterNames); });
            QFuture<void> bottomFuture = QtConcurrent::run([&]() { processCluster(*bottomClusterDataset, bottomClusterNames); });

            topFuture.waitForFinished();
            middleFuture.waitForFinished();
            bottomFuture.waitForFinished();
        }



        // Ensure that the types match
        QMutex mapMutex; // Mutex to protect shared access to _clusterGeneMeanExpressionMap

        if (speciesClusterDatasetFull.isValid() && mainPointDatasetFull.isValid()) {
            auto speciesclusters = speciesClusterDatasetFull->getClusters();
            auto mainPointDimensionNames = mainPointDatasetFull->getDimensionNames();

            // Parallel processing of species clusters
            QtConcurrent::blockingMap(speciesclusters, [&](const auto& species) {
                auto speciesIndices = species.getIndices();
                auto speciesName = species.getName();

                std::vector<uint32_t> topIndices;
                std::vector<uint32_t> middleIndices;
                std::vector<uint32_t> bottomIndices;

                // Determine cluster membership for the species
                for (uint32_t i = 0; i < speciesIndices.size(); ++i) {
                    if (std::binary_search(topClusterNames.begin(), topClusterNames.end(), speciesIndices[i])) {
                        topIndices.push_back(i);
                    }
                    if (std::binary_search(middleClusterNames.begin(), middleClusterNames.end(), speciesIndices[i])) {
                        middleIndices.push_back(i);
                    }
                    if (std::binary_search(bottomClusterNames.begin(), bottomClusterNames.end(), speciesIndices[i])) {
                        bottomIndices.push_back(i);
                    }
                }

                // Parallel processing of gene expressions within each species
                QtConcurrent::blockingMap(mainPointDimensionNames, [&](const auto& geneName) {
                    // Manually find the index of the geneName
                    int geneIndex = std::distance(mainPointDimensionNames.begin(),
                    std::find(mainPointDimensionNames.begin(), mainPointDimensionNames.end(), geneName));

                if (geneIndex == mainPointDimensionNames.size()) {
                    // Handle case where geneName is not found if necessary
                    return; // Skip processing if the index is invalid
                }

                auto processCells = [&](const std::vector<uint32_t>& indices, const QString& cellType) {
                    std::vector<float> resultContainer(indices.size());
                    mainPointDatasetFull->populateDataForDimensions(resultContainer, std::vector<int>{geneIndex}, indices);
                    float mean = calculateMean(resultContainer);
                    QMutexLocker locker(&mapMutex);
                    _clusterGeneMeanExpressionMap[speciesName][geneName][cellType] = std::make_pair(indices.size(), mean);
                    };

                processCells(speciesIndices, "allCells");
                processCells(topIndices, "topCells");
                processCells(middleIndices, "middleCells");
                processCells(bottomIndices, "bottomCells");
                    });
                });

            _meanMapComputed = true;
        }



    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    qDebug() << "\n\n++++++++++++++++++Time taken for computeGeneMeanExpressionMap : " + QString::number(duration / 1000.0) + " s";

}
*/
// ============================================================================================================
//  Gene - trait correlation across species
//
//  Question: which genes have an expression that correlates with a trait across species?
//
//   1. species      : the species picked in the species explorer that have a trait value and cells of the selected
//                     cell types (none / fewer than two usable picked -> every species with a trait value)
//   2. cell types   : one or several (Shift + click on the labels of the status bar). Species with fewer than "min cells"
//                     cells of them are left out (the mean of a handful of cells is noise).
//   3. matrix X     : N species x G genes, mean expression of the gene over the cells of the selected cell types
//                     (pooling: "Pool cells" or "Equal cell-type weighting")
//   4. trait t      : one value per species, same order as the rows of X
//   5. r_j          : correlation between column j of X and t. Method: Pearson, Spearman (ranks) or Pearson on
//                     phylogenetically independent contrasts (corrects for the shared ancestry of the species)
//   6. statistics   : two-sided p-value (t distribution, N - 2 degrees of freedom), Benjamini-Hochberg FDR over all
//                     genes, 95% confidence interval (Fisher z) and the leave-one-species-out range of r_j, which shows
//                     whether a single species drives the result.
//
//  Caching: the mean expression of every (species, cell type) pair is computed once and kept. Changing the species,
//  the trait or the pooling only re-combines cached means; only newly selected cell types / species are read from
//  the dataset. The cache is dropped when one of the underlying datasets changes.
// ============================================================================================================

/**
 * Convert the species tree (QJsonObject with "children", "name" and "branchLength") into the plain node list used by the
 * contrast kernel. Leaves whose name is not in \p speciesIndex stay in the tree but belong to no species.
 * @return index of the converted node in \p nodes
 */
static int appendPicNodes(const QJsonObject& node, const std::map<QString, int>& speciesIndex, std::vector<PicNode>& nodes)
{
    const int index = static_cast<int>(nodes.size());
    nodes.emplace_back();
    nodes[index].branchLength = node.value("branchLength").toDouble(0.0);

    const QJsonArray children = node.value("children").toArray();
    if (children.isEmpty()) {
        const auto it = speciesIndex.find(node.value("name").toString());
        if (it != speciesIndex.end()) {
            nodes[index].leafIndex = it->second;
        }
        return index;
    }

    for (const QJsonValue& child : children) {
        const int childIndex = appendPicNodes(child.toObject(), speciesIndex, nodes);
        nodes[index].children.push_back(childIndex); // (nodes may have been reallocated: always index, never keep a reference)
    }
    return index;
}

void SettingsAction::applyClusterLabelSelectionStyles()
{
    for (auto it = _clusterLabelsByName.constBegin(); it != _clusterLabelsByName.constEnd(); ++it)
    {
        ClickableLabel* label = it.value().data();
        if (!label)
        {
            continue;
        }
        const bool selected = _selectedClusterNames.contains(it.key());
        label->setStyleSheet(label->property(selected ? "selectedStyle" : "normalStyle").toString());
    }
}

void SettingsAction::updateTraitReverseSearchFeature()
{
    // Many events request a refresh within the same event-loop burst (species selection, trait change, cell type click,
    // gene click, new model, ...). The single-shot timer coalesces them into one computation.
    if (_traitCorrelationTimer) {
        _traitCorrelationTimer->start();
    }
    else {
        computeTraitCorrelation();
    }
}

void SettingsAction::computeTraitCorrelation()
{
    // ---- 1. Validate inputs ---------------------------------------------------------------------------
    if (!_mainPointsDataset.getCurrentDataset().isValid() || !_speciesNamesDataset.getCurrentDataset().isValid() || !_bottomClusterNamesDataset.getCurrentDataset().isValid()) {
        return;
    }

    QString traitName = _reverseSearchTraitSelectionString.getString();
    QStringList cellTypes = _selectedClusterNames;
    if (cellTypes.isEmpty() && !_selectedClusterName.isEmpty()) {
        cellTypes << _selectedClusterName;
    }
    if (traitName.isEmpty() || cellTypes.isEmpty()) {
        // nothing to correlate with: do not leave the values of a previous trait in the table
        _geneTraitValuesComputed.clear();
        _geneTraitPValues.clear();
        _geneTraitFdrValues.clear();
        _geneTraitExtras.clear();
        _traitSampleSize = 0;
        _traitMethodNote.clear();
        _traitContributionShown = false;
        _traitValueLabel.clear();
        _traitMatrix = TraitMatrix();
        publishTraitCorrelationToModel();
        return;
    }

    Dataset<Points> mainPointsDataset = _mainPointsDataset.getCurrentDataset();
    const std::vector<QString> geneNames = mainPointsDataset->getDimensionNames();
    const size_t geneCount = geneNames.size();
    if (geneCount == 0) {
        return;
    }

    // ---- 2. Trait value per species (metadata of the Phylogenetic Tree View) ----------------------------
    QString metaOptions;
    auto phytv = mv::plugins().getPluginFactory("XSCPhyTVu");
    if (phytv)
    {
        for (auto phytvwPlugin : mv::plugins().getPluginsByFactory(phytv))
        {
            if (phytvwPlugin->getGuiName() != "Phylogenetic Tree View")
                continue;

            auto traitDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(phytvwPlugin->findChildByPath("XSCPhyTVuPlugin Chart/Metadata Options/Trait dataset"));
            if (!traitDatasetPickerAction)
                continue;

            auto traitDataset = traitDatasetPickerAction->getCurrentDataset();
            if (!traitDataset.isValid())
                continue;

            auto data = mv::data().getDataset<XSCTreeMeta>(traitDataset->getId());
            if (data.isValid())
            {
                auto dataString = data->getTreeMetaData();
                metaOptions = filterData(traitName, "Numeric", dataString);
            }
        }
    }
    const QJsonObject traitObject = QJsonDocument::fromJson(metaOptions.toUtf8()).object()["data"].toObject();

    // ---- 3. Cache: valid as long as the underlying datasets stay the same ----------------------------------
    Dataset<Clusters> speciesDataset = _speciesNamesDataset.getCurrentDataset();
    Dataset<Clusters> cellTypeDataset = _bottomClusterNamesDataset.getCurrentDataset();
    const QString cacheKey = mainPointsDataset->getId() + QLatin1Char('|') + speciesDataset->getId() + QLatin1Char('|') + cellTypeDataset->getId();
    if (cacheKey != _traitCache.key) {
        _traitCache = TraitCache();
        _traitCache.key = cacheKey;
    }

    const auto speciesClusters = speciesDataset->getClusters();
    const auto cellTypeClusters = cellTypeDataset->getClusters();

    // sorted point indices of every species (once per dataset, sorted concurrently)
    if (!_traitCache.speciesBuilt) {
        struct SortJob {
            int index = 0;
            std::vector<int> points;
        };
        std::vector<SortJob> sortJobs(speciesClusters.size());
        for (int i = 0; i < static_cast<int>(sortJobs.size()); ++i) {
            sortJobs[i].index = i;
        }
        QtConcurrent::blockingMap(sortJobs, [&](SortJob& job) {
            const auto& idx = speciesClusters[job.index].getIndices();
            job.points.assign(idx.begin(), idx.end());
            std::sort(job.points.begin(), job.points.end());
            });
        for (auto& job : sortJobs) {
            _traitCache.speciesSorted[speciesClusters[job.index].getName()] = std::move(job.points);
        }
        _traitCache.speciesBuilt = true;
    }

    // sorted point indices of the selected cell types (on first use)
    for (const QString& cellType : cellTypes) {
        if (_traitCache.cellTypeSorted.count(cellType) > 0) {
            continue;
        }
        std::vector<int> sortedPoints;
        for (const auto& cluster : cellTypeClusters) {
            if (cluster.getName() == cellType) {
                const auto& idx = cluster.getIndices();
                sortedPoints.assign(idx.begin(), idx.end());
                std::sort(sortedPoints.begin(), sortedPoints.end());
                break;
            }
        }
        _traitCache.cellTypeSorted[cellType] = std::move(sortedPoints);
    }

    // ---- 4. Species that have a numeric trait value ----------------------------------------------------------
    std::map<QString, double> traitPerSpecies; // std::map: stable, sorted order
    for (const auto& entry : _traitCache.speciesSorted) {
        if (!traitObject.contains(entry.first)) {
            continue;
        }
        bool ok = false;
        const double traitValue = traitObject.value(entry.first).toVariant().toString().toDouble(&ok);
        if (ok) {
            traitPerSpecies[entry.first] = traitValue;
        }
    }

    // Minimum number of cells. Pool cells: summed over the selected cell types. Equal cell-type weighting: every cell type
    // needs it (a cell type with one cell would otherwise weigh as much as one with thousands of cells).
    const int minCells = std::max(1, _traitMinCells.getValue());
    const bool equalWeighting = (_traitPoolingMode.getCurrentText() == "Equal cell-type weighting");

    const auto entryKey = [](const QString& species, const QString& cellType) {
        return species + QLatin1Char('\x1f') + cellType;
        };

    // Make sure the mean expression of every (species, selected cell type) pair is in the cache.
    // Missing pairs are computed concurrently; the worker count respects the memory budget.
    const auto ensureEntries = [&](const std::vector<QString>& speciesList) {
        struct EntryJob {
            QString key;
            const std::vector<int>* speciesPoints = nullptr;
            const std::vector<int>* cellTypePoints = nullptr;
            TraitCache::Entry entry;
        };
        std::vector<EntryJob> jobs;
        size_t largestPointSet = 0;
        for (const QString& species : speciesList) {
            for (const QString& cellType : cellTypes) {
                const QString key = entryKey(species, cellType);
                if (_traitCache.entries.count(key) > 0) {
                    continue;
                }
                const std::vector<int>& speciesPoints = _traitCache.speciesSorted.at(species);
                const std::vector<int>& cellTypePoints = _traitCache.cellTypeSorted.at(cellType);
                largestPointSet = std::max(largestPointSet, std::min(speciesPoints.size(), cellTypePoints.size()));
                jobs.push_back({ key, &speciesPoints, &cellTypePoints, {} });
            }
        }
        if (jobs.empty()) {
            return;
        }

        // reading expression data can take a moment the first time: show it
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);
        struct WaitCursorGuard { ~WaitCursorGuard() { QGuiApplication::restoreOverrideCursor(); } } waitCursorGuard;

        std::vector<int> allGeneIndices(geneCount);
        std::iota(allGeneIndices.begin(), allGeneIndices.end(), 0);

        QThreadPool meansPool;
        meansPool.setMaxThreadCount(memorySafeThreadCount(largestPointSet));
        QtConcurrent::blockingMap(&meansPool, jobs, [&](EntryJob& job) {
            const std::vector<int> points = intersectSortedIndicesToInt(*job.speciesPoints, *job.cellTypePoints);
            job.entry.cellCount = static_cast<int>(points.size());
            if (points.empty()) {
                return; // remembered as "no cells" so it is not looked up again
            }
            job.entry.means.assign(geneCount, 0.0f);
            computeChunkedGeneMeans(mainPointsDataset, allGeneIndices, points,
                [&job](int start, const std::vector<int>& chunkGenes, const std::vector<float>& chunkMeans) {
                    for (size_t g = 0; g < chunkGenes.size(); ++g) {
                        job.entry.means[static_cast<size_t>(start) + g] = chunkMeans[g];
                    }
                });
            });

        for (auto& job : jobs) {
            _traitCache.entries[job.key] = std::move(job.entry);
        }
        };

    const auto isUsableSpecies = [&](const QString& species) {
        int totalCells = 0;
        bool anyCellTypeBigEnough = false;
        for (const QString& cellType : cellTypes) {
            const auto it = _traitCache.entries.find(entryKey(species, cellType));
            if (it != _traitCache.entries.end()) {
                totalCells += it->second.cellCount;
                anyCellTypeBigEnough = anyCellTypeBigEnough || it->second.cellCount >= minCells;
            }
        }
        return equalWeighting ? anyCellTypeBigEnough : totalCells >= minCells;
        };

    // ---- 5. Usable species ---------------------------------------------------------------------------------------
    // The cell data of EVERY species with a trait value is read once and cached, so that picking other species afterwards
    // is instant (no dataset access, only a re-combination of cached means).
    std::vector<QString> everySpecies;
    for (const auto& entry : traitPerSpecies) {
        everySpecies.push_back(entry.first);
    }
    ensureEntries(everySpecies);

    std::vector<QString> usable; // trait value AND enough cells of the selected cell type(s)
    for (const QString& species : everySpecies) {
        if (isUsableSpecies(species)) {
            usable.push_back(species);
        }
    }

    _geneTraitValuesComputed.clear();
    _geneTraitPValues.clear();
    _geneTraitFdrValues.clear();
    _geneTraitExtras.clear();
    _traitSampleSize = 0;
    _traitMethodNote.clear();
    _traitContributionShown = false;
    _traitValueLabel.clear();
    _traitMatrix = TraitMatrix();
    if (usable.size() < kMinSpeciesForTraitCorrelation) {
        qDebug() << "Trait correlation needs at least" << kMinSpeciesForTraitCorrelation << "species with trait values and at least" << minCells << "cells of the selected cell types; found" << usable.size();
        _traitMethodNote = QString("No trait correlation: only %1 species have a trait value and at least %2 cells of the selected cell type(s)").arg(usable.size()).arg(minCells);
        publishTraitCorrelationToModel();
        return;
    }

    // ---- 6. Method and species selection ---------------------------------------------------------------------------
    TraitMethod method = TraitMethod::Pearson;
    const QString methodText = _traitCorrelationMethod.getCurrentText();
    if (methodText.startsWith("Spearman")) {
        method = TraitMethod::Spearman;
    }
    else if (methodText.startsWith("Phylogenetic")) {
        method = TraitMethod::Phylogenetic;
    }
    const TraitScope scope = (_traitSpeciesScope.getCurrentText() == "Across selected species only") ? TraitScope::AcrossSelected : TraitScope::SelectedContribution;

    // the species picked in the species explorer, as indices into `usable`
    const QStringList explorerSelection = _speciesExplorerInMap.getSelectedOptions();
    std::vector<size_t> picked;
    QStringList pickedNames;
    for (size_t i = 0; i < usable.size(); ++i) {
        if (explorerSelection.contains(usable[i])) {
            picked.push_back(i);
            pickedNames << usable[i];
        }
    }
    const int ignoredPicks = static_cast<int>(explorerSelection.size()) - static_cast<int>(picked.size()); // picked but without trait value / enough cells

    SpeciesSelectionPlan plan = planSpeciesSelection(usable.size(), picked, scope, method);

    const auto namesOf = [&usable](const std::vector<size_t>& indices) {
        std::vector<QString> names;
        names.reserve(indices.size());
        for (const size_t index : indices) {
            names.push_back(usable[index]);
        }
        return names;
        };
    std::vector<QString> population = namesOf(plan.population);

    // ---- 7. Species tree for phylogenetic contrasts (falls back to Pearson when it is not usable) -------------------
    std::vector<PicNode> treeNodes;
    int treeRoot = 0;
    size_t speciesInTree = 0;
    bool treeFallback = false;

    if (method == TraitMethod::Phylogenetic) {
        // the species tree is the reference tree of the phylogenetic tree view
        const auto treePicker = _referenceTreeDataset.getCurrentDataset();
        if (treePicker.isValid()) {
            auto referenceTree = mv::data().getDataset<XSCTree>(treePicker.getDatasetId());
            if (referenceTree.isValid()) {
                std::map<QString, int> speciesIndex;
                for (size_t i = 0; i < population.size(); ++i) {
                    speciesIndex[population[i]] = static_cast<int>(i);
                }
                treeRoot = appendPicNodes(referenceTree->getTreeData(), speciesIndex, treeNodes);

                double totalBranchLength = 0.0;
                for (const auto& node : treeNodes) {
                    totalBranchLength += node.branchLength;
                    if (node.leafIndex >= 0) {
                        ++speciesInTree;
                    }
                }
                if (totalBranchLength <= 0.0) {
                    for (auto& node : treeNodes) {
                        node.branchLength = 1.0; // the tree has no branch lengths: use its topology only
                    }
                }
            }
        }

        if (speciesInTree < 3) {
            // no usable tree: say so instead of silently pretending a phylogenetic correction was applied
            method = TraitMethod::Pearson;
            treeFallback = true;
            treeNodes.clear();
            plan = planSpeciesSelection(usable.size(), picked, scope, method);
            population = namesOf(plan.population);
        }
    }

    // ---- 8. Expression matrix X (N species x G genes) and trait vector t ----------------------------------------
    const TraitPooling pooling = equalWeighting ? TraitPooling::EqualCellTypeWeight : TraitPooling::PoolCells;

    std::vector<std::vector<CellTypeMeans>> partsPerSpecies(population.size());
    std::vector<double> traitValues;
    traitValues.reserve(population.size());
    for (size_t i = 0; i < population.size(); ++i) {
        for (const QString& cellType : cellTypes) {
            const auto& entry = _traitCache.entries.at(entryKey(population[i], cellType));
            if (entry.cellCount > 0 && (!equalWeighting || entry.cellCount >= minCells)) {
                partsPerSpecies[i].push_back({ static_cast<size_t>(entry.cellCount), &entry.means });
            }
        }
        traitValues.push_back(traitPerSpecies.at(population[i]));
    }

    std::vector<std::vector<float>> expressionMatrix(population.size(), std::vector<float>(geneCount, 0.0f)); // [species][gene]
    std::vector<const std::vector<float>*> expressionRows;
    for (const auto& row : expressionMatrix) {
        expressionRows.push_back(&row);
    }

    // `plan.selected` already refers to rows of the matrix: for the contribution scope the population is everybody (same indices)
    const TraitStatisticsContext statisticsContext = prepareTraitStatisticsContext(method, traitValues, treeNodes, treeRoot, plan.selected);
    std::vector<GeneTraitStatistics> statistics(geneCount);

    // ---- 9. Statistics of every gene (parallel over gene ranges) ----------------------------------------------------
    // Every worker owns a range of genes: it combines the cached per cell type means into the species rows of X and
    // computes correlation, p-value, confidence interval, leave-one-out range and contribution. Disjoint ranges -> no locking.
    auto geneRanges = splitGeneRanges(geneCount, static_cast<size_t>(std::max(1, QThread::idealThreadCount())) * 4);
    QtConcurrent::blockingMap(geneRanges, [&](GeneRange& range) {
        for (size_t i = 0; i < population.size(); ++i) {
            combineCellTypeMeans(partsPerSpecies[i], pooling, range, expressionMatrix[i]);
        }
        computeTraitStatistics(expressionRows, statisticsContext, range, statistics);
        });

    // ---- 10. Multiple testing correction and result -------------------------------------------------------------------
    // p-value, FDR, confidence interval and leave-one-out always describe the correlation across the compared species.
    std::vector<float> pValues(geneCount);
    for (size_t g = 0; g < geneCount; ++g) {
        pValues[g] = statistics[g].pValue;
    }
    const std::vector<float> fdrValues = benjaminiHochberg(pValues);

    _geneTraitValuesComputed.reserve(geneCount);
    _geneTraitPValues.reserve(geneCount);
    _geneTraitFdrValues.reserve(geneCount);
    _geneTraitExtras.reserve(geneCount);
    for (size_t g = 0; g < geneCount; ++g) {
        GeneTraitExtras extras;
        extras.ciLow = statistics[g].ciLow;
        extras.ciHigh = statistics[g].ciHigh;
        extras.looMin = statistics[g].leaveOneOutMin;
        extras.looMax = statistics[g].leaveOneOutMax;
        if (plan.contribution) {
            extras.overall = statistics[g].correlation; // the cell shows the contribution, keep the correlation next to it
        }
        _geneTraitValuesComputed[geneNames[g]] = plan.contribution ? statistics[g].selectionScore : statistics[g].correlation;
        _geneTraitPValues[geneNames[g]] = statistics[g].pValue;
        _geneTraitFdrValues[geneNames[g]] = fdrValues[g];
        _geneTraitExtras[geneNames[g]] = extras;
    }

    // ---- 11. Description for the tooltips ------------------------------------------------------------------------------
    const int sampleSize = (method == TraitMethod::Phylogenetic) ? static_cast<int>(speciesInTree) : static_cast<int>(population.size());
    QString methodName;
    if (method == TraitMethod::Phylogenetic) {
        methodName = QString("Phylogenetic contrasts (%1 of %2 species found in the species tree)").arg(speciesInTree).arg(population.size());
    }
    else if (method == TraitMethod::Spearman) {
        methodName = QString("Spearman rank correlation across %1 species").arg(sampleSize);
    }
    else {
        methodName = QString("Pearson correlation across %1 species").arg(sampleSize);
    }
    if (treeFallback) {
        methodName += "\nNo usable species tree: phylogenetic correction NOT applied";
    }

    QString selectionText;
    if (plan.contribution) {
        if (picked.empty()) {
            selectionText = "No species selected: the value is the overall correlation";
        }
        else {
            QStringList shown = pickedNames.mid(0, 4);
            const QString more = pickedNames.size() > 4 ? QString(" and %1 more").arg(pickedNames.size() - 4) : QString();
            selectionText = QString("Value = contribution of the %1 selected species (%2%3) to the overall correlation\n"
                "(mean of z(gene) x z(trait) over them; equals the correlation when all species are selected, can exceed 1)")
                .arg(pickedNames.size()).arg(shown.join(", ")).arg(more);
        }
    }
    else if (plan.selectionTooSmall) {
        selectionText = QString("Fewer than %1 species selected: all %2 species are used").arg(kMinSpeciesAcrossSelection).arg(population.size());
    }
    else if (!picked.empty()) {
        selectionText = QString("Correlation across the %1 selected species").arg(population.size());
    }
    else {
        selectionText = "No species selected: all species are used";
    }
    if (ignoredPicks > 0) {
        selectionText += QString("\n%1 selected species ignored (no trait value or fewer than %2 cells)").arg(ignoredPicks).arg(minCells);
    }

    _traitSampleSize = sampleSize;
    _traitMethodNote = methodName + "\n" + selectionText;
    _traitContributionShown = plan.contribution;

    // keep the matrix: it feeds the "expression vs trait" plot and the matrix export (the workers are finished, so it can be moved)
    _traitMatrix = TraitMatrix();
    _traitMatrix.species = population;
    _traitMatrix.trait = traitValues;
    _traitMatrix.expression = std::move(expressionMatrix);
    _traitMatrix.selected.assign(population.size(), 0);
    for (size_t i = 0; i < population.size(); ++i) {
        _traitMatrix.selected[i] = explorerSelection.contains(population[i]) ? 1 : 0;
    }
    _traitMatrix.geneIndex.reserve(geneCount);
    for (size_t g = 0; g < geneCount; ++g) {
        _traitMatrix.geneIndex[geneNames[g]] = g;
    }
    _traitMatrix.traitName = traitName;

    const QString methodShortName = (method == TraitMethod::Phylogenetic) ? "phylogenetic contrasts" : (method == TraitMethod::Spearman ? "Spearman" : "Pearson");
    _traitValueLabel = QString(plan.contribution ? "Trait contribution of selected species (%1)" : "Trait correlation (%1)").arg(methodShortName);

    publishTraitCorrelationToModel();
}

void SettingsAction::publishTraitCorrelationToModel()
{
    QVariant variant = _listModel.getVariant();
    auto* model = qobject_cast<QStandardItemModel*>(variant.value<QAbstractItemModel*>());
    if (!model || model->columnCount() <= kTraitCorrelationColumn || model->rowCount() == 0) {
        return;
    }

    {
        // Writing ~20k cells one by one would emit (and re-sort the proxy for) every single change.
        // Block the signals, write everything, then announce the whole column once.
        QSignalBlocker blocker(model);
        for (int row = 0; row < model->rowCount(); ++row) {
            auto* idItem = model->item(row, 0);
            auto* correlationItem = model->item(row, kTraitCorrelationColumn);
            if (!idItem || !correlationItem) {
                continue;
            }

            const QString gene = idItem->text();
            const auto rIt = _geneTraitValuesComputed.find(gene);
            const auto pIt = _geneTraitPValues.find(gene);
            const auto qIt = _geneTraitFdrValues.find(gene);
            const auto eIt = _geneTraitExtras.find(gene);
            setTraitCorrelationItemData(correlationItem,
                rIt != _geneTraitValuesComputed.end() ? static_cast<double>(rIt->second) : 0.0,
                pIt != _geneTraitPValues.end() ? static_cast<double>(pIt->second) : 1.0,
                qIt != _geneTraitFdrValues.end() ? static_cast<double>(qIt->second) : 1.0,
                eIt != _geneTraitExtras.end() ? eIt->second : GeneTraitExtras(),
                _traitSampleSize, _traitMethodNote);
        }
    }

    emit model->dataChanged(model->index(0, kTraitCorrelationColumn), model->index(model->rowCount() - 1, kTraitCorrelationColumn), { Qt::DisplayRole, Qt::UserRole, Qt::ToolTipRole, kTraitPValueRole, kTraitFdrRole, kTraitStatsRole });
}



void SettingsAction::findTopNGenesPerCluster() {

    int n = _topNGenesFilter.getValue();

    if (_clusterNameToGeneNameToExpressionValue.empty() || n <= 0) {
        return;
    }

    // startCodeTimer("findTopNGenesPerCluster");

    enum class SelectionOption {
        AbsoluteTopN,
        PositiveTopN,
        NegativeTopN
    };
    auto optionValue = _typeofTopNGenes.getCurrentText();
    SelectionOption option = SelectionOption::AbsoluteTopN;
    if (optionValue == "Positive") {
        option = SelectionOption::PositiveTopN;
    }
    else if (optionValue == "Negative") {
        option = SelectionOption::NegativeTopN;
    }

    _uniqueReturnGeneList.clear();
    std::map<QString, std::vector<QString>> geneAppearanceCounter;
    std::map<QString, std::vector<std::pair<QString, int>>> rankingMap;
    std::vector<QString> speciesOrder;
    speciesOrder.reserve(_clusterNameToGeneNameToExpressionValue.size());

    // ---- Stage 1 (parallel): order the genes of every species by differential expression ----
    // Sorting ~20k genes per species dominates this function and species are independent of each other.
    struct GeneRankingEntry {
        QString geneName;
        float differenceMeanValue;
        float meanSelected;
    };
    struct SpeciesRanking {
        QString speciesName;
        const std::map<QString, Stats>* geneStats = nullptr;
        std::vector<GeneRankingEntry> sortedGenes;
    };

    std::vector<SpeciesRanking> speciesRankings;
    speciesRankings.reserve(_clusterNameToGeneNameToExpressionValue.size());
    for (const auto& outerPair : _clusterNameToGeneNameToExpressionValue) {
        speciesRankings.push_back({ outerPair.first, &outerPair.second, {} });
        speciesOrder.push_back(outerPair.first);
    }

    QtConcurrent::blockingMap(speciesRankings, [option](SpeciesRanking& ranking) {
        ranking.sortedGenes.reserve(ranking.geneStats->size());
        for (const auto& innerPair : *ranking.geneStats) {
            const auto& stats = innerPair.second;
            ranking.sortedGenes.push_back({ innerPair.first, stats.meanSelected - stats.meanNonSelected, stats.meanSelected });
        }

        if (option == SelectionOption::AbsoluteTopN) {
            std::sort(ranking.sortedGenes.begin(), ranking.sortedGenes.end(), [](const auto& a, const auto& b) {
                return std::abs(a.differenceMeanValue) > std::abs(b.differenceMeanValue);
                });
        }
        else {
            std::sort(ranking.sortedGenes.begin(), ranking.sortedGenes.end(), [](const auto& a, const auto& b) {
                return a.differenceMeanValue > b.differenceMeanValue;
                });
            if (option == SelectionOption::NegativeTopN) {
                std::reverse(ranking.sortedGenes.begin(), ranking.sortedGenes.end());
            }
        }
        });

    // ---- Stage 2 (serial, cheap): collect the top-N genes and the per-gene rank of every species ----
    for (const auto& ranking : speciesRankings) {
        const auto& speciesName = ranking.speciesName;
        const auto& geneExpressionVec = ranking.sortedGenes;
        for (int i = 0; i < static_cast<int>(geneExpressionVec.size()); ++i) {
            const auto& entry = geneExpressionVec[i];
            if (i < n) {
                _uniqueReturnGeneList.insert(entry.geneName);
                if (entry.meanSelected > 0) {
                    geneAppearanceCounter[entry.geneName].push_back(speciesName);
                }
            }
            const int rank = (option == SelectionOption::NegativeTopN) ? static_cast<int>(geneExpressionVec.size()) - i : i + 1;
            rankingMap[entry.geneName].emplace_back(speciesName, rank);
        }
    }

    //iterate std::map<QString, std::vector<std::pair<QString, int>>> rankingMap;
    // Iterating over the map
    if (_performGeneTableTsneAction.isChecked()) {
        std::vector<float> rankOrder;
        _geneOrder.clear();
        _geneOrder.reserve(rankingMap.size());
        for (const auto& item : rankingMap) {
            _geneOrder.push_back(item.first);
        }
        rankOrder.resize(_geneOrder.size() * speciesOrder.size(), 0.0f); // Initialize with 0.0f for clarity

        std::unordered_map<QString, int> speciesIndexMap;
        speciesIndexMap.reserve(speciesOrder.size());
        for (int i = 0; i < static_cast<int>(speciesOrder.size()); ++i) {
            speciesIndexMap[speciesOrder[i]] = i;
        }

        for (int geneIndex = 0; geneIndex < _geneOrder.size(); ++geneIndex) {
            const QString& gene = _geneOrder[geneIndex];
            const auto& speciesRanks = rankingMap[gene];
            for (const auto& speciesRank : speciesRanks) {
                const QString& species = speciesRank.first;
                const float rank = (speciesRank.second <= n) ? 1.0f : 0.0f;
                int speciesIndex = speciesIndexMap[species];
                rankOrder[geneIndex * speciesOrder.size() + speciesIndex] = rank;
            }
        }

        if (!_geneSimilarityPoints.isValid())
        {
            qDebug() << "Gene similarity dataset is not valid";
            return;
        }
        QString pointDataId = _geneSimilarityPoints->getId();
        int pointDimSize = static_cast<int>(speciesOrder.size());
        int pointIndicesSize = static_cast<int>(_geneOrder.size());

        if (_selectedPointsTSNEDatasetForGeneTable.isValid())
        {
            auto runningAction = dynamic_cast<TriggerAction*>(_selectedPointsTSNEDatasetForGeneTable->findChildByPath("TSNE/TsneComputationAction/Running"));

            if (runningAction)
            {

                if (runningAction->isChecked())
                {
                    auto stopAction = dynamic_cast<TriggerAction*>(_selectedPointsTSNEDatasetForGeneTable->findChildByPath("TSNE/TsneComputationAction/Stop"));
                    if (stopAction)
                    {
                        stopAction->trigger();
                        //std::this_thread::sleep_for(std::chrono::seconds(5));
                    }
                }

            }
            mv::data().removeDataset(_selectedPointsTSNEDatasetForGeneTable);
        }

        populatePointData(pointDataId, rankOrder, pointIndicesSize, pointDimSize, speciesOrder);

        mv::plugin::AnalysisPlugin* analysisPlugin = nullptr;
        auto scatterplotModificationsGeneSimilarity = [this]() {
            if (_selectedPointsTSNEDatasetForGeneTable.isValid()) {
                auto scatterplotViewFactory = mv::plugins().getPluginFactory("Scatterplot View");
                mv::gui::DatasetPickerAction* colorDatasetPickerAction = nullptr;
                mv::gui::DatasetPickerAction* pointDatasetPickerAction = nullptr;
                mv::gui::ViewPluginSamplerAction* samplerActionAction = nullptr;
                if (scatterplotViewFactory) {
                    for (auto plugin : mv::plugins().getPluginsByFactory(scatterplotViewFactory)) {
                        if (plugin->getGuiName() == "Scatterplot Cell Selection Overview") {
                            pointDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Position"));
                            if (pointDatasetPickerAction) {
                                pointDatasetPickerAction->setCurrentText("");

                                pointDatasetPickerAction->setCurrentDataset(_selectedPointsTSNEDatasetForGeneTable);

                                colorDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Color"));
                                if (colorDatasetPickerAction)
                                {
                                    colorDatasetPickerAction->setCurrentText("");

                                    if (_geneSimilarityClusterColoring.isValid())
                                    {
                                        colorDatasetPickerAction->setCurrentDataset(_geneSimilarityClusterColoring);
                                        auto legendViewFactory = mv::plugins().getPluginFactory("ChartLegend View");
                                        if (legendViewFactory)
                                        {
                                            for (auto legendPlugin : mv::plugins().getPluginsByFactory(legendViewFactory))
                                            {
                                                if (legendPlugin->getGuiName() == "Legend View")
                                                {
                                                    //legendPlugin->printChildren();
                                                    auto legendDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Cluster dataset"));
                                                    if (legendDatasetPickerAction)
                                                    {
                                                        legendDatasetPickerAction->setCurrentDataset(_geneSimilarityClusterColoring);
                                                    }
                                                    auto chartTitle = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Chart Title"));
                                                    if (chartTitle)
                                                    {
                                                        chartTitle->setString("Cell types");
                                                    }
                                                    /*
                                                    auto selectionColor = dynamic_cast<ColorAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Selection color"));
                                                    if (selectionColor)
                                                    {
                                                        selectionColor->setColor(QColor(53, 126, 199));
                                                    }
                                                    auto selectionStringDelimiter = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Delimiter"));
                                                    if (selectionStringDelimiter)
                                                    {
                                                        selectionStringDelimiter->setString(",");
                                                    }

                                                    auto selectionClustersString = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Cluster Selection string"));
                                                    if (selectionClustersString)
                                                    {
                                                        selectionClustersString->setString(""); //TODO
                                                    }
                                                    */
                                                }
                                            }
                                        }

                                    }
                                }

                                samplerActionAction = plugin->findChildByPath<mv::gui::ViewPluginSamplerAction>("Sampler");

                                if (samplerActionAction)
                                {
                                    samplerActionAction->setHtmlViewGeneratorFunction([this](const ViewPluginSamplerAction::SampleContext& toolTipContext) -> QString {
                                        QString clusterDatasetId = _speciesNamesDataset.getCurrentDataset().getDatasetId();
                                        return generateTooltip(toolTipContext, clusterDatasetId, true, "GlobalPointIndices");
                                        });
                                }
                            }
                        }
                    }
                }
            }

            };



        {
            //startCodeTimer("Part10");
            analysisPlugin = mv::plugins().requestPlugin<AnalysisPlugin>("tSNE Analysis", { _geneSimilarityPoints });
            if (!analysisPlugin) {
                qDebug() << "Could not find create TSNE Analysis";
                return;
            }
            _selectedPointsTSNEDatasetForGeneTable = analysisPlugin->getOutputDataset();
            int groupID2 = 10 * 3;
            _selectedPointsTSNEDatasetForGeneTable->setGroupIndex(groupID2);
            if (_selectedPointsTSNEDatasetForGeneTable.isValid())
            {
                //_selectedPointsTSNEDatasetForGeneTable->printChildren();
                bool skip = false;
                int perplexity = std::min(static_cast<int>(_geneOrder.size()), _tsnePerplexity.getValue());
                if (perplexity < 5)
                {
                    qDebug() << "Perplexity is less than 5";
                    skip = true;
                    //_startComputationTriggerAction.setDisabled(false);
                }
                if (!skip)
                {
                    if (perplexity != _tsnePerplexity.getValue())
                    {
                        _tsnePerplexity.setValue(perplexity);
                    }

                    auto perplexityAction = dynamic_cast<IntegralAction*>(_selectedPointsTSNEDatasetForGeneTable->findChildByPath("TSNE/Perplexity"));
                    if (perplexityAction)
                    {
                        //qDebug() << "Perplexity: Found";
                        perplexityAction->setValue(perplexity);
                    }
                    else
                    {
                        qDebug() << "Perplexity: Not Found";
                    }

                    QString knnAlgorithmValue = _performGeneTableTsneKnn.getCurrentText();
                    QString distanceMetricValue = _performGeneTableTsneDistance.getCurrentText();
                    if (knnAlgorithmValue != "")
                    {
                        auto knnAction = dynamic_cast<OptionAction*>(_selectedPointsTSNEDatasetForGeneTable->findChildByPath("TSNE/kNN Algorithm"));
                        if (knnAction)
                        {
                            //qDebug() << "Knn: Found";
                            try {
                                knnAction->setCurrentText(knnAlgorithmValue);
                            }
                            catch (const std::exception& e) {
                                qDebug() << "An exception occurred in setting knn value: " << e.what();
                            }
                        }
                        else
                        {
                            qDebug() << "Knn: Not Found";
                        }
                    }
                    if (distanceMetricValue != "")
                    {
                        auto distanceAction = dynamic_cast<OptionAction*>(_selectedPointsTSNEDatasetForGeneTable->findChildByPath("TSNE/Distance metric"));
                        if (distanceAction)
                        {
                            //qDebug() << "Distance: Found";
                            try {
                                distanceAction->setCurrentText(distanceMetricValue);
                            }
                            catch (const std::exception& e) {
                                qDebug() << "An exception occurred in setting distance value: " << e.what();
                            }
                        }
                        else
                        {
                            qDebug() << "Distance: Not Found";
                        }
                    }

                    scatterplotModificationsGeneSimilarity();

                    auto startAction = dynamic_cast<TriggerAction*>(_selectedPointsTSNEDatasetForGeneTable->findChildByPath("TSNE/TsneComputationAction/Start"));
                    if (startAction) {

                        startAction->trigger();

                        analysisPlugin->getOutputDataset()->setSelectionIndices({});
                    }
                }
            }
            //stopCodeTimer("Part10");
        }

        std::vector<int> selectedIndices;
        std::vector<int> nonselectedIndices;
        selectedIndices.reserve(_geneOrder.size()); // Pre-allocate memory
        nonselectedIndices.reserve(_geneOrder.size()); // Pre-allocate memory
        for (int i = 0; i < _geneOrder.size(); i++)
        {
            if (_uniqueReturnGeneList.find(_geneOrder[i]) != _uniqueReturnGeneList.end())
            {
                selectedIndices.push_back(i);
            }
            else
            {
                nonselectedIndices.push_back(i);
            }
        }
        QString clusterDataId = _geneSimilarityClusterColoring->getId();
        QColor selectedColor = QColor("#00A2ED");
        QColor nonSelectedColor = QColor("#ff5d12");
        std::map<QString, std::pair<QColor, std::vector<int>>> selectedClusterMap;
        selectedClusterMap["TopNSelectedGenes"] = { selectedColor, selectedIndices };
        selectedClusterMap["NonTopNGenes"] = { nonSelectedColor, nonselectedIndices };

        populateClusterData(clusterDataId, selectedClusterMap);
    }

    //stopCodeTimer("findTopNGenesPerCluster");
    QVariant returnedmodel = createModelFromData(_clusterNameToGeneNameToExpressionValue, geneAppearanceCounter, rankingMap, n);

    setModifiedTriggeredData(returnedmodel);
    _selectedGene.setString("");
    //return returnedmodel;
}

