#pragma once
#include <actions/WidgetAction.h>
#include <actions/IntegralAction.h>
#include <actions/OptionAction.h>
#include <actions/OptionsAction.h>
#include <actions/ToggleAction.h>
#include "actions/DatasetPickerAction.h"
#include "PointData/PointData.h"
#include "ClusterData/ClusterData.h"
#include "event/EventListener.h"
#include "actions/Actions.h"
#include "Plugin.h"
#include "DataHierarchyItem.h"
#include "Set.h"
#include <AnalysisPlugin.h>
#include <memory>
#include <algorithm>    
#include <QDebug>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QGridLayout>
#include <QFormLayout>
#include <QString>
#include <string>
#include <event/Event.h>
#include <QDebug>
#include <QLabel>
#include <string>
#include "actions/VariantAction.h"
#include "actions/GroupAction.h"
#include "QStatusBar"
#include <widgets/FlowLayout.h>
#include <QSplitter>
#include <QTableView>
#include <QListView>
#include <vector>
#include <set>
#include <QLineEdit>
#include <QColor>
#include <QMessageBox>

#include <QMouseEvent>
using namespace mv::gui;
class QMenu;
class CrossSpeciesComparisonGeneDetectPlugin;

class FetchMetaData;
namespace mv
{
    class CoreInterface;
}

class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags())
        : QLabel(parent, f) {
    }

    ~ClickableLabel() override = default;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        Q_UNUSED(event);
        emit clicked();
    }

signals:
    void clicked();
};

class CustomLineEdit : public QLineEdit {
    Q_OBJECT

public:
    explicit CustomLineEdit(QWidget* parent = nullptr) : QLineEdit(parent) {
        QIcon defocusIcon = mv::util::StyledIcon("times-circle");
        _action = addAction(defocusIcon, QLineEdit::TrailingPosition);
        _action->setVisible(false); // Initially hide the action icon

        connect(_action, &QAction::triggered, this, &CustomLineEdit::defocusLineEdit);
    }

signals:
    void textboxSelectedForTyping();
    void textboxDeselectedNotTypingAnymore();
    //void textValueChanged();

protected:
    void focusInEvent(QFocusEvent* e) override {
        _action->setVisible(true);
        emit textboxSelectedForTyping();
        QLineEdit::focusInEvent(e);
    }

    void focusOutEvent(QFocusEvent* e) override {
        // Only hide the defocus icon and emit the deselection signal if the text box is empty
        if (this->text().isEmpty()) {
            _action->setVisible(false);
            emit textboxDeselectedNotTypingAnymore();
        }
        QLineEdit::focusOutEvent(e);
    }

 


private slots:
    void defocusLineEdit() {
        this->blockSignals(true); // Block signals to prevent textChanged from being emitted
        this->clear();
        this->blockSignals(false); // Unblock signals

        this->clearFocus(); // Always clear focus when the defocus button is clicked
        // Optionally, clear the text here if needed
        //this->clear();
        _action->setVisible(false); // Hide the defocus icon
        emit textboxDeselectedNotTypingAnymore(); // Emit the deselection signal
    }

private:
    QAction* _action; // Action for the defocus icon
};




struct ClusterOrderContainer {
    int count;
    QColor color;
    QString name;
};


struct SpeciesDetailsStats {
    int rank;
    int abundanceTop;
    int abundanceMiddle;
    int countAbundanceNumerator;
    float meanSelected;
    int countSelected;
    float meanNonSelected;
    int countNonSelected;
    
    //float meanAll;
    //int countAll;

};

struct Stats {
    
    float meanSelected;
    int countSelected;
    float meanNonSelected;
    int countNonSelected;
    QColor color;
    int abundanceTop;
    int  abundanceMiddle;
    int countAbundanceNumerator;
    //float meanAll;
    //int countAll;

};

struct SpeciesColorCountStorageVals {
    QColor color;
    int selectedCellsCount;
    int nonSelectedCellsCount;
    int abundanceTop;
    int abundanceMiddle;
    int countAbundanceNumerator;
    //int allCellsCount;
};

struct StatisticsSingle {
    float meanVal;
    int countVal;

};

struct InitialStatistics {
    float meanVal;
    float differentialVal;
    int abundanceTop;
    int abundanceMiddle;
    int countAbundanceNumerator;
    int rankVal;
    QString geneName;
};

class SettingsAction : public WidgetAction
{
public:
    class OptionSelectionAction : public GroupAction
    {
    protected:
        class Widget : public mv::gui::WidgetActionWidget {
        public:
            Widget(QWidget* parent, OptionSelectionAction* optionSelectionAction);
        };

        QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override {
            return new OptionSelectionAction::Widget(parent, this);
        };

    public:
        OptionSelectionAction(SettingsAction& SettingsAction);

    protected:
        SettingsAction& _settingsAction;
    };



protected:

    class Widget : public mv::gui::WidgetActionWidget {
    public:
        Widget(QWidget* parent, SettingsAction* SettingsAction);
    };

    QWidget* getWidget(QWidget* parent, const std::int32_t& widgetFlags) override {
        return new SettingsAction::Widget(parent, this);
    };

public:
    SettingsAction(CrossSpeciesComparisonGeneDetectPlugin& CrossSpeciesComparisonGeneDetectPlugins);

public: // Action getters

    VariantAction& getListModelAction() { return _listModel; }
    StringAction& getSelectedGeneAction() { return _selectedGene; }
    StringAction&  getSelectedRowIndexAction() { return _selectedRowIndex; }
    DatasetPickerAction& getFilteringEditTreeDatasetAction() { return _filteringEditTreeDataset; }
    OptionSelectionAction& getOptionSelectionAction() { return _optionSelectionAction; }
    TriggerAction& getStartComputationTriggerAction() { return _startComputationTriggerAction; }
    DatasetPickerAction& getReferenceTreeDatasetAction() { return _referenceTreeDataset; }
    DatasetPickerAction& getMainPointsDataset() { return _mainPointsDataset; }
    DatasetPickerAction& getEmbeddingDataset() { return _embeddingDataset; }
    DatasetPickerAction& getSpeciesNamesDataset() { return _speciesNamesDataset; }
    DatasetPickerAction& getBottomClusterNamesDataset() { return _bottomClusterNamesDataset; }
    DatasetPickerAction& getMiddleClusterNamesDataset() { return _middleClusterNamesDataset; }
    DatasetPickerAction& getTopClusterNamesDataset() { return _topClusterNamesDataset; }
    VariantAction& getFilteredGeneNames() { return _filteredGeneNamesVariant; }
    IntegralAction& getTopNGenesFilter() { return _topNGenesFilter; }
    StringAction& getGeneNamesConnection() { return _geneNamesConnection; }
    TriggerAction& getCreateRowMultiSelectTree() { return _createRowMultiSelectTree; }
    ToggleAction& getPerformGeneTableTsneAction() { return _performGeneTableTsneAction; }
    IntegralAction& getTsnePerplexity() { return _tsnePerplexity; }
    OptionsAction& getHiddenShowncolumns() { return _hiddenShowncolumns; }
    DatasetPickerAction& getScatterplotEmbeddingPointsUMAPOption() { return _scatterplotEmbeddingPointsUMAPOption; }
    OptionAction& getScatterplotReembedColorOption() { return _scatterplotReembedColorOption; }
    StringAction& getSelctedSpeciesVals() { return _selectedSpeciesVals; }
    TriggerAction& getRemoveRowSelection() { return _removeRowSelection; }
    StringAction& getStatusColorAction() { return _statusColorAction; }
    OptionAction& getTypeofTopNGenes() { return _typeofTopNGenes; }
    ToggleAction& getUsePreComputedTSNE() { return _usePreComputedTSNE; }
    OptionsAction& getSpeciesExplorerInMap() { return _speciesExplorerInMap; }
    OptionsAction& getTopHierarchyClusterNamesFrequencyInclusionList() { return _topHierarchyClusterNamesFrequencyInclusionList; }
    TriggerAction& getSpeciesExplorerInMapTrigger() { return _speciesExplorerInMapTrigger; }
    TriggerAction& getRevertRowSelectionChangesToInitial() { return _revertRowSelectionChangesToInitial; }
    ToggleAction& getApplyLogTransformation() { return _applyLogTransformation; }
    ToggleAction& getToggleScatterplotSelection() { return _toggleScatterplotSelection; }
    OptionAction& getClusterCountSortingType() { return _clusterCountSortingType; }
    IntegralAction& getPerformGeneTableTsnePerplexity() { return _performGeneTableTsnePerplexity; }
    OptionAction& getPerformGeneTableTsneKnn() { return _performGeneTableTsneKnn; }
    OptionAction& getPerformGeneTableTsneDistance() { return _performGeneTableTsneDistance; }
    TriggerAction& getPerformGeneTableTsneTrigger() { return _performGeneTableTsneTrigger; }
    TriggerAction& getSaveGeneTable() { return _saveGeneTable; }
    TriggerAction& getSaveSpeciesTable() { return _saveSpeciesTable; }
    TriggerAction& getComputeTreesToDisplayFromHierarchy() { return _computeTreesToDisplayFromHierarchy; }
    StringAction& getClusterOrderHierarchy() { return _clusterOrderHierarchy; }
    ToggleAction& getMapForHierarchyItemsChangeMethodStopForProjectLoadBlocker() { return _mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker; }
    StringAction& getRightClickedCluster() { return _rightClickedCluster; }
    StringAction& getTopSelectedHierarchyStatus() { return _topSelectedHierarchyStatus; }
    TriggerAction& getClearRightClickedCluster() { return _clearRightClickedCluster; }

   mv::Dataset<Points>& getSelectedPointsTSNEDatasetForGeneTable() { return _selectedPointsTSNEDatasetForGeneTable; }

    QStringList   getCurrentHierarchyItemsMiddleForTable() { return _currentHierarchyItemsMiddleForTable; }
    //IntegralAction& setPerformGeneTableTsnePerplexity() { return _performGeneTableTsnePerplexity; }
    //tsne relatedDatasets
    /*
       mv::Dataset<Points>        _selectedPointsTSNEDataset;
   mv::Dataset<Points>        _selectedPointsDataset;
   mv::Dataset<Points>        _selectedPointsEmbeddingDataset;

   mv::Dataset<Clusters>        _tsneDatasetSpeciesColors;
   mv::Dataset<Clusters>        _tsneDatasetClusterColors;
   mv::Dataset<Points>        _tsneDatasetExpressionColors;
    */

   mv::Dataset<Points>& getSelectedPointsTSNEDataset() { return _selectedPointsTSNEDataset; }
   mv::Dataset<Points>& getSelectedPointsDataset() { return _selectedPointsDataset; }
   mv::Dataset<Points>& getSelectedPointsEmbeddingDataset() { return _selectedPointsEmbeddingDataset; }

   mv::Dataset<Clusters>& getTsneDatasetSpeciesColors() { return _tsneDatasetSpeciesColors; }
   mv::Dataset<Clusters>& getTsneDatasetClusterColors() { return _tsneDatasetClusterColors; }
   mv::Dataset<Points>& getTsneDatasetExpressionColors() { return _tsneDatasetExpressionColors; }
    std::vector<std::seed_seq::result_type>& getSelectedIndicesFromStorage() { return _selectedIndicesFromStorage; }
   mv::Dataset<Points> & getFilteredUMAPDatasetPoints() { return _filteredUMAPDatasetPoints; }
   mv::Dataset<Points> & getFilteredUMAPDatasetColors() { return _filteredUMAPDatasetColors; }
   mv::Dataset<Points> & getFilteredUMAPDatasetClusters() { return _filteredUMAPDatasetClusters; }
    QStatusBar* getStatusBarActionWidget() const { return _statusBarActionWidget; }
    QMessageBox* getPopupMessageInit() const { return _popupMessageInit; }
    QMessageBox* getPopupMessageTreeCreationCompletion() const { return _popupMessageTreeCreationCompletion; }
    QStringList& getInitColumnNames() { return _initColumnNames; }
    mv::gui::FlowLayout* getSelectedCellClusterInfoStatusBar() const { return _selectedCellClusterInfoStatusBar; }
    QTableView* getGeneTableView() const { return _geneTableView; }
    CustomLineEdit* getSearchBox() const { return _searchBox; }
    QTableView* getSelectionDetailsTable() const { return _selectionDetailsTable; }
    std::map<QString, SpeciesColorCountStorageVals> & getSelectedSpeciesCellCountMap() { return _selectedSpeciesCellCountMap; }
    QHBoxLayout* getTableSplitter() const { return _splitter; }
    std::vector<QString>& getCustomOrderClustersFromHierarchy() { return _customOrderClustersFromHierarchy; }
    std::unordered_map<QString, std::vector<QString>>& getClusterPositionMap() { return _clusterPositionMap; }
    std::unordered_map<QString,std::unordered_map<QString, std::unordered_map<QString, QString>>>& getPrecomputedTreesFromTheHierarchy() { return _precomputedTreesFromTheHierarchy; }
    bool& getProjectOpened() { return _projectOpened; }

    std::map<QString, std::map<QString, Stats>>& getClusterNameToGeneNameToExpressionValue() { return _clusterNameToGeneNameToExpressionValue; }
    std::unordered_map<QString, std::unordered_map<QString, int>>& getClusterSpeciesFrequencyMap() { return _clusterSpeciesFrequencyMap; }
    
    QSet<QString>& getUniqueReturnGeneList() { return _uniqueReturnGeneList; }
    std::vector<QString>& getTotalGeneList() { return _totalGeneList; }
   mv::Dataset<Points>& getGeneSimilarityPoints() { return _geneSimilarityPoints; }
    //std::vector<QString>& getGeneSimilarityClusters() { return _geneSimilarityClusters; }
   mv::Dataset<Clusters>& getGeneSimilarityClusterColoring() { return _geneSimilarityClusterColoring; }
    std::vector<QString>& getGeneOrder() { return _geneOrder; }
    bool& getPauseStatusUpdates() { return _pauseStatusUpdates; }
    bool& setPauseStatusUpdates(bool flag) { return _pauseStatusUpdates = flag; }
    //bool getErroredOutFlag() const { return _erroredOutFlag; }
    //bool setErrorOutFlag(bool flag) { return _erroredOutFlag = flag; }
    //void triggerTrippleHierarchyFrequencyChange();
    void computeGeneMeanExpressionMap();
    void precomputeTreesFromHierarchy();
    //void computeGeneMeanExpressionMapExperimental();
    void populatePointDataConcurrently(QString datasetId, const std::vector<float>& pointVector, int numPoints, int numDimensions, std::vector<QString> dimensionNames);
    void populatePointData(QString& datasetId, std::vector<float>& pointVector, int& numPoints, int& numDimensions, std::vector<QString>& dimensionNames);
    void populateClusterData(QString& datasetId, std::map<QString, std::pair<QColor, std::vector<int>>>& clusterMap);
    QStringList getSystemModeColor();
    double* condensedDistanceMatrix(const std::vector<float>& items);
    std::string mergeToNewick(int* merge, int numOfLeaves);
    QString createJsonTreeFromNewick(QString tree, std::vector<QString> leafNames, std::map <QString, Stats> speciesMeanValues);
    void disableActions();
    void enableActions();
    void removeSelectionTableRows(QStringList* selectedLeaves);
    void enableDisableButtonsAutomatically();
    void removeDatasets(int groupId);
    void clearTemporaryDatasetHandles();
    void updateClusterInfoStatusBar();
    QVariant createModelFromData(const std::map<QString, std::map<QString, Stats>>& map, const std::map<QString, std::vector<QString>>& geneCounter, const std::map<QString, std::vector<std::pair<QString, int>>>& rankingMap,const int& n);
    void findTopNGenesPerCluster();
    QString generateTooltip(const ViewPluginSamplerAction::SampleContext& toolTipContext, const QString& clusterDatasetId, bool showTooltip, QString indicesType);
    void createClusterPositionMap();
    //void computeGeneMeanExpressionMapForHierarchyItemsChangeExperimental(QString hierarchyType);
    void computeFrequencyMapForHierarchyItemsChange(QString hierarchyType);
    void computeHierarchyAppearanceVector();
    void exportTableViewToCSVForGenes(QTableView* tableView);
    void exportTableViewToCSVPerGene(QTableView* tableView);
    void clearTableSelection(QTableView* tableView);
private:
    
    void updateSelectedSpeciesCounts(QJsonObject& node, const std::map<QString, int>& speciesCountMap);
    void updateButtonTriggered();
    void setModifiedTriggeredData(QVariant geneListTable);
public: // Serialization

    /**
     * Load widget action from variant map
     * @param Variant map representation of the widget action
     */
    void fromVariantMap(const QVariantMap& variantMap) override;

    /**
     * Save widget action to variant map
     * @return Variant map representation of the widget action
     */
    QVariantMap toVariantMap() const override;

protected:
    CrossSpeciesComparisonGeneDetectPlugin& _crossSpeciesComparisonGeneDetectPlugin;
    VariantAction                 _listModel;
    StringAction                  _selectedGene;
    DatasetPickerAction          _filteringEditTreeDataset;
    StringAction                _selectedRowIndex;
    OptionSelectionAction         _optionSelectionAction;
    TriggerAction              _startComputationTriggerAction;
    DatasetPickerAction    _referenceTreeDataset;
    std::unordered_map<QString, std::unordered_map<QString,std::pair<int,float>>> _clusterGeneMeanExpressionMap;

    std::unordered_map<QString,  std::unordered_map<QString, int>> _clusterSpeciesFrequencyMap;
    DatasetPickerAction    _mainPointsDataset;
    DatasetPickerAction    _speciesNamesDataset;
    DatasetPickerAction    _bottomClusterNamesDataset;
    DatasetPickerAction    _middleClusterNamesDataset;
    DatasetPickerAction    _topClusterNamesDataset;
    DatasetPickerAction    _embeddingDataset;
    std::map<QString, std::map<QString, Stats>> _clusterNameToGeneNameToExpressionValue;
    VariantAction           _filteredGeneNamesVariant;
    IntegralAction          _topNGenesFilter;
    StringAction           _geneNamesConnection;
    TriggerAction         _createRowMultiSelectTree;
    ToggleAction            _performGeneTableTsneAction;
    IntegralAction         _tsnePerplexity;
    OptionsAction          _hiddenShowncolumns;
    StringAction            _selectedSpeciesVals;
    OptionAction                    _typeofTopNGenes;

   mv::Dataset<Points>        _selectedPointsTSNEDataset;
   mv::Dataset<Points>        _selectedPointsDataset;
   mv::Dataset<Points>        _selectedPointsEmbeddingDataset;
   mv::Dataset<Points>        _filteredUMAPDatasetPoints;
   mv::Dataset<Points>        _filteredUMAPDatasetColors;
   mv::Dataset<Points>        _filteredUMAPDatasetClusters;

   mv::Dataset<Clusters>        _tsneDatasetSpeciesColors;
   mv::Dataset<Clusters>        _tsneDatasetClusterColors;  
   mv::Dataset<Points>        _tsneDatasetExpressionColors;

   mv::Dataset<Points>             _geneSimilarityPoints;
    //std::vector<QString>        _geneSimilarityClusters;
   mv::Dataset<Clusters>           _geneSimilarityClusterColoring;

    TriggerAction          _removeRowSelection;
    TriggerAction           _revertRowSelectionChangesToInitial;
    DatasetPickerAction           _scatterplotEmbeddingPointsUMAPOption;
    OptionAction           _scatterplotReembedColorOption;
    StringAction    _statusColorAction;
    std::vector<std::seed_seq::result_type> _selectedIndicesFromStorage;
    QStatusBar*                     _statusBarActionWidget;
    QMessageBox* _popupMessageInit;
    QMessageBox* _popupMessageTreeCreationCompletion;
    mv::gui::FlowLayout*            _selectedCellClusterInfoStatusBar;
    //mv::gui::FlowLayout     _clustersLayout;
    QStringList _initColumnNames;
    ToggleAction                  _usePreComputedTSNE;
    QLabel* _currentCellSelectionClusterInfoLabel;
    std::map<QString, SpeciesColorCountStorageVals>       _selectedSpeciesCellCountMap;
    OptionsAction                               _speciesExplorerInMap;
    TriggerAction                               _speciesExplorerInMapTrigger;
    QTableView* _geneTableView;                /** Table view for the data */
    QTableView* _selectionDetailsTable;    /** Table view for the selection details */
    QHBoxLayout* _splitter;
    CustomLineEdit* _searchBox;
    ToggleAction    _applyLogTransformation;
    ToggleAction    _toggleScatterplotSelection;
    //bool _erroredOutFlag;
    bool _meanMapComputed;
    OptionAction                _clusterCountSortingType;

    std::vector<QString> _customOrderClustersFromHierarchy;
    std::unordered_map<QString, int> _customOrderClustersFromHierarchyMap;
    std::vector<QString>     _totalGeneList;
    QSet<QString>               _uniqueReturnGeneList;
    IntegralAction                _performGeneTableTsnePerplexity;
    OptionsAction                 _topHierarchyClusterNamesFrequencyInclusionList;
    OptionAction                   _performGeneTableTsneKnn;
    OptionAction                   _performGeneTableTsneDistance;
    TriggerAction                  _performGeneTableTsneTrigger;
    TriggerAction                 _computeTreesToDisplayFromHierarchy;
   mv::Dataset<Points>                _selectedPointsTSNEDatasetForGeneTable;
    bool                           _pauseStatusUpdates=false;
    QStringList                   _deleteDatasetIds;
    std::vector<QString> _geneOrder;
    StringAction             _clusterOrderHierarchy;
    std::unordered_map<QString, std::vector<QString>> _clusterPositionMap;
    std::unordered_map<QString, std::unordered_map<QString, std::unordered_map<QString, QString>>>  _precomputedTreesFromTheHierarchy;
    ToggleAction _mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker;
    //std::vector<QString> _speciesOrder;
    StringAction              _rightClickedCluster;
    TriggerAction              _clearRightClickedCluster;
    std::map<QString, std::vector<bool>> _topHierarchyClusterMap;
    TriggerAction                 _saveGeneTable;
    TriggerAction                 _saveSpeciesTable;
    QStringList                   _currentHierarchyItemsMiddleForTable;
    StringAction               _topSelectedHierarchyStatus;
    bool                      _projectOpened=false;
};
