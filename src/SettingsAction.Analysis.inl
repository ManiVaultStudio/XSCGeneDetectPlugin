namespace {
    /** Calls a function when it goes out of scope (reports one finished background read, also when it returned early). */
    struct StatusTick {
        explicit StatusTick(std::function<void()> callback) : done(std::move(callback)) {}
        std::function<void()> done;
        ~StatusTick() {
            if (done) {
                done();
            }
        }
        StatusTick(const StatusTick&) = delete;
        StatusTick& operator=(const StatusTick&) = delete;
    };
}

/*
void SettingsAction::triggerTrippleHierarchyFrequencyChange()
{
    XSC_TRACE("SettingsAction::triggerTrippleHierarchyFrequencyChange");
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
    XSC_TRACE("SettingsAction::filterData");
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
    XSC_TRACE("SettingsAction::updateButtonTriggered");
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
        QElapsedTimer updateTimer;
        updateTimer.start();
        showStatusMessage(QStringLiteral("Updating the selection statistics..."), QStringLiteral("#007bff"));
        removeTemporaryDatasets(); // all temporary datasets of the previous update (groups 10, 20 and 30), not only group 10
        qDebug() << "[Update] previous temporary datasets removed:" << updateTimer.elapsed() << "ms";
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

        std::vector<int> pointsDatasetallColumnIndices(PointStreaming::usableGeneCount(pointsDatasetallColumnNameList.size(), pointsDatasetRaw->getNumDimensions()));
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
                                _selectedTsneAnalysisPlugin = analysisPlugin; // remembered so that the next update can destroy it
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

                    // position of every cell inside the selection (-1: not selected). A flat array is much faster than a hash
                    // map for the millions of lookups below and it can be read from several threads.
                    std::vector<int> selectedPositionOf(static_cast<size_t>(pointsDatasetRaw->getNumPoints()), -1);
                    for (int selectedPosition = 0; selectedPosition < static_cast<int>(_selectedIndicesFromStorage.size()); ++selectedPosition) {
                        const size_t globalIndex = static_cast<size_t>(_selectedIndicesFromStorage[selectedPosition]);
                        if (globalIndex < selectedPositionOf.size()) {
                            selectedPositionOf[globalIndex] = selectedPosition;
                        }
                    }

                    //startCodeTimer("Part12");
                    //startCodeTimer("Part12.1");
                    QFuture<void> futureClusterCVals = QtConcurrent::run([&]() {
                        for (const auto& clusters : std::as_const(clustersValuesAll)) { // (as_const: no detaching deep copy of all clusters)
                            const auto& clusterIndices = clusters.getIndices();
                            auto clusterName = clusters.getName();
                            auto clusterColor = clusters.getColor();
                            std::vector<int> filteredIndices;
                            filteredIndices.reserve(clusterIndices.size());

                            for (const auto index : clusterIndices) {
                                const int selectedPosition = (static_cast<size_t>(index) < selectedPositionOf.size()) ? selectedPositionOf[index] : -1;
                                if (selectedPosition >= 0) {
                                    filteredIndices.push_back(selectedPosition);
                                }
                            }

                            selectedClustersMap[clusterName] = { clusterColor, std::move(filteredIndices) };
                        }
                        });
                    QFuture<void> futureSpeciesCVals = QtConcurrent::run([&]() {
                        for (const auto& clusters : std::as_const(speciesValuesAll)) {
                            const auto& clusterIndices = clusters.getIndices();
                            auto clusterName = clusters.getName();
                            auto clusterColor = clusters.getColor();
                            std::vector<int> filteredIndices;
                            filteredIndices.reserve(clusterIndices.size());

                            for (const auto index : clusterIndices) {
                                const int selectedPosition = (static_cast<size_t>(index) < selectedPositionOf.size()) ? selectedPositionOf[index] : -1;
                                if (selectedPosition >= 0) {
                                    filteredIndices.push_back(selectedPosition);
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
                            const int selectedPosition = (static_cast<size_t>(globalIndex) < selectedPositionOf.size()) ? selectedPositionOf[globalIndex] : -1;
                            if (selectedPosition >= 0) {
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

                    // Cluster abundance per species: how many of its cells lie in the included top-hierarchy clusters (abundanceTop,
                    // abundanceMiddle counts the clusters that contain a selected cell) and how many selected cells do (numerator).
                    // The cell counts per cluster do not depend on the selection: they are computed once per hierarchy and inclusion
                    // and cached. For every update only the selected cells are visited, instead of every cell of every cluster.
                    std::vector<const std::vector<bool>*> includedClusterBits;
                    for (const auto& cluster : _topHierarchyClusterMap) {
                        if (inclusionSet.find(cluster.first) != inclusionSet.end()) {
                            includedClusterBits.push_back(&cluster.second);
                        }
                    }
                    const size_t clusterCount = includedClusterBits.size();
                    const size_t speciesTotal = speciesValuesAll.size();
                    QStringList sortedInclusion = inclusionList;
                    sortedInclusion.sort();
                    const QString abundanceKey = QString::number(_topHierarchyGeneration) + QLatin1Char('|') + speciesDataset->getId()
                        + QLatin1Char('|') + sortedInclusion.join(QLatin1Char('\x1f'));
                    if (abundanceKey != _abundanceCache.key) {
                        _abundanceCache = AbundanceCache();
                        _abundanceCache.key = abundanceKey;
                        _abundanceCache.clusterSizes.assign(speciesTotal, std::vector<int>(clusterCount, 0));
                        for (size_t s = 0; s < speciesTotal; ++s) {
                            const auto& speciesIndices = speciesValuesAll[s].getIndices();
                            for (size_t k = 0; k < clusterCount; ++k) {
                                const std::vector<bool>& bits = *includedClusterBits[k];
                                int size = 0;
                                for (const auto point : speciesIndices) {
                                    if (bits[static_cast<size_t>(point)]) {
                                        ++size;
                                    }
                                }
                                _abundanceCache.clusterSizes[s][k] = size;
                            }
                        }
                    }

                    for (size_t s = 0; s < speciesTotal; ++s) {
                        auto& meta = speciesComputationMeta[s];
                        std::vector<char> clusterPresent(clusterCount, 0);
                        for (const int globalIndex : selectedGlobalIndicesPerSpecies[s]) {
                            for (size_t k = 0; k < clusterCount; ++k) {
                                if ((*includedClusterBits[k])[static_cast<size_t>(globalIndex)]) {
                                    meta.countAbundanceNumerator++;
                                    clusterPresent[k] = 1;
                                }
                            }
                        }
                        for (size_t k = 0; k < clusterCount; ++k) {
                            const int size = _abundanceCache.clusterSizes[s][k];
                            meta.abundanceTop += size;
                            if (clusterPresent[k]) {
                                meta.abundanceMiddle += size;
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

                        // ---- Stage 1 (parallel over row blocks): mean expression of the SELECTED cells, per species ----
                        // Every selected cell is streamed once. A block of cells adds its values straight into per-species
                        // sums for all genes; the blocks are merged in a fixed order, so the result does not depend on timing.
                        // Stage 1 (the full scan of the selected cells) only depends on the selected cells, their species and the genes:
                        // when those are unchanged (a change of top-N, cell type or trait) the previous result is reused.
                        const auto hashInts = [](std::uint64_t seed, const auto& values) {
                            std::uint64_t hash = seed;
                            for (const auto value : values) {
                                hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(value)) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
                            }
                            return hash;
                            };
                        const QString stage1Key = QString::number(hashInts(hashInts(1469598103934665603ull, _selectedIndicesFromStorage), selectedPointSpeciesIndex), 16)
                            + QLatin1Char('|') + pointsDatasetRaw->getId() + QLatin1Char('|') + QString::number(totalGeneCount) + QLatin1Char('|')
                            + QString::number(speciesCount) + QLatin1Char('|') + QString::number(_selectedIndicesFromStorage.size());
                        const bool stage1Cached = (stage1Key == _selectedMeansCache.key);
                        auto& speciesSelectedMeans = _selectedMeansCache.means; // the cache owns the matrix (no second copy)
                        if (!stage1Cached) {
                        const std::vector<std::uint64_t> geneColumns(pointsDatasetallColumnIndices.begin(), pointsDatasetallColumnIndices.end());
                        using SpeciesGeneSums = std::vector<std::vector<double>>;
                        // Each thread holds one species x genes sum table; keep all of them within a fixed memory budget.
                        const std::size_t stage1TableBytes = static_cast<std::size_t>(speciesCount) * static_cast<std::size_t>(totalGeneCount) * sizeof(double);
                        const std::size_t stage1BudgetBytes = std::size_t{ 256 } << 20;
                        XSC_LOG("stage 1: selected cells", _selectedIndicesFromStorage.size(), "species", speciesCount, "genes", totalGeneCount);
                        const int stage1Threads = std::max(1, std::min({ 64,
                            static_cast<int>(std::thread::hardware_concurrency()),
                            static_cast<int>(stage1BudgetBytes / std::max<std::size_t>(stage1TableBytes, 1)) }));
                        struct Stage1State {
                            SpeciesGeneSums          sums;
                            PointStreaming::SegmentColumns segment;
                        };
                        const Stage1State stage1 = PointStreaming::reduceRows(pointsDatasetRaw.get(), _selectedIndicesFromStorage, stage1Threads, static_cast<std::uint64_t>(totalGeneCount),
                            [&]() {
                                Stage1State state;
                                state.sums.assign(static_cast<std::size_t>(speciesCount), std::vector<double>(totalGeneCount, 0.0));
                                return state;
                            },
                            [&](Stage1State& state, std::size_t selectedRowIndex, const PointDataAccessView& view, std::uint64_t row, std::uint64_t offset) {
                                const int speciesIndex = selectedPointSpeciesIndex[selectedRowIndex];
                                if (speciesIndex < 0) {
                                    return;
                                }
                                state.segment.update(geneColumns, offset, view.columns);
                                PointStreaming::accumulateSegment(view, row, state.segment, state.sums[speciesIndex].data(), nullptr);
                            },
                            [](Stage1State& into, Stage1State& from) {
                                for (std::size_t s = 0; s < into.sums.size(); ++s) {
                                    for (std::size_t g = 0; g < into.sums[s].size(); ++g) {
                                        into.sums[s][g] += from.sums[s][g];
                                    }
                                }
                            });
                        const SpeciesGeneSums& speciesSums = stage1.sums;

                        speciesSelectedMeans.assign(speciesCount, std::vector<float>(totalGeneCount, 0.0f));
                        for (int speciesIndex = 0; speciesIndex < speciesCount; ++speciesIndex) {
                            const int selectedCount = speciesComputationMeta[speciesIndex].selectedCellCount;
                            if (selectedCount <= 0) {
                                continue;
                            }
                            for (int geneOffset = 0; geneOffset < totalGeneCount; ++geneOffset) {
                                speciesSelectedMeans[speciesIndex][geneOffset] =
                                    static_cast<float>(speciesSums[speciesIndex][geneOffset] / static_cast<double>(selectedCount));
                            }
                        }

                        _selectedMeansCache.key = stage1Key;
                        }
                        XSC_LOG("stage 1: done");
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

                        // the genes in name order: the per-species maps are filled in this order, so every insert is at the end (linear)
                        std::vector<int> genesByName(static_cast<size_t>(totalGeneCount));
                        std::iota(genesByName.begin(), genesByName.end(), 0);
                        std::sort(genesByName.begin(), genesByName.end(), [&](int a, int b) {
                            return pointsDatasetallColumnNameList[a] < pointsDatasetallColumnNameList[b];
                            });

                        QtConcurrent::blockingMap(speciesJobs, [&](SpeciesStatsJob& job) {
                            const auto& meta = speciesComputationMeta[job.speciesIndex];
                            const auto& baselineMeans = _clusterGeneMeanExpressionMap.at(meta.name);
                            const auto& selectedMeans = speciesSelectedMeans[job.speciesIndex];

                            for (const int geneIndex : genesByName) {
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
                                job.target->emplace_hint(job.target->end(), geneName, valueStats); // (the target is empty, the names are ascending)
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
                    qDebug() << "[Update] expression statistics finished:" << updateTimer.elapsed() << "ms";
                    findTopNGenesPerCluster();
                    qDebug() << "[Update] top genes and gene table model finished:" << updateTimer.elapsed() << "ms";
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
        showStatusMessage(QString("Selection statistics updated in %1 ms").arg(updateTimer.elapsed()), QStringLiteral("#28a745"));
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
    XSC_TRACE("SettingsAction::updateClusterInfoStatusBar");
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
                        requestSpeciesDifferentialExpression(); // (does nothing while the species DE window is closed)
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
    XSC_TRACE("SettingsAction::setModifiedTriggeredData");
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
    XSC_TRACE("createTreeInitial");
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
    XSC_TRACE("SettingsAction::precomputeTreesFromHierarchy");
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
        std::vector<int> allGeneIndices(PointStreaming::usableGeneCount(mainPointDimensionNames.size(), mainPointsDataset->getNumDimensions()));
        std::iota(allGeneIndices.begin(), allGeneIndices.end(), 0);

        QVector<Cluster> topClusters = topClusterNamesDataset->getClusters();
        QVector<Cluster>  middleClusters = middleClusterNamesDataset->getClusters();
        QVector<Cluster>  bottomClusters = bottomClusterNamesDataset->getClusters();

        if (!mainPointDimensionNames.empty()) {
            std::map<QString, QVector<Cluster>> combinedClusters = {
                { "top", topClusters },
                { "middle", middleClusters },
                { "bottom", bottomClusters }
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
    XSC_TRACE("SettingsAction::computeGeneMeanExpressionMap");
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
            std::vector<int> allGeneIndices(PointStreaming::usableGeneCount(mainPointDimensionNames.size(), mainPointDatasetFull->getNumDimensions()));
            std::iota(allGeneIndices.begin(), allGeneIndices.end(), 0);
            QMutex mapMutex;

            // every worker reads (cells of its species) x kGeneChunkSize floats at a time: bound the number of workers
            size_t largestSpecies = 0;
            for (const auto& species : std::as_const(speciesclusters)) {
                largestSpecies = std::max(largestSpecies, species.getIndices().size());
            }
            QThreadPool meanMapPool;
            meanMapPool.setMaxThreadCount(parallelWorkers(largestSpecies));

            QtConcurrent::blockingMap(&meanMapPool, speciesclusters, [&](const auto& species) { XSC_LOG("species job:", species.getName());
                const auto& speciesIndices = species.getIndices();
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
            _meanMapSignature = currentMeanMapSignature();
        }
    }


    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    qDebug() << "Time taken for computeGeneMeanExpressionMap : " + QString::number(duration / 1000.0) + " s";
}

void SettingsAction::computeHierarchyAppearanceVector()
{
    XSC_TRACE("SettingsAction::computeHierarchyAppearanceVector");
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
    {
        return;
    }

    auto startTimer = std::chrono::high_resolution_clock::now();
    qDebug() << "computeHierarchyAppearanceVector Start";
    _topHierarchyClusterMap.clear();
    ++_topHierarchyGeneration;

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
    XSC_TRACE("SettingsAction::computeFrequencyMapForHierarchyItemsChange");
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
    XSC_TRACE("SettingsAction::computeGeneMeanExpressionMapForHierarchyItemsChangeExperimental");
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
            std::vector<int> geneColumnsSpeciesSummary(mainPointDimensionNames.size());
            std::iota(geneColumnsSpeciesSummary.begin(), geneColumnsSpeciesSummary.end(), 0);
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

                const auto speciesAggregates = PointStreaming::aggregateColumns(mainPointDatasetFull.get(), indices, geneColumnsSpeciesSummary);
                for (int i = 0; i < mainPointDimensionNames.size(); i++) {
                    auto& geneName = mainPointDimensionNames[i];
                    const float topMean = speciesAggregates.rows > 0
                        ? static_cast<float>(speciesAggregates.sums[i] / static_cast<double>(speciesAggregates.rows))
                        : 0.0f;

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
    XSC_TRACE("SettingsAction::computeGeneMeanExpressionMapExperimental");
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

            // Parallel processing of species clusters. Each species streams its cells once per cell group and derives
            // the mean of every gene from that single pass (instead of one pass per gene and cell group).
            std::vector<int> geneColumnsCellGroups(mainPointDimensionNames.size());
            std::iota(geneColumnsCellGroups.begin(), geneColumnsCellGroups.end(), 0);

            QtConcurrent::blockingMap(speciesclusters, [&](const auto& species) { XSC_LOG("species job:", species.getName());
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

                auto storeMeans = [&](const std::vector<uint32_t>& indices, const QString& cellType) {
                    const auto aggregates = PointStreaming::aggregateColumns(mainPointDatasetFull.get(), indices, geneColumnsCellGroups);
                    QMutexLocker locker(&mapMutex);
                    for (std::size_t g = 0; g < geneColumnsCellGroups.size(); ++g) {
                        const float mean = aggregates.rows > 0
                            ? static_cast<float>(aggregates.sums[g] / static_cast<double>(aggregates.rows))
                            : 0.0f;
                        _clusterGeneMeanExpressionMap[speciesName][mainPointDimensionNames[g]][cellType] = std::make_pair(indices.size(), mean);
                    }
                };

                storeMeans(speciesIndices, "allCells");
                storeMeans(topIndices, "topCells");
                storeMeans(middleIndices, "middleCells");
                storeMeans(bottomIndices, "bottomCells");
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
    XSC_TRACE("appendPicNodes");
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
    XSC_TRACE("SettingsAction::applyClusterLabelSelectionStyles");
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
    XSC_TRACE("SettingsAction::updateTraitReverseSearchFeature");
    // Many events request a refresh within the same event-loop burst (species selection, trait change, cell type click,
    // gene click, new model, ...). The single-shot timer coalesces them into one computation.
    if (_traitCorrelationTimer) {
        _traitCorrelationTimer->start();
    }
    else {
        computeTraitCorrelation();
    }
}

void SettingsAction::onTraitJobsFinished()
{
    XSC_TRACE("SettingsAction::onTraitJobsFinished");
    QGuiApplication::restoreOverrideCursor();
    if (_traitJobs) {
        for (auto& job : *_traitJobs) {
            _traitCache.entries[job.key] = std::move(job.entry);
        }
        _traitJobs.reset();
    }
    _traitJobDataset = Dataset<Points>();
    _traitJobRunning = false;
    _traitRecomputePending = false;

    computeTraitCorrelation(); // all means are cached now (or the selection changed meanwhile and new ones are read)
}

void SettingsAction::computeTraitCorrelation()
{
    XSC_TRACE("SettingsAction::computeTraitCorrelation");
    QElapsedTimer traitTimer;
    traitTimer.start();
    // The expression data is read in the background (see below); until that is finished a new request only has to be remembered,
    // because the cache the background job fills must not be touched.
    if (_traitJobRunning) {
        _traitRecomputePending = true;
        return;
    }

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
        _traitSignificanceBasis.clear();
        publishTraitCorrelationToModel();
        return;
    }

    Dataset<Points> mainPointsDataset = _mainPointsDataset.getCurrentDataset();
    const std::vector<QString> allGeneNames = mainPointsDataset->getDimensionNames();
    if (allGeneNames.empty()) {
        return;
    }

    // Genes to correlate. The gene table lists the top-N genes (a few hundred): reading only their expression instead of all
    // ~20,000 genes is what makes the analysis fast. The selected gene is always included (it feeds the plot).
    const bool allGenes = (_traitGeneScope.getCurrentText() == "All genes (slower)") || _uniqueReturnGeneList.isEmpty();
    const QString selectedGeneName = _selectedGene.getString();
    std::vector<int> geneDims;
    std::vector<QString> geneNames;
    const size_t usableGenes = PointStreaming::usableGeneCount(allGeneNames.size(), mainPointsDataset->getNumDimensions());
    for (size_t g = 0; g < usableGenes; ++g) {
        if (allGenes || _uniqueReturnGeneList.contains(allGeneNames[g]) || allGeneNames[g] == selectedGeneName) {
            geneDims.push_back(static_cast<int>(g));
            geneNames.push_back(allGeneNames[g]);
        }
    }
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
    // The key holds the datasets only, NOT the gene set: genes that were read before stay cached, and a new gene (for example
    // the selected one) reads only that gene (see missingEntryJobs). Before, every change of the gene set re-read all species.
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

    // The (species, cell type) pairs with genes whose mean expression is not cached yet. Only those genes are read.
    const auto missingEntryJobs = [&](const std::vector<QString>& speciesList) {
        std::vector<TraitEntryJob> jobs;
        for (const QString& species : speciesList) {
            for (const QString& cellType : cellTypes) {
                const QString key = entryKey(species, cellType);
                const auto cached = _traitCache.entries.find(key);
                const bool complete = cached != _traitCache.entries.end() && cached->second.known.size() == usableGenes;
                TraitEntryJob job;
                job.key = key;
                job.speciesPoints = &_traitCache.speciesSorted.at(species);
                job.cellTypePoints = &_traitCache.cellTypeSorted.at(cellType);
                if (complete) {
                    if (cached->second.cellCount == 0) {
                        continue; // no cells of this cell type in the species: nothing to read
                    }
                    for (const int gene : geneDims) {
                        if (!cached->second.known[static_cast<size_t>(gene)]) {
                            job.genes.push_back(gene);
                        }
                    }
                    if (job.genes.empty()) {
                        continue; // everything needed is cached
                    }
                    job.entry = std::move(cached->second); // moved, not copied: it is put back into the cache when the reads are done
                }
                else {
                    job.genes = geneDims;
                    job.entry.means.assign(usableGenes, 0.0f);
                    job.entry.known.assign(usableGenes, 0);
                }
                jobs.push_back(std::move(job));
            }
        }
        return jobs;
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

    // Reading expression data is the slow part. When pairs are missing it runs in the BACKGROUND, so the interface stays
    // responsive; the computation continues in onTraitJobsFinished() with everything cached. Only the listed genes are read.
    {
        auto missingJobs = missingEntryJobs(everySpecies);
        if (!missingJobs.empty()) {
            size_t largestPointSet = 0;
            for (const auto& job : missingJobs) {
                largestPointSet = std::max(largestPointSet, std::min(job.speciesPoints->size(), job.cellTypePoints->size()));
            }

            _traitJobs = std::make_shared<std::vector<TraitEntryJob>>(std::move(missingJobs));
            _traitJobDataset = mainPointsDataset;
            _traitJobRunning = true;
            QGuiApplication::setOverrideCursor(Qt::BusyCursor);

            const auto jobs = _traitJobs;
            const Dataset<Points>* datasetHandle = &_traitJobDataset; // member: stays valid while the job runs
            const int workers = parallelWorkers(largestPointSet);

            // progress: every finished read is reported to the status bar (from the worker thread, queued to the GUI thread)
            const int traitTotal = static_cast<int>(jobs->size());
            const auto traitProgress = std::make_shared<std::atomic<int>>(0);
            const auto traitReport = [this, traitProgress, traitTotal]() {
                const int done = ++(*traitProgress);
                QMetaObject::invokeMethod(this, [this, done, traitTotal]() {
                    showStatusMessage(QString("Reading the expression for the trait correlation: %1 of %2 species / cell type pairs").arg(done).arg(traitTotal),
                        QStringLiteral("#007bff"));
                    }, Qt::QueuedConnection);
                };
            showStatusMessage(QString("Reading the expression for the trait correlation: 0 of %1 species / cell type pairs").arg(traitTotal), QStringLiteral("#007bff"));

            _traitJobWatcher.setFuture(QtConcurrent::run([jobs, datasetHandle, workers, traitReport]() {
                QThreadPool pool;
                pool.setMaxThreadCount(workers);
                QtConcurrent::blockingMap(&pool, *jobs, [&](TraitEntryJob& job) {
                    StatusTick tick{ traitReport };
                    const std::vector<int> points = intersectSortedIndicesToInt(*job.speciesPoints, *job.cellTypePoints);
                    job.entry.cellCount = static_cast<int>(points.size());
                    if (points.empty()) {
                        job.entry.known.assign(job.entry.known.size(), 1); // no cells: nothing to read, remembered as "no cells"
                        return;
                    }
                    // only the genes that are not cached yet; their means are written at their gene index
                    computeChunkedGeneMeans(*datasetHandle, job.genes, points,
                        [&job](int, const std::vector<int>& chunkGenes, const std::vector<float>& chunkMeans) {
                            for (size_t g = 0; g < chunkGenes.size(); ++g) {
                                const size_t gene = static_cast<size_t>(chunkGenes[g]);
                                job.entry.means[gene] = chunkMeans[g];
                                job.entry.known[gene] = 1;
                            }
                        });
                    });
                }));
            return;
        }
    }

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
    _traitSignificanceBasis.clear();
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

    // ---- 7. Species tree ------------------------------------------------------------------------------------------
    // The reference tree of the phylogenetic tree view. It is needed for the phylogenetic contrasts AND for the significance
    // of the other methods: related species are not independent, which makes plain p-values too optimistic.
    std::vector<PicNode> treeNodes;
    int treeRoot = 0;
    size_t speciesInTree = 0;
    const auto buildTree = [&]() {
        treeNodes.clear();
        treeRoot = 0;
        speciesInTree = 0;
        const auto treePicker = _referenceTreeDataset.getCurrentDataset();
        if (!treePicker.isValid()) {
            return;
        }
        auto referenceTree = mv::data().getDataset<XSCTree>(treePicker.getDatasetId());
        if (!referenceTree.isValid()) {
            return;
        }

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
        };

    buildTree();
    bool treeFallback = false;
    if (method == TraitMethod::Phylogenetic && speciesInTree < 3) {
        // no usable tree: say so instead of silently pretending a phylogenetic correction was applied
        method = TraitMethod::Pearson;
        treeFallback = true;
        plan = planSpeciesSelection(usable.size(), picked, scope, method);
        population = namesOf(plan.population);
        buildTree(); // for the new population
    }
    const bool treeUsable = speciesInTree >= 3;

    // ---- 8. Expression matrix X (N species x G genes) and trait vector t ----------------------------------------
    const TraitPooling pooling = equalWeighting ? TraitPooling::EqualCellTypeWeight : TraitPooling::PoolCells;

    std::vector<std::vector<CellTypeMeans>> partsPerSpecies(population.size());
    std::vector<int> cellsPerSpecies(population.size(), 0);
    std::vector<double> traitValues;
    traitValues.reserve(population.size());
    // The cache holds the means of every usable gene; the statistics work on the selected genes, so they are gathered here.
    // (Reserved up front: pointers to the gathered rows must stay valid.)
    std::vector<std::vector<float>> gatheredMeans;
    gatheredMeans.reserve(population.size() * cellTypes.size());
    for (size_t i = 0; i < population.size(); ++i) {
        for (const QString& cellType : cellTypes) {
            const auto& entry = _traitCache.entries.at(entryKey(population[i], cellType));
            if (entry.cellCount > 0 && (!equalWeighting || entry.cellCount >= minCells)) {
                if (geneCount == usableGenes) {
                    // every usable gene is selected: geneDims is 0..usableGenes-1, so the cached row is used as it is (no copy)
                    partsPerSpecies[i].push_back({ static_cast<size_t>(entry.cellCount), &entry.means });
                }
                else {
                    std::vector<float> selectedGenes(geneCount);
                    for (size_t g = 0; g < geneCount; ++g) {
                        selectedGenes[g] = entry.means[static_cast<size_t>(geneDims[g])];
                    }
                    gatheredMeans.push_back(std::move(selectedGenes));
                    partsPerSpecies[i].push_back({ static_cast<size_t>(entry.cellCount), &gatheredMeans.back() });
                }
                cellsPerSpecies[i] += entry.cellCount;
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

    // Phylogenetically corrected statistics next to the chosen method (when that is not already the phylogenetic one)
    const bool phyloSeparate = (method != TraitMethod::Phylogenetic) && treeUsable;
    const TraitStatisticsContext phyloContext = phyloSeparate ? prepareTraitStatisticsContext(TraitMethod::Phylogenetic, traitValues, treeNodes, treeRoot) : TraitStatisticsContext();

    std::vector<GeneTraitStatistics> statistics(geneCount);
    std::vector<GeneTraitStatistics> phyloStatistics(phyloSeparate ? geneCount : 0);

    // ---- 9. Statistics of every gene (parallel over gene ranges) ----------------------------------------------------
    // Every worker owns a range of genes: it combines the cached per cell type means into the species rows of X and
    // computes correlation, p-value, confidence interval, leave-one-out range and contribution. Disjoint ranges -> no locking.
    auto geneRanges = splitGeneRanges(geneCount, static_cast<size_t>(std::max(1, QThread::idealThreadCount())) * 4);
    QtConcurrent::blockingMap(geneRanges, [&](GeneRange& range) {
        for (size_t i = 0; i < population.size(); ++i) {
            combineCellTypeMeans(partsPerSpecies[i], pooling, range, expressionMatrix[i]);
        }
        computeTraitStatistics(expressionRows, statisticsContext, range, statistics);
        if (phyloSeparate) {
            computeTraitStatistics(expressionRows, phyloContext, range, phyloStatistics);
        }
        });

    // ---- 10. Multiple testing correction and result -------------------------------------------------------------------
    // p-value, FDR, confidence interval and leave-one-out describe the correlation across the compared species (chosen method).
    std::vector<float> pValues(geneCount);
    for (size_t g = 0; g < geneCount; ++g) {
        pValues[g] = statistics[g].pValue;
    }
    const std::vector<float> fdrValues = benjaminiHochberg(pValues);

    std::vector<float> phyloFdrValues;
    if (phyloSeparate) {
        std::vector<float> phyloPValues(geneCount);
        for (size_t g = 0; g < geneCount; ++g) {
            phyloPValues[g] = phyloStatistics[g].pValue;
        }
        phyloFdrValues = benjaminiHochberg(phyloPValues);
    }

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

        if (phyloSeparate) {
            extras.phyloCorrelation = phyloStatistics[g].correlation;
            extras.phyloPValue = phyloStatistics[g].pValue;
            extras.phyloFdr = phyloFdrValues[g];
            extras.significanceFdr = phyloFdrValues[g];
        }
        else {
            if (method == TraitMethod::Phylogenetic) {
                extras.phyloCorrelation = statistics[g].correlation;
                extras.phyloPValue = statistics[g].pValue;
                extras.phyloFdr = fdrValues[g];
            }
            extras.significanceFdr = fdrValues[g];
        }

        _geneTraitValuesComputed[geneNames[g]] = plan.contribution ? statistics[g].selectionScore : statistics[g].correlation;
        _geneTraitPValues[geneNames[g]] = statistics[g].pValue;
        _geneTraitFdrValues[geneNames[g]] = fdrValues[g];
        _geneTraitExtras[geneNames[g]] = extras;
    }

    _traitSignificanceBasis = (phyloSeparate || method == TraitMethod::Phylogenetic)
        ? QString("phylogenetically corrected FDR")
        : QString("FDR of the chosen method (no usable species tree: the shared ancestry of the species is NOT corrected,\nso p-values are too optimistic for related species)");

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
    else if (!treeUsable) {
        methodName += "\nNo usable species tree: the shared ancestry of the species is not corrected";
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
    const QString genesText = (allGenes || _uniqueReturnGeneList.isEmpty())
        ? QString("Genes correlated: all %1 genes (FDR over all genes)").arg(geneCount)
        : QString("Genes correlated: %1 listed top-N genes (FDR over these genes)").arg(geneCount);
    _traitMethodNote = methodName + "\n" + selectionText + "\n" + genesText;
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
    _traitMatrix.cells = cellsPerSpecies;

    const QString methodShortName = (method == TraitMethod::Phylogenetic) ? "phylogenetic contrasts" : (method == TraitMethod::Spearman ? "Spearman" : "Pearson");
    _traitValueLabel = QString(plan.contribution ? "Trait contribution of selected species (%1)" : "Trait correlation (%1)").arg(methodShortName);

    publishTraitCorrelationToModel();
    showStatusMessage(QString("Trait correlation: %1 species, %2 genes, computed in %3 ms").arg(population.size()).arg(geneCount).arg(traitTimer.elapsed()),
        QStringLiteral("#28a745"));
}

void SettingsAction::publishTraitCorrelationToModel()
{
    XSC_TRACE("SettingsAction::publishTraitCorrelationToModel");
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
                _traitSampleSize, _traitMethodNote, _traitSignificanceBasis);
            if (rIt == _geneTraitValuesComputed.end() && _traitSampleSize > 0) {
                correlationItem->setData("Not part of the trait analysis: the trait correlation is computed for the listed top-N genes only\n(setting Trait Correlation Genes -> All genes (slower) includes every gene).", Qt::ToolTipRole);
            }
        }
    }

    emit model->dataChanged(model->index(0, kTraitCorrelationColumn), model->index(model->rowCount() - 1, kTraitCorrelationColumn), { Qt::DisplayRole, Qt::UserRole, Qt::ToolTipRole, kTraitPValueRole, kTraitFdrRole, kTraitStatsRole, kTraitSignificanceFdrRole });
}



void SettingsAction::findTopNGenesPerCluster() {
    XSC_TRACE("SettingsAction::findTopNGenesPerCluster");

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
        int nameIndex; // position of the gene in the alphabetical order of the species' statistics
    };
    struct SpeciesRanking {
        QString speciesName;
        const std::map<QString, Stats>* geneStats = nullptr;
        std::vector<GeneRankingEntry> sortedGenes;
        std::vector<int> rankByNameIndex; // rank of every gene, indexed by nameIndex (Negative counts from the other end)
    };

    std::vector<SpeciesRanking> speciesRankings;
    speciesRankings.reserve(_clusterNameToGeneNameToExpressionValue.size());
    for (const auto& outerPair : _clusterNameToGeneNameToExpressionValue) {
        speciesRankings.push_back({ outerPair.first, &outerPair.second,{},{} });
        speciesOrder.push_back(outerPair.first);
    }

    QtConcurrent::blockingMap(speciesRankings, [option](SpeciesRanking& ranking) {
        ranking.sortedGenes.reserve(ranking.geneStats->size());
        int nameIndex = 0;
        for (const auto& innerPair : *ranking.geneStats) {
            const auto& stats = innerPair.second;
            ranking.sortedGenes.push_back({ innerPair.first, stats.meanSelected - stats.meanNonSelected, stats.meanSelected, nameIndex++ });
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

        const int total = static_cast<int>(ranking.sortedGenes.size());
        ranking.rankByNameIndex.assign(static_cast<size_t>(total), 0);
        for (int i = 0; i < total; ++i) {
            ranking.rankByNameIndex[static_cast<size_t>(ranking.sortedGenes[static_cast<size_t>(i)].nameIndex)] = (option == SelectionOption::NegativeTopN) ? total - i : i + 1;
        }
        });

    // ---- Stage 2 (serial, cheap): collect the top-N genes ----
    for (const auto& ranking : speciesRankings) {
        const auto& speciesName = ranking.speciesName;
        const int limit = std::min(n, static_cast<int>(ranking.sortedGenes.size()));
        for (int i = 0; i < limit; ++i) {
            const auto& entry = ranking.sortedGenes[static_cast<size_t>(i)];
            _uniqueReturnGeneList.insert(entry.geneName);
            if (entry.meanSelected > 0) {
                geneAppearanceCounter[entry.geneName].push_back(speciesName);
            }
        }
    }

    // ---- Stage 3: the rank of every gene in every species ----
    // Every species walks its genes in alphabetical order, like the (ordered) map itself: a moving iterator finds each
    // gene's entry in O(1) instead of a map lookup per gene and species (~500k lookups for 25 species x 20k genes).
    for (const auto& ranking : speciesRankings) {
        auto mapIt = rankingMap.begin();
        int nameIndex = 0;
        for (const auto& innerPair : *ranking.geneStats) {
            const QString& geneName = innerPair.first;
            while (mapIt != rankingMap.end() && mapIt->first < geneName) {
                ++mapIt;
            }
            if (mapIt == rankingMap.end() || mapIt->first != geneName) {
                mapIt = rankingMap.emplace_hint(mapIt, geneName, std::vector<std::pair<QString, int>>());
            }
            mapIt->second.emplace_back(ranking.speciesName, ranking.rankByNameIndex[static_cast<size_t>(nameIndex++)]);
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
            _geneTsneAnalysisPlugin = analysisPlugin; // remembered so that the next update can destroy it
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



// ============================================================================================================
//  Species differential expression: every species against all the others
//
//  Why it is fast: for every (species, selected cell type) pair the mean expression and the fraction of cells expressing every
//  gene are read ONCE (in the background, with a cell cap) and cached. After that, all comparisons - 25 species against the
//  rest, any top-N, any direction, any significance level - are a few arithmetic operations per gene on those cached values.
//  Only the cell type selection or the cell cap need new data.
//
//  Why it is sound: the species, not the cells, are the replicates. A species contributes ONE value per gene and the
//  comparison "species A against the other species" is a single observation against a control sample.
// ============================================================================================================

/** Leaf names of a species tree in the order of the tree (related species end up next to each other). */
static void collectLeafNamesInOrder(const QJsonObject& node, QStringList& names)
{
    XSC_TRACE("collectLeafNamesInOrder");
    const QJsonArray children = node.value("children").toArray();
    if (children.isEmpty()) {
        const QString name = node.value("name").toString();
        if (!name.isEmpty()) {
            names << name;
        }
        return;
    }
    for (const QJsonValue& child : children) {
        collectLeafNamesInOrder(child.toObject(), names);
    }
}

void SettingsAction::showStatusMessage(const QString& text, const QString& backgroundColor)
{
    if (!_statusBarActionWidget) {
        return;
    }
    _statusBarActionWidget->showMessage(text);
    _statusBarActionWidget->setStyleSheet("QStatusBar{padding-left:8px;background:" + backgroundColor + ";color:white;}");
}

void SettingsAction::setSpeciesDeActive(bool active)
{
    XSC_TRACE("SettingsAction::setSpeciesDeActive");
    _deActive = active;
    if (active) {
        requestSpeciesDifferentialExpression();
    }
}

void SettingsAction::requestSpeciesDifferentialExpression()
{
    XSC_TRACE("SettingsAction::requestSpeciesDifferentialExpression");
    if (!_deActive) {
        return;
    }
    if (_deTimer) {
        _deTimer->start(); // restarts the single-shot timer: a burst of changes gives one computation
    }
    else {
        computeSpeciesDifferentialExpression();
    }
}

void SettingsAction::onSpeciesDeJobsFinished()
{
    XSC_TRACE("SettingsAction::onSpeciesDeJobsFinished");
    QGuiApplication::restoreOverrideCursor();
    if (_deJobs) {
        for (auto& job : *_deJobs) {
            _deCache.moments[job.key] = std::move(job.entry);
        }
        _deJobs.reset();
    }
    _deJobDataset = Dataset<Points>();
    _deJobRunning = false;
    _deRecomputePending = false;

    computeSpeciesDifferentialExpression(); // everything is cached now (or the selection changed meanwhile)
}

void SettingsAction::computeSpeciesDifferentialExpression()
{
    XSC_TRACE("SettingsAction::computeSpeciesDifferentialExpression");
    QElapsedTimer deTimer;
    deTimer.start();
    if (!_deActive) {
        return;
    }
    // The cache is being filled in the background: just remember that something changed.
    if (_deJobRunning) {
        _deRecomputePending = true;
        return;
    }

    const auto finishWith = [this](const QString& message) {
        _deResult = SpeciesDeResult();
        _deResult.message = message;
        _deState = DeState();
        if (_speciesDeFinished) {
            _speciesDeFinished();
        }
        };

    // ---- 1. Inputs -------------------------------------------------------------------------------------------
    if (!_mainPointsDataset.getCurrentDataset().isValid() || !_speciesNamesDataset.getCurrentDataset().isValid() || !_bottomClusterNamesDataset.getCurrentDataset().isValid()) {
        finishWith("The datasets are not available yet.");
        return;
    }

    QStringList cellTypes = _selectedClusterNames;
    if (cellTypes.isEmpty() && !_selectedClusterName.isEmpty()) {
        cellTypes << _selectedClusterName;
    }
    if (cellTypes.isEmpty()) {
        finishWith("Select a cell type first (click a cell type label of the status bar; Shift + click selects several).");
        return;
    }

    Dataset<Points> mainPointsDataset = _mainPointsDataset.getCurrentDataset();
    const std::vector<QString> geneNames = mainPointsDataset->getDimensionNames();
    const size_t geneCount = geneNames.size();
    if (geneCount == 0) {
        finishWith("The dataset has no genes.");
        return;
    }

    // ---- 2. Cache: valid as long as the datasets and the cell cap stay the same ------------------------------
    Dataset<Clusters> speciesDataset = _speciesNamesDataset.getCurrentDataset();
    Dataset<Clusters> cellTypeDataset = _bottomClusterNamesDataset.getCurrentDataset();
    const int maxCells = std::max(0, _deMaxCells.getValue());
    const QString cacheKey = mainPointsDataset->getId() + QLatin1Char('|') + speciesDataset->getId() + QLatin1Char('|') + cellTypeDataset->getId() + QLatin1Char('|') + QString::number(maxCells);
    if (cacheKey != _deCache.key) {
        _deCache = DeCache();
        _deCache.key = cacheKey;
    }

    const auto speciesClusters = speciesDataset->getClusters();
    const auto cellTypeClusters = cellTypeDataset->getClusters();

    // sorted point indices of every species (once per dataset, sorted concurrently)
    if (!_deCache.sortedBuilt) {
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
            _deCache.speciesSorted[speciesClusters[job.index].getName()] = std::move(job.points);
        }
        _deCache.sortedBuilt = true;
    }
    // sorted point indices of the selected cell types (on first use)
    for (const QString& cellType : cellTypes) {
        if (_deCache.cellTypeSorted.count(cellType) > 0) {
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
        _deCache.cellTypeSorted[cellType] = std::move(sortedPoints);
    }

    std::vector<QString> allSpecies;
    for (const auto& entry : _deCache.speciesSorted) {
        allSpecies.push_back(entry.first);
    }
    std::sort(allSpecies.begin(), allSpecies.end());

    const auto entryKey = [](const QString& species, const QString& cellType) {
        return species + QLatin1Char('\x1f') + cellType;
        };

    // ---- 3. Read what is missing, in the BACKGROUND -----------------------------------------------------------
    {
        std::vector<DeEntryJob> missing;
        for (const QString& species : allSpecies) {
            for (const QString& cellType : cellTypes) {
                const QString key = entryKey(species, cellType);
                if (_deCache.moments.count(key) == 0) {
                    missing.push_back({ key, &_deCache.speciesSorted.at(species), &_deCache.cellTypeSorted.at(cellType),{} });
                }
            }
        }

        if (!missing.empty()) {
            size_t largestPointSet = 0;
            for (const auto& job : missing) {
                largestPointSet = std::max(largestPointSet, std::min(job.speciesPoints->size(), job.cellTypePoints->size()));
            }
            if (maxCells > 0) {
                largestPointSet = std::min(largestPointSet, static_cast<size_t>(maxCells));
            }

            _deJobs = std::make_shared<std::vector<DeEntryJob>>(std::move(missing));
            _deJobDataset = mainPointsDataset;
            _deJobRunning = true;
            QGuiApplication::setOverrideCursor(Qt::BusyCursor);

            const auto jobs = _deJobs;
            const Dataset<Points>* datasetHandle = &_deJobDataset;
            const int workers = parallelWorkers(largestPointSet);
            std::vector<int> allGeneIndices(PointStreaming::usableGeneCount(geneCount, mainPointsDataset->getNumDimensions()));
            std::iota(allGeneIndices.begin(), allGeneIndices.end(), 0);

            const int deTotal = static_cast<int>(jobs->size());
            const auto deProgress = std::make_shared<std::atomic<int>>(0);
            const auto deReport = [this, deProgress, deTotal]() {
                const int done = ++(*deProgress);
                QMetaObject::invokeMethod(this, [this, done, deTotal]() {
                    showStatusMessage(QString("Reading the expression for the species differential expression: %1 of %2 species / cell type pairs").arg(done).arg(deTotal),
                        QStringLiteral("#007bff"));
                    }, Qt::QueuedConnection);
                };
            showStatusMessage(QString("Reading the expression for the species differential expression: 0 of %1 species / cell type pairs").arg(deTotal), QStringLiteral("#007bff"));

            _deJobWatcher.setFuture(QtConcurrent::run([this, jobs, datasetHandle, workers, maxCells, geneCount, allGeneIndices, deReport]() {
                QThreadPool pool;
                pool.setMaxThreadCount(workers);
                QtConcurrent::blockingMap(&pool, *jobs, [&](DeEntryJob& job) {
                    StatusTick tick{ deReport };
                    std::vector<int> points = intersectSortedIndicesToInt(*job.speciesPoints, *job.cellTypePoints);

                    // cell cap: an even sample of the cells (deterministic, so the result does not change from run to run)
                    if (maxCells > 0 && static_cast<int>(points.size()) > maxCells) {
                        std::vector<int> sampled;
                        sampled.reserve(static_cast<size_t>(maxCells));
                        for (int i = 0; i < maxCells; ++i) {
                            sampled.push_back(points[static_cast<size_t>((static_cast<long long>(i) * static_cast<long long>(points.size())) / maxCells)]);
                        }
                        points.swap(sampled);
                    }

                    job.entry.cellCount = static_cast<int>(points.size());
                    if (points.empty()) {
                        return; // remembered as "no cells" so it is not looked up again
                    }
                    job.entry.means.assign(geneCount, 0.0f);
                    job.entry.detection.assign(geneCount, 0.0f);
                    computeChunkedGeneMeansAndDetection(*datasetHandle, allGeneIndices, points,
                        [&job](int start, const std::vector<int>& chunkGenes, const std::vector<float>& means, const std::vector<float>& detection) {
                            for (size_t g = 0; g < chunkGenes.size(); ++g) {
                                job.entry.means[static_cast<size_t>(start) + g] = means[g];
                                job.entry.detection[static_cast<size_t>(start) + g] = detection[g];
                            }
                        });
                    });
                }));
            return; // continues in onSpeciesDeJobsFinished()
        }
    }

    // ---- 4. One value per species and gene: the pooled mean expression and detection -----------------------
    const int minCells = std::max(1, _deMinCells.getValue());
    const bool equalWeighting = (_traitPoolingMode.getCurrentText() == "Equal cell-type weighting");
    const TraitPooling pooling = equalWeighting ? TraitPooling::EqualCellTypeWeight : TraitPooling::PoolCells;

    // species in the order of the species tree (related species next to each other); the others alphabetically after them
    QStringList treeOrder;
    {
        const auto treePicker = _referenceTreeDataset.getCurrentDataset();
        if (treePicker.isValid()) {
            auto referenceTree = mv::data().getDataset<XSCTree>(treePicker.getDatasetId());
            if (referenceTree.isValid()) {
                collectLeafNamesInOrder(referenceTree->getTreeData(), treeOrder);
            }
        }
    }
    std::vector<QString> ordered;
    for (const QString& name : treeOrder) {
        if (std::find(allSpecies.begin(), allSpecies.end(), name) != allSpecies.end() && std::find(ordered.begin(), ordered.end(), name) == ordered.end()) {
            ordered.push_back(name);
        }
    }
    for (const QString& name : allSpecies) {
        if (std::find(ordered.begin(), ordered.end(), name) == ordered.end()) {
            ordered.push_back(name);
        }
    }

    DeState state;
    state.geneNames = geneNames;
    const GeneRange fullRange{ 0, geneCount };
    for (const QString& species : ordered) {
        std::vector<CellTypeMeans> meanParts;
        std::vector<CellTypeMeans> detectionParts;
        int totalCells = 0;
        bool anyCellTypeBigEnough = false;
        for (const QString& cellType : cellTypes) {
            const auto it = _deCache.moments.find(entryKey(species, cellType));
            if (it == _deCache.moments.end() || it->second.cellCount <= 0) {
                continue;
            }
            const DeCache::Entry& entry = it->second;
            totalCells += entry.cellCount;
            anyCellTypeBigEnough = anyCellTypeBigEnough || entry.cellCount >= minCells;
            if (!equalWeighting || entry.cellCount >= minCells) {
                meanParts.push_back({ static_cast<size_t>(entry.cellCount), &entry.means });
                detectionParts.push_back({ static_cast<size_t>(entry.cellCount), &entry.detection });
            }
        }
        if (!(equalWeighting ? anyCellTypeBigEnough : totalCells >= minCells)) {
            continue; // too few cells: the mean would be noise
        }

        std::vector<float> meanRow(geneCount, 0.0f);
        std::vector<float> detectionRow(geneCount, 0.0f);
        combineCellTypeMeans(meanParts, pooling, fullRange, meanRow);
        combineCellTypeMeans(detectionParts, pooling, fullRange, detectionRow);
        state.species.push_back(species);
        state.means.push_back(std::move(meanRow));
        state.detection.push_back(std::move(detectionRow));
        state.cells.push_back(totalCells);
    }

    const size_t speciesCount = state.species.size();
    if (speciesCount < 3) {
        finishWith(QString("At least 3 species with %1 or more cells of the selected cell type(s) are needed; found %2.").arg(minCells).arg(speciesCount));
        return;
    }

    state.totalSum.assign(geneCount, 0.0);
    state.totalSumSq.assign(geneCount, 0.0);
    state.totalDetection.assign(geneCount, 0.0);
    for (size_t s = 0; s < speciesCount; ++s) {
        for (size_t g = 0; g < geneCount; ++g) {
            const double mean = static_cast<double>(state.means[s][g]);
            state.totalSum[g] += mean;
            state.totalSumSq[g] += mean * mean;
            state.totalDetection[g] += static_cast<double>(state.detection[s][g]);
        }
    }

    // ---- 5. Every species against the others (parallel over species) -----------------------------------------
    const DeDirection direction = _deDirection.getCurrentIndex() == 1 ? DeDirection::Down : (_deDirection.getCurrentIndex() == 2 ? DeDirection::Either : DeDirection::Up);
    const DeRank rank = _deRankBy.getCurrentIndex() == 1 ? DeRank::ModeratedT : DeRank::Effect;
    const size_t topCount = static_cast<size_t>(std::max(1, _deTopGenes.getValue()));
    const double minDetection = static_cast<double>(std::max(0, _deMinDetection.getValue())) / 100.0;
    const double level = significanceLevel();

    std::vector<const std::vector<float>*> meanPointers;
    for (const auto& row : state.means) {
        meanPointers.push_back(&row);
    }

    std::vector<SpeciesDeComparison> comparisons(speciesCount);
    std::vector<size_t> speciesIndices(speciesCount);
    std::iota(speciesIndices.begin(), speciesIndices.end(), static_cast<size_t>(0));
    // The statistics are kept while the comparison stays the same (cells, cell types, cell cap, pooling, compared species).
    // Top-N, direction, rank and minimum detection only select genes from them: changing those is then instant.
    const QString statisticsKey = cacheKey + QLatin1Char('|') + cellTypes.join(QLatin1Char('\x1f')) + QLatin1Char('|')
        + (equalWeighting ? QStringLiteral("equal") : QStringLiteral("pooled")) + QLatin1Char('|') + QString::number(minCells) + QLatin1Char('|')
        + QStringList(state.species.begin(), state.species.end()).join(QLatin1Char('\x1f'));
    if (!_deStats || _deStats->key != statisticsKey) {
        auto fresh = std::make_shared<DeStatisticsCacheImpl>();
        buildDeStatistics(meanPointers, state.detection, state.totalSum, state.totalSumSq, state.totalDetection, *fresh);
        fresh->key = statisticsKey;
        _deStats = fresh;
    }

    QtConcurrent::blockingMap(speciesIndices, [&](size_t& s) {
        const DeSpeciesStatistics& statistics = _deStats->statistics[s];
        const std::vector<float>& fdr = _deStats->fdr[s];
        const std::vector<float>& restDetection = _deStats->restDetection[s];

        const std::vector<size_t> top = selectTopDeGenes(statistics, fdr, state.detection[s], restDetection, level, minDetection, direction, rank, topCount);

        SpeciesDeComparison& comparison = comparisons[s];
        comparison.species = state.species[s];
        comparison.cells = state.cells[s];
        comparison.topGenes.reserve(static_cast<int>(top.size()));
        for (const size_t g : top) {
            SpeciesDeGene gene;
            gene.gene = geneNames[g];
            gene.geneIndex = static_cast<int>(g);
            gene.delta = statistics.delta[g];
            gene.restMean = statistics.restMean[g];
            gene.restSd = statistics.restSd[g];
            gene.t = statistics.t[g];
            gene.pValue = statistics.pValue[g];
            gene.fdr = fdr[g];
            gene.detection = state.detection[s][g];
            gene.restDetection = restDetection[g];
            comparison.topGenes.push_back(gene);
        }
        });

    // ---- 6. Result: one row per species (species tree order). Species with too few cells keep their row; their cells are grey (no value). ----
    SpeciesDeResult result;
    result.valid = true;
    result.cellTypes = cellTypes;
    std::unordered_map<QString, size_t> stateIndex; // species -> index in the compared species
    for (size_t s = 0; s < speciesCount; ++s) {
        stateIndex[state.species[s]] = s;
    }
    std::vector<int> rowOfState(speciesCount, -1);
    for (const QString& species : ordered) {
        const int row = static_cast<int>(result.species.size());
        result.species << species;
        const bool takesPart = stateIndex.count(species) > 0;
        result.included.push_back(takesPart);
        SpeciesDeComparison empty;
        empty.species = species;
        result.comparisons.push_back(empty);
        if (takesPart) {
            rowOfState[stateIndex[species]] = row;
        }
    }
    for (size_t s = 0; s < speciesCount; ++s) {
        result.comparisons[rowOfState[s]] = comparisons[s];
    }

    std::unordered_map<QString, int> seen;
    for (size_t s = 0; s < speciesCount; ++s) {
        for (const SpeciesDeGene& gene : comparisons[s].topGenes) {
            if (seen.count(gene.gene) == 0) {
                seen[gene.gene] = static_cast<int>(result.heatmapGenes.size());
                result.heatmapGenes << gene.gene;
                result.heatmapOwner << rowOfState[s];
            }
        }
    }
    const double others = static_cast<double>(speciesCount - 1);
    const float noValue = std::numeric_limits<float>::quiet_NaN();
    result.heatmapDelta.resize(static_cast<int>(result.species.size()));
    for (int row = 0; row < result.species.size(); ++row) {
        result.heatmapDelta[row].assign(static_cast<int>(result.heatmapGenes.size()), noValue);
    }
    for (int column = 0; column < result.heatmapGenes.size(); ++column) {
        int geneIndex = -1;
        for (const SpeciesDeGene& gene : result.comparisons[result.heatmapOwner[column]].topGenes) {
            if (gene.gene == result.heatmapGenes[column]) {
                geneIndex = gene.geneIndex;
                break;
            }
        }
        if (geneIndex < 0) {
            continue;
        }
        const size_t g = static_cast<size_t>(geneIndex);
        for (size_t s = 0; s < speciesCount; ++s) {
            const double own = static_cast<double>(state.means[s][g]);
            result.heatmapDelta[rowOfState[s]][column] = static_cast<float>(own - (state.totalSum[g] - own) / others);
        }
    }

    result.message = QString("%1 species, each against the other %2. Cell type(s): %3. Cells pooled per species (%4), at most %5 cells per species and cell type; "
        "species with fewer than %6 cells are left out. The test treats the species as replicates (single case against the other species, moderated variance), "
        "genes with FDR below %7 and detected in at least %8% of the cells are ranked.")
        .arg(speciesCount).arg(speciesCount - 1).arg(cellTypes.join(", "))
        .arg(equalWeighting ? "equal cell-type weighting" : "all cells pooled")
        .arg(maxCells > 0 ? QString::number(maxCells) : QString("all")).arg(minCells).arg(level).arg(static_cast<int>(minDetection * 100.0 + 0.5));

    showStatusMessage(QString("Species differential expression: %1 species, %2 genes, computed in %3 ms")
        .arg(speciesCount).arg(result.heatmapGenes.size()).arg(deTimer.elapsed()), QStringLiteral("#28a745"));
    _deState = std::move(state);
    _deResult = std::move(result);
    if (_speciesDeFinished) {
        _speciesDeFinished();
    }
}

SpeciesDeDetail SettingsAction::computeSpeciesDeDetail(const QString& species)
{
    XSC_TRACE("SettingsAction::computeSpeciesDeDetail");
    SpeciesDeDetail detail;
    const auto it = std::find(_deState.species.begin(), _deState.species.end(), species);
    if (it == _deState.species.end() || _deState.species.size() < 3) {
        detail.message = QStringLiteral("No comparison: the species differential expression needs at least 3 species with cells of the selected cell type(s).");
        return detail;
    }
    const size_t focus = static_cast<size_t>(std::distance(_deState.species.begin(), it));
    const size_t speciesCount = _deState.species.size();
    const size_t geneCount = _deState.geneNames.size();

    // 1 against n-1: the focus species against every other species that takes part in the comparison
    std::vector<size_t> group;
    for (size_t i = 0; i < speciesCount; ++i) {
        if (i != focus) {
            group.push_back(i);
            detail.comparison << _deState.species[i];
        }
    }

    std::vector<const std::vector<float>*> meanPointers;
    for (const auto& row : _deState.means) {
        meanPointers.push_back(&row);
    }
    const DeSpeciesStatistics statistics = focusAgainstGroup(meanPointers, focus, group);
    const std::vector<float> fdr = benjaminiHochberg(statistics.pValue);
    std::vector<float> restDetection(geneCount, 0.0f);
    for (size_t g = 0; g < geneCount; ++g) {
        double sum = 0.0;
        for (const size_t c : group) {
            sum += static_cast<double>(_deState.detection[c][g]);
        }
        restDetection[g] = static_cast<float>(sum / static_cast<double>(group.size()));
    }

    // the same selection rules as in the overview
    const DeDirection direction = _deDirection.getCurrentIndex() == 1 ? DeDirection::Down : (_deDirection.getCurrentIndex() == 2 ? DeDirection::Either : DeDirection::Up);
    const DeRank rank = _deRankBy.getCurrentIndex() == 1 ? DeRank::ModeratedT : DeRank::Effect;
    const std::vector<size_t> top = selectTopDeGenes(statistics, fdr, _deState.detection[focus], restDetection, significanceLevel(),
        static_cast<double>(std::max(0, _deMinDetection.getValue())) / 100.0, direction, rank, static_cast<size_t>(std::max(1, _deTopGenes.getValue())));

    detail.valid = true;
    detail.species = species;
    detail.genes.reserve(static_cast<int>(geneCount));
    for (const QString& gene : _deState.geneNames) {
        detail.genes << gene;
    }
    detail.delta = QVector<float>(statistics.delta.begin(), statistics.delta.end());
    detail.restMean = QVector<float>(statistics.restMean.begin(), statistics.restMean.end());
    detail.restSd = QVector<float>(statistics.restSd.begin(), statistics.restSd.end());
    detail.t = QVector<float>(statistics.t.begin(), statistics.t.end());
    detail.pValue = QVector<float>(statistics.pValue.begin(), statistics.pValue.end());
    detail.fdr = QVector<float>(fdr.begin(), fdr.end());
    detail.detection = QVector<float>(_deState.detection[focus].begin(), _deState.detection[focus].end());
    detail.restDetection = QVector<float>(restDetection.begin(), restDetection.end());
    for (const size_t g : top) {
        detail.top << static_cast<int>(g);
    }
    return detail;
}
