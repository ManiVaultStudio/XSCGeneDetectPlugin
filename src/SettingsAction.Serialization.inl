inline SettingsAction::OptionSelectionAction::OptionSelectionAction(SettingsAction& SettingsAction) :
    GroupAction(nullptr, "XSCGeneDetectPluginOptionSelectionAction"),
    _settingsAction(SettingsAction)
{
    setText("Options");
    setIcon(mv::util::StyledIcon("wrench"));
    //addAction(&_settingsAction.getTableModelAction());
    //addAction(&_settingsAction.getSelectedGeneAction());
    //addAction(&_settingsAction.getSelectedRowIndexAction());
    //addAction(&_settingsAction.getFilteringTreeDatasetAction());
    //addAction(&_settingsAction.getOptionSelectionAction());
    //addAction(&_settingsAction.getStartComputationTriggerAction());
    //addAction(&_settingsAction.getReferenceTreeDatasetAction());
    //addAction(&_settingsAction.getMainPointsDataset());
    //addAction(&_settingsAction.getHierarchyTopClusterDataset());
    //addAction(&_settingsAction.getHierarchyMiddleClusterDataset());
    //addAction(&_settingsAction.getHierarchyBottomClusterDataset());
    //addAction(&_settingsAction.getSpeciesNamesDataset());
    //addAction(&_settingsAction.getSelectedClusterNames());


}


void SettingsAction::fromVariantMap(const QVariantMap& variantMap)
{
    WidgetAction::fromVariantMap(variantMap);

    _geneNamesConnection.fromParentVariantMap(variantMap);
    _createRowMultiSelectTree.fromParentVariantMap(variantMap);
    _listModel.fromParentVariantMap(variantMap);
    _selectedGene.fromParentVariantMap(variantMap);
    _mainPointsDataset.fromParentVariantMap(variantMap);
    _embeddingDataset.fromParentVariantMap(variantMap);
    _speciesNamesDataset.fromParentVariantMap(variantMap);
    _bottomClusterNamesDataset.fromParentVariantMap(variantMap);
    _middleClusterNamesDataset.fromParentVariantMap(variantMap);
    _topClusterNamesDataset.fromParentVariantMap(variantMap);
    _filteredGeneNamesVariant.fromParentVariantMap(variantMap);
    _topNGenesFilter.fromParentVariantMap(variantMap);
    _filteringEditTreeDataset.fromParentVariantMap(variantMap);
    _referenceTreeDataset.fromParentVariantMap(variantMap);
    _selectedRowIndex.fromParentVariantMap(variantMap);
    _performGeneTableTsneAction.fromParentVariantMap(variantMap);
    _tsnePerplexity.fromParentVariantMap(variantMap);
    _performGeneTableTsnePerplexity.fromParentVariantMap(variantMap);
    _performGeneTableTsneKnn.fromParentVariantMap(variantMap);
    _performGeneTableTsneDistance.fromParentVariantMap(variantMap);
    _performGeneTableTsneTrigger.fromParentVariantMap(variantMap);
    _hiddenShowncolumns.fromParentVariantMap(variantMap);
    _speciesExplorerInMap.fromParentVariantMap(variantMap);
    _topHierarchyClusterNamesFrequencyInclusionList.fromParentVariantMap(variantMap);
    _scatterplotReembedColorOption.fromParentVariantMap(variantMap);
    _scatterplotEmbeddingPointsUMAPOption.fromParentVariantMap(variantMap);
    _selectedSpeciesVals.fromParentVariantMap(variantMap);
    _clusterOrderHierarchy.fromParentVariantMap(variantMap);
    _rightClickedCluster.fromParentVariantMap(variantMap);
    _topSelectedHierarchyStatus.fromParentVariantMap(variantMap);
    _clearRightClickedCluster.fromParentVariantMap(variantMap);
    _removeRowSelection.fromParentVariantMap(variantMap);
    _revertRowSelectionChangesToInitial.fromParentVariantMap(variantMap);
    _speciesExplorerInMapTrigger.fromParentVariantMap(variantMap);
    _statusColorAction.fromParentVariantMap(variantMap);
    _typeofTopNGenes.fromParentVariantMap(variantMap);
    _clusterCountSortingType.fromParentVariantMap(variantMap);
    _usePreComputedTSNE.fromParentVariantMap(variantMap);
    _applyLogTransformation.fromParentVariantMap(variantMap);

}

QVariantMap SettingsAction::toVariantMap() const
{
    QVariantMap variantMap = WidgetAction::toVariantMap();

    _geneNamesConnection.insertIntoVariantMap(variantMap);
    _createRowMultiSelectTree.insertIntoVariantMap(variantMap);
    _listModel.insertIntoVariantMap(variantMap);
    _selectedGene.insertIntoVariantMap(variantMap);
    _mainPointsDataset.insertIntoVariantMap(variantMap);
    _embeddingDataset.insertIntoVariantMap(variantMap);
    _speciesNamesDataset.insertIntoVariantMap(variantMap);
    _bottomClusterNamesDataset.insertIntoVariantMap(variantMap);
    _middleClusterNamesDataset.insertIntoVariantMap(variantMap);
    _topClusterNamesDataset.insertIntoVariantMap(variantMap);
    _filteredGeneNamesVariant.insertIntoVariantMap(variantMap);
    _topNGenesFilter.insertIntoVariantMap(variantMap);
    _filteringEditTreeDataset.insertIntoVariantMap(variantMap);
    _referenceTreeDataset.insertIntoVariantMap(variantMap);
    _selectedRowIndex.insertIntoVariantMap(variantMap);
    _performGeneTableTsneAction.insertIntoVariantMap(variantMap);
    _tsnePerplexity.insertIntoVariantMap(variantMap);
    _performGeneTableTsnePerplexity.insertIntoVariantMap(variantMap);
    _performGeneTableTsneDistance.insertIntoVariantMap(variantMap);
    _performGeneTableTsneKnn.insertIntoVariantMap(variantMap);
    _performGeneTableTsneTrigger.insertIntoVariantMap(variantMap);
    _hiddenShowncolumns.insertIntoVariantMap(variantMap);
    _speciesExplorerInMap.insertIntoVariantMap(variantMap);
    _topHierarchyClusterNamesFrequencyInclusionList.insertIntoVariantMap(variantMap);
    _scatterplotReembedColorOption.insertIntoVariantMap(variantMap);
    _scatterplotEmbeddingPointsUMAPOption.insertIntoVariantMap(variantMap);
    _selectedSpeciesVals.insertIntoVariantMap(variantMap);
    _clusterOrderHierarchy.insertIntoVariantMap(variantMap);
    _rightClickedCluster.insertIntoVariantMap(variantMap);
    _topSelectedHierarchyStatus.insertIntoVariantMap(variantMap);
    _clearRightClickedCluster.insertIntoVariantMap(variantMap);
    _removeRowSelection.insertIntoVariantMap(variantMap);
    _revertRowSelectionChangesToInitial.insertIntoVariantMap(variantMap);
    _speciesExplorerInMapTrigger.insertIntoVariantMap(variantMap);
    _statusColorAction.insertIntoVariantMap(variantMap);
    _typeofTopNGenes.insertIntoVariantMap(variantMap);
    _clusterCountSortingType.insertIntoVariantMap(variantMap);
    _usePreComputedTSNE.insertIntoVariantMap(variantMap);
    _applyLogTransformation.insertIntoVariantMap(variantMap);
    return variantMap;
}
