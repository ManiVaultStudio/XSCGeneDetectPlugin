#pragma once

#include "XSCDebug.h"
#include <actions/WidgetAction.h>
#include <actions/IntegralAction.h>
#include <actions/OptionAction.h>
#include <actions/OptionsAction.h>
#include <actions/ToggleAction.h>
#include "actions/DatasetPickerAction.h"
#include "PointData/PointData.h"
#include "ClusterData/ClusterData.h"
#include <XSCTreeData/XSCTreeData.h>
#include <XSCTreeMetaData/XSCTreeMetaData.h>
#include "event/EventListener.h"
#include "actions/Actions.h"
#include "Plugin.h"
#include "DataHierarchyItem.h"
#include "Set.h"
#include <AnalysisPlugin.h>
#include <QGuiApplication>
#include <memory>
#include <functional>
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
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <cmath>
#include <limits>
#include <numeric>
#include <QSet>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QColor>
#include <QMessageBox>
#include <QTimer>
#include <QFutureWatcher>
#include <memory>
#include <QPointer>

#include <QMouseEvent>
using namespace mv::gui;
class QMenu;
class XSCGeneDetectPlugin;

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
    int rank = 0;
    int abundanceTop = 0;
    int abundanceMiddle = 0;
    int countAbundanceNumerator = 0;
    float meanSelected = 0.0f;
    int countSelected = 0;
    float meanNonSelected = 0.0f;
    int countNonSelected = 0;

    //float meanAll;
    //int countAll;

};

struct Stats {

    float meanSelected = 0.0f;
    int countSelected = 0;
    float meanNonSelected = 0.0f;
    int countNonSelected = 0;
    QColor color;
    int abundanceTop = 0;
    int  abundanceMiddle = 0;
    int countAbundanceNumerator = 0;
    //float meanAll;
    //int countAll;

};

struct SpeciesColorCountStorageVals {
    QColor color;
    int selectedCellsCount = 0;
    int nonSelectedCellsCount = 0;
    int abundanceTop = 0;
    int abundanceMiddle = 0;
    int countAbundanceNumerator = 0;
    //int allCellsCount;
};

struct StatisticsSingle {
    float meanVal = 0.0f;
    int countVal = 0;

};

struct InitialStatistics {
    float meanVal = 0.0f;
    float differentialVal = 0.0f;
    int abundanceTop = 0;
    int abundanceMiddle = 0;
    int countAbundanceNumerator = 0;
    int rankVal = 0;
    QString geneName;
};

/** Column of the gene table that holds the trait correlation (see createModelFromData). */
inline constexpr int kTraitCorrelationColumn = 4;
/** Extra item roles of the trait correlation cell (DisplayRole/UserRole hold the correlation coefficient r). */
inline constexpr int kTraitPValueRole = Qt::UserRole + 1;   /** two-sided p-value of r */
inline constexpr int kTraitFdrRole = Qt::UserRole + 2;   /** Benjamini-Hochberg FDR (q-value) over all genes */
inline constexpr int kTraitStatsRole = Qt::UserRole + 3;   /** QVariantList: N species, CI low, CI high, leave-one-out min, max, overall r, phylogenetic r, p, FDR */
inline constexpr int kTraitSignificanceFdrRole = Qt::UserRole + 4; /** the FDR that decides "significant": phylogenetically corrected when a species tree is usable */
inline constexpr int kWeightedPValueRole = Qt::UserRole + 5;       /** weighted appearance column: p-value of the rank consistency (robust rank aggregation) */
inline constexpr int kWeightedFdrRole = Qt::UserRole + 6;       /** weighted appearance column: FDR of the rank consistency */
/** Column of the gene table that holds the weighted appearance (rank weighted number of species). */
inline constexpr int kWeightedAppearanceColumn = 5;
/** Further statistics columns of the gene table (hidden by default; shown through the column chooser). */
inline constexpr int kBestPositionColumn = 6;        /** best (smallest) position of the gene in the top-N ordering of any species */
inline constexpr int kMeanDifferentialColumn = 7;    /** mean differential expression over all species */
inline constexpr int kPositiveSpeciesColumn = 8;     /** percentage of species in which the differential expression is positive */
inline constexpr int kMeanExpressionColumn = 9;      /** mean expression in the selected cells, averaged over the species */

/** One gene of a species-versus-rest differential expression comparison. */
struct SpeciesDeGene {
    QString gene;
    int geneIndex = -1;          /** index of the gene in the dataset */
    float delta = 0.0f;          /** mean of the species - mean of the other species' means */
    float restMean = 0.0f;       /** mean of the other species' means */
    float restSd = 0.0f;         /** standard deviation of the other species' means */
    float t = 0.0f;              /** moderated t statistic */
    float pValue = 1.0f;
    float fdr = 1.0f;            /** Benjamini-Hochberg FDR over all genes of the comparison */
    float detection = 0.0f;      /** fraction of the species' cells in which the gene is detected */
    float restDetection = 0.0f;  /** the same, averaged over the other species */
};

/** The top genes of one species against all the others. */
struct SpeciesDeComparison {
    QString species;
    int cells = 0;                       /** cells of the species that entered (selected cell types, after the cell cap) */
    QVector<SpeciesDeGene> topGenes;     /** best genes first */
};

/** Result for all species: one comparison per species (each against the rest). */
struct SpeciesDeResult {
    bool valid = false;
    QString message;                         /** why there is no result, or what was computed */
    QStringList cellTypes;
    QStringList species;                     /** species in the order of the species tree (related species next to each other) */
    QVector<SpeciesDeComparison> comparisons;/** same order as \p species */
    QStringList heatmapGenes;                /** union of the top genes, grouped by the species they are top genes of */
    QVector<int> heatmapOwner;               /** index (into species) of the species a heatmap gene belongs to */
    QVector<QVector<float>> heatmapDelta;    /** [species][heatmap gene]: difference to the other species */
};

/** All genes of one species-versus-rest comparison (for the volcano plot). */
struct SpeciesDeDetail {
    bool valid = false;
    QString species;
    QStringList genes;
    QVector<float> delta, restMean, restSd, t, pValue, fdr, detection, restDetection;
    QVector<int> top;                        /** indices of the top genes, best first */
};

/** Small formulas shared by the tables, tooltips and exports. */
namespace GeneStats {

    /** log2(mean selected / mean non-selected); false when a mean is not positive (the ratio is undefined then). */
    inline bool log2FoldChange(double meanSelected, double meanNonSelected, double& result) {
        if (meanSelected <= 0.0 || meanNonSelected <= 0.0) {
            return false;
        }
        result = std::log2(meanSelected / meanNonSelected);
        return true;
    }

    /** part / whole in percent (0 when whole is 0). */
    inline double percent(double part, double whole) {
        return whole != 0.0 ? part / whole * 100.0 : 0.0;
    }

    /**
     * Position (1 = strongest) of a gene in the top-N ordering of a species, from the stored rank. The stored rank counts from
     * the other end for the "Negative" type (see findTopNGenesPerCluster). 0 when it cannot be determined.
     */
    inline int positionFromRank(int storedRank, int listLength, bool negativeType) {
        if (storedRank < 1 || listLength < 1) {
            return 0;
        }
        return negativeType ? listLength - storedRank + 1 : storedRank;
    }

    /** Weight of an appearance: 1 for position 1 down to 1/topN for position topN; 0 outside the top-N. */
    inline double rankWeight(int position, int topN) {
        if (topN <= 0 || position < 1 || position > topN) {
            return 0.0;
        }
        return static_cast<double>(topN - position + 1) / static_cast<double>(topN);
    }

} // namespace GeneStats

/** The data behind the last trait correlation: one row per species of the comparison (kept for the plot and the matrix export). */
struct TraitMatrix {
    std::vector<QString> species;                       /** species of the comparison */
    std::vector<double> trait;                          /** trait value per species */
    std::vector<std::vector<float>> expression;         /** [species][gene]: mean expression of the selected cell type(s) */
    std::vector<char> selected;                         /** species that are selected in the species explorer */
    std::vector<int> cells;                             /** cells per species that entered the mean (of the selected cell types) */
    std::unordered_map<QString, size_t> geneIndex;      /** gene name -> column of \p expression */
    QString traitName;
    bool empty() const { return species.empty(); }
};

/** Additional statistics of the trait correlation of one gene (NaN = not available). */
struct GeneTraitExtras {
    float ciLow = std::numeric_limits<float>::quiet_NaN();   /** lower end of the 95% confidence interval */
    float ciHigh = std::numeric_limits<float>::quiet_NaN();  /** upper end of the 95% confidence interval */
    float looMin = std::numeric_limits<float>::quiet_NaN();  /** smallest correlation when one species is left out */
    float looMax = std::numeric_limits<float>::quiet_NaN();  /** largest correlation when one species is left out */
    float overall = std::numeric_limits<float>::quiet_NaN(); /** correlation across all compared species (set when the cell shows the contribution of the selected species) */
    float phyloCorrelation = std::numeric_limits<float>::quiet_NaN(); /** phylogenetically corrected correlation (contrasts); NaN without a usable species tree */
    float phyloPValue = std::numeric_limits<float>::quiet_NaN();
    float phyloFdr = std::numeric_limits<float>::quiet_NaN();
    float significanceFdr = 1.0f;                            /** FDR that decides significance: phylogenetic when available, else the chosen method's */
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
    SettingsAction(XSCGeneDetectPlugin& XSCGeneDetectPlugins);
    ~SettingsAction() override;

public: // Action getters

    VariantAction& getListModelAction() { return _listModel; }
    StringAction& getSelectedGeneAction() { return _selectedGene; }
    StringAction& getSelectedRowIndexAction() { return _selectedRowIndex; }
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
    OptionAction& getTraitGeneScope() { return _traitGeneScope; }        /** correlate the listed genes only (fast) or all genes */
    DecimalAction& getTraitFdrThreshold() { return _traitFdrThreshold; }  /** FDR below which a trait correlation counts as significant */
    IntegralAction& getMaxWorkerThreads() { return _maxWorkerThreads; }    /** upper limit of parallel workers */
    IntegralAction& getMemoryBudgetMb() { return _memoryBudgetMb; }        /** scratch memory the parallel workers may use together */
    // ---- species differential expression (one species against the others) ----
    IntegralAction& getDeTopGenes() { return _deTopGenes; }
    OptionAction& getDeDirection() { return _deDirection; }
    OptionAction& getDeRankBy() { return _deRankBy; }
    IntegralAction& getDeMinDetection() { return _deMinDetection; }
    IntegralAction& getDeMaxCells() { return _deMaxCells; }
    IntegralAction& getDeMinCells() { return _deMinCells; }
    TriggerAction& getShowSpeciesDe() { return _showSpeciesDe; }

    /** The window is open: results are computed (and kept up to date). Closing it stops all work. */
    void setSpeciesDeActive(bool active);
    /** Recompute when something relevant changed (debounced; does nothing while the window is closed). */
    void requestSpeciesDifferentialExpression();
    const SpeciesDeResult& getSpeciesDeResult() const { return _deResult; }
    /** All genes of the comparison of \p species with the others (fast: derived from the cached species means). */
    SpeciesDeDetail computeSpeciesDeDetail(const QString& species);
    /** Called (on the interface thread) whenever a new result is available. */
    void setSpeciesDeFinishedCallback(std::function<void()> callback) { _speciesDeFinished = std::move(callback); }

    TriggerAction& getSaveTraitMatrix() { return _saveTraitMatrix; }   /** save the species x genes expression matrix with the trait */
    TriggerAction& getShowTraitPlot() { return _showTraitPlot; }       /** plot the expression of the selected gene against the trait */
    const TraitMatrix& getTraitMatrix() const { return _traitMatrix; }
    const std::unordered_map<QString, float>& getGeneTraitValues() const { return _geneTraitValuesComputed; }
    const std::unordered_map<QString, GeneTraitExtras>& getGeneTraitExtras() const { return _geneTraitExtras; }
    const std::unordered_map<QString, float>& getGeneTraitPValues() const { return _geneTraitPValues; }
    ToggleAction& getSaveIncludeParameters() { return _saveIncludeParameters; } /** write the analysis parameters as '#' comment lines on top of the CSV */
    TriggerAction& getComputeTreesToDisplayFromHierarchy() { return _computeTreesToDisplayFromHierarchy; }
    StringAction& getClusterOrderHierarchy() { return _clusterOrderHierarchy; }
    ToggleAction& getMapForHierarchyItemsChangeMethodStopForProjectLoadBlocker() { return _mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker; }
    StringAction& getRightClickedCluster() { return _rightClickedCluster; }
    StringAction& getTopSelectedHierarchyStatus() { return _topSelectedHierarchyStatus; }
    TriggerAction& getClearRightClickedCluster() { return _clearRightClickedCluster; }
    StringAction& getReverseSearchTraitSelectionString() { return _reverseSearchTraitSelectionString; }
    OptionAction& getTraitPoolingMode() { return _traitPoolingMode; }   /** how several selected cell types are combined */
    OptionAction& getTraitCorrelationMethod() { return _traitCorrelationMethod; } /** Pearson / Spearman / phylogenetic contrasts */
    OptionAction& getTraitSpeciesScope() { return _traitSpeciesScope; }          /** what the species selection means for the value */
    IntegralAction& getTraitMinCells() { return _traitMinCells; }         /** species with fewer cells of the cell type are ignored */
    ToggleAction& getTraitSignificantOnly() { return _traitSignificantOnly; } /** list only genes with FDR < 0.05 */

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
    mv::Dataset<Points>& getFilteredUMAPDatasetPoints() { return _filteredUMAPDatasetPoints; }
    mv::Dataset<Points>& getFilteredUMAPDatasetColors() { return _filteredUMAPDatasetColors; }
    mv::Dataset<Points>& getFilteredUMAPDatasetClusters() { return _filteredUMAPDatasetClusters; }
    QStatusBar* getStatusBarActionWidget() const { return _statusBarActionWidget; }
    QMessageBox* getPopupMessageInit() const { return _popupMessageInit; }
    QMessageBox* getPopupMessageTreeCreationCompletion() const { return _popupMessageTreeCreationCompletion; }
    QStringList& getInitColumnNames() { return _initColumnNames; }
    mv::gui::FlowLayout* getSelectedCellClusterInfoStatusBar() const { return _selectedCellClusterInfoStatusBar; }
    QTableView* getGeneTableView() const { return _geneTableView; }
    CustomLineEdit* getSearchBox() const { return _searchBox; }
    QTableView* getSelectionDetailsTable() const { return _selectionDetailsTable; }
    std::map<QString, SpeciesColorCountStorageVals>& getSelectedSpeciesCellCountMap() { return _selectedSpeciesCellCountMap; }
    QHBoxLayout* getTableSplitter() const { return _splitter; }
    std::vector<QString>& getCustomOrderClustersFromHierarchy() { return _customOrderClustersFromHierarchy; }
    std::unordered_map<QString, std::vector<QString>>& getClusterPositionMap() { return _clusterPositionMap; }
    std::unordered_map<QString, std::unordered_map<QString, std::unordered_map<QString, QString>>>& getPrecomputedTreesFromTheHierarchy() { return _precomputedTreesFromTheHierarchy; }
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
    QVariant createModelFromData(const std::map<QString, std::map<QString, Stats>>& map, const std::map<QString, std::vector<QString>>& geneCounter, const std::map<QString, std::vector<std::pair<QString, int>>>& rankingMap, const int& n);
    void findTopNGenesPerCluster();
    QString filterData(QString& target, const QString& filter, const QJsonObject& config);
    QString generateTooltip(const ViewPluginSamplerAction::SampleContext& toolTipContext, const QString& clusterDatasetId, bool showTooltip, QString indicesType);
    void createClusterPositionMap();
    //void computeGeneMeanExpressionMapForHierarchyItemsChangeExperimental(QString hierarchyType);
    void computeFrequencyMapForHierarchyItemsChange(QString hierarchyType);
    void computeHierarchyAppearanceVector();
    void exportTableViewToCSVForGenes(QTableView* tableView);
    void exportTableViewToCSVPerGene(QTableView* tableView);
    void exportTraitMatrixToCSV(QTableView* tableView);   /** species x genes matrix + trait of the listed genes */

    // ---- CSV export helpers (see SettingsAction_Data.inl) ----
    /** True when trait columns of \p model hold a computed result (otherwise they would only contain placeholder zeros). */
    bool traitColumnsAvailable(const QAbstractItemModel* model) const;
    /** Column names of the exported trait statistics; they describe what the values mean for the current method/scope. */
    QStringList traitExportHeaders() const;
    /** Values matching traitExportHeaders() for one row of the gene table. */
    QStringList traitExportValues(const QAbstractItemModel* model, int row) const;
    /** '#' comment lines that describe the analysis behind an exported table (cell types, species, trait, method, ...). */
    QStringList buildExportParameterLines(const QString& tableTitle, int rowCount, bool withTraitColumns, int significantRows);
    void clearTableSelection(QTableView* tableView);

    // ---- Trait correlation ---------------------------------------------------------------------------
    /**
     * Request a refresh of the "Trait Correlation" gene-table column.
     * The request is debounced: several requests inside one event-loop burst (species selection, trait
     * change, cluster click, gene click, ...) result in a single computation.
     */
    void updateTraitReverseSearchFeature();

    // ---- Species explorer selection ------------------------------------------------------------------
    /** Set the species-explorer selection, but only when it really differs (avoids duplicate change signals). */
    void setSpeciesExplorerSelection(const QStringList& species);

    /**
     * Species the explorer should show after a gene row was clicked: the species in which the gene appears.
     * (Kept as a function so the policy lives in one place.)
     * @param geneSpecies species in which the clicked gene appears
     */
    QStringList resolveExplorerSelectionForGene(const QStringList& geneSpecies);

    /** Must be called whenever the cell selection that feeds the tables changes (invalidates cached means). */
    void markSelectionChanged() { ++_selectionRevision; }

    /** Worker count for tasks that each need roughly \p pointsPerWorker x 256 floats of scratch memory (respects the settings). */
    int parallelWorkers(size_t pointsPerWorker) const;

    /** Number of genes ranked in \p species (length of its top-N ordering); 0 when unknown. */
    int speciesGeneListLength(const QString& species) const;
    int  weightedTopN() const { return _weightedTopN; }              /** top-N the tables were computed with */
    bool weightedNegative() const { return _weightedNegative; }      /** ... with the "Negative" type */

    /** FDR below which trait correlations and rank consistencies count as significant. */
    double significanceLevel() const { return static_cast<double>(_traitFdrThreshold.getValue()); }

    /** Remove the temporary datasets of the last update (embedding subsets, colors, filtered UMAP, gene similarity). */
    void removeTemporaryDatasets();

    /** Reset only the handles of the datasets that every update replaces (group 10); the reusable groups 20 and 30 keep theirs. */
    void clearUpdateDatasetHandles();

    /** Identifies the datasets the cached species means belong to (main points + species clusters). */
    QString currentMeanMapSignature() const;

private:
    // ---- Trait correlation internals -----------------------------------------------------------------
    /** Per (cluster, cell selection) cache so that changing only the trait / species never re-reads expression data. */
    struct TraitCache {
        /** Mean expression of one cell type in one species. */
        struct Entry {
            int cellCount = 0;               /** cells of the cell type in the species (0 -> means stays empty) */
            std::vector<float> means;        /** mean expression of every gene over those cells */
        };

        QString key;                                                        /** ids of the datasets the cache was built from */
        bool speciesBuilt = false;                                          /** speciesSorted is valid */
        std::unordered_map<QString, std::vector<int>> speciesSorted;        /** point indices per species (sorted) */
        std::unordered_map<QString, std::vector<int>> cellTypeSorted;       /** point indices per cell type (sorted), filled on first use */
        std::unordered_map<QString, Entry> entries;                         /** "species\x1fcellType" -> mean expression */
    };

    /** One (species, cell type) pair whose mean expression still has to be read. */
    struct TraitEntryJob {
        QString key;
        const std::vector<int>* speciesPoints = nullptr;
        const std::vector<int>* cellTypePoints = nullptr;
        TraitCache::Entry entry;
    };

    void onTraitJobsFinished();                /** the background reading is done: store the means and finish the computation */

    // ---- species differential expression: cache of the species means (per species and cell type), background job ----
    struct DeCache {
        struct Entry {
            int cellCount = 0;                   /** cells that were read (after the cell cap) */
            std::vector<float> means;            /** mean expression of every gene */
            std::vector<float> detection;        /** fraction of the cells in which every gene is detected */
        };
        QString key;
        bool sortedBuilt = false;
        std::unordered_map<QString, std::vector<int>> speciesSorted;
        std::unordered_map<QString, std::vector<int>> cellTypeSorted;
        std::unordered_map<QString, Entry> moments;
    };
    struct DeEntryJob {
        QString key;
        const std::vector<int>* speciesPoints = nullptr;
        const std::vector<int>* cellTypePoints = nullptr;
        DeCache::Entry entry;
    };
    /** The pooled per species values of the last computation (kept for the detail view). */
    struct DeState {
        std::vector<QString> species;
        std::vector<std::vector<float>> means;       // [species][gene]
        std::vector<std::vector<float>> detection;   // [species][gene]
        std::vector<int> cells;
        std::vector<double> totalSum, totalSumSq, totalDetection;
        std::vector<QString> geneNames;
    };
    void computeSpeciesDifferentialExpression();
    void onSpeciesDeJobsFinished();
    void applyClusterLabelSelectionStyles();   /** (re)style the cell-type labels of the status bar from _selectedClusterNames */

    void computeTraitCorrelation();            /** the actual (non-debounced) computation */
    void publishTraitCorrelationToModel();     /** write _geneTraitValuesComputed into the gene table model */


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
    XSCGeneDetectPlugin& _XSCGeneDetectPlugin;
    VariantAction                 _listModel;
    StringAction                  _selectedGene;
    DatasetPickerAction          _filteringEditTreeDataset;
    StringAction                _selectedRowIndex;
    OptionSelectionAction         _optionSelectionAction;
    TriggerAction              _startComputationTriggerAction;
    DatasetPickerAction    _referenceTreeDataset;
    std::unordered_map<QString, std::unordered_map<QString, std::pair<int, float>>> _clusterGeneMeanExpressionMap;

    std::unordered_map<QString, std::unordered_map<QString, int>> _clusterSpeciesFrequencyMap;
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
    std::unordered_map<QString, float> _geneTraitValuesComputed;   /** gene -> Pearson r */
    std::unordered_map<QString, float> _geneTraitPValues;          /** gene -> two-sided p-value */
    std::unordered_map<QString, float> _geneTraitFdrValues;        /** gene -> Benjamini-Hochberg FDR */
    std::unordered_map<QString, GeneTraitExtras> _geneTraitExtras; /** gene -> confidence interval, leave-one-out range */
    int     _traitSampleSize = 0;                                  /** species that entered the last computation */
    TraitMatrix _traitMatrix;                                      /** data of the last trait correlation */
    bool    _traitContributionShown = false;                       /** the trait column shows the contribution of selected species (not the correlation) */
    QString _traitValueLabel;                                      /** what the trait column holds, e.g. "Trait correlation (Pearson)" */
    QString _traitMethodNote;                                      /** description of the method used (for the tooltips) */
    QString _traitSignificanceBasis;                               /** which FDR decides "significant" (for tooltips and exports) */
    int     _weightedTopN = 0;                                     /** top-N value the weighted appearance column was computed with */
    bool    _weightedNegative = false;                             /** that computation used the "Negative" type (ranks count from the other end) */
    TriggerAction          _removeRowSelection;
    TriggerAction           _revertRowSelectionChangesToInitial;
    DatasetPickerAction           _scatterplotEmbeddingPointsUMAPOption;
    OptionAction           _scatterplotReembedColorOption;
    StringAction    _statusColorAction;
    std::vector<std::seed_seq::result_type> _selectedIndicesFromStorage;
    QStatusBar* _statusBarActionWidget = nullptr;
    QMessageBox* _popupMessageInit = nullptr;
    QMessageBox* _popupMessageTreeCreationCompletion = nullptr;
    mv::gui::FlowLayout* _selectedCellClusterInfoStatusBar = nullptr;
    //mv::gui::FlowLayout     _clustersLayout;
    QStringList _initColumnNames;
    ToggleAction                  _usePreComputedTSNE;
    QLabel* _currentCellSelectionClusterInfoLabel = nullptr;
    std::map<QString, SpeciesColorCountStorageVals>       _selectedSpeciesCellCountMap;
    OptionsAction                               _speciesExplorerInMap;
    TriggerAction                               _speciesExplorerInMapTrigger;
    QTableView* _geneTableView = nullptr;                /** Table view for the data */
    QTableView* _selectionDetailsTable = nullptr;    /** Table view for the selection details */
    QHBoxLayout* _splitter = nullptr;
    CustomLineEdit* _searchBox = nullptr;
    ToggleAction    _applyLogTransformation;
    ToggleAction    _toggleScatterplotSelection;
    //bool _erroredOutFlag;
    bool _meanMapComputed = false;
    QString _meanMapSignature;       /** datasets the cached species means were computed for */
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
    bool                           _pauseStatusUpdates = false;
    QStringList                   _deleteDatasetIds;
    std::vector<QString> _geneOrder;
    StringAction             _clusterOrderHierarchy;
    std::unordered_map<QString, std::vector<QString>> _clusterPositionMap;
    std::unordered_map<QString, std::unordered_map<QString, std::unordered_map<QString, QString>>>  _precomputedTreesFromTheHierarchy;
    ToggleAction _mapForHierarchyItemsChangeMethodStopForProjectLoadBlocker;
    //std::vector<QString> _speciesOrder;
    StringAction              _rightClickedCluster;
    TriggerAction              _clearRightClickedCluster;
    StringAction              _reverseSearchTraitSelectionString;
    OptionAction              _traitPoolingMode;      /** "Pool cells" (default) or "Equal cell-type weighting" */
    OptionAction              _traitCorrelationMethod;/** "Pearson" (default), "Spearman (rank)" or "Phylogenetic contrasts" */
    OptionAction              _traitSpeciesScope;     /** "Contribution of selected species" (default) or "Across selected species only" */
    IntegralAction            _traitMinCells;         /** minimum cells of a cell type in a species for the species to take part */
    ToggleAction              _traitSignificantOnly;  /** gene table: only genes with a significant trait correlation */
    OptionAction              _traitGeneScope;        /** "Listed genes (fast)" (default) or "All genes (slower)" */
    DecimalAction             _traitFdrThreshold;     /** significance level of the trait correlation (default 0.05) */
    IntegralAction            _maxWorkerThreads;      /** parallel workers (default: all cores) */
    IntegralAction            _memoryBudgetMb;        /** scratch memory budget of the parallel workers (default 1024 MB) */
    IntegralAction            _deTopGenes;            /** species differential expression: genes shown per species */
    OptionAction              _deDirection;           /** up in the species / down in the species / either */
    OptionAction              _deRankBy;              /** effect size or moderated t */
    IntegralAction            _deMinDetection;        /** minimum percentage of cells expressing the gene (in the higher group) */
    IntegralAction            _deMaxCells;            /** cell cap per species and cell type (0 = all cells) */
    IntegralAction            _deMinCells;            /** species with fewer cells are left out */
    TriggerAction             _showSpeciesDe;         /** open the species differential expression window */
    std::map<QString, std::vector<bool>> _topHierarchyClusterMap;
    TriggerAction                 _saveGeneTable;
    TriggerAction                 _saveSpeciesTable;
    ToggleAction                  _saveIncludeParameters; /** CSV export: add the analysis parameters on top */
    TriggerAction                 _saveTraitMatrix;
    TriggerAction                 _showTraitPlot;
    QStringList                   _currentHierarchyItemsMiddleForTable;
    StringAction               _topSelectedHierarchyStatus;
    bool                      _projectOpened = false;
    ClickableLabel* _selectedClusterLabel = nullptr;
    QString _selectedClusterName;                                 /** primary (last clicked) cell type */
    QStringList _selectedClusterNames;                            /** all selected cell types (Shift + click adds/removes) */
    QHash<QString, QPointer<ClickableLabel>> _clusterLabelsByName; /** status bar label of every cell type */

    // ---- Trait correlation state ----------------------------------------------------------------------
    QTimer* _traitCorrelationTimer = nullptr;   /** debounce timer, see updateTraitReverseSearchFeature() */
    QPointer<mv::plugin::AnalysisPlugin> _selectedTsneAnalysisPlugin; /** t-SNE of the selected cells (destroyed with its datasets) */
    QPointer<mv::plugin::AnalysisPlugin> _geneTsneAnalysisPlugin;     /** t-SNE of the gene similarity points */
    DeCache     _deCache;
    DeState     _deState;
    SpeciesDeResult _deResult;
    std::shared_ptr<std::vector<DeEntryJob>> _deJobs;
    QFutureWatcher<void> _deJobWatcher;
    mv::Dataset<Points> _deJobDataset;
    QTimer* _deTimer = nullptr;
    bool        _deActive = false;
    bool        _deJobRunning = false;
    bool        _deRecomputePending = false;
    std::function<void()> _speciesDeFinished;
    TraitCache  _traitCache;
    std::shared_ptr<std::vector<TraitEntryJob>> _traitJobs;  /** reading in the background (see computeTraitCorrelation) */
    QFutureWatcher<void> _traitJobWatcher;
    bool        _traitJobRunning = false;
    bool        _traitRecomputePending = false;              /** another request arrived while the reading was running */
    mv::Dataset<Points> _traitJobDataset;                    /** keeps the dataset alive while the background job reads it */
    quint64     _selectionRevision = 0;             /** bumped by markSelectionChanged() */

    // ---- Main dataset selection watcher ---------------------------------------------------------------
    mv::Dataset<Points>      _watchedMainPointsDataset;  /** kept alive so the connection below stays valid */
    QMetaObject::Connection  _mainSelectionConnection;
};
