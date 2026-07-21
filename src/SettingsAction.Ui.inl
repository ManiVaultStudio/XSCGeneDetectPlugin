SettingsAction::SettingsAction(XSCGeneDetectPlugin& XSCGeneDetectPlugin) :
    WidgetAction(&XSCGeneDetectPlugin, "XSCGeneDetectPlugin Settings"),
    _XSCGeneDetectPlugin(XSCGeneDetectPlugin),
    _listModel(this, "List Model"),
    _selectedGene(this, "Selected Gene"),
    _filteringEditTreeDataset(this, "Filtering Tree Dataset"),
    _selectedRowIndex(this, "Selected Row Index"),
    _optionSelectionAction(*this),
    _startComputationTriggerAction(this, "Update"),
    _referenceTreeDataset(this, "Reference Tree Dataset"),
    _mainPointsDataset(this, "Main Points Dataset"),
    _embeddingDataset(this, "Embedding Dataset"),
    //_hierarchyBottomClusterDataset(this, "Hierarchy Bottom Cluster Dataset"),
    //_hierarchyMiddleClusterDataset(this, "Hierarchy Middle Cluster Dataset"),
    //_hierarchyTopClusterDataset(this, "Hierarchy Top Cluster Dataset"),
    _speciesNamesDataset(this, "Species Names"),
    _bottomClusterNamesDataset(this, "Bottom Cluster Names"),
    _middleClusterNamesDataset(this, "Middle Cluster Names"),
    _topClusterNamesDataset(this, "Top Cluster Names"),
    //_calculationReferenceCluster(this, "Calculation Reference Cluster"),
    _filteredGeneNamesVariant(this, "Filtered Gene Names"),
    _topNGenesFilter(this, "Top N"),
    _geneNamesConnection(this, "Gene Names Connection"),
    _createRowMultiSelectTree(this, "Create Row MultiSelect Tree"),
    _performGeneTableTsneAction(this, "Perform Gene Table TSNE"),
    _tsnePerplexity(this, "TSNE Perplexity"),
    _hiddenShowncolumns(this, "Hidden Shown Columns"),
    _scatterplotReembedColorOption(this, "Embed Color"),
    _scatterplotEmbeddingPointsUMAPOption(this, "Embedding UMAP Points"),
    _selectedSpeciesVals(this, "Selected Species Vals"),
    _removeRowSelection(this, "DeSelect"),
    _revertRowSelectionChangesToInitial(this, "Revert"),
    _statusColorAction(this, "Status color"),
    _typeofTopNGenes(this, "N Type"),
    _usePreComputedTSNE(this, "Use Precomputed TSNE"),
    _speciesExplorerInMap(this, "Leaves Explorer Options"),
    _topHierarchyClusterNamesFrequencyInclusionList(this, "Top Hierarchy Cluster Names Frequency Inclusion List"),
    _speciesExplorerInMapTrigger(this, "Explore"),
    _applyLogTransformation(this, "Gene mapping log"),
    _clusterCountSortingType(this, "Cluster Count Sorting Type"),
    _currentCellSelectionClusterInfoLabel(nullptr),
    _performGeneTableTsnePerplexity(this, "Perform Gene Table TSNE Perplexity"),
    _performGeneTableTsneKnn(this, "Perform Gene Table TSNE Knn"),
    _performGeneTableTsneDistance(this, "Perform Gene Table TSNE Distance"),
    _performGeneTableTsneTrigger(this, "Perform Gene Table TSNE Trigger"),
    _computeTreesToDisplayFromHierarchy(this, "Compute Trees To Display From Hierarchy"),
    _clusterOrderHierarchy(this, "Cluster Order Hierarchy"),
    _rightClickedCluster(this, "Right Clicked Cluster"),
    _topSelectedHierarchyStatus(this, "Top Selected Hierarchy Status"),
    _clearRightClickedCluster(this, "Clear Right Clicked Cluster"),
    _toggleScatterplotSelection(this, "Show Scatterplot Selection"),
    _mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker(this, "Map For Hierarchy Items Change Method Stop For Project Load Blocker"),
    _saveSpeciesTable(this, "Save Left Gene Table"),
    _saveGeneTable(this, "Save Right Species Selection Table")
{
    _mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.setChecked(true);
    setSerializationName("CSCGDV:XSC Gene Detect Plugin Settings");
    _statusBarActionWidget = new QStatusBar();


    _popupMessageInit = new QMessageBox();
    _popupMessageInit->setIcon(QMessageBox::Information);
    _popupMessageInit->setWindowTitle("Computation in Progress");
    _popupMessageInit->setText("Data Precomputation in Progress");
    _popupMessageInit->setInformativeText(
        "The system is currently precomputing essential data to enhance your interactive exploration experience. "
        "This process may take some time based on the input data size, your memory and processor. <br> This popup will close automatically once initialization is complete. "
        "Thank you for using Cytosplore EvoViewer.<br><br>"
        "Please visit our website <a href=\"https://viewer.cytosplore.org/\">https://viewer.cytosplore.org/</a> to learn more."
    );
    _popupMessageInit->setTextFormat(Qt::RichText); // Enable rich text formatting
    _popupMessageInit->setTextInteractionFlags(Qt::TextBrowserInteraction); // Enable link interaction
    _popupMessageInit->setStandardButtons(QMessageBox::NoButton);
    _popupMessageInit->setModal(true);

    // Create a message box to notify the user about the completion of the tree creation process
    _popupMessageTreeCreationCompletion = new QMessageBox();
    _popupMessageTreeCreationCompletion->setIcon(QMessageBox::Information);
    _popupMessageTreeCreationCompletion->setWindowTitle("Tree Creation Completed");
    _popupMessageTreeCreationCompletion->setText(
        "The precomputed tree method has been completed. "
        "Right-click is available in the hierarchy view. "
        "To explore expression values across species for each cluster in the phylogenetic tree, "
    );
    _popupMessageTreeCreationCompletion->setInformativeText(
        "Right Click available in the cluster hierarchy view."
    );
    _popupMessageTreeCreationCompletion->setStandardButtons(QMessageBox::Ok);
    _popupMessageTreeCreationCompletion->setModal(false);
    /*
    _popupMessageInit = new QMessageBox();
    _popupMessageInit->setIcon(QMessageBox::Information);
    _popupMessageInit->setWindowTitle("Computation in Progress");
    _popupMessageInit->setText(
        "<div style='text-align: center;'>"
        "<h2 style='color: #333;'>Data Precomputation in Progress</h2>"
        "</div>"
    );
    _popupMessageInit->setInformativeText(
        "<div style='text-align: center;'>"
        "The system is currently precomputing essential data to enhance your interactive exploration experience. "
        "This process may take some time based on your data size and processor. The popup will close automatically "
        "once initialization is complete. Thank you for using Cytosplore EvoViewer."
        "</div>"
    );
    _popupMessageInit->setStandardButtons(QMessageBox::NoButton);
    _popupMessageInit->setModal(true);
    */

    _searchBox = new CustomLineEdit();
    QIcon searchIcon = mv::util::StyledIcon("search");
    QAction* searchAction = new QAction(_searchBox);
    searchAction->setIcon(searchIcon);
    _searchBox->addAction(searchAction, QLineEdit::LeadingPosition);
    _searchBox->setPlaceholderText("Search ID...");
    _searchBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _searchBox->setMaximumHeight(22);

    //_searchBox->setMinimumWidth(100);
    _searchBox->setMaximumWidth(800);
    _searchBox->setAutoFillBackground(true);
    _searchBox->setStyleSheet("QLineEdit { background-color: white; }");
    _searchBox->setClearButtonEnabled(false);
    _searchBox->setFocusPolicy(Qt::StrongFocus);
    _meanMapComputed = false;
    _statusBarActionWidget->setStatusTip("Status");
    _statusBarActionWidget->setMaximumHeight(22);
    //_statusBarActionWidget->setFixedWidth(120);
    //_statusBarActionWidget->setMinimumWidth(100);
    _statusBarActionWidget->setMaximumWidth(800);
    _statusBarActionWidget->setAutoFillBackground(true);
    _statusBarActionWidget->setSizeGripEnabled(false);

    _geneTableView = new QTableView();
    _selectionDetailsTable = new QTableView();

    QPalette palette = _geneTableView->palette();
    palette.setColor(QPalette::Base, Qt::white); // Background color
    palette.setColor(QPalette::Text, Qt::black); // Text color

    _geneTableView->setPalette(palette);
    _selectionDetailsTable->setPalette(palette);


    _splitter = new QHBoxLayout();
    _geneTableView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _geneTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    _geneTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    _geneTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _geneTableView->setAlternatingRowColors(false);
    _geneTableView->setSortingEnabled(true);
    _geneTableView->setShowGrid(true);
    _geneTableView->setGridStyle(Qt::SolidLine);
    _geneTableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _geneTableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _geneTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _geneTableView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _geneTableView->setCornerButtonEnabled(false);
    _geneTableView->setWordWrap(false);
    _geneTableView->setTabKeyNavigation(false);
    _geneTableView->setAcceptDrops(false);
    _geneTableView->setDropIndicatorShown(false);
    _geneTableView->setDragEnabled(false);
    _geneTableView->setDragDropMode(QAbstractItemView::NoDragDrop);
    _geneTableView->setDragDropOverwriteMode(false);
    _geneTableView->setAutoScroll(false);
    _geneTableView->setAutoScrollMargin(16);
    _geneTableView->setAutoFillBackground(true);
    _geneTableView->setFrameShape(QFrame::NoFrame);
    _geneTableView->setFrameShadow(QFrame::Plain);
    _geneTableView->setLineWidth(0);
    _geneTableView->setMidLineWidth(0);
    _geneTableView->setFocusPolicy(Qt::NoFocus);
    _geneTableView->setContextMenuPolicy(Qt::NoContextMenu);
    _geneTableView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _geneTableView->setMinimumSize(QSize(0, 0));
    _geneTableView->setMaximumSize(QSize(16777215, 16777215));
    _geneTableView->setBaseSize(QSize(0, 0));
    _geneTableView->setFocusPolicy(Qt::StrongFocus);
    _geneTableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    //only highlight multiple rows if shiuft is pressed
    _geneTableView->setSelectionBehavior(QAbstractItemView::SelectRows);

    // removeDatasets(groupIDDeletion);
     //removeDatasets(groupId1);
     //removeDatasets(groupId2);
     //removeDatasets(groupId3);

    _selectionDetailsTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _selectionDetailsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _selectionDetailsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    _selectionDetailsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _selectionDetailsTable->setAlternatingRowColors(false);
    _selectionDetailsTable->setSortingEnabled(true);
    _selectionDetailsTable->setShowGrid(true);
    _selectionDetailsTable->setGridStyle(Qt::SolidLine);
    _selectionDetailsTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _selectionDetailsTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _selectionDetailsTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _selectionDetailsTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _selectionDetailsTable->setCornerButtonEnabled(false);
    _selectionDetailsTable->setWordWrap(false);
    _selectionDetailsTable->setTabKeyNavigation(false);
    _selectionDetailsTable->setAcceptDrops(false);
    _selectionDetailsTable->setDropIndicatorShown(false);
    _selectionDetailsTable->setDragEnabled(false);
    _selectionDetailsTable->setDragDropMode(QAbstractItemView::NoDragDrop);
    _selectionDetailsTable->setDragDropOverwriteMode(false);
    _selectionDetailsTable->setAutoScroll(false);
    _selectionDetailsTable->setAutoScrollMargin(16);
    _selectionDetailsTable->setAutoFillBackground(true);
    _selectionDetailsTable->setFrameShape(QFrame::NoFrame);
    _selectionDetailsTable->setFrameShadow(QFrame::Plain);
    _selectionDetailsTable->setLineWidth(0);
    _selectionDetailsTable->setMidLineWidth(0);
    _selectionDetailsTable->setFocusPolicy(Qt::NoFocus);
    _selectionDetailsTable->setContextMenuPolicy(Qt::NoContextMenu);
    _selectionDetailsTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _selectionDetailsTable->setMinimumSize(QSize(0, 0));
    _selectionDetailsTable->setMaximumSize(QSize(16777215, 16777215));
    _selectionDetailsTable->setBaseSize(QSize(0, 0));
    _selectionDetailsTable->setFocusPolicy(Qt::StrongFocus);
    _selectionDetailsTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _selectionDetailsTable->setSelectionBehavior(QAbstractItemView::SelectRows);



    _selectedCellClusterInfoStatusBar = new mv::gui::FlowLayout();


    _listModel.setSerializationName("CSCGDV:List Model");
    _selectedGene.setSerializationName("CSCGDV:Selected Gene");
    _mainPointsDataset.setSerializationName("CSCGDV:Main Points Dataset");
    _embeddingDataset.setSerializationName("CSCGDV:Embedding Dataset");
    _speciesNamesDataset.setSerializationName("CSCGDV:Species Names Dataset");
    _bottomClusterNamesDataset.setSerializationName("CSCGDV:Cluster Names Dataset");
    _middleClusterNamesDataset.setSerializationName("CSCGDV:Middle Cluster Names Dataset");
    _topClusterNamesDataset.setSerializationName("CSCGDV:Top Cluster Names Dataset");
    _filteredGeneNamesVariant.setSerializationName("CSCGDV:Filtered Gene Names");
    _topNGenesFilter.setSerializationName("CSCGDV:Top N Genes Filter");
    _filteringEditTreeDataset.setSerializationName("CSCGDV:Filtering Tree Dataset");
    _referenceTreeDataset.setSerializationName("CSCGDV:Reference Tree Dataset");
    _selectedRowIndex.setSerializationName("CSCGDV:Selected Row Index");
    _geneNamesConnection.setSerializationName("CSCGDV:Gene Names Connection");
    _selectedSpeciesVals.setSerializationName("CSCGDV:Selected Species Vals");
    _clusterOrderHierarchy.setSerializationName("CSCGDV:Cluster Order Hierarchy");
    _rightClickedCluster.setSerializationName("CSCGDV:Right Clicked Cluster");
    _topSelectedHierarchyStatus.setSerializationName("CSCGDV:Top Selected Hierarchy Status");
    _clearRightClickedCluster.setSerializationName("CSCGDV:Clear Right Clicked Cluster");
    _removeRowSelection.setSerializationName("CSCGDV:Remove Row Selection");
    _removeRowSelection.setDisabled(true);
    _revertRowSelectionChangesToInitial.setSerializationName("CSCGDV:Revert Row Selection Changes To Initial");
    _revertRowSelectionChangesToInitial.setDisabled(true);
    _speciesExplorerInMapTrigger.setSerializationName("CSCGDV:Species Explorer In Map Trigger");
    _speciesExplorerInMapTrigger.setDisabled(true);
    _statusColorAction.setSerializationName("CSCGDV:Status Color");
    _selectedGene.setDisabled(true);
    _selectedGene.setString("");
    _createRowMultiSelectTree.setSerializationName("CSCGDV:Create Row MultiSelect Tree");
    _performGeneTableTsneAction.setSerializationName("CSCGDV:Perform Gene Table TSNE");
    _performGeneTableTsnePerplexity.setSerializationName("CSCGDV:Gene Table TSNE Perplexity");
    _performGeneTableTsnePerplexity.setMinimum(1);
    _performGeneTableTsnePerplexity.setMaximum(50);
    _performGeneTableTsnePerplexity.setValue(15);
    _performGeneTableTsneKnn.setSerializationName("CSCGDV:Gene Table TSNE Knn");
    _performGeneTableTsneKnn.initialize({ "FLANN","HNSW","ANNOY" }, "ANNOY");
    _performGeneTableTsneDistance.setSerializationName("CSCGDV:Gene Table TSNE Distance");
    _performGeneTableTsneDistance.initialize({ "Euclidean","Cosine","Inner Product","Manhattan","Hamming","Dot" }, "Dot");
    _performGeneTableTsneTrigger.setSerializationName("CSCGDV:Gene Table TSNE Trigger");
    _performGeneTableTsneTrigger.setDisabled(true);
    _clusterOrderHierarchy.setString("");
    _rightClickedCluster.setString("");
    _topSelectedHierarchyStatus.setString("");
    _tsnePerplexity.setSerializationName("CSCGDV:TSNE Perplexity");
    _tsnePerplexity.setMinimum(1);
    _tsnePerplexity.setMaximum(50);
    _tsnePerplexity.setValue(30);
    _usePreComputedTSNE.setSerializationName("CSCGDV:Use Precomputed TSNE");
    _usePreComputedTSNE.setChecked(true);
    _applyLogTransformation.setChecked(false);
    _toggleScatterplotSelection.setChecked(false);
    _performGeneTableTsneAction.setChecked(false);
    _hiddenShowncolumns.setSerializationName("CSCGDV:Hidden Shown Columns");
    _speciesExplorerInMap.setSerializationName("CSCGDV:Species Explorer In Map");
    _topHierarchyClusterNamesFrequencyInclusionList.setSerializationName("CSCGDV:Top Hierarchy Cluster Names Frequency Inclusion List");
    _scatterplotReembedColorOption.setSerializationName("CSCGDV:Scatterplot Reembedding Color Option");
    _scatterplotEmbeddingPointsUMAPOption.setSerializationName("CSCGDV:Scatterplot Embedding UMAP Points Option");
    _typeofTopNGenes.setSerializationName("CSCGDV:Type of Top N Genes");
    _clusterCountSortingType.setSerializationName("CSCGDV:Cluster Count Sorting Type");
    _applyLogTransformation.setSerializationName("CSCGDV:Apply Log Transformation");
    _createRowMultiSelectTree.setDisabled(true);
    _selectedRowIndex.setDisabled(true);
    _selectedRowIndex.setString("");
    _scatterplotReembedColorOption.initialize({ "Species","Cluster","Expression" }, "Species");
    _typeofTopNGenes.initialize({ "Absolute","Negative","Positive" }, "Positive");
    _clusterCountSortingType.initialize({ "Count","Name","Hierarchy View" }, "Count");
    _topNGenesFilter.setDefaultWidgetFlags(IntegralAction::WidgetFlag::SpinBox);

    QIcon updateIcon = mv::util::StyledIcon("play");
    _startComputationTriggerAction.setIcon(updateIcon);
    _startComputationTriggerAction.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);

    QIcon exploreIcon = mv::util::StyledIcon("wpexplorer");
    _speciesExplorerInMapTrigger.setIcon(exploreIcon);
    _speciesExplorerInMapTrigger.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);

    QIcon removeIcon = mv::util::StyledIcon("backspace");
    _removeRowSelection.setIcon(removeIcon);
    _removeRowSelection.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);


    QIcon saveGeneTableIcon = mv::util::StyledIcon("save");
    _saveGeneTable.setIcon(saveGeneTableIcon);
    _saveGeneTable.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);

    QIcon saveSpeciesTableIcon = mv::util::StyledIcon("save");
    _saveSpeciesTable.setIcon(saveSpeciesTableIcon);
    _saveSpeciesTable.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);

    QIcon revertIcon = mv::util::StyledIcon("undo");
    _revertRowSelectionChangesToInitial.setIcon(revertIcon);
    _revertRowSelectionChangesToInitial.setDefaultWidgetFlags(TriggerAction::WidgetFlag::IconText);

    _scatterplotEmbeddingPointsUMAPOption.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == PointType;
        });
    _filteringEditTreeDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == XSCTreeType;
        });
    _referenceTreeDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == XSCTreeType;
        });
    _mainPointsDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == PointType;
        });
    _speciesNamesDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == ClusterType;
        });
    _bottomClusterNamesDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == ClusterType;
        });
    _middleClusterNamesDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == ClusterType;
        });
    _topClusterNamesDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == ClusterType;
        });
    _embeddingDataset.setFilterFunction([this](mv::Dataset<DatasetImpl> dataset) -> bool {
        return dataset->getDataType() == PointType;
        });
    const auto changetooltipTopN = [this]() -> void
        {
            _topNGenesFilter.setToolTip("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setText("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setIconText("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setToolTip("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setObjectName("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setWhatsThis("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
        };
    connect(&_topNGenesFilter, &IntegralAction::valueChanged, this, changetooltipTopN);

    const auto updatespeciesExplorerInMap = [this]() -> void
        {

            QStringList leafValues = _speciesExplorerInMap.getSelectedOptions();


            removeSelectionTableRows(&leafValues);
            enableDisableButtonsAutomatically();
        };
    connect(&_speciesExplorerInMap, &OptionsAction::selectedOptionsChanged, this, updatespeciesExplorerInMap);

    const int delayMs = 500; // Delay in milliseconds
    /*
    QTimer* bottomTimer = new QTimer(this);
    bottomTimer->setSingleShot(true);
    const auto updateBottomHierarchyClusterNamesFrequencyInclusionList = [this, bottomTimer, delayMs]() -> void
    {
        bottomTimer->start(delayMs);
    };
    connect(bottomTimer, &QTimer::timeout, this, [this]() {
        //computeFrequencyMapForHierarchyItemsChange("bottom");
        computeHierarchyAppearanceVector("bottom");
        _statusColorAction.setString("M");
    });
    connect(&_bottomHierarchyClusterNamesFrequencyInclusionList, &OptionsAction::selectedOptionsChanged, this, updateBottomHierarchyClusterNamesFrequencyInclusionList);

    QTimer* middleTimer = new QTimer(this);
    middleTimer->setSingleShot(true);
    const auto updateMiddleHierarchyClusterNamesFrequencyInclusionList = [this, middleTimer, delayMs]() -> void
    {
        middleTimer->start(delayMs);
    };
    connect(middleTimer, &QTimer::timeout, this, [this]() {
        //computeFrequencyMapForHierarchyItemsChange("middle");
        computeHierarchyAppearanceVector("middle");
        _statusColorAction.setString("M");
    });
    connect(&_middleHierarchyClusterNamesFrequencyInclusionList, &OptionsAction::selectedOptionsChanged, this, updateMiddleHierarchyClusterNamesFrequencyInclusionList);
    */
    QTimer* topTimer = new QTimer(this);
    topTimer->setSingleShot(true);
    const auto updateTopHierarchyClusterNamesFrequencyInclusionList = [this, topTimer, delayMs]() -> void
        {
            topTimer->start(delayMs);
        };
    connect(topTimer, &QTimer::timeout, this, [this]() {
        //computeFrequencyMapForHierarchyItemsChange("top");
        _statusColorAction.setString("M");
        });
    connect(&_topHierarchyClusterNamesFrequencyInclusionList, &OptionsAction::selectedOptionsChanged, this, updateTopHierarchyClusterNamesFrequencyInclusionList);

    const auto updateGeneFilteringTrigger = [this]() -> void
        {
            disableActions();
            _pauseStatusUpdates = true;
            _speciesExplorerInMap.setSelectedOptions(QStringList{});
            //_erroredOutFlag = false;
            QApplication::processEvents();
            updateButtonTriggered();
            enableActions();

            QApplication::processEvents();
            auto pointsDataset = _mainPointsDataset.getCurrentDataset();
            pointsDataset->setSelectionIndices(_selectedIndicesFromStorage);
            _statusColorAction.setString("C");
            
        };
    connect(&_startComputationTriggerAction, &TriggerAction::triggered, this, updateGeneFilteringTrigger);
    const auto updateCreateRowMultiSelectTreeTrigger = [this]() -> void {

        if (_filteringEditTreeDataset.getCurrentDataset().isValid())
        {
            auto treeDataset = mv::data().getDataset<XSCTree>(_filteringEditTreeDataset.getCurrentDataset().getDatasetId());

            QStringList selectedRowsStrList = _geneNamesConnection.getString().split("*%$@*@$%*");


            if (treeDataset.isValid() && selectedRowsStrList.size() > 0)
            {
                //if (speciesSelectedIndicesCounter.size() > 0)
                {
                    //QJsonObject valueStringReference = createJsonTree(speciesSelectedIndicesCounter);
                    //if (!valueStringReference.isEmpty())
                    {
                        //treeDataset->setTreeData(valueStringReference);
                        //events().notifyDatasetDataChanged(treeDataset);
                        //TODO:: add the tree to the tree dataset
/*
                        QString treeData = createJsonTreeFromNewick(QString::fromStdString(modifiedNewick), leafnames);
                        if (!treeData.isEmpty())
                        {

                            QJsonObject valueStringReference = QJsonDocument::fromJson(treeData.toUtf8()).object();
                            if (!valueStringReference.isEmpty())
                            {
                                treeDataset->setTreeData(valueStringReference);
                                events().notifyDatasetDataChanged(treeDataset);
                            }
                        }
                        */

                    }
                }


            }




        }


        };

    connect(&_createRowMultiSelectTree, &TriggerAction::triggered, this, updateCreateRowMultiSelectTreeTrigger);

    const auto updateMainPointsDataset = [this]() -> void {

        if (!_meanMapComputed)
        {
            computeGeneMeanExpressionMap();
        }


        if (_mainPointsDataset.getCurrentDataset().isValid())
        {
            _totalGeneList.clear();
            auto fullDataset = mv::data().getDataset<Points>(_mainPointsDataset.getCurrentDataset().getDatasetId());
            auto dimensions = fullDataset->getNumDimensions();
            _totalGeneList = fullDataset->getDimensionNames();
            if (dimensions > 0) {
                _topNGenesFilter.setMinimum(1);
                _topNGenesFilter.setMaximum(dimensions);

                _topNGenesFilter.setValue(std::min(10, static_cast<int>(dimensions)));
            }
            else {
                _topNGenesFilter.setMinimum(0);
                _topNGenesFilter.setMaximum(0);
                _topNGenesFilter.setValue(0);
            }

            _topNGenesFilter.setToolTip("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setText("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setIconText("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setObjectName("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setWhatsThis("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            const auto mainSelectionChanged = [this]() -> void {
                _toggleScatterplotSelection.setChecked(true);
                };
            connect(&fullDataset, &Dataset<Points>::dataSelectionChanged, this, mainSelectionChanged);

        }
        else
        {
            _topNGenesFilter.setMinimum(0);
            _topNGenesFilter.setMaximum(0);
            _topNGenesFilter.setValue(0);
            _topNGenesFilter.setToolTip("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setText("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setIconText("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setObjectName("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");
            _topNGenesFilter.setWhatsThis("Top N genes: 0 to " + QString::number(_topNGenesFilter.getMaximum()) + " (Current: " + QString::number(_topNGenesFilter.getValue()) + ")");

        }

        };

    connect(&_mainPointsDataset, &DatasetPickerAction::currentIndexChanged, this, updateMainPointsDataset);


    const auto updateSpeciesNameDataset = [this]() -> void {
        _selectedSpeciesCellCountMap.clear();
        QStringList speciesOptions = {};
        if (_speciesNamesDataset.getCurrentDataset().isValid())
        {
            auto clusterFullDataset = mv::data().getDataset<Clusters>(_speciesNamesDataset.getCurrentDataset().getDatasetId());
            auto clusterValuesData = clusterFullDataset->getClusters();
            if (!clusterValuesData.isEmpty())
            {
                for (auto clusters : clusterValuesData)
                {
                    {
                        auto name = clusters.getName();
                        auto color = clusters.getColor();
                        _selectedSpeciesCellCountMap[name].color = color;
                        _selectedSpeciesCellCountMap[name].selectedCellsCount = 0;
                        _selectedSpeciesCellCountMap[name].nonSelectedCellsCount = 0;
                        speciesOptions.append(name);
                    }

                }

            }

        }
        _speciesExplorerInMap.setOptions(speciesOptions);

        if (!_meanMapComputed)
        {
            computeGeneMeanExpressionMap();
        }
        };

    connect(&_speciesNamesDataset, &DatasetPickerAction::currentIndexChanged, this, updateSpeciesNameDataset);
    const auto updateTopHierarchyDatasetChanged = [this]() -> void {

        if (_topClusterNamesDataset.getCurrentDataset().isValid())
        {

            auto clusterFullDataset = mv::data().getDataset<Clusters>(_topClusterNamesDataset.getCurrentDataset().getDatasetId());
            auto clusters = clusterFullDataset->getClusters();
            QStringList clusterNames = {};
            if (!clusters.isEmpty())
            {
                for (auto cluster : clusters)
                {
                    clusterNames.append(cluster.getName());
                }
            }
            _topHierarchyClusterNamesFrequencyInclusionList.setOptions(clusterNames);
            QString removalString = "Non-Neuronal";
            //if removal string present remove it
            if (clusterNames.contains(removalString))
            {
                clusterNames.removeAll(removalString);

            }
            _topHierarchyClusterNamesFrequencyInclusionList.setSelectedOptions(clusterNames);
        }
        else
        {
            _topHierarchyClusterNamesFrequencyInclusionList.setOptions({});
        }
        computeHierarchyAppearanceVector();

        };

    connect(&_topClusterNamesDataset, &DatasetPickerAction::currentIndexChanged, this, updateTopHierarchyDatasetChanged);

    const auto updateMiddleHierarchyDatasetChanged = [this]() -> void {


        };

    connect(&_middleClusterNamesDataset, &DatasetPickerAction::currentIndexChanged, this, updateMiddleHierarchyDatasetChanged);

    const auto updateBottomHierarchyDatasetChanged = [this]() -> void {


        };

    connect(&_bottomClusterNamesDataset, &DatasetPickerAction::currentIndexChanged, this, updateBottomHierarchyDatasetChanged);

    const auto updateScatterplotColor = [this]() -> void {
        auto selectedColorType = _scatterplotReembedColorOption.getCurrentText();
        if (selectedColorType != "")
        {
            auto scatterplotViewFactory = mv::plugins().getPluginFactory("Scatterplot View");
            mv::gui::DatasetPickerAction* colorDatasetPickerAction;
            mv::gui::DatasetPickerAction* pointDatasetPickerAction;
            mv::gui::ViewPluginSamplerAction* samplerActionAction;
            if (scatterplotViewFactory) {
                for (auto plugin : mv::plugins().getPluginsByFactory(scatterplotViewFactory)) {
                    if (plugin->getGuiName() == "Scatterplot Cell Selection Overview") {
                        pointDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Position"));
                        if (pointDatasetPickerAction) {


                            if (pointDatasetPickerAction->getCurrentDataset() == _selectedPointsTSNEDataset) {
                                colorDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Color"));
                                if (colorDatasetPickerAction)
                                {




                                    auto selectedColorType = _scatterplotReembedColorOption.getCurrentText();
                                    if (selectedColorType != "")
                                    {
                                        auto legendViewFactory = mv::plugins().getPluginFactory("ChartLegend View");
                                        DatasetPickerAction* legendDatasetPickerAction;
                                        StringAction* chartTitle;
                                        //ColorAction* selectionColor;
                                        //StringAction* selectionStringDelimiter;
                                        //StringAction* selectionClustersString;
                                        if (legendViewFactory)
                                        {
                                            for (auto legendPlugin : mv::plugins().getPluginsByFactory(legendViewFactory))
                                            {
                                                if (legendPlugin->getGuiName() == "Legend View")
                                                {
                                                    //legendPlugin->printChildren();
                                                    legendDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Cluster dataset"));
                                                    chartTitle = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Chart Title"));
                                                
                                                    //selectionColor = dynamic_cast<ColorAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Selection color"));
                                                    //selectionStringDelimiter = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Delimiter"));
                                                    //selectionClustersString = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Cluster Selection string"));
                                                }
                                            }
                                        }
                                        
                                         
                                        
                                        
                                        
                                        if (selectedColorType == "Cluster")
                                        {
                                            if (_bottomClusterNamesDataset.getCurrentDataset().isValid())
                                            {
                                                colorDatasetPickerAction->setCurrentText("");
                                                colorDatasetPickerAction->setCurrentDataset(_bottomClusterNamesDataset.getCurrentDataset());
                                                if (legendDatasetPickerAction)
                                                {
                                                    legendDatasetPickerAction->setCurrentDataset(_bottomClusterNamesDataset.getCurrentDataset());
                                                }
                                                if (chartTitle)
                                                {
                                                    chartTitle->setString("Cell types");
                                                }
                                                /*
                                                if (selectionColor)
                                                {
                                                    selectionColor->setColor(QColor(53, 126, 199));
                                                }
                                                
                                                if (selectionStringDelimiter)
                                                {
                                                    selectionStringDelimiter->setString(",");
                                                }
                                                
                                                if (selectionClustersString)
                                                {
                                                    selectionClustersString->setString(""); //TODO
                                                }
                                                */
                                            }
                                        }
                                        else if (selectedColorType == "Species")
                                        {
                                            if (_speciesNamesDataset.getCurrentDataset().isValid())
                                            {
                                                colorDatasetPickerAction->setCurrentText("");
                                                colorDatasetPickerAction->setCurrentDataset(_speciesNamesDataset.getCurrentDataset());
                                                if (legendDatasetPickerAction)
                                                {
                                                    legendDatasetPickerAction->setCurrentDataset(_speciesNamesDataset.getCurrentDataset());
                                                }
                                                if (chartTitle)
                                                {
                                                    chartTitle->setString("Species");
                                                }
                                               /*
                                                if (selectionColor)
                                                {
                                                    selectionColor->setColor(QColor(53, 126, 199));
                                                }
                                               
                                                if (selectionStringDelimiter)
                                                {
                                                    selectionStringDelimiter->setString(",");
                                                }
                                                
                                                if (selectionClustersString)
                                                {
                                                    selectionClustersString->setString(""); //TODO
                                                }
                                                */
                                            }
                                        }
                                        else if (selectedColorType == "Expression")
                                        {
                                            if (_tsneDatasetExpressionColors.isValid())
                                            {
                                                colorDatasetPickerAction->setCurrentText("");
                                                colorDatasetPickerAction->setCurrentDataset(_tsneDatasetExpressionColors);
                                                if (legendDatasetPickerAction)
                                                {
                                                    legendDatasetPickerAction->setCurrentDataset(_tsneDatasetExpressionColors);
                                                }
                                                if (chartTitle)
                                                {
                                                    chartTitle->setString("Gene expression");
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

        }

        };
    connect(&_scatterplotReembedColorOption, &OptionAction::currentIndexChanged, this, updateScatterplotColor);


    const auto updateStatus = [this]() -> void {
        if (_pauseStatusUpdates)
        {
            return;
        }
        auto string = _statusColorAction.getString();
        QString labelText = "";
        QString backgroundColor = "none";
        if (string == "C")
        {
            _startComputationTriggerAction.setDisabled(true);
            //if (_popupMessageInit->isVisible())
            //{
                //_popupMessageInit->hide();
            //}
        }
        else
        {
            _startComputationTriggerAction.setDisabled(false);
        }
        if (string == "M")
        {
            _removeRowSelection.trigger();
        }

        if (string == "C") {
            labelText = "Updated";
            backgroundColor = "#28a745"; // Green

        }
        else if (string == "M") {
            labelText = "Outdated";
            backgroundColor = "#ffc107"; // Gold

        }
        else if (string == "E") {
            labelText = "Error";
            backgroundColor = "#dc3545"; // Red
        }
        else if (string == "R")
        {
            labelText = "Processing";
            backgroundColor = "#007bff"; // Blue
        }
        else {
            labelText = "Unknown";
            backgroundColor = "#6c757d"; // Grey
        }




        // Update the _statusBarActionWidget with the new label text and background color
        _statusBarActionWidget->showMessage("Status: " + labelText);
        _statusBarActionWidget->setStyleSheet("QStatusBar{padding-left:8px;background:" + backgroundColor + ";color:white;}");


        };
    connect(&_statusColorAction, &StringAction::stringChanged, this, updateStatus);

    /*const auto updateSelectedCellClusterInfoBox = [this]() -> void {


        // Clear any previous message
        _selectedCellClusterInfoStatusBar->clearMessage();

        // Check if there's a previously added label and remove it
        if (_currentCellSelectionClusterInfoLabel != nullptr) {
            _selectedCellClusterInfoStatusBar->removeWidget(_currentCellSelectionClusterInfoLabel);
            delete _currentCellSelectionClusterInfoLabel; // Delete the previous label to avoid memory leaks
            _currentCellSelectionClusterInfoLabel = nullptr; // Reset the pointer to indicate there's no current label
        }

        // Create a new QLabel
        _currentCellSelectionClusterInfoLabel = new QLabel;
        auto string = _selectedCellClusterInfoBox.getString();
        QString htmlText = string;
        _currentCellSelectionClusterInfoLabel->setText(htmlText);
        _selectedCellClusterInfoStatusBar->addWidget(_currentCellSelectionClusterInfoLabel);


        QLayoutItem* layoutItem;

        while ((layoutItem = _selectedCellClusterInfoStatusBar->takeAt(0)) != nullptr) {
            delete layoutItem->widget();
            delete layoutItem;
        }

        for (cluster : clusters) {
            auto clusterLabel = new QLabel(parent, clusterName);
            clusterLabel->setStyleSheet("");
            _selectedCellClusterInfoStatusBar->addWidget(clusterLabel);
        }



        };*/

    const auto updateEmbeddingDataset = [this]() -> void {


        };
    connect(&_embeddingDataset, &DatasetPickerAction::currentIndexChanged, this, updateEmbeddingDataset);


    const auto updateTypeOfTopNGenesFilter = [this]() -> void {
        _statusColorAction.setString("M");

        };
    connect(&_typeofTopNGenes, &OptionAction::currentIndexChanged, this, updateTypeOfTopNGenesFilter);


    const auto updateClusterOrderHierarchy = [this]() -> void {

        _customOrderClustersFromHierarchy.clear();
        if (_clusterOrderHierarchy.getString() != "")
        {
            QStringList clusterOrderHierarchyList = _clusterOrderHierarchy.getString().split(" @%$,$%@ ");
            for (auto clusterOrderHierarchyItem : clusterOrderHierarchyList)
            {

                _customOrderClustersFromHierarchy.push_back(clusterOrderHierarchyItem);
            }
        }




        };
    connect(&_clusterOrderHierarchy, &StringAction::stringChanged, this, updateClusterOrderHierarchy);

    const auto updateRightClickedCluster = [this]() -> void {


        //qDebug() << "Cluster Name and Level: " << _rightClickedCluster.getString();
        QString orderedClusters = _rightClickedCluster.getString();
        auto geneName = _selectedGene.getString();
        if (orderedClusters == "" || geneName == "")
        {
            _clearRightClickedCluster.trigger();
            //qDebug() << "Strings Empty, orderedClusters, genename" << orderedClusters << geneName;
            return;
        }
        QStringList clusterNameAndLevel = orderedClusters.split(" @%$,$%@ ");
        if (clusterNameAndLevel.size() == 2)
        {
            QString clusterName = clusterNameAndLevel.at(0);
            QString clusterLevelTemp = clusterNameAndLevel.at(1);
            if (clusterName == "" || clusterLevelTemp == "")
            {
                _clearRightClickedCluster.trigger();
                //qDebug() << "Strings Empty clustername, clusterLevelTemp" << clusterName << clusterLevelTemp;
                return;
            }
            QString clusterLevel;
            if (clusterLevelTemp == "1")
            {
                clusterLevel = "top";
            }
            else if (clusterLevelTemp == "2")
            {
                clusterLevel = "middle";
            }
            else if (clusterLevelTemp == "3")
            {
                clusterLevel = "bottom";
            }
            else
            {

                _clearRightClickedCluster.trigger();
                //qDebug() << "Cluster Level not 1,2,3" << clusterLevelTemp;
                return;

            }

            //qDebug() << "Cluster Name: " << clusterName << " Cluster Level: " << clusterLevel;

            auto referenceTreeDataset = _referenceTreeDataset.getCurrentDataset();
            if (referenceTreeDataset.isValid()) {
                auto referenceTree = mv::data().getDataset<XSCTree>(referenceTreeDataset.getDatasetId());
                if (referenceTree.isValid()) {
                    QString speciesData = _precomputedTreesFromTheHierarchy[clusterLevel][clusterName][geneName];
                    QJsonObject speciesDataJson = QJsonDocument::fromJson(speciesData.toUtf8()).object();
                    //check if QJsonObject isValid
                    if (speciesDataJson.isEmpty())
                    {
                        _clearRightClickedCluster.trigger();
                        //qDebug() << "Species Data Json Empty";
                        return;
                    }
                    referenceTree->setTreeData(speciesDataJson);
                    events().notifyDatasetDataChanged(referenceTree);
                }
                else
                {
                    _clearRightClickedCluster.trigger();
                    qDebug() << "Reference Tree Invalid";
                    return;
                }

            }
            else
            {
                _clearRightClickedCluster.trigger();
                qDebug() << "Reference Tree Dataset Invalid";
                return;
            }

        }

        };
    connect(&_rightClickedCluster, &StringAction::stringChanged, this, updateRightClickedCluster);
    const auto updateTopSelectedHierarchyStatus = [this]() -> void {



        };
    connect(&_topSelectedHierarchyStatus, &StringAction::stringChanged, this, updateTopSelectedHierarchyStatus);

    const auto updateApplyLogTransformation = [this]() -> void {
        _statusColorAction.setString("M");

        };
    connect(&_applyLogTransformation, &ToggleAction::toggled, this, updateApplyLogTransformation);
    const auto updateMapForHierarchyItemsChangeMethodStopForProjectLoadBlocker = [this]() -> void {
        if (!_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker.isChecked())
        {
            // _startComputationTriggerAction.setDisabled(false);
             //computeFrequencyMapForHierarchyItemsChange("top");

             //_startComputationTriggerAction.trigger();

             //QFuture<void> future = QtConcurrent::run([this]() { computeFrequencyMapForHierarchyItemsChange("top"); });
            QFuture<void> future1 = QtConcurrent::run([this]() { computeGeneMeanExpressionMap(); });
            QFuture<void> future2 = QtConcurrent::run([this]() { computeHierarchyAppearanceVector(); });

            //future.waitForFinished();

            future1.waitForFinished();
            future2.waitForFinished();
            _startComputationTriggerAction.trigger();

            /*

            _popupMessageInit->show();
            QApplication::processEvents();
            try {
                QFuture<void> future1 = QtConcurrent::run([this]() { computeGeneMeanExpressionMap(); });
                QFuture<void> future2 = QtConcurrent::run([this]() { computeFrequencyMapForHierarchyItemsChange("top"); });
                future1.waitForFinished();
                future2.waitForFinished();
            }
            catch (const std::exception& e) {
                std::cerr << "Error during computation: " << e.what() << std::endl;
                _popupMessageInit->hide();
                QApplication::processEvents();
                return;
            }

            try {
                QFuture<void> future3 = QtConcurrent::run([this]() { precomputeTreesFromHierarchy(); });
                QFuture<void> future4 = QtConcurrent::run([this]() { _startComputationTriggerAction.trigger(); });
                //future3.waitForFinished();
                future4.waitForFinished();
            }
            catch (const std::exception& e) {
                std::cerr << "Error during tree precomputation: " << e.what() << std::endl;
                _popupMessageInit->hide();
                QApplication::processEvents();
                return;
            }

            _popupMessageInit->hide();
            QApplication::processEvents();
            */
        }
        else
        {
            _startComputationTriggerAction.setDisabled(true);
        }

        };
    connect(&_mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker, &ToggleAction::toggled, this, updateMapForHierarchyItemsChangeMethodStopForProjectLoadBlocker);
    const auto updateToggleScatterplotSelection = [this]() -> void {

        auto scatterplotViewFactory = mv::plugins().getPluginFactory("Scatterplot View");
        mv::gui::DecimalAction* overlayopacityAction;
        mv::gui::DecimalAction* overlayscaleAction;

        if (scatterplotViewFactory) {
            for (auto plugin : mv::plugins().getPluginsByFactory(scatterplotViewFactory)) {
                if (plugin->getGuiName() == "Scatterplot Embedding View") {
                    
                    overlayopacityAction = dynamic_cast<DecimalAction*>(plugin->findChildByPath("Settings/Selection/Opacity"));
                    if (overlayopacityAction)
                    {
                        //qDebug() << "Overlay opacity action found";
                        if (_toggleScatterplotSelection.isChecked())
                        {
                            overlayopacityAction->setValue(100.0);
                        }
                        else
                        {
                            overlayopacityAction->setValue(0.0);
                        }
                    }
                    overlayscaleAction = dynamic_cast<DecimalAction*>(plugin->findChildByPath("Settings/Selection/Scale"));
                    if (overlayscaleAction)
                    {
                        //qDebug() << "Overlay opacity action found";
                        if (_toggleScatterplotSelection.isChecked())
                        {
                            overlayscaleAction->setValue(200.0);
                        }
                        else
                        {
                            overlayscaleAction->setValue(100.0);
                        }
                    }

                }
            }
        }

        };
    connect(&_toggleScatterplotSelection, &ToggleAction::toggled, this, updateToggleScatterplotSelection);


    const auto recomputeGeneTableTSNE = [this]() -> void {
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
                        QApplication::processEvents();
                    }
                }

            }


            auto startAction = dynamic_cast<TriggerAction*>(_selectedPointsTSNEDatasetForGeneTable->findChildByPath("TSNE/TsneComputationAction/Start"));
            if (startAction) {

                startAction->trigger();
            }

        }

        };
    connect(&_performGeneTableTsneTrigger, &TriggerAction::triggered, this, recomputeGeneTableTSNE);

    const auto triggerSaveGeneTable = [this]() -> void {

        exportTableViewToCSVPerGene(_geneTableView);

        /*if (_selectedGene.getString() != "")
        {
            exportTableViewToCSVPerGene(_geneTableView);
        }
        else
        {
            exportTableViewToCSV(_geneTableView);_selectionDetailsTable
        }*/

        };
    connect(&_saveGeneTable, &TriggerAction::triggered, this, triggerSaveGeneTable);

    const auto triggerSaveSpeciesTable = [this]() -> void {


        exportTableViewToCSVForGenes(_geneTableView);
        

        };
    connect(&_saveSpeciesTable, &TriggerAction::triggered, this, triggerSaveSpeciesTable);
    
    
    const auto updateComputeTreesToDisplayFromHierarchy = [this]() -> void {

        _computeTreesToDisplayFromHierarchy.setDisabled(true);
        precomputeTreesFromHierarchy();
        _computeTreesToDisplayFromHierarchy.setDisabled(false);

        };

    connect(&_computeTreesToDisplayFromHierarchy, &TriggerAction::triggered, this, updateComputeTreesToDisplayFromHierarchy);

    const auto updateGeneTableTSNECheck = [this]() -> void {
        _statusColorAction.setString("M");

        };
    connect(&_performGeneTableTsneAction, &ToggleAction::toggled, this, updateGeneTableTSNECheck);
    const auto updateClusterCountSortingType = [this]() -> void {
        updateClusterInfoStatusBar();

        };
    connect(&_clusterCountSortingType, &OptionAction::currentIndexChanged, this, updateClusterCountSortingType);
    QTimer* debounceTimer = new QTimer(this);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(500); // 500 milliseconds wait time

    const auto debouncelambda = [this]() -> void { // Capture debounceTimer by 
        _statusColorAction.setString("M");
        disableActions();

        findTopNGenesPerCluster();
        if (_projectOpened)
        {
            _statusColorAction.setString("C");
        }
      
        enableActions();
        };

    connect(debounceTimer, &QTimer::timeout, this, debouncelambda);

    const auto updateTopGenesSlider = [this, debounceTimer]() -> void { // Capture debounceTimer by reference
        //wait to see if any more updates are coming then call findTopNGenesPerCluster();
        // Restart the timer every time the value changes
        debounceTimer->start();
        };
    connect(&_topNGenesFilter, &IntegralAction::valueChanged, this, updateTopGenesSlider);


    
}
