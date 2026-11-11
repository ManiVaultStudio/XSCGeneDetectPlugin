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
            else {
                // Picker labels can retain whitespace from comma-separated metadata names.
                const QString normalized = key.trimmed();
                for (auto value = categoryObject.begin(); value != categoryObject.end(); ++value) {
                    if (value.key().trimmed() == normalized) {
                        keyObject.insert(it.key(), value.value());
                        break;
                    }
                }
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
    if (_updateWorkflowRunning)
        return;

    const auto updateTiming = std::make_shared<xsc::timing::AsyncSpan>("update.total");
    const bool baselineNeeded = !_meanMapComputed || _meanMapSignature != currentMeanMapSignature();
    struct UpdateWorkflowData
    {
        using BaselineMeans = std::unordered_map<QString, std::unordered_map<QString, std::pair<int, float>>>;

        mv::Dataset<Points> points;
        QString pointsId;
        QString baselineSignature;
        std::vector<std::seed_seq::result_type> selectedRows;
        std::vector<std::seed_seq::result_type> selectionSnapshot;
        std::vector<std::vector<std::uint64_t>> speciesRows;
        std::vector<QString> speciesNames;
        std::vector<QString> dimensionNames;
        std::vector<int> geneColumns;
        std::vector<int> selectedSpeciesIndex;
        std::vector<std::size_t> selectedCellCounts;
        QString selectedMeansKey;
        QString cachedSelectedMeansKey;
        int workerLimit = 1;
        std::size_t memoryBudgetBytes = 64u << 20;
        bool inputsReady = false;
        bool needsBaselineScan = false;
        bool needsSelectedScan = false;
        bool selectedScanFinished = false;
        bool staleInputs = false;
        bool published = false;
        BaselineMeans baselineMeans;
        std::vector<std::vector<float>> selectedMeans;
    };

    const auto data = std::make_shared<UpdateWorkflowData>();
    auto workflow = std::make_unique<mv::workflow::WorkflowPlan>(QStringLiteral("Update selection statistics"));

    workflow->addSequentialStage(QStringLiteral("Prepare selection update"), [this, data](const mv::workflow::WorkflowPlan::Job&, const mv::workflow::SharedWorkflowExecutionContext& context) {
        XSC_TRACE("update.stage.Prepare selection update");
        showStatusMessage(QStringLiteral("Preparing selection update..."), QStringLiteral("#007bff"));

        const auto pointsPicker = _mainPointsDataset.getCurrentDataset();
        const auto embeddingPicker = _embeddingDataset.getCurrentDataset();
        const auto speciesPicker = _speciesNamesDataset.getCurrentDataset();
        const auto clusterPicker = _bottomClusterNamesDataset.getCurrentDataset();
        const auto treePicker = _referenceTreeDataset.getCurrentDataset();
        if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked() || !pointsPicker.isValid() || !embeddingPicker.isValid() ||
            !speciesPicker.isValid() || !clusterPicker.isValid() || !treePicker.isValid())
        {
            context->setProgress(1.0);
            return;
        }

        data->points = mv::data().getDataset<Points>(pointsPicker->getId());
        auto embeddingPoints = mv::data().getDataset<Points>(embeddingPicker->getId());
        auto species = mv::data().getDataset<Clusters>(speciesPicker->getId());
        auto clusters = mv::data().getDataset<Clusters>(clusterPicker->getId());
        auto referenceTree = mv::data().getDataset<XSCTree>(treePicker->getId());
        if (!data->points.isValid() || !embeddingPoints.isValid() || !species.isValid() || !clusters.isValid() || !referenceTree.isValid() ||
            speciesPicker->getParent() != pointsPicker || clusterPicker->getParent() != pointsPicker || embeddingPicker->getParent() != pointsPicker)
        {
            context->setProgress(1.0);
            return;
        }

        const auto selected = data->points->getSelectionIndices();
        data->pointsId = data->points->getId();
        data->baselineSignature = currentMeanMapSignature();
        data->selectionSnapshot.assign(selected.begin(), selected.end());
        data->selectedRows = data->selectionSnapshot;
        data->dimensionNames = data->points->getDimensionNames();
        const auto geneCount = PointStreaming::usableGeneCount(data->dimensionNames.size(), data->points->getNumDimensions());
        data->geneColumns.resize(geneCount);
        std::iota(data->geneColumns.begin(), data->geneColumns.end(), 0);
        data->workerLimit = std::max(1, _maxWorkerThreads.getValue());
        data->memoryBudgetBytes = static_cast<std::size_t>(std::max(64, _memoryBudgetMb.getValue())) << 20;

        const auto speciesClusters = species->getClusters();
        data->speciesRows.reserve(speciesClusters.size());
        data->speciesNames.reserve(speciesClusters.size());
        for (const auto& speciesCluster : speciesClusters)
        {
            data->speciesNames.push_back(speciesCluster.getName());
            data->speciesRows.emplace_back(speciesCluster.getIndices().begin(), speciesCluster.getIndices().end());
        }

        data->needsBaselineScan = !_meanMapComputed || _meanMapSignature != data->baselineSignature;
        data->cachedSelectedMeansKey = _selectedMeansCache.key;
        data->inputsReady = true;
        context->setProgress(1.0);
    }, mv::workflow::WorkflowPlan::JobThreadAffinity::GuiThread, 0.015);

    workflow->addSequentialStage(QStringLiteral("Index selected cells by species"), [data](const mv::workflow::WorkflowPlan::Job&, const mv::workflow::SharedWorkflowExecutionContext& context) {
        XSC_TRACE("update.stage.Index selected cells by species");
        if (!data->inputsReady) { context->setProgress(1.0); return; }
        std::sort(data->selectedRows.begin(), data->selectedRows.end());
        std::vector<int> selectedPositionOf(static_cast<std::size_t>(data->points->getNumPoints()), -1);
        for (int selectedPosition = 0; selectedPosition < static_cast<int>(data->selectedRows.size()); ++selectedPosition)
        {
            const auto globalIndex = static_cast<std::size_t>(data->selectedRows[selectedPosition]);
            if (globalIndex < selectedPositionOf.size())
                selectedPositionOf[globalIndex] = selectedPosition;
        }

        data->selectedSpeciesIndex.assign(data->selectedRows.size(), -1);
        data->selectedCellCounts.assign(data->speciesRows.size(), 0);
        for (std::size_t speciesIndex = 0; speciesIndex < data->speciesRows.size(); ++speciesIndex)
        {
            for (const auto globalIndex : data->speciesRows[speciesIndex])
            {
                const auto selectedPosition = globalIndex < selectedPositionOf.size() ? selectedPositionOf[static_cast<std::size_t>(globalIndex)] : -1;
                if (selectedPosition >= 0)
                {
                    data->selectedSpeciesIndex[static_cast<std::size_t>(selectedPosition)] = static_cast<int>(speciesIndex);
                    ++data->selectedCellCounts[speciesIndex];
                }
            }
        }

        data->selectedMeansKey = selectedMeansKey(data->pointsId, data->selectedRows, data->selectedSpeciesIndex,
            data->geneColumns.size(), data->speciesRows.size());
        data->needsSelectedScan = !data->selectedRows.empty() && !data->speciesRows.empty() && !data->geneColumns.empty()
            && data->selectedMeansKey != data->cachedSelectedMeansKey;
        context->setProgress(1.0);
    }, mv::workflow::WorkflowPlan::JobThreadAffinity::CurrentWorkerThread, 0.015);
    workflow->addSequentialStage(QStringLiteral("Scan species-wide baseline means"), [this, data](const mv::workflow::WorkflowPlan::Job&, const mv::workflow::SharedWorkflowExecutionContext& context) {
        XSC_TRACE("update.stage.Scan species-wide baseline means");
        if (!data->inputsReady || !data->needsBaselineScan)
        {
            context->setProgress(1.0);
            return;
        }

        qInfo() << "Computing gene mean expression map";
        const QPointer<mv::Task> progressTask(context->getTask());
        if (progressTask)
        {
            progressTask->setName(QStringLiteral("Compute gene mean expression map"));
            progressTask->setProgressDescription(QStringLiteral("Scanning species-wide baseline means..."));
        }
        QElapsedTimer scanTimer;
        scanTimer.start();
        qInfo() << "[Baseline] scanning" << data->points->getNumPoints() << "cells," << data->geneColumns.size()
                << "genes across" << data->speciesRows.size() << "species";
        int lastProgressPercent = -1;
        // Baseline and selected sums share the same decoded matrix tiles.
        // Append the selected per-species groups and obtain both in one pass.
        auto groups = data->speciesRows;
        if (data->needsSelectedScan) {
            groups.resize(data->speciesRows.size() * 2);
            for (std::size_t i = 0; i < data->selectedRows.size(); ++i) {
                const int species = data->selectedSpeciesIndex[i];
                if (species >= 0) groups[data->speciesRows.size() + species].push_back(data->selectedRows[i]);
            }
        }
        const auto aggregated = PointStreaming::aggregateGroupedColumns(data->points.get(), groups, data->geneColumns,
            data->workerLimit, data->memoryBudgetBytes, false,
            [context, progressTask, &scanTimer, &lastProgressPercent](std::size_t completed, std::size_t total) {
                if (total > 0)
                    context->setProgress(static_cast<double>(completed) / static_cast<double>(total));
                const int percent = total > 0 ? static_cast<int>(100 * completed / total) : 100;
                if (lastProgressPercent < 0 || percent >= lastProgressPercent + 5 || completed == total)
                {
                    if (progressTask)
                        progressTask->setProgressDescription(QStringLiteral("Baseline means and fast gene blocks: %1% (%2 / %3 tiles)").arg(percent).arg(completed).arg(total));
                    qInfo() << "[Baseline]" << completed << "/" << total << "blocks (" << percent << "% ) at" << scanTimer.elapsed() << "ms";
                    lastProgressPercent = percent;
                }
            });

        for (std::size_t speciesIndex = 0; speciesIndex < data->speciesRows.size(); ++speciesIndex)
        {
            auto& means = data->baselineMeans[data->speciesNames[speciesIndex]];
            means.reserve(data->geneColumns.size());
            const auto count = aggregated.counts[speciesIndex];
            if (count == 0)
                continue;
            for (std::size_t geneIndex = 0; geneIndex < data->geneColumns.size(); ++geneIndex)
                means[data->dimensionNames[geneIndex]] = { static_cast<int>(count), static_cast<float>(aggregated.sums[speciesIndex][geneIndex] / count) };
        }
        if (data->needsSelectedScan) {
            const auto speciesCount = data->speciesRows.size();
            data->selectedMeans.assign(speciesCount, std::vector<float>(data->geneColumns.size(), 0.0f));
            for (std::size_t species = 0; species < speciesCount; ++species) {
                const auto count = aggregated.counts[speciesCount + species];
                if (count == 0) continue;
                for (std::size_t gene = 0; gene < data->geneColumns.size(); ++gene)
                    data->selectedMeans[species][gene] = static_cast<float>(aggregated.sums[speciesCount + species][gene] / count);
            }
            data->selectedScanFinished = true;
            qInfo() << "[Update] baseline and selected means prepared in one scan";
        }
        if (progressTask)
            progressTask->setProgressDescription(QStringLiteral("Baseline means ready; continuing selection update..."));
        context->setProgress(1.0);
    }, mv::workflow::WorkflowPlan::JobThreadAffinity::CurrentWorkerThread, baselineNeeded ? 0.86 : 0.01);

    workflow->addSequentialStage(QStringLiteral("Scan selected gene means"), [this, data](const mv::workflow::WorkflowPlan::Job&, const mv::workflow::SharedWorkflowExecutionContext& context) {
        XSC_TRACE("update.stage.Scan selected gene means");
        if (!data->inputsReady || !data->needsSelectedScan || data->selectedScanFinished || data->geneColumns.empty())
        {
            context->setProgress(1.0);
            return;
        }

        if (const auto task = context->getTask())
        {
            task->setName(QStringLiteral("Scan selected gene means"));
            task->setProgressDescription(QStringLiteral("Reading selected cells..."));
        }

        using SpeciesGeneSums = std::vector<std::vector<double>>;
        struct Stage1State
        {
            SpeciesGeneSums sums;
            PointStreaming::SegmentColumns segment;
        };

        const auto speciesCount = data->speciesRows.size();
        const auto totalGeneCount = data->geneColumns.size();
        const auto stage1TableBytes = speciesCount * totalGeneCount * sizeof(double);
        const int stage1Threads = std::max(1, std::min({ data->workerLimit,
            static_cast<int>(std::thread::hardware_concurrency()),
            static_cast<int>(data->memoryBudgetBytes / (stage1TableBytes + PointStreaming::kTargetReadBytes)) }));
        const std::vector<std::uint64_t> geneColumns(data->geneColumns.begin(), data->geneColumns.end());
        qInfo() << "[Update] selected gene read:" << stage1Threads << "workers, storage backend"
                << (data->points->getStorageBackendType() ? static_cast<int>(*data->points->getStorageBackendType()) : -1);
        if (const auto storage = data->points->getTiledStorage())
            qInfo() << "[Update] selected gene read: tile order, geometry" << storage->rowTileSize() << "rows x" << storage->columnTileSize() << "columns";

        std::atomic<int> lastProgressPercent { -1 };
        const auto stage1 = PointStreaming::reduceRows(data->points.get(), data->selectedRows, stage1Threads, static_cast<std::uint64_t>(totalGeneCount),
            [speciesCount, totalGeneCount]() {
                Stage1State state;
                state.sums.assign(speciesCount, std::vector<double>(totalGeneCount, 0.0));
                return state;
            },
            [data, &geneColumns](Stage1State& state, std::size_t selectedRowIndex, const PointDataAccessView& view, std::uint64_t row, std::uint64_t offset) {
                const int speciesIndex = data->selectedSpeciesIndex[selectedRowIndex];
                if (speciesIndex < 0)
                    return;
                state.segment.update(geneColumns, offset, view.columns);
                PointStreaming::accumulateSegment(view, row, state.segment, state.sums[static_cast<std::size_t>(speciesIndex)].data(), nullptr);
            },
            [](Stage1State& into, Stage1State& from) {
                for (std::size_t speciesIndex = 0; speciesIndex < into.sums.size(); ++speciesIndex)
                    for (std::size_t geneIndex = 0; geneIndex < into.sums[speciesIndex].size(); ++geneIndex)
                        into.sums[speciesIndex][geneIndex] += from.sums[speciesIndex][geneIndex];
            },
            [context, task = QPointer<mv::Task>(context->getTask()), &lastProgressPercent](std::size_t completed, std::size_t total) {
                const double progress = total > 0 ? static_cast<double>(completed) / static_cast<double>(total) : 1.0;
                context->setProgress(progress);
                const int percent = total > 0 ? static_cast<int>(100 * completed / total) : 100;
                int previous = lastProgressPercent.load();
                if (percent >= previous + 10 || completed == total)
                {
                    if (lastProgressPercent.compare_exchange_strong(previous, percent))
                    {
                        if (task)
                            task->setProgressDescription(QStringLiteral("Selected gene means: %1% (%2 / %3 blocks)").arg(percent).arg(completed).arg(total));
                        qInfo() << "[Update] selected gene read:" << completed << "/" << total << "blocks (" << percent << "% )";
                    }
                }
            });

        data->selectedMeans.assign(speciesCount, std::vector<float>(totalGeneCount, 0.0f));
        for (std::size_t speciesIndex = 0; speciesIndex < speciesCount; ++speciesIndex)
        {
            const auto selectedCount = data->selectedCellCounts[speciesIndex];
            if (selectedCount == 0)
                continue;
            for (std::size_t geneIndex = 0; geneIndex < totalGeneCount; ++geneIndex)
                data->selectedMeans[speciesIndex][geneIndex] = static_cast<float>(stage1.sums[speciesIndex][geneIndex] / static_cast<double>(selectedCount));
        }
        context->setProgress(1.0);
    }, mv::workflow::WorkflowPlan::JobThreadAffinity::CurrentWorkerThread, baselineNeeded ? 0.01 : 0.86);

    workflow->addSequentialStage(QStringLiteral("Publish selection datasets and update views"), [this, data](const mv::workflow::WorkflowPlan::Job&, const mv::workflow::SharedWorkflowExecutionContext& context) {
        XSC_TRACE("update.stage.Publish selection datasets and update views");
        if (data->inputsReady)
        {
            auto currentPoints = _mainPointsDataset.getCurrentDataset();
            const auto currentSelection = currentPoints.isValid() ? currentPoints->getSelectionIndices() : std::vector<std::seed_seq::result_type>{};
            if (!currentPoints.isValid() || currentPoints->getId() != data->pointsId || currentMeanMapSignature() != data->baselineSignature || currentSelection != data->selectionSnapshot)
            {
                data->staleInputs = true;
                context->setProgress(1.0);
                return;
            }

            if (data->needsBaselineScan)
            {
                _clusterGeneMeanExpressionMap = std::move(data->baselineMeans);
                _meanMapComputed = true;
                _meanMapSignature = data->baselineSignature;
            }
            if (data->needsSelectedScan)
            {
                _selectedMeansCache.key = data->selectedMeansKey;
                _selectedMeansCache.means = std::move(data->selectedMeans);
            }

            updateButtonTriggeredCore(context);
            data->published = true;
        }
        context->setProgress(1.0);
    }, mv::workflow::WorkflowPlan::JobThreadAffinity::GuiThread, 0.10);

    mv::workflow::WorkflowOptions options;
    options.reporting.progress = true;
    options.reporting.finishedNotification = false;

    _updateWorkflowRunning = true;
    startCodeTimer("UpdateGeneFilteringTrigger");
    mv::workflow::WorkflowResultFuture future;
    try
    {
        future = mv::Application::getWorkflowPlanExecutor().execute(std::move(workflow), nullptr, options);
    }
    catch (const std::exception& exception)
    {
        stopCodeTimer("UpdateGeneFilteringTrigger");
        _updateWorkflowRunning = false;
        _pauseStatusUpdates = false;
        showStatusMessage(QStringLiteral("Could not start selection update: %1").arg(QString::fromUtf8(exception.what())), QStringLiteral("#dc3545"));
        _statusColorAction.setString("E");
        enableActions();
        return;
    }
    if (auto task = future.getTask())
    {
        task->setGuiScopes({ mv::Task::GuiScope::Foreground });
        auto notification = new mv::util::Notification(QPointer<mv::Task>(task), nullptr);
        QObject::connect(notification, &mv::util::Notification::finished, notification, &QObject::deleteLater);
        notification->show();
        notification->raise();
    }
    future.onFinished(this, [this, data, updateTiming](const mv::workflow::SharedWorkflowResult& result) {
        updateTiming->finish();
        stopCodeTimer("UpdateGeneFilteringTrigger");
        _updateWorkflowRunning = false;

        if (data->staleInputs)
        {
            showStatusMessage(QStringLiteral("Selection or datasets changed during the update. Run Update again."), QStringLiteral("#d99500"));
            _statusColorAction.setString("M");
        }
        else if (!result || result->getStatus() != mv::workflow::WorkflowResultBase::Status::Success)
        {
            showStatusMessage(QStringLiteral("Selection update failed."), QStringLiteral("#dc3545"));
            _statusColorAction.setString("E");
        }
        else if (_statusColorAction.getString() != "E")
        {
            _statusColorAction.setString("C");
        }

        _pauseStatusUpdates = false;
        enableActions();
        if (data->published && !data->staleInputs)
        {
            auto pointsDataset = _mainPointsDataset.getCurrentDataset();
            if (pointsDataset.isValid())
                if (pointsDataset->getSelectionIndices() != _selectedIndicesFromStorage) pointsDataset->setSelectionIndices(_selectedIndicesFromStorage);
        }
        if (data->published && !data->staleInputs)
            QTimer::singleShot(0, &_XSCGeneDetectPlugin, [this]() { _XSCGeneDetectPlugin.prepareExplorationForCurrentTable(); });
    });
}

void SettingsAction::updateButtonTriggeredCore(const mv::workflow::SharedWorkflowExecutionContext& publicationContext)
{
    XSC_TRACE("SettingsAction::updateButtonTriggeredCore");
    if (_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
    {
        qDebug() << "Map for hierarchy items change method stop for project load blocker is checked";
        return;
    }

    try {
        // _startComputationTriggerAction.setDisabled(true);
        //startCodeTimer("Part1");

        int groupIDDeletion = 10;
        int groupID1 = 10 * 2;
        int groupID2 = 10 * 3;
        QElapsedTimer updateTimer;
        updateTimer.start();
        showStatusMessage(QStringLiteral("Updating selection"), QStringLiteral("#007bff"));
        removeTemporaryDatasets(); // remove only the previous update's queued dataset ids; keep linked reusable datasets
        qDebug() << "[Update] previous temporary datasets removed:" << updateTimer.elapsed() << "ms";
        const auto logUpdateStage = [&updateTimer, &publicationContext](const char* stage) {
            qInfo() << "[Update]" << stage << "at" << updateTimer.elapsed() << "ms";
            const QString stageName = QString::fromLatin1(stage);
            double progress = 0.02;
            if (stageName == QStringLiteral("selected expression subset created")) progress = 0.12;
            else if (stageName == QStringLiteral("preparing embedding selection")) progress = 0.18;
            else if (stageName == QStringLiteral("selected embedding subset created")) progress = 0.28;
            else if (stageName == QStringLiteral("temporary datasets prepared")) progress = 0.38;
            else if (stageName == QStringLiteral("species and cell-type membership prepared")) progress = 0.48;
            else if (stageName == QStringLiteral("selected gene means ready")) progress = 0.65;
            else if (stageName == QStringLiteral("selected versus rest statistics ready")) progress = 0.76;
            else if (stageName == QStringLiteral("expression statistics finished")) progress = 0.86;
            else if (stageName == QStringLiteral("top genes and gene table model finished")) progress = 0.96;
            publicationContext->setProgress(progress);
        };
        auto pointsDataset = _mainPointsDataset.getCurrentDataset();
        auto embeddingDataset = _embeddingDataset.getCurrentDataset();
        auto speciesDataset = _speciesNamesDataset.getCurrentDataset();
        auto clusterDataset = _bottomClusterNamesDataset.getCurrentDataset();
        auto referenceTreeDataset = _referenceTreeDataset.getCurrentDataset();
        // This string mirrors the linked PhyTVu species selection. Preserve it across
        // recomputations so the GeneDetect explorer remains synchronized.
        logUpdateStage("resetting gene connection");
        _geneNamesConnection.setString("");
        logUpdateStage("validating input datasets");
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

        auto embeddingDatasetRaw = mv::data().getDataset<Points>(embeddingDataset->getId());
        auto pointsDatasetRaw = mv::data().getDataset<Points>(pointsDataset->getId());
        auto pointsDatasetallColumnNameList = pointsDatasetRaw->getDimensionNames();
        auto embeddingDatasetallColumnNameList = embeddingDatasetRaw->getDimensionNames();
        // Project restoration can assign the pickers before their data is ready,
        // so their change callbacks are not a reliable initialization barrier.
        // The table model needs this list even on the first explicit Update.
        _totalGeneList = pointsDatasetallColumnNameList;
        if (!_meanMapComputed || _meanMapSignature != currentMeanMapSignature()) {
            logUpdateStage("preparing species-wide baseline means");
            computeGeneMeanExpressionMap();
            if (!_meanMapComputed || _meanMapSignature != currentMeanMapSignature()) {
                throw std::runtime_error("Species-wide baseline means could not be initialized");
            }
            logUpdateStage("species-wide baseline means ready");
        }
        if (_topHierarchyClusterMap.empty()) {
            computeHierarchyAppearanceVector();
        }
        //stopCodeTimer("Part3");
        //startCodeTimer("Part4");
        std::vector<int> embeddingDatasetColumnIndices(embeddingDatasetallColumnNameList.size());
        std::iota(embeddingDatasetColumnIndices.begin(), embeddingDatasetColumnIndices.end(), 0);

        std::vector<int> pointsDatasetallColumnIndices(PointStreaming::usableGeneCount(pointsDatasetallColumnNameList.size(), pointsDatasetRaw->getNumDimensions()));
        std::iota(pointsDatasetallColumnIndices.begin(), pointsDatasetallColumnIndices.end(), 0);
        qInfo() << "[Update] input:" << pointsDatasetRaw->getNumPoints() << "cells,"
                << _selectedIndicesFromStorage.size() << "selected," << pointsDatasetallColumnIndices.size() << "genes";
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

                    // The selection was just read from this dataset. Setting it again
                    // needlessly resolves all linked selections before creating the subset.
                    logUpdateStage("creating selected expression subset");
                    _selectedPointsDataset = pointsDatasetRaw->createSubsetFromSelection("SelectedPointsDataset");
                    logUpdateStage("selected expression subset created");
                    _selectedPointsDataset->setGroupIndex(groupIDDeletion);
                    trackTemporaryDataset(_selectedPointsDataset->getId(), groupIDDeletion);

                    if (!_tsneDatasetExpressionColors.isValid())
                    {
                        _tsneDatasetExpressionColors = mv::data().createDataset("Points", "TSNEDatasetExpressionColors", _selectedPointsDataset);
                        _tsneDatasetExpressionColors->setGroupIndex(groupIDDeletion);
                        trackTemporaryDataset(_tsneDatasetExpressionColors->getId(), groupIDDeletion);
                        mv::events().notifyDatasetAdded(_tsneDatasetExpressionColors);

                    }

                    logUpdateStage("preparing embedding selection");
                    if (embeddingDatasetRaw->getSelectionIndices() != _selectedIndicesFromStorage) {
                        embeddingDatasetRaw->setSelectionIndices(_selectedIndicesFromStorage);
                    }
                    logUpdateStage("creating selected embedding subset");
                    _selectedPointsEmbeddingDataset = embeddingDatasetRaw->createSubsetFromSelection("TSNEDataset", _selectedPointsDataset);
                    logUpdateStage("selected embedding subset created");
                    _selectedPointsEmbeddingDataset->setGroupIndex(groupIDDeletion);
                    trackTemporaryDataset(_selectedPointsEmbeddingDataset->getId(), groupIDDeletion);


                    if (!_tsneDatasetSpeciesColors.isValid())
                    {
                        _tsneDatasetSpeciesColors = mv::data().createDataset("Cluster", "TSNEDatasetSpeciesColors", _selectedPointsDataset);
                        _tsneDatasetSpeciesColors->setGroupIndex(groupIDDeletion);
                        trackTemporaryDataset(_tsneDatasetSpeciesColors->getId(), groupIDDeletion);
                        mv::events().notifyDatasetAdded(_tsneDatasetSpeciesColors);
                    }

                    if (!_tsneDatasetClusterColors.isValid())
                    {
                        _tsneDatasetClusterColors = mv::data().createDataset("Cluster", "TSNEDatasetClusterColors", _selectedPointsDataset);
                        _tsneDatasetClusterColors->setGroupIndex(groupIDDeletion);
                        trackTemporaryDataset(_tsneDatasetClusterColors->getId(), groupIDDeletion);
                        mv::events().notifyDatasetAdded(_tsneDatasetClusterColors);
                    }


                    if (!_filteredUMAPDatasetPoints.isValid())
                    {
                        _filteredUMAPDatasetPoints = mv::data().createDataset("Points", "Filtered UMAP Dataset Points");
                        _filteredUMAPDatasetPoints->setGroupIndex(groupID1);
                        trackTemporaryDataset(_filteredUMAPDatasetPoints->getId(), groupID1);
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
                        trackTemporaryDataset(_filteredUMAPDatasetColors->getId(), groupID1);
                        mv::events().notifyDatasetAdded(_filteredUMAPDatasetColors);

                        _filteredUMAPDatasetClusters = mv::data().createDataset("Cluster", "Filtered UMAP Dataset Clusters", _filteredUMAPDatasetPoints);
                        _filteredUMAPDatasetClusters->setGroupIndex(groupID1);
                        trackTemporaryDataset(_filteredUMAPDatasetClusters->getId(), groupID1);
                        mv::events().notifyDatasetAdded(_filteredUMAPDatasetClusters);

                    }



                    /*if (!_selectedPointsEmbeddingDataset.isValid())
                    {
                        _selectedPointsEmbeddingDataset = mv::data().createDataset("Points", "TSNEDataset", _selectedPointsDataset);
                        _selectedPointsEmbeddingDataset->setGroupIndex(10);
                        trackTemporaryDataset(_selectedPointsEmbeddingDataset->getId(), 10);
                        mv::events().notifyDatasetAdded(_selectedPointsEmbeddingDataset);

                    }*/


                    if (!_geneSimilarityPoints.isValid())
                    {
                        _geneSimilarityPoints = mv::data().createDataset("Points", "GeneSimilarityPoints");
                        _geneSimilarityPoints->setGroupIndex(groupID2);
                        trackTemporaryDataset(_geneSimilarityPoints->getId(), groupID2);
                        mv::events().notifyDatasetAdded(_geneSimilarityPoints);
                    }
                    if (!_geneSimilarityClusterColoring.isValid())
                    {
                        _geneSimilarityClusterColoring = mv::data().createDataset("Cluster", "GeneSimilarityClusterColoring", _geneSimilarityPoints);
                        _geneSimilarityClusterColoring->setGroupIndex(groupID2);
                        trackTemporaryDataset(_geneSimilarityClusterColoring->getId(), groupID2);
                        mv::events().notifyDatasetAdded(_geneSimilarityClusterColoring);

                    }
                    //_geneSimilarityClusters.clear();
                    //stopCodeTimer("Part6.1");
                    logUpdateStage("temporary datasets prepared");
                    if (_selectedPointsDataset.isValid() && _selectedPointsEmbeddingDataset.isValid() && _tsneDatasetSpeciesColors.isValid() && _tsneDatasetClusterColors.isValid() && _geneSimilarityPoints.isValid() && _geneSimilarityClusterColoring.isValid())
                    {
                        //startCodeTimer("Part6.2");
                        //_tsneDatasetSpeciesColors->getClusters() = QVector<Cluster>();
                        //events().notifyDatasetDataChanged(_tsneDatasetSpeciesColors);
                        //_tsneDatasetClusterColors->getClusters() = QVector<Cluster>();
                        //events().notifyDatasetDataChanged(_tsneDatasetClusterColors);
                        _geneSimilarityClusterColoring->getClusters() = QVector<Cluster>();
                        events().notifyDatasetDataChanged(_geneSimilarityClusterColoring);
                        logUpdateStage("gene similarity views cleared");
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
                        logUpdateStage("selection expression colors prepared");
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
                                logUpdateStage("creating tSNE analysis");
                                analysisPlugin = mv::plugins().requestPlugin<AnalysisPlugin>("tSNE Analysis", { _selectedPointsEmbeddingDataset });
                                if (!analysisPlugin) {
                                    qDebug() << "Could not find create TSNE Analysis";
                                    return;
                                }
                                _selectedTsneAnalysisPlugin = analysisPlugin; // remembered so that the next update can destroy it
                                _selectedPointsTSNEDataset = analysisPlugin->getOutputDataset();
                                _selectedPointsTSNEDataset->setGroupIndex(groupIDDeletion);
                                trackTemporaryDataset(_selectedPointsTSNEDataset->getId(), groupIDDeletion);
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

                                        logUpdateStage("starting tSNE analysis");
                                        startAction->trigger();
                                        logUpdateStage("tSNE start returned");

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
                                    trackTemporaryDataset(_selectedPointsTSNEDataset->getId(), groupIDDeletion);
                                    mv::events().notifyDatasetAdded(_selectedPointsTSNEDataset);

                                    auto umapDatasetRaw = mv::data().getDataset<Points>(umapDataset->getId());
                                    auto dimNames = umapDatasetRaw->getDimensionNames();
                                    int preComputedEmbeddingColumnsSize = umapDatasetRaw->getNumDimensions();
                                    std::vector<float> resultContainerPreComputedUMAP(selectedIndicesFromStorageSize * preComputedEmbeddingColumnsSize);
                                    std::vector<int> preComputedEmbeddingColumnIndices(preComputedEmbeddingColumnsSize);

                                    std::iota(preComputedEmbeddingColumnIndices.begin(), preComputedEmbeddingColumnIndices.end(), 0);

                                    logUpdateStage("reading selected precomputed embedding");
                                    umapDatasetRaw->populateDataForDimensions(resultContainerPreComputedUMAP, preComputedEmbeddingColumnIndices, _selectedIndicesFromStorage);
                                    logUpdateStage("selected precomputed embedding read");

                                    QString datasetId = _selectedPointsTSNEDataset->getId();
                                    populatePointData(datasetId, resultContainerPreComputedUMAP, selectedIndicesFromStorageSize, preComputedEmbeddingColumnsSize, dimNames);

                                    if (_selectedPointsTSNEDataset.isValid())
                                    {
                                        logUpdateStage("updating selected embedding scatterplot");
                                        scatterplotModificationsLowDimUMAP();
                                        logUpdateStage("selected embedding scatterplot updated");
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
                    logUpdateStage("preparing species and cell-type membership");
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
                    logUpdateStage("species and cell-type membership prepared");
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
                        const QString stage1Key = selectedMeansKey(pointsDatasetRaw->getId(), _selectedIndicesFromStorage,
                            selectedPointSpeciesIndex, totalGeneCount, speciesCount);
                        const bool stage1Cached = (stage1Key == _selectedMeansCache.key);
                        logUpdateStage(stage1Cached ? "reusing selected gene means" : "computing selected gene means");
                        auto& speciesSelectedMeans = _selectedMeansCache.means; // the cache owns the matrix (no second copy)
                        if (!stage1Cached) {
                        const std::vector<std::uint64_t> geneColumns(pointsDatasetallColumnIndices.begin(), pointsDatasetallColumnIndices.end());
                        using SpeciesGeneSums = std::vector<std::vector<double>>;
                        // Each thread holds one species x genes sum table; keep all of them within a fixed memory budget.
                        const std::size_t stage1TableBytes = static_cast<std::size_t>(speciesCount) * static_cast<std::size_t>(totalGeneCount) * sizeof(double);
                        const std::size_t stage1BudgetBytes = static_cast<std::size_t>(std::max(64, _memoryBudgetMb.getValue())) << 20;
                        XSC_LOG("stage 1: selected cells", _selectedIndicesFromStorage.size(), "species", speciesCount, "genes", totalGeneCount);
                        const int stage1Threads = std::max(1, std::min({ std::max(1, _maxWorkerThreads.getValue()),
                            static_cast<int>(std::thread::hardware_concurrency()),
                            static_cast<int>(stage1BudgetBytes / (stage1TableBytes + PointStreaming::kTargetReadBytes)) }));
                        struct Stage1State {
                            SpeciesGeneSums          sums;
                            PointStreaming::SegmentColumns segment;
                        };
                        qInfo() << "[Update] selected gene read:" << stage1Threads << "workers,"
                                << "storage backend" << (pointsDatasetRaw->getStorageBackendType()
                                    ? static_cast<int>(*pointsDatasetRaw->getStorageBackendType()) : -1);
                        int lastReadProgress = -1;
                        if (const auto storage = pointsDatasetRaw->getTiledStorage()) {
                            qInfo() << "[Update] selected gene read: tile order, geometry"
                                    << storage->rowTileSize() << "rows x" << storage->columnTileSize() << "columns";
                        }
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
                            },
                            [&](std::size_t completed, std::size_t total) {
                                const int percent = static_cast<int>(100 * completed / total);
                                if (lastReadProgress < 0 || percent >= lastReadProgress + 10 || completed == total) {
                                    qInfo() << "[Update] selected gene read:" << completed << "/" << total
                                            << "blocks (" << percent << "%) at" << updateTimer.elapsed() << "ms";
                                    lastReadProgress = percent;
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
                        logUpdateStage("selected gene means ready");
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

                                // Baselines are stored in an unordered_map; their iteration order
                                // does not follow genesByName. Look up each gene by its key.
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

                    logUpdateStage("selected versus rest statistics ready");
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
        showStatusMessage(QStringLiteral("Selection ready"), QStringLiteral("#28a745"));
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


    _meanMapComputed = false;
    _meanMapSignature.clear();
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
            // A tiled source must be visited in storage order once. Independent
            // species scans repeatedly decode the same tiles and compete for cache.
            std::vector<std::vector<std::uint64_t>> groups;
            groups.reserve(speciesclusters.size());
            for (const auto& species : speciesclusters) groups.emplace_back(species.getIndices().begin(), species.getIndices().end());
            QElapsedTimer baselineTimer;
            baselineTimer.start();
            qInfo() << "[Baseline] scanning" << mainPointDatasetFull->getNumPoints() << "cells,"
                    << allGeneIndices.size() << "genes across" << groups.size() << "species";
            int lastProgressPercent = -1;
            const auto aggregated = PointStreaming::aggregateGroupedColumns(mainPointDatasetFull.get(), groups, allGeneIndices,
                std::max(1, _maxWorkerThreads.getValue()), static_cast<std::size_t>(std::max(64, _memoryBudgetMb.getValue())) << 20, false,
                [&](std::size_t completed, std::size_t total) {
                    const int percent = static_cast<int>(100 * completed / total);
                    if (lastProgressPercent < 0 || percent >= lastProgressPercent + 5 || completed == total) {
                        qInfo() << "[Baseline]" << completed << "/" << total << "blocks (" << percent
                                << "%) at" << baselineTimer.elapsed() << "ms";
                        lastProgressPercent = percent;
                    }
                });
            for (size_t species=0; species<groups.size(); ++species) {
                auto& means = _clusterGeneMeanExpressionMap[speciesclusters[species].getName()];
                means.reserve(allGeneIndices.size());
                const auto count = aggregated.counts[species];
                if (count == 0) continue;
                for (size_t gene=0; gene<allGeneIndices.size(); ++gene)
                    means[mainPointDimensionNames[gene]] = { static_cast<int>(count), static_cast<float>(aggregated.sums[species][gene]/count) };
            }

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

void SettingsAction::invalidateTraitResults()
{
    _traitPublishedKey.clear();
    if (_traitSampleSize == 0 && _geneTraitValuesComputed.empty()) return;
    _geneTraitValuesComputed.clear(); _geneTraitPValues.clear();
    _geneTraitFdrValues.clear(); _geneTraitExtras.clear();
    _traitSampleSize = 0; _traitMatrix = TraitMatrix();
    _traitMethodNote.clear(); _traitSignificanceBasis.clear();
    _traitContributionShown = false; _traitValueLabel.clear();
    publishTraitCorrelationToModel();
    _XSCGeneDetectPlugin.refreshTraitPlot();
}

void SettingsAction::updateTraitReverseSearchFeature()
{
    XSC_TRACE("SettingsAction::updateTraitReverseSearchFeature");
    if (!_traitWorkflow.active) { invalidateTraitResults(); return; }
    if (_explorationSuspended) return;
    if (_deWorkflow.busy()) {
        _traitWorkflow.pending = true;
        if (!_traitProgress.running()) _traitProgress.begin(this, QStringLiteral("Trait correlation"));
        _traitProgress.state()->phase(QStringLiteral("Waiting for differential expression preparation"), 0);
        return;
    }
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
    _traitReadTiming.reset();
    const bool failed = _traitReadCancel && !*_traitReadCancel && _traitJobWatcher.future().isCanceled();
    if (!failed && _traitJobs && _traitReadCancel && !*_traitReadCancel) {
        for (auto& job : *_traitJobs) {
            _traitCache.entries[job.key] = std::move(job.entry);
        }
    }
    _traitJobs.reset();
    _traitJobDataset = Dataset<Points>();
    _traitWorkflow.finish();
    _traitWorkflow.pending = false;
    if ((_traitReadCancel && *_traitReadCancel) || !_traitWorkflow.active) {
        if (!_explorationSuspended) prepareExplorationAnalysis();
        return;
    }
    if (failed) {
        _traitProgress.cancel();
        showStatusMessage(QStringLiteral("Trait data could not be read. Run Update to retry."), QStringLiteral("#dc3545"));
        if (_deWorkflow.active) requestSpeciesDifferentialExpression();
        return;
    }

    computeTraitCorrelation(); // all means are cached now (or the selection changed meanwhile and new ones are read)
}

void SettingsAction::computeTraitCorrelation()
{
    XSC_TRACE("SettingsAction::computeTraitCorrelation");
    if (_explorationSuspended || !_traitWorkflow.active) return;
    // The expression data is read in the background (see below); until that is finished a new request only has to be remembered,
    // because the cache the background job fills must not be touched.
    if (_traitWorkflow.busy()) {
        _traitWorkflow.pending = true;
        return;
    }
    if (_deWorkflow.busy()) { _traitWorkflow.pending = true; return; }

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
        _traitProgress.cancel();
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
        if (_deWorkflow.active) requestSpeciesDifferentialExpression();
        return;
    }

    Dataset<Points> mainPointsDataset = _mainPointsDataset.getCurrentDataset();
    const std::vector<QString> allGeneNames = mainPointsDataset->getDimensionNames();
    if (allGeneNames.empty()) {
        return;
    }

    // Genes to correlate. The gene table lists the top-N genes (a few hundred): reading only their expression instead of all
    // ~20,000 genes is what makes the analysis fast. The selected gene is always included (it feeds the plot).
    const bool allGenes = _traitWindowOpen && (_traitGeneScope.getCurrentText() == "All genes (slower)");
    const auto requestScope = traitRequestScope(_traitWindowOpen, allGenes);
    const QString selectedGeneName = _selectedGene.getString();
    std::vector<int> geneDims;
    std::vector<QString> geneNames;
    const size_t usableGenes = PointStreaming::usableGeneCount(allGeneNames.size(), mainPointsDataset->getNumDimensions());
    for (size_t g = 0; g < usableGenes; ++g) {
        if (includesTraitGene(requestScope, _uniqueReturnGeneList.contains(allGeneNames[g]), allGeneNames[g] == selectedGeneName)) {
            geneDims.push_back(static_cast<int>(g));
            geneNames.push_back(allGeneNames[g]);
        }
    }
    const size_t geneCount = geneNames.size();
    if (geneCount == 0) {
        return;
    }
    const auto progress = _traitProgress.running() ? _traitProgress.state()
        : _traitProgress.begin(this, QStringLiteral("Trait correlation: %1").arg(traitName));

    // ---- 2. Trait value per species (metadata of the Phylogenetic Tree View) ----------------------------
    QString metaOptions;
    auto phytv = mv::plugins().getPluginFactory("XSCPhyTVu");
    if (phytv)
    {
        for (auto phytvwPlugin : mv::plugins().getPluginsByFactory(phytv))
        {
            // Display names can change when a project is restored or a view is
            // renamed. Use the linked reference tree to identify the view.
            auto referencePicker = phytvwPlugin->findChildByPath<DatasetPickerAction>("XSCPhyTVuPlugin Chart/Chart Options/Reference tree");
            if (referencePicker && referencePicker->getCurrentDataset().isValid() &&
                _referenceTreeDataset.getCurrentDataset().isValid() &&
                referencePicker->getCurrentDataset().getDatasetId() != _referenceTreeDataset.getCurrentDataset().getDatasetId())
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
                if (!QJsonDocument::fromJson(metaOptions.toUtf8()).object()["data"].toObject().isEmpty())
                    break;
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
    int matchingTraitNames = 0;
    for (const auto& entry : _traitCache.speciesSorted) {
        if (!traitObject.contains(entry.first)) {
            continue;
        }
        bool ok = false;
        ++matchingTraitNames;
        const double traitValue = traitObject.value(entry.first).toVariant().toString().toDouble(&ok);
        if (ok && std::isfinite(traitValue)) {
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
                    // Keep already-known means usable when an interactive
                    // color request cancels this incremental read.
                    job.entry = cached->second;
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
            _traitWorkflow.beginRead();

            const auto jobs = _traitJobs;
            const auto cancel = _traitReadCancel = _traitWorkflow.cancellation();
            const Dataset<Points>* datasetHandle = &_traitJobDataset; // member: stays valid while the job runs
            const int workers = parallelWorkers(largestPointSet);
            const std::size_t scratchBudgetBytes = static_cast<std::size_t>(std::max(64, _memoryBudgetMb.getValue())) << 20;

            showStatusMessage(QStringLiteral("Reading trait data"), QStringLiteral("#007bff"));

            _traitReadTiming = std::make_unique<xsc::timing::AsyncSpan>("trait.read.total");
            _traitJobWatcher.setFuture(QtConcurrent::run(&_explorationPool, [this, jobs, datasetHandle, workers, scratchBudgetBytes, cancel, progress]() {
            XSC_TRACE("trait.read.worker");
                progress->phase(QStringLiteral("Reading species expression means"), 0.02f);
                std::vector<int> geneToSlot(PointStreaming::usableGeneCount(
                    datasetHandle->get()->getDimensionNames().size(), datasetHandle->get()->getNumDimensions()), -1);
                std::vector<int> requestedGenes;
                for (const auto& job : *jobs) {
                    for (const int gene : job.genes) {
                        if (gene >= 0 && static_cast<size_t>(gene) < geneToSlot.size() && geneToSlot[static_cast<size_t>(gene)] < 0) {
                            geneToSlot[static_cast<size_t>(gene)] = static_cast<int>(requestedGenes.size());
                            requestedGenes.push_back(gene);
                        }
                    }
                }
                std::sort(requestedGenes.begin(), requestedGenes.end());
                std::fill(geneToSlot.begin(), geneToSlot.end(), -1);
                for (size_t slot = 0; slot < requestedGenes.size(); ++slot)
                    geneToSlot[static_cast<size_t>(requestedGenes[slot])] = static_cast<int>(slot);

                // For a dense request, all species/cell-type pairs share one read of the matrix.
                // Preserve the sparse-column reader for very small gene sets to avoid scanning empty columns.
                const size_t geneSpan = requestedGenes.empty() ? 0 : static_cast<size_t>(requestedGenes.back()) + 1;
                const auto source = datasetHandle->get();
                const bool tiledSource = !source->isProxy() && source->isFull() && source->getTiledStorage();
                bool groupedRead = tiledSource || requestedGenes.size() >= 32 || requestedGenes.size() * 4 >= geneSpan;
                if (groupedRead) {
                    std::vector<std::vector<std::uint64_t>> groupedPoints;
                    groupedPoints.reserve(jobs->size());
                    std::vector<int> points;
                    for (auto& job : *jobs) {
                        points = intersectSortedIndicesToInt(*job.speciesPoints, *job.cellTypePoints);
                        job.entry.cellCount = static_cast<int>(points.size());
                        if (points.empty()) {
                            job.entry.known.assign(job.entry.known.size(), 1); // no cells: nothing to read, remembered as "no cells"
                            groupedPoints.emplace_back();
                        }
                        else {
                            groupedPoints.emplace_back(points.begin(), points.end());
                        }
                    }

                    const std::size_t stateBytes = jobs->size() * requestedGenes.size() * sizeof(double) + PointStreaming::kTargetReadBytes;
                    const std::size_t indexBytes = PointStreaming::groupedIndexMemoryBytes(groupedPoints);
                    groupedRead = indexBytes <= scratchBudgetBytes && stateBytes <= scratchBudgetBytes - indexBytes;
                    if (groupedRead) {
                        const auto grouped = PointStreaming::aggregateGroupedColumns(datasetHandle->get(), groupedPoints,
                            requestedGenes, workers, scratchBudgetBytes, false,
                            [cancel, progress](std::size_t done, std::size_t total) {
                                if (*cancel) throw std::runtime_error("Cancelled");
                                progress->advance(done, total, 0.02f, 0.60f);
                            });
                        for (size_t jobIndex = 0; jobIndex < jobs->size(); ++jobIndex) {
                            auto& job = (*jobs)[jobIndex];
                            const std::uint64_t count = grouped.counts[jobIndex];
                            if (count == 0)
                                continue;
                            const double inverseCount = 1.0 / static_cast<double>(count);
                            for (const int gene : job.genes) {
                                const size_t slot = static_cast<size_t>(geneToSlot[static_cast<size_t>(gene)]);
                                job.entry.means[static_cast<size_t>(gene)] = static_cast<float>(grouped.sums[jobIndex][slot] * inverseCount);
                                job.entry.known[static_cast<size_t>(gene)] = 1;
                            }
                        }
                    }
                    else {
                        groupedPoints.clear();
                        groupedPoints.shrink_to_fit();
                    }
                }
                if (!groupedRead) {
                    QThreadPool pool;
                    pool.setMaxThreadCount(workers);
                    QtConcurrent::blockingMap(&pool, *jobs, [&](TraitEntryJob& job) {
                        const std::vector<int> points = intersectSortedIndicesToInt(*job.speciesPoints, *job.cellTypePoints);
                        job.entry.cellCount = static_cast<int>(points.size());
                        if (points.empty()) {
                            job.entry.known.assign(job.entry.known.size(), 1);
                            return;
                        }
                        computeChunkedGeneMeans(*datasetHandle, job.genes, points,
                            [&job, cancel](int, const std::vector<int>& chunkGenes, const std::vector<float>& chunkMeans) {
                                if (*cancel) throw std::runtime_error("Cancelled");
                                for (size_t g = 0; g < chunkGenes.size(); ++g) {
                                    const size_t gene = static_cast<size_t>(chunkGenes[g]);
                                    job.entry.means[gene] = chunkMeans[g];
                                    job.entry.known[gene] = 1;
                                }
                            });
                    });
                }
                QMetaObject::invokeMethod(this, [this, cancel]() {
                    if (*cancel) return;
                    showStatusMessage(QStringLiteral("Trait data ready"), QStringLiteral("#007bff"));
                    }, Qt::QueuedConnection);
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

    if (usable.size() < kMinSpeciesForTraitCorrelation) {
        _traitProgress.cancel();
        _geneTraitValuesComputed.clear(); _geneTraitPValues.clear(); _geneTraitFdrValues.clear(); _geneTraitExtras.clear();
        _traitSampleSize = 0; _traitMatrix = TraitMatrix(); _traitMethodNote.clear();
        qDebug() << "Trait correlation needs at least" << kMinSpeciesForTraitCorrelation << "species with trait values and at least" << minCells << "cells of the selected cell types; found" << usable.size();
        qInfo() << "[Trait] trait:" << traitName << "cell types:" << cellTypes
                << "metadata species:" << traitObject.size() << "expression species:" << speciesClusters.size()
                << "matching species names:" << matchingTraitNames << "numeric trait matches:" << traitPerSpecies.size()
                << "species passing cell threshold:" << usable.size();
        if (traitObject.isEmpty()) {
            _traitMethodNote = QString("No trait correlation: no numeric metadata was found for trait '%1' in the linked tree view.").arg(traitName);
        }
        else if (matchingTraitNames == 0) {
            _traitMethodNote = QString("No trait correlation: species names in trait '%1' do not match the expression species names.").arg(traitName);
            qInfo() << "[Trait] metadata names:" << traitObject.keys().mid(0, 5);
            QStringList expressionNames;
            for (const auto& entry : _traitCache.speciesSorted) expressionNames << entry.first;
            qInfo() << "[Trait] expression names:" << expressionNames.mid(0, 5);
        }
        else if (traitPerSpecies.empty()) {
            _traitMethodNote = QString("No trait correlation: matching species have no valid numeric values for trait '%1'.").arg(traitName);
        }
        else {
            _traitMethodNote = QString("No trait correlation: %1 species have numeric values for '%2', but only %3 have at least %4 cells of the selected cell type(s): %5.")
                .arg(traitPerSpecies.size()).arg(traitName).arg(usable.size()).arg(minCells).arg(cellTypes.join(", "));
            for (const QString& cellType : cellTypes) {
                qInfo() << "[Trait] cell type:" << cellType << "dataset cells:" << _traitCache.cellTypeSorted.at(cellType).size();
            }
        }
        publishTraitCorrelationToModel();
        if (_deWorkflow.active) requestSpeciesDifferentialExpression();
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

    // Pool the small species x genes matrix on the GUI thread, then snapshot it.
    // All expensive per-gene statistics and multiple-testing correction run in the worker.
    for (size_t i = 0; i < population.size(); ++i)
        combineCellTypeMeans(partsPerSpecies[i], pooling, GeneRange{ 0, geneCount }, expressionMatrix[i]);
    const bool phyloSeparate = (method != TraitMethod::Phylogenetic) && treeUsable;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto addVector = [&hash](const auto& values) {
        const quint64 count = values.size();
        hash.addData(QByteArrayView(reinterpret_cast<const char*>(&count), sizeof(count)));
        if (!values.empty()) hash.addData(QByteArrayView(reinterpret_cast<const char*>(values.data()),
            static_cast<qsizetype>(values.size() * sizeof(values.front()))));
    };
    const std::vector<int> options{ static_cast<int>(method), treeRoot, phyloSeparate ? 1 : 0 };
    addVector(options); addVector(geneDims); addVector(traitValues); addVector(plan.selected);
    for (const auto& row : expressionMatrix) addVector(row);
    for (const auto& node : treeNodes) {
        addVector(std::vector<int>{ node.leafIndex });
        addVector(std::vector<double>{ node.branchLength });
        addVector(node.children);
    }
    const QByteArray statisticsKey = hash.result();
    if (!_traitStatistics || _traitStatistics->key != statisticsKey) {
        auto batch = std::make_shared<TraitStatisticsBatch>();
        const auto cancel = _traitWorkflow.cancellation();
        const auto selected = plan.selected;
        const int mathWorkers = std::max(1, _maxWorkerThreads.getValue());
        _traitWorkflow.beginCompute();
        disconnect(&_traitStatisticsWatcher, nullptr, this, nullptr);
        connect(&_traitStatisticsWatcher, &QFutureWatcher<void>::finished, this, [this, batch, cancel]() {
            _traitStatsTiming.reset();
            _traitWorkflow.finish();
            if (*cancel || !_traitWorkflow.active) {
                if (!_explorationSuspended) prepareExplorationAnalysis();
                return;
            }
            if (!*cancel && batch->key.isEmpty()) {
                _traitProgress.cancel();
                showStatusMessage(QStringLiteral("Trait calculation could not finish. Run Update to retry."), QStringLiteral("#dc3545"));
                if (_deWorkflow.active) requestSpeciesDifferentialExpression();
                return;
            }
            if (!*cancel && !batch->key.isEmpty()) _traitStatistics = batch;
            computeTraitCorrelation();
        });
        _traitStatsTiming = std::make_unique<xsc::timing::AsyncSpan>("trait.statistics.total");
        _traitStatisticsWatcher.setFuture(QtConcurrent::run(&_explorationPool, [batch, cancel, matrix = std::move(expressionMatrix),
            traitValues, treeNodes, treeRoot, method, selected, phyloSeparate, statisticsKey, geneCount, progress, mathWorkers]() {
            XSC_TRACE("trait.statistics.worker");
            progress->phase(QStringLiteral("Computing trait correlations and significance"), 0.62f);
            if (*cancel) return;
            std::vector<const std::vector<float>*> rows;
            for (const auto& row : matrix) rows.push_back(&row);
            const auto context = prepareTraitStatisticsContext(method, traitValues, treeNodes, treeRoot, selected);
            const auto phyloContext = phyloSeparate
                ? prepareTraitStatisticsContext(TraitMethod::Phylogenetic, traitValues, treeNodes, treeRoot) : TraitStatisticsContext();
            batch->statistics.resize(geneCount);
            batch->phyloStatistics.resize(phyloSeparate ? geneCount : 0);
            // Disjoint short ranges retain prompt cancellation and use the
            // configured worker count without serializing per-gene statistics.
            auto ranges = splitGeneRanges(geneCount, (geneCount + 31) / 32);
            QThreadPool pool;
            pool.setMaxThreadCount(mathWorkers);
            std::atomic<std::size_t> completed{0};
            QtConcurrent::blockingMap(&pool, ranges, [&](GeneRange& range) {
                if (*cancel) return;
                computeTraitStatistics(rows, context, range, batch->statistics);
                if (phyloSeparate) computeTraitStatistics(rows, phyloContext, range, batch->phyloStatistics);
                progress->advance(completed.fetch_add(range.end - range.begin) + range.end - range.begin, geneCount, 0.62f, 0.94f);
            });
            if (*cancel) return;
            progress->phase(QStringLiteral("Correcting trait significance values"), 0.95f);
            std::vector<float> pValues(geneCount);
            for (size_t g = 0; g < geneCount; ++g) pValues[g] = batch->statistics[g].pValue;
            batch->fdr = benjaminiHochberg(pValues);
            if (phyloSeparate) {
                for (size_t g = 0; g < geneCount; ++g) pValues[g] = batch->phyloStatistics[g].pValue;
                batch->phyloFdr = benjaminiHochberg(pValues);
            }
            if (!*cancel) batch->key = statisticsKey;
        }));
        return;
    }
    const auto& statistics = _traitStatistics->statistics;
    const auto& phyloStatistics = _traitStatistics->phyloStatistics;
    const auto& fdrValues = _traitStatistics->fdr;
    const auto& phyloFdrValues = _traitStatistics->phyloFdr;
    const auto currentModel = _listModel.getVariant().value<QAbstractItemModel*>();
    if (_traitPublishedKey == statisticsKey && _traitPublishedModel == currentModel) {
        for (std::size_t i = 0; i < _traitMatrix.species.size(); ++i)
            _traitMatrix.selected[i] = explorerSelection.contains(_traitMatrix.species[i]) ? 1 : 0;
        _traitProgress.finish();
        _XSCGeneDetectPlugin.refreshTraitPlot();
        if (_deWorkflow.active) requestSpeciesDifferentialExpression();
        return;
    }
    progress->phase(QStringLiteral("Publishing trait table and plots"), 0.98f);
    _geneTraitValuesComputed.clear(); _geneTraitPValues.clear(); _geneTraitFdrValues.clear(); _geneTraitExtras.clear();
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
    const QString genesText = !_traitWindowOpen ? QStringLiteral("On-demand correlation for the selected gene; FDR is restricted to this requested gene, not all genes.") : allGenes
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
    _traitPublishedKey = statisticsKey; _traitPublishedModel = currentModel;
    showStatusMessage(QString("Trait ready: %1 species, %2 genes").arg(population.size()).arg(geneCount),
        QStringLiteral("#28a745"));
    _traitProgress.finish();
    if (_deWorkflow.active) requestSpeciesDifferentialExpression();
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
                rIt != _geneTraitValuesComputed.end() ? _traitSampleSize : 0, _traitMethodNote, _traitSignificanceBasis);
            if (rIt == _geneTraitValuesComputed.end() && _traitSampleSize > 0 && _traitWindowOpen) {
                correlationItem->setData("Not part of the trait analysis: the trait correlation is computed for the listed top-N genes only\n(setting Trait Correlation Genes -> All genes (slower) includes every gene).", Qt::ToolTipRole);
            }
        }
    }

    emit model->dataChanged(model->index(0, kTraitCorrelationColumn), model->index(model->rowCount() - 1, kTraitCorrelationColumn), { Qt::DisplayRole, Qt::UserRole, Qt::ToolTipRole, kTraitPValueRole, kTraitFdrRole, kTraitStatsRole, kTraitSignificanceFdrRole });
    _XSCGeneDetectPlugin.refreshTraitColumnVisibility();
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
            trackTemporaryDataset(_selectedPointsTSNEDatasetForGeneTable->getId(), groupID2);
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

void SettingsAction::cancelExplorationAnalysis()
{
    XSC_TRACE("SettingsAction::cancelExplorationAnalysis");
    _explorationSuspended = true;
    _traitProgress.cancel(); _deProgress.cancel();
    _deWorkflow.cancel(); _traitWorkflow.cancel();
    invalidateTraitResults();
    if (_deTimer) _deTimer->stop();
    if (_traitCorrelationTimer) _traitCorrelationTimer->stop();
    _traitStatistics.reset();
    _traitPublishedKey.clear(); _dePublishedKey.clear();
    _deStats.reset();
    _deOverview.reset();
}

void SettingsAction::prepareExplorationAnalysis()
{
    _explorationSuspended = false;
    _traitWorkflow.setActive(_traitWindowOpen || !_selectedGene.getString().isEmpty());
    _deWorkflow.resume(); _traitWorkflow.resume();
    _explorationPool.setMaxThreadCount(1);
    _explorationPool.setThreadPriority(QThread::LowPriority);
    if (_deWorkflow.active) {
        _traitWorkflow.pending = _traitWorkflow.active && !_reverseSearchTraitSelectionString.getString().isEmpty();
        requestSpeciesDifferentialExpression();
    }
    else if (_traitWorkflow.active && !_reverseSearchTraitSelectionString.getString().isEmpty()) updateTraitReverseSearchFeature();
}


void SettingsAction::setTraitAnalysisActive(bool active)
{
    if (!active && _traitWindowOpen) _traitWorkflow.cancel();
    _traitWindowOpen = active;
    _traitWorkflow.setActive(active || !_selectedGene.getString().isEmpty());
    if (_traitWorkflow.active) updateTraitReverseSearchFeature();
}

void SettingsAction::requestTraitForSelectedGene()
{
    if (_selectedGene.getString().isEmpty()) return;
    _traitWorkflow.setActive(true);
    updateTraitReverseSearchFeature();
}

void SettingsAction::setSpeciesDeActive(bool active)
{
    XSC_TRACE("SettingsAction::setSpeciesDeActive");
    _deWorkflow.setActive(active);
    if (active) {
        requestSpeciesDifferentialExpression();
    }
    else {
        if (_deTimer) _deTimer->stop();
        _deProgress.cancel();
    }
}

void SettingsAction::requestSpeciesDifferentialExpression()
{
    XSC_TRACE("SettingsAction::requestSpeciesDifferentialExpression");
    if (!_deWorkflow.active) {
        return;
    }
    if (_explorationSuspended) return;
    if (_traitWorkflow.busy()) {
        _deWorkflow.pending = true;
        if (!_deProgress.running()) _deProgress.begin(this, QStringLiteral("Differential expression"));
        _deProgress.state()->phase(QStringLiteral("Waiting for trait preparation"), 0);
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
    _deReadTiming.reset();
    const bool failed = _deReadCancel && !*_deReadCancel && _deJobWatcher.future().isCanceled();
    if (!failed && _deJobs && _deReadCancel && !*_deReadCancel) {
        for (auto& job : *_deJobs) {
            _deCache.moments[job.key] = std::move(job.entry);
        }
    }
    _deJobs.reset();
    _deJobDataset = Dataset<Points>();
    _deWorkflow.finish();
    _deWorkflow.pending = false;
    if ((_deReadCancel && *_deReadCancel) || !_deWorkflow.active) {
        if (!_explorationSuspended) prepareExplorationAnalysis();
        return;
    }
    if (failed) {
        _deProgress.cancel();
        showStatusMessage(QStringLiteral("DE data could not be read. Run Update to retry."), QStringLiteral("#dc3545"));
        if (_traitWorkflow.pending) { _traitWorkflow.pending = false; updateTraitReverseSearchFeature(); }
        return;
    }

    computeSpeciesDifferentialExpression(); // everything is cached now (or the selection changed meanwhile)
}

void SettingsAction::computeSpeciesDifferentialExpression()
{
    XSC_TRACE("SettingsAction::computeSpeciesDifferentialExpression");
    if (_explorationSuspended) return;
    if (!_deWorkflow.active) {
        return;
    }
    // The cache is being filled in the background: just remember that something changed.
    if (_deWorkflow.busy()) {
        _deWorkflow.pending = true;
        return;
    }
    if (_traitWorkflow.busy()) { _deWorkflow.pending = true; return; }

    const auto progress = _deProgress.running() ? _deProgress.state()
        : _deProgress.begin(this, QStringLiteral("Differential expression"));
    const auto finishWith = [this](const QString& message) {
        _deProgress.cancel();
        _dePublishedKey.clear();
        _deResult = SpeciesDeResult();
        _deResult.message = message;
        _deState = DeState();
        if (_speciesDeFinished) {
            _speciesDeFinished();
        }
        if (_traitWorkflow.pending) { _traitWorkflow.pending = false; updateTraitReverseSearchFeature(); }
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
            _deWorkflow.beginRead();

            const auto jobs = _deJobs;
            const auto cancel = _deReadCancel = _deWorkflow.cancellation();
            const Dataset<Points>* datasetHandle = &_deJobDataset;
            const int workers = parallelWorkers(largestPointSet);
            const std::size_t scratchBudgetBytes = static_cast<std::size_t>(std::max(64, _memoryBudgetMb.getValue())) << 20;
            std::vector<int> allGeneIndices(PointStreaming::usableGeneCount(geneCount, mainPointsDataset->getNumDimensions()));
            std::iota(allGeneIndices.begin(), allGeneIndices.end(), 0);

            showStatusMessage(QStringLiteral("Reading DE data"), QStringLiteral("#007bff"));

            // Build each species/cell-type row group once, then stream their union together.
            // Separate jobs reread/decompress the same matrix tiles for every pair.
            _deReadTiming = std::make_unique<xsc::timing::AsyncSpan>("de.read.total");
            _deJobWatcher.setFuture(QtConcurrent::run(&_explorationPool, [this, jobs, datasetHandle, workers, maxCells, geneCount, allGeneIndices, scratchBudgetBytes, cancel, progress]() {
            XSC_TRACE("de.read.worker");
                progress->phase(QStringLiteral("Reading differential expression means"), 0.02f);
                std::vector<std::vector<std::uint64_t>> groupedPoints;
                groupedPoints.reserve(jobs->size());
                std::vector<int> points;
                for (auto& job : *jobs) {
                    points = intersectSortedIndicesToInt(*job.speciesPoints, *job.cellTypePoints);

                    // Keep the existing deterministic, evenly spaced cell cap.
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
                        groupedPoints.emplace_back();
                        continue;
                    }
                    groupedPoints.emplace_back(points.begin(), points.end());
                    job.entry.means.assign(geneCount, 0.0f);
                    job.entry.detection.assign(geneCount, 0.0f);
                }

                const std::size_t groupStateBytes = jobs->size() * allGeneIndices.size()
                    * (sizeof(double) + sizeof(std::uint64_t) + 2 * sizeof(float)) + PointStreaming::kTargetReadBytes;
                const std::size_t groupIndexBytes = PointStreaming::groupedIndexMemoryBytes(groupedPoints);
                const bool groupedFitsBudget = groupIndexBytes <= scratchBudgetBytes
                    && groupStateBytes <= scratchBudgetBytes - groupIndexBytes;
                if (groupedFitsBudget) {
                    const auto grouped = PointStreaming::aggregateGroupedColumns(datasetHandle->get(), groupedPoints,
                        allGeneIndices, workers, scratchBudgetBytes, true,
                        [cancel, progress](std::size_t done, std::size_t total) {
                            if (*cancel) throw std::runtime_error("Cancelled");
                            progress->advance(done, total, 0.02f, 0.65f);
                        });
                    for (size_t jobIndex = 0; jobIndex < jobs->size(); ++jobIndex) {
                        auto& entry = (*jobs)[jobIndex].entry;
                        const std::uint64_t count = grouped.counts[jobIndex];
                        if (count == 0)
                            continue;
                        const double inverseCount = 1.0 / static_cast<double>(count);
                        for (size_t gene = 0; gene < allGeneIndices.size(); ++gene) {
                            entry.means[gene] = static_cast<float>(grouped.sums[jobIndex][gene] * inverseCount);
                            entry.detection[gene] = static_cast<float>(grouped.nonZero[jobIndex][gene] * inverseCount);
                        }
                    }
                }
                else {
                    // Very small memory budgets cannot hold the grouped index; keep the low-memory
                    // per-pair streaming path in that case.
                    groupedPoints.clear();
                    groupedPoints.shrink_to_fit();
                    QThreadPool pool;
                    pool.setMaxThreadCount(workers);
                    std::vector<size_t> jobIndices(jobs->size());
                    std::iota(jobIndices.begin(), jobIndices.end(), size_t(0));
                    QtConcurrent::blockingMap(&pool, jobIndices, [&](size_t& jobIndex) {
                        auto& job = (*jobs)[jobIndex];
                        std::vector<int> points = intersectSortedIndicesToInt(*job.speciesPoints, *job.cellTypePoints);
                        if (maxCells > 0 && static_cast<int>(points.size()) > maxCells) {
                            std::vector<int> sampled;
                            sampled.reserve(static_cast<size_t>(maxCells));
                            for (int i = 0; i < maxCells; ++i)
                                sampled.push_back(points[static_cast<size_t>((static_cast<long long>(i) * static_cast<long long>(points.size())) / maxCells)]);
                            points.swap(sampled);
                        }
                        if (points.empty())
                            return;
                        computeChunkedGeneMeansAndDetection(*datasetHandle, allGeneIndices, points,
                            [&job, cancel](int start, const std::vector<int>& chunkGenes, const std::vector<float>& means, const std::vector<float>& detection) {
                                if (*cancel) throw std::runtime_error("Cancelled");
                                for (size_t g = 0; g < chunkGenes.size(); ++g) {
                                    job.entry.means[static_cast<size_t>(start) + g] = means[g];
                                    job.entry.detection[static_cast<size_t>(start) + g] = detection[g];
                                }
                            });
                    });
                }
                QMetaObject::invokeMethod(this, [this, cancel]() { if (*cancel) return; showStatusMessage(QStringLiteral("DE data ready"), QStringLiteral("#007bff")); }, Qt::QueuedConnection);
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

    // The statistics are kept while the comparison stays the same (cells, cell types, cell cap, pooling, compared species).
    // Top-N, direction, rank and minimum detection only select genes from them: changing those is then instant.
    const QString statisticsKey = cacheKey + QLatin1Char('|') + cellTypes.join(QLatin1Char('\x1f')) + QLatin1Char('|')
        + (equalWeighting ? QStringLiteral("equal") : QStringLiteral("pooled")) + QLatin1Char('|') + QString::number(minCells) + QLatin1Char('|')
        + QStringList(state.species.begin(), state.species.end()).join(QLatin1Char('\x1f'));
    if (!_deStats || _deStats->key != statisticsKey) {
        auto fresh = std::make_shared<DeStatisticsCacheImpl>();
        const auto cancel = _deWorkflow.cancellation();
        _deWorkflow.beginCompute();
        disconnect(&_deStatisticsWatcher, nullptr, this, nullptr);
        connect(&_deStatisticsWatcher, &QFutureWatcher<void>::finished, this, [this, fresh, cancel]() {
            _deStatsTiming.reset();
            _deWorkflow.finish();
            if (*cancel || !_deWorkflow.active) {
                if (!_explorationSuspended) prepareExplorationAnalysis();
                return;
            }
            if (!*cancel && fresh->key.isEmpty()) {
                _deProgress.cancel();
                showStatusMessage(QStringLiteral("DE calculation could not finish. Run Update to retry."), QStringLiteral("#dc3545"));
                if (_traitWorkflow.pending) { _traitWorkflow.pending = false; updateTraitReverseSearchFeature(); }
                return;
            }
            if (!*cancel && !fresh->key.isEmpty()) _deStats = fresh;
            computeSpeciesDifferentialExpression();
        });
        _deStatsTiming = std::make_unique<xsc::timing::AsyncSpan>("de.statistics.total");
        _deStatisticsWatcher.setFuture(QtConcurrent::run(&_explorationPool, [fresh, snapshot = state, statisticsKey, cancel, progress]() {
            XSC_TRACE("de.statistics.worker");
            progress->phase(QStringLiteral("Computing differential expression tests"), 0.65f);
            std::vector<const std::vector<float>*> rows;
            for (const auto& row : snapshot.means) rows.push_back(&row);
            buildDeStatistics(rows, snapshot.detection, snapshot.totalSum, snapshot.totalSumSq, snapshot.totalDetection, *fresh, cancel);
            // Species details are cached by speciesDeDetail() when requested.
            if (!*cancel) fresh->key = statisticsKey;
        }));
        return;
    }

    const QString overviewKey = statisticsKey + QLatin1Char('|') + QString::number(static_cast<int>(direction))
        + QLatin1Char('|') + QString::number(static_cast<int>(rank)) + QLatin1Char('|') + QString::number(topCount)
        + QLatin1Char('|') + QString::number(minDetection, 'g', 17) + QLatin1Char('|') + QString::number(level, 'g', 17)
        + QLatin1Char('|') + QStringList(ordered.begin(), ordered.end()).join(QLatin1Char('\x1f'));
    if (!_deOverview || _deOverview->key != overviewKey) {
        auto batch = std::make_shared<DeOverviewBatch>();
        const auto cancel = _deWorkflow.cancellation();
        const auto statisticsSnapshot = _deStats;
        _deWorkflow.beginCompute();
        disconnect(&_deStatisticsWatcher, nullptr, this, nullptr);
        connect(&_deStatisticsWatcher, &QFutureWatcher<void>::finished, this, [this, batch, cancel]() {
            _deStatsTiming.reset();
            _deWorkflow.finish();
            if (*cancel || !_deWorkflow.active) {
                if (!_explorationSuspended) prepareExplorationAnalysis();
                return;
            }
            if (!*cancel && batch->key.isEmpty()) {
                _deProgress.cancel();
                showStatusMessage(QStringLiteral("DE overview could not finish. Run Update to retry."), QStringLiteral("#dc3545"));
                if (_traitWorkflow.pending) { _traitWorkflow.pending = false; updateTraitReverseSearchFeature(); }
                return;
            }
            if (!*cancel) _deOverview = batch;
            computeSpeciesDifferentialExpression();
        });
        _deStatsTiming = std::make_unique<xsc::timing::AsyncSpan>("de.statistics.total");
        _deStatisticsWatcher.setFuture(QtConcurrent::run(&_explorationPool, [batch, cancel, statisticsSnapshot, state, speciesCount,
            geneNames, cellTypes, ordered, direction, rank, topCount, minDetection, level, maxCells, minCells,
            equalWeighting, overviewKey, progress]() {
            XSC_TRACE("de.statistics.worker");
            progress->phase(QStringLiteral("Building differential expression overview"), 0.90f);
            if (*cancel) return;
            std::vector<SpeciesDeComparison> comparisons(speciesCount);
            std::vector<size_t> speciesIndices(speciesCount);
            std::iota(speciesIndices.begin(), speciesIndices.end(), static_cast<size_t>(0));
            QtConcurrent::blockingMap(speciesIndices, [&](size_t& s) {
                if (*cancel) return;
                const DeSpeciesStatistics& statistics = statisticsSnapshot->statistics[s];
                const std::vector<float>& fdr = statisticsSnapshot->fdr[s];
                const std::vector<float>& restDetection = statisticsSnapshot->restDetection[s];

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
                if (*cancel) return;
                progress->advance(column + 1, result.heatmapGenes.size(), 0.94f, 0.99f);
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

            if (*cancel) return;
            batch->result = std::move(result);
            batch->key = overviewKey;
        }));
        return;
    }
    if (_dePublishedKey == overviewKey) {
        _deProgress.finish();
        if (_traitWorkflow.pending) { _traitWorkflow.pending = false; updateTraitReverseSearchFeature(); }
        return;
    }
    _deState = std::move(state);
    _deResult = _deOverview->result;
    _dePublishedKey = overviewKey;
    showStatusMessage(QString("DE ready: %1 species, %2 genes").arg(speciesCount).arg(_deResult.heatmapGenes.size()), QStringLiteral("#28a745"));
    if (_speciesDeFinished) {
        _speciesDeFinished();
    }
    _deProgress.finish();
    if (_traitWorkflow.pending) { _traitWorkflow.pending = false; updateTraitReverseSearchFeature(); }
}

SpeciesDeDetail SettingsAction::computeSpeciesDeDetail(const QString& species)
{
    QStringList others;
    for (const auto& name : _deState.species) if (name != species) others << name;
    return computeSpeciesDeGroups({ species }, others);
}

SpeciesDeDetail SettingsAction::computeSpeciesDeGroups(const QStringList& groupA, const QStringList& groupB)
{
    XSC_TRACE("SettingsAction::computeSpeciesDeGroups");
    SpeciesDeDetail detail;
    if (groupA.isEmpty() || groupB.isEmpty()) { detail.message = "Select at least one species in each group."; return detail; }
    std::vector<size_t> a, group;
    const auto append = [&](const QStringList& names, std::vector<size_t>& indices) {
        for (const auto& name : names) {
            const auto it = std::find(_deState.species.begin(), _deState.species.end(), name);
            if (it == _deState.species.end()) return false;
            const size_t index = static_cast<size_t>(it - _deState.species.begin());
            if (std::find(indices.begin(), indices.end(), index) == indices.end()) indices.push_back(index);
        }
        return true;
    };
    for (const auto& name : groupA) if (groupB.contains(name)) { detail.message = "Groups must be disjoint; deselect shared species from one group."; return detail; }
    if (!append(groupA, a) || !append(groupB, group)) { detail.message = "A selected species has insufficient cells. Recompute and select available species."; return detail; }
    const size_t focus = a.front();
    const size_t geneCount = _deState.geneNames.size();
    detail.comparison = groupB;
    std::vector<const std::vector<float>*> meanPointers;
    for (const auto& row : _deState.means) meanPointers.push_back(&row);
    DeSpeciesStatistics computed;
    std::vector<float> computedFdr, restDetection(geneCount, 0.0f), focusDetection(geneCount, 0.0f);
    const bool cached = a.size() == 1 && group.size() + 1 == _deState.species.size() && _deStats;
    if (!cached) {
        computed = a.size() == 1 && group.size() >= 2 ? focusAgainstGroup(meanPointers, focus, group) : groupsAgainstGroups(meanPointers, a, group);
        computedFdr = benjaminiHochberg(computed.pValue);
    }
    if (cached) {
        // The details test uses a prior over ALL species, unlike the overview's
        // rest-only prior. Keep its historical statistics and cache them separately.
        const size_t count = _deState.species.size();
        if (_deStats->detailBuilt.size() != count) {
            _deStats->detailBuilt.assign(count, 0);
            _deStats->detailStatistics.resize(count); _deStats->detailFdr.resize(count);
        }
        if (!_deStats->detailBuilt[focus]) {
            _deStats->detailStatistics[focus] = focusAgainstGroup(meanPointers, focus, group);
            _deStats->detailFdr[focus] = benjaminiHochberg(_deStats->detailStatistics[focus].pValue);
            _deStats->detailBuilt[focus] = 1;
        }
    }
    const DeSpeciesStatistics& statistics = cached ? _deStats->detailStatistics[focus] : computed;
    const std::vector<float>& fdr = cached ? _deStats->detailFdr[focus] : computedFdr;
    for (size_t g = 0; g < geneCount; ++g) {
        for (const size_t c : group) restDetection[g] += _deState.detection[c][g] / static_cast<float>(group.size());
        for (const size_t c : a) focusDetection[g] += _deState.detection[c][g] / static_cast<float>(a.size());
    }
    // the same selection rules as in the overview
    const DeDirection direction = _deDirection.getCurrentIndex() == 1 ? DeDirection::Down : (_deDirection.getCurrentIndex() == 2 ? DeDirection::Either : DeDirection::Up);
    const DeRank rank = _deRankBy.getCurrentIndex() == 1 ? DeRank::ModeratedT : DeRank::Effect;
    const std::vector<size_t> top = selectTopDeGenes(statistics, fdr, focusDetection, restDetection, a.size() + group.size() == 2 ? 2.0 : significanceLevel(),
        static_cast<double>(std::max(0, _deMinDetection.getValue())) / 100.0, direction, rank, static_cast<size_t>(std::max(1, _deTopGenes.getValue())));

    detail.valid = true;
    detail.message = a.size() + group.size() == 2 ? QStringLiteral("One species in each group: effect sizes only; no significance evidence (p and FDR are 1).") : QStringLiteral("Equal species weights; moderated species-level comparison. Species relatedness is not corrected.");
    detail.species = groupA.join(QStringLiteral(" + "));
    detail.groupACount = static_cast<int>(a.size());
    detail.groupBCount = static_cast<int>(group.size());
    detail.genes.reserve(static_cast<int>(geneCount));
    for (const QString& gene : _deState.geneNames) {
        detail.genes << gene;
    }
    detail.delta = QVector<float>(statistics.delta.begin(), statistics.delta.end());
    detail.restMean = QVector<float>(statistics.restMean.begin(), statistics.restMean.end());
    detail.focusSd = QVector<float>(statistics.focusSd.begin(), statistics.focusSd.end());
    detail.restSd = QVector<float>(statistics.restSd.begin(), statistics.restSd.end());
    detail.t = QVector<float>(statistics.t.begin(), statistics.t.end());
    detail.pValue = QVector<float>(statistics.pValue.begin(), statistics.pValue.end());
    detail.fdr = QVector<float>(fdr.begin(), fdr.end());
    detail.detection = QVector<float>(focusDetection.begin(), focusDetection.end());
    detail.restDetection = QVector<float>(restDetection.begin(), restDetection.end());
    for (const size_t g : top) {
        detail.top << static_cast<int>(g);
    }
    return detail;
}
