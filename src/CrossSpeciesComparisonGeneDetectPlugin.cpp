#include "CrossSpeciesComparisonGeneDetectPlugin.h"

#include <util/Serialization.h>

#include <event/Event.h>
#include <CrossSpeciesComparisonTreeData/CrossSpeciesComparisonTreeData.h>
#include <DatasetsMimeData.h>
#include <QHeaderView> 
#include <QDebug>
#include <QMimeData>
#include <QShortcut>
#include <QSplitter>
#include <QRandomGenerator>
#include <QColor>
#include <QJsonArray> 
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include<actions/ColorMap1DAction.h>
//#include<QTooltip>
#include <QRegularExpression> 

#ifdef _WIN32
#include <execution>
#endif
Q_PLUGIN_METADATA(IID "studio.manivault.CrossSpeciesComparisonGeneDetectPlugin")

using namespace mv;


void applyLogTransformation(std::vector<float>& values) {
    

#ifdef _WIN32
    std::transform(std::execution::par, values.begin(), values.end(), values.begin(),
        [](float value) { return std::log(value + 1); });
#else
    std::transform(values.begin(), values.end(), values.begin(),
        [](float value) { return std::log(value + 1); });
#endif

}
float getNormalizedSizeRank(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;

    // Avoid division by zero
    if (max == min) {
        return 0.0f; // If max equals min, return 0 to avoid a division by zero error
    }

    float normalizedValue = (value - min) / (max - min);
    return normalizedValue * 100.0f; // Return size as a percentage
}
float getNormalizedSizePosNeg(float value, float min, float max) {
    // Calculate the maximum absolute value of min and max
    float maxAbs = std::max(std::abs(min), std::abs(max));

    if (value >= 0) {
        // Normalize positive values with respect to the maximum absolute value
        return (value / maxAbs) * 50.0f;
    }
    else {
        // Normalize negative values with respect to the maximum absolute value
        return (value / -maxAbs) * 50.0f;
    }
}



void updateRowVisibility(const QSet<QString>& uniqueReturnGeneList, QTableView* geneTableView, QSortFilterProxyModel* proxyModel) {

    for (int i = 0; i < proxyModel->rowCount(); i++) {
        QString geneName = proxyModel->index(i, 0).data().toString();
        int geneAppearanceSpeciesNamesCount = proxyModel->index(i, 1).data().toInt();

        if (!uniqueReturnGeneList.contains(geneName) || geneAppearanceSpeciesNamesCount < 1) {
            geneTableView->hideRow(i);
        }
        else {
            geneTableView->showRow(i);
        }
    }
}

void makeAllRowsVisible(QTableView* geneTableView, QSortFilterProxyModel* proxyModel) {
    int rowCount = proxyModel->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        geneTableView->showRow(row);
    }
}

float getNormalizedSize(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;

    float normalizedValue = (value - min) / (max - min);
    return normalizedValue * 100.0f; // Return size as a percentage
}

float getPowNormalizedSize(float value, float min, float max, float alpha) {
    // Scale down the range to avoid precision issues
    const float scale = 1e6f; // Adjust the scale as needed
    float scaledMin = min / scale;
    float scaledMax = max / scale;
    float scaledValue = value / scale;

    if (scaledValue < scaledMin) scaledValue = scaledMin;
    if (scaledValue > scaledMax) scaledValue = scaledMax;

    float normalizedValue = std::pow((scaledValue - scaledMin) / (scaledMax - scaledMin), alpha);
    return normalizedValue * 100.0f; // Return size as a percentage
}


float getLogNormalizedSize(float value, float min, float max) {
    // Avoid precision issues and ensure the range does not include zero (since log(0) is undefined)
    const float scale = 1e6f;
    float scaledMin = min / scale;
    float scaledMax = max / scale;
    float scaledValue = value / scale;

    // Clamp scaledValue within the scaledMin and scaledMax range
    if (scaledValue < scaledMin) scaledValue = scaledMin;
    if (scaledValue > scaledMax) scaledValue = scaledMax;

    // Apply logarithmic normalization
    float logMin = std::log(scaledMin);
    float logMax = std::log(scaledMax);
    float logValue = std::log(scaledValue);

    float normalizedValue = (logValue - logMin) / (logMax - logMin);
    return normalizedValue * 100.0f; // Return as percentage
}
float getRelativeNormalizedSize(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;

    // Calculate the midpoint to reduce the impact of large ranges
    float rangeMid = min + (max - min) / 2.0f;
    float relMin = min - rangeMid;
    float relMax = max - rangeMid;
    float relValue = value - rangeMid;

    float normalizedValue = (relValue - relMin) / (relMax - relMin);
    return normalizedValue * 100.0f;
}
float getSegmentedNormalizedSize(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;

    // Define segment ranges manually (e.g., in ranges of millions)
    float segmentSize = (max - min) / 10.0f;  // Divide range into 10 segments for example
    float normalizedValue = 0.0f;

    // Normalize within each segment range
    for (float i = min; i < max; i += segmentSize) {
        if (value >= i && value < i + segmentSize) {
            normalizedValue = (value - i) / segmentSize;
            break;
        }
    }
    return normalizedValue * 100.0f;
}
float getShiftedNormalizedSize(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;

    // Shift values by subtracting min to reduce range
    float shiftedValue = value - min;
    float shiftedMax = max - min;

    // Normalize the shifted value
    float normalizedValue = shiftedValue / shiftedMax;
    return normalizedValue * 100.0f;
}
float getFixedPointNormalizedSize(float value, float min, float max) {
    int scaleFactor = 1e6;  // Scale to an integer level that fits in `int`
    int intValue = static_cast<int>(value * scaleFactor);
    int intMin = static_cast<int>(min * scaleFactor);
    int intMax = static_cast<int>(max * scaleFactor);

    // Prevent overflow by clamping
    if (intValue < intMin) intValue = intMin;
    if (intValue > intMax) intValue = intMax;

    // Calculate normalized value
    float normalizedValue = (float)(intValue - intMin) / (intMax - intMin);
    return normalizedValue * 100.0f;
}
std::pair<QColor, QColor> getColorMAp(float value, float min, float max, const QString& colorMap) {
    if (value < min) value = min;
    if (value > max) value = max;

    float normalizedValue = (value - min) / (max - min);
    QColor color;
    QColor textColor;

    if (colorMap == "greyscale") {
        int grayValue = static_cast<int>((1 - normalizedValue) * 255);
        color = QColor(grayValue, grayValue, grayValue);
        textColor = (grayValue > 127) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    } else if (colorMap == "bupu") {
        int blueValue = static_cast<int>((1 - normalizedValue) * 255);
        int purpleValue = static_cast<int>(normalizedValue * 255);
        color = QColor(blueValue, 0, purpleValue);
        textColor = (blueValue + purpleValue > 255) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    } else if (colorMap == "gnpu") {
        int greenValue = static_cast<int>((1 - normalizedValue) * 255);
        int purpleValue = static_cast<int>(normalizedValue * 255);
        color = QColor(0, greenValue, purpleValue);
        textColor = (greenValue + purpleValue > 255) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    } else {
        // Default to greyscale if an unknown color map is provided
        int grayValue = static_cast<int>((1 - normalizedValue) * 255);
        color = QColor(grayValue, grayValue, grayValue);
        textColor = (grayValue > 127) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    }

    return std::make_pair(color, textColor);
}

std::map<QString, SpeciesDetailsStats> convertToStatisticsMap(const QString& formattedStatistics) {
    std::map<QString, SpeciesDetailsStats> statisticsMap;

    QStringList speciesStatsList = formattedStatistics.split(";", Qt::SkipEmptyParts); // Qt 5.14 and later

    // qDebug() << speciesStatsList;
    QRegularExpression regex(R"(Species: (.*), Rank: (\d+), AbundanceTop: (\d+),  AbundanceMiddle: (\d+),  CountAbundanceNumerator: (\d+), MeanSelected: ([\d.]+), CountSelected: (\d+), MeanNotSelected: ([\d.]+), CountNotSelected: (\d+))");

    for (const QString& speciesStats : speciesStatsList) {
        QRegularExpressionMatch match = regex.match(speciesStats.trimmed());
        if (match.hasMatch()) {
            QString species = match.captured(1);
            SpeciesDetailsStats stats = {
                match.captured(2).toInt(),  // Rank
                match.captured(3).toInt(),  // AbundanceTop
                match.captured(4).toInt(),  // AbundanceMiddle
                match.captured(5).toInt(),  // CountAbundanceNumerator
                match.captured(6).toFloat(),  // MeanSelected
                match.captured(7).toInt(),  // CountSelected
                match.captured(8).toFloat(),  // MeanNotSelected
                match.captured(9).toInt()  // CountNotSelected
            };
            statisticsMap[species] = stats;
        }
    }

    return statisticsMap;
}



CrossSpeciesComparisonGeneDetectPlugin::CrossSpeciesComparisonGeneDetectPlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _settingsAction(*this)
{

}

void CrossSpeciesComparisonGeneDetectPlugin::init()
{
    auto& shortcuts = getShortcuts();

    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "Update Info", "Click update button to update outdated table. Current cells need to be selected in the scatterplot" });
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "Select species", "Click individual species in the table to explore gene expression across dataset for specific species" });
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "Select genes", "Click individual genes in the table to see species details" });
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "Explore", "Explore multiple selected species" });
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "Revert", "Revert to initial selected species" });
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "Toggle scatterplot selection", "Select and deselect selection in the scatterplot" });
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "Top N", "View N number of highly differentially expressed genes per species" });
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "N Type", "Type of differential expression calculation" });
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "DeSelect", "Remove current gene selection scatterplot to view whole dataset scatterplot" });
    shortcuts.add({ QKeySequence(Qt::MouseButton::LeftButton), "Search", "Search all genes in the dataset" });
    //getLearningCenterAction().setPluginTitle("Gene Identification View");






    const auto updateSelectedRowIndex = [this]() -> void
        {

            if (_settingsAction.getFilteringEditTreeDatasetAction().getCurrentDataset().isValid())
            {
                auto treeDataset = mv::data().getDataset<CrossSpeciesComparisonTree>(_settingsAction.getFilteringEditTreeDatasetAction().getCurrentDataset().getDatasetId());
              
                QStringList selectedRowsStrList = _settingsAction.getSelectedRowIndexAction().getString().split(",");
                QList<int> selectedRows;
                for (const QString& str : selectedRowsStrList) {
                    selectedRows << str.toInt();
                }

                if (selectedRows.size()==1)
                {
                    int selectedRow = selectedRows[0];
                    if (treeDataset.isValid() && _settingsAction.getGeneTableView() && selectedRow >= 0)
                    {
                        QString treeData = _settingsAction.getGeneTableView()->model()->index(selectedRow, 2).data().toString();
                        //qDebug()<< "Tree data: " << treeData;
                        if (!treeData.isEmpty())
                        {

                            QJsonObject valueStringReference = QJsonDocument::fromJson(treeData.toUtf8()).object();
                            if (!valueStringReference.isEmpty())
                            {
                                treeDataset->setTreeData(valueStringReference);
                                events().notifyDatasetDataChanged(treeDataset);
                                //QString firstColumnValue = _settingsAction.getGeneTableView()->model()->index(selectedRow, 0).data().toString();
                               // _settingsAction.getGeneNamesConnection().setString(firstColumnValue);
                            }
                        }
                    }
                }
                if (selectedRows.size() > 1)
                {
                    _settingsAction.getCreateRowMultiSelectTree().setEnabled(true);
                }
                else
                {
                    _settingsAction.getCreateRowMultiSelectTree().setDisabled(true);
                }
                QStringList firstColumnValues;
                for (int row : selectedRows) {
                    firstColumnValues << _settingsAction.getGeneTableView()->model()->index(row, 0).data().toString();
                }
                QString firstColumnValue = firstColumnValues.join("*%$@*@$%*");
                //_settingsAction.getGeneNamesConnection().setString(firstColumnValue);


            }

        };

    connect(&_settingsAction.getSelectedRowIndexAction(), &StringAction::stringChanged, this, updateSelectedRowIndex);

    const auto updateSelectedGene = [this]() -> void
        {


        };

    connect(&_settingsAction.getSelectedGeneAction(), &StringAction::stringChanged, this, updateSelectedGene);

    const auto removeRowSelectionTable = [this]() -> void
        {
            auto statusString = _settingsAction.getStatusColorAction().getString();
            if (_settingsAction.getGeneTableView() && _settingsAction.getGeneTableView()->selectionModel()) {
                // Clear the current index if there's no selection
                _settingsAction.getGeneTableView()->clearSelection();

                // Temporarily disable the selection mode to remove highlight
                QAbstractItemView::SelectionMode oldMode = _settingsAction.getGeneTableView()->selectionMode();
                _settingsAction.getGeneTableView()->setSelectionMode(QAbstractItemView::NoSelection);

                // Clear the current index
                _settingsAction.getGeneTableView()->selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);

                // Restore the original selection mode
                _settingsAction.getGeneTableView()->setSelectionMode(oldMode);
                // Update the view to ensure changes are reflected
                _settingsAction.getGeneTableView()->update();
                _settingsAction.getSelctedSpeciesVals().setString("");


                if (_settingsAction.getScatterplotEmbeddingPointsUMAPOption().getCurrentDataset().isValid() && _settingsAction.getBottomClusterNamesDataset().getCurrentDataset().isValid())
                {

                    auto scatterplotViewFactory = mv::plugins().getPluginFactory("Scatterplot View");
                    mv::gui::DatasetPickerAction* colorDatasetPickerAction;
                    mv::gui::DatasetPickerAction* pointDatasetPickerAction;
                    mv::gui::ViewPluginSamplerAction* samplerActionAction;


                    if (scatterplotViewFactory) {
                        for (auto plugin : mv::plugins().getPluginsByFactory(scatterplotViewFactory)) {
                            if (plugin->getGuiName() == "Scatterplot Embedding View") {
                                pointDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Position"));
                                if (pointDatasetPickerAction) {
                                    pointDatasetPickerAction->setCurrentText("");

                                    pointDatasetPickerAction->setCurrentDataset(_settingsAction.getScatterplotEmbeddingPointsUMAPOption().getCurrentDataset());

                                    colorDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Color"));
                                    if (colorDatasetPickerAction)
                                    {
                                        colorDatasetPickerAction->setCurrentText("");
                                        colorDatasetPickerAction->setCurrentDataset(_settingsAction.getBottomClusterNamesDataset().getCurrentDataset());
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
                                                        legendDatasetPickerAction->setCurrentDataset(_settingsAction.getBottomClusterNamesDataset().getCurrentDataset());
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
                                    samplerActionAction = plugin->findChildByPath<mv::gui::ViewPluginSamplerAction>("Sampler");

                                    if (samplerActionAction)
                                    {
                                        samplerActionAction->setHtmlViewGeneratorFunction([this](const ViewPluginSamplerAction::SampleContext& toolTipContext) -> QString {
                                            QString clusterDatasetId = _settingsAction.getSpeciesNamesDataset().getCurrentDataset().getDatasetId();
                                            return _settingsAction.generateTooltip(toolTipContext, clusterDatasetId,true, "GlobalPointIndices");
                                            });
                                    }

                                }
                            }
                        }
                    }
                    _settingsAction.getScatterplotEmbeddingPointsUMAPOption().getCurrentDataset()->setSelectionIndices(_settingsAction.getSelectedIndicesFromStorage());
                    mv::events().notifyDatasetDataSelectionChanged(_settingsAction.getScatterplotEmbeddingPointsUMAPOption().getCurrentDataset());

                    _settingsAction.getStatusColorAction().setString("C");

                }
            }
            else {
                qDebug() << "TableView or its selection model is null";
            }



            if ( _settingsAction.getSelectedPointsTSNEDatasetForGeneTable().isValid())
            {

                {
                    _settingsAction.getSelectedPointsTSNEDatasetForGeneTable()->setSelectionIndices({});
                    mv::events().notifyDatasetDataSelectionChanged(_settingsAction.getSelectedPointsTSNEDatasetForGeneTable());

                }
            }




            _settingsAction.getRemoveRowSelection().setDisabled(true);
            //_settingsAction.enableDisableButtonsAutomatically();
            _settingsAction.getStatusColorAction().setString(statusString);
            selectedCellStatisticsStatusBarRemove();
            selectedCellCountStatusBarAdd();
            _settingsAction.getSelectedGeneAction().setString("");
            _settingsAction.getSpeciesExplorerInMap().setSelectedOptions(QStringList{});
            _settingsAction.getClearRightClickedCluster().trigger();
            updatePhylogeneticTree();
        };

    connect(&_settingsAction.getRemoveRowSelection(), &TriggerAction::triggered, this, removeRowSelectionTable);


    const auto updateSpeciesExplorerInMap = [this]() -> void
        {
          
            geneExplorer();
            _settingsAction.enableDisableButtonsAutomatically();
            _settingsAction.clearTableSelection(_settingsAction.getSelectionDetailsTable());
        };

    connect(&_settingsAction.getSpeciesExplorerInMapTrigger(), &TriggerAction::triggered, this, updateSpeciesExplorerInMap);


    const auto updateRevertRowSelectionChangesToInitial = [this]() -> void
        {
            // Check if _settingsAction is valid and initialized
            if (&_settingsAction) {
                QString selectedSpeciesVals = _settingsAction.getSelctedSpeciesVals().getString();

                // Check if the string is not empty
                if (!selectedSpeciesVals.isEmpty()) {
                    QStringList autoSpecies = selectedSpeciesVals.split(" @%$,$%@ ");

                    // Further check if autoSpecies is not just a list with an empty string
                    if (!(autoSpecies.size() == 1 && autoSpecies.first().isEmpty())) {
                        if (_settingsAction.getSpeciesExplorerInMap().getNumberOfOptions() > 0)
                        {
                            _settingsAction.getSpeciesExplorerInMap().setSelectedOptions(autoSpecies);
                            //_settingsAction.getRevertRowSelectionChangesToInitial().setDisabled(true);
                            geneExplorer();
                        }
                        
                    }

                }

            }
            //_settingsAction.getRevertRowSelectionChangesToInitial().setDisabled(true);
            _settingsAction.clearTableSelection(_settingsAction.getSelectionDetailsTable());
            _settingsAction.getClearRightClickedCluster().trigger();
           
        };

    connect(&_settingsAction.getRevertRowSelectionChangesToInitial(), &TriggerAction::triggered, this, updateRevertRowSelectionChangesToInitial);


    const auto updateSelctedSpeciesVals = [this]() -> void
        {
            _settingsAction.enableDisableButtonsAutomatically();

        };

    connect(&_settingsAction.getSelctedSpeciesVals(), &StringAction::stringChanged, this, updateSelctedSpeciesVals);


    const auto updateTableModel = [this]() -> void
        {
            //_settingsAction.setErrorOutFlag(false);
            modifyListData();
            //if (_settingsAction.getErroredOutFlag())
            //{
                //_settingsAction.getStatusColorAction().setString("E");

            //}
            //else
            //{
            _settingsAction.getRemoveRowSelection().trigger();
            _settingsAction.setPauseStatusUpdates(false);
            //_settingsAction.getStatusColorAction().setString("C");
        //}
        
            updatePhylogeneticTree();

            
        
        };

    connect(&_settingsAction.getListModelAction(), &VariantAction::variantChanged, this, updateTableModel);

    const auto updateHideShowColumns = [this]() -> void {

        auto shownColumns = _settingsAction.getHiddenShowncolumns().getSelectedOptions();

        QStandardItemModel* model = qobject_cast<QStandardItemModel*>(_settingsAction.getGeneTableView()->model());

        if (model) {
            for (int i = 0; i < model->columnCount(); i++) {
                if (!shownColumns.contains(model->horizontalHeaderItem(i)->text())) {
                    _settingsAction.getGeneTableView()->hideColumn(i);
                }
                else
                {
                    _settingsAction.getGeneTableView()->showColumn(i);

                }
            }
            emit model->layoutChanged();
        }
        };
    connect(&_settingsAction.getHiddenShowncolumns(), &OptionsAction::selectedOptionsChanged, this, updateHideShowColumns);


    const auto triggerInit = [this]() -> void
        {
            int groupIDDeletion = 10;
            int groupId1 = 10 * 2;
            int groupId2 = 10 * 3;
            _settingsAction.clearTemporaryDatasetHandles();
            _settingsAction.removeDatasets(groupIDDeletion);
            _settingsAction.removeDatasets(groupId1);
            _settingsAction.removeDatasets(groupId2);
            _settingsAction.removeDatasets(-1);
            _settingsAction.getMapForHierarchyItemsChangeMethodStopForProjectLoadBlocker().setChecked(false);




            auto scatterplotViewFactory = mv::plugins().getPluginFactory("Scatterplot View");
            mv::gui::DatasetPickerAction* colorDatasetPickerAction;
            mv::gui::DatasetPickerAction* pointDatasetPickerAction;
            mv::gui::ViewPluginSamplerAction* samplerActionAction;

            if (scatterplotViewFactory) {
                for (auto plugin : mv::plugins().getPluginsByFactory(scatterplotViewFactory)) {
                    if (plugin->getGuiName() == "Scatterplot Embedding View") {
                        
                        /////////
                        
                        auto colormapScatterplot = dynamic_cast<ColorMap1DAction*>(plugin->findChildByPath("Settings/Coloring/1D Color map"));;
                        if (colormapScatterplot)
                        {
                            auto legendViewFactory = mv::plugins().getPluginFactory("ChartLegend View");
                            if (legendViewFactory)
                            {
                                for (auto legendPlugin : mv::plugins().getPluginsByFactory(legendViewFactory))
                                {
                                    if (legendPlugin->getGuiName() == "Legend View")
                                    {
                                        auto colormapLegend = dynamic_cast<ColorMap1DAction*>(plugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Color map"));;
                                        if (colormapLegend)
                                        {
                                            //TODO
                                            //colormapScatterplot->publish("EvoViewer:ScatterplotColorMap");
                                            //colormapScatterplot->setConnectionPermissionsToAll();
                                            //colormapLegend->setConnectionPermissionsToAll();
                                           // mv::actions().connectPrivateActions(colormapScatterplot, colormapLegend,"EvoViewer:ScatterplotColorMap");
                                            
                                           //colormapLegend->connectToPublicActionByName("EvoViewer:ScatterplotColorMap");
                                        }
                                    }
                                }
                            }
                        }
                        
                        
                        ///////
                        pointDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Position"));
                        if (pointDatasetPickerAction) {
                            pointDatasetPickerAction->setCurrentText("");

                            pointDatasetPickerAction->setCurrentDataset(_settingsAction.getScatterplotEmbeddingPointsUMAPOption().getCurrentDataset());

                            colorDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Color"));
                            if (colorDatasetPickerAction)
                            {
                                colorDatasetPickerAction->setCurrentText("");
                                colorDatasetPickerAction->setCurrentDataset(_settingsAction.getBottomClusterNamesDataset().getCurrentDataset());
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
                                                legendDatasetPickerAction->setCurrentDataset(_settingsAction.getBottomClusterNamesDataset().getCurrentDataset());
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
                            samplerActionAction = plugin->findChildByPath<mv::gui::ViewPluginSamplerAction>("Sampler");

                            if (samplerActionAction)
                            {
                                samplerActionAction->setHtmlViewGeneratorFunction([this](const ViewPluginSamplerAction::SampleContext& toolTipContext) -> QString {
                                    QString clusterDatasetId = _settingsAction.getSpeciesNamesDataset().getCurrentDataset().getDatasetId();
                                    return _settingsAction.generateTooltip(toolTipContext, clusterDatasetId, true, "GlobalPointIndices");
                                    });
                            }

                        }
                    }
                }
            }



            _settingsAction.getProjectOpened() = true;

        };

    connect(&mv::projects(), &AbstractProjectManager::projectOpened , this, triggerInit);

    

    //change height of headers

    

    //make long strings in the cells visible and not ...shortened
    _settingsAction.getGeneTableView()->setTextElideMode(Qt::ElideNone);
    _settingsAction.getGeneTableView()->setWordWrap(true);
    _settingsAction.getGeneTableView()->setAlternatingRowColors(false);
    _settingsAction.getGeneTableView()->setSortingEnabled(true);

    //on hovering a cell, show the full text available in a tooltip
    connect(_settingsAction.getGeneTableView(), &QTableView::entered, [this](const QModelIndex& index) {
        if (index.isValid()) {
            QString text = index.data().toString();
            if (!text.isEmpty()) {
                _settingsAction.getGeneTableView()->setToolTip(text);
            }
        }
        });



    _settingsAction.getGeneTableView()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _settingsAction.getGeneTableView()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _settingsAction.getGeneTableView()->sortByColumn(1, Qt::DescendingOrder);

    _settingsAction.getGeneTableView()->verticalHeader()->hide();
    _settingsAction.getGeneTableView()->setMouseTracking(true);
    _settingsAction.getGeneTableView()->setToolTipDuration(10000);
    QFont fontLs = _settingsAction.getGeneTableView()->horizontalHeader()->font();
    fontLs.setBold(true);
    _settingsAction.getGeneTableView()->horizontalHeader()->setFont(fontLs);
    _settingsAction.getGeneTableView()->setStyleSheet("QTableView::item:selected { background-color: #00A2ED; }");
    _settingsAction.getGeneTableView()->horizontalHeader()->setHighlightSections(false);
    _settingsAction.getGeneTableView()->verticalHeader()->setHighlightSections(false);






    _settingsAction.getSelectionDetailsTable()->setTextElideMode(Qt::ElideNone);
    _settingsAction.getSelectionDetailsTable()->setWordWrap(true);
    _settingsAction.getSelectionDetailsTable()->setAlternatingRowColors(false);
    _settingsAction.getSelectionDetailsTable()->setSortingEnabled(true);

    //on hovering a cell, show the full text available in a tooltip
    connect(_settingsAction.getSelectionDetailsTable(), &QTableView::entered, [this](const QModelIndex& index) {
        if (index.isValid()) {
            QString text = index.data().toString();
            if (!text.isEmpty()) {
                _settingsAction.getSelectionDetailsTable()->setToolTip(text);
            }
        }
        });



    _settingsAction.getSelectionDetailsTable()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _settingsAction.getSelectionDetailsTable()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _settingsAction.getSelectionDetailsTable()->sortByColumn(2, Qt::DescendingOrder);

    _settingsAction.getSelectionDetailsTable()->verticalHeader()->hide();
    _settingsAction.getSelectionDetailsTable()->setMouseTracking(true);
    _settingsAction.getSelectionDetailsTable()->setToolTipDuration(10000);
    QFont fontTbl = _settingsAction.getSelectionDetailsTable()->horizontalHeader()->font();
    fontTbl.setBold(true);
    _settingsAction.getSelectionDetailsTable()->horizontalHeader()->setFont(fontTbl);
    _settingsAction.getSelectionDetailsTable()->setStyleSheet("QTableView::item:selected { background-color: #00A2ED; }");
    _settingsAction.getSelectionDetailsTable()->horizontalHeader()->setHighlightSections(false);
    _settingsAction.getSelectionDetailsTable()->verticalHeader()->setHighlightSections(false);


    ////////////*********temporary changes need to fix**********///////////////

    // Set the default alignment and text wrapping
    _settingsAction.getSelectionDetailsTable()->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter | (Qt::Alignment)Qt::TextWordWrap);

    // Adjust the minimum height of the header
    _settingsAction.getSelectionDetailsTable()->horizontalHeader()->setMinimumHeight(50);

    ////////////*********temporary changes need to fix**********///////////////










    auto mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto mainOptionsLayout = new QHBoxLayout();
    mainOptionsLayout->setSpacing(5);
    mainOptionsLayout->setContentsMargins(1, 1, 1, 1);

    auto extraOptionsGroup = new VerticalGroupAction(this, "Settings");
    extraOptionsGroup->setIcon(mv::util::StyledIcon("cog"));
    extraOptionsGroup->addAction(&_settingsAction.getListModelAction());
    extraOptionsGroup->addAction(&_settingsAction.getSelectedGeneAction());
    extraOptionsGroup->addAction(&_settingsAction.getSelectedRowIndexAction());
    extraOptionsGroup->addAction(&_settingsAction.getFilteringEditTreeDatasetAction());
    extraOptionsGroup->addAction(&_settingsAction.getOptionSelectionAction());
    extraOptionsGroup->addAction(&_settingsAction.getFilteredGeneNames());
    extraOptionsGroup->addAction(&_settingsAction.getCreateRowMultiSelectTree());
    //extraOptionsGroup->addAction(&_settingsAction.getPerformGeneTableTsneAction());
    extraOptionsGroup->addAction(&_settingsAction.getGeneNamesConnection());
    extraOptionsGroup->addAction(&_settingsAction.getHiddenShowncolumns());
    auto datasetAndLinkerOptionsGroup = new VerticalGroupAction(this, "Dataset and Linker Options");
    datasetAndLinkerOptionsGroup->setIcon(mv::util::StyledIcon("link"));
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getReferenceTreeDatasetAction());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getMainPointsDataset());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getEmbeddingDataset());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getSpeciesNamesDataset());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getBottomClusterNamesDataset());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getMiddleClusterNamesDataset());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getTopClusterNamesDataset());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getScatterplotEmbeddingPointsUMAPOption());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getSpeciesExplorerInMap());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getSelctedSpeciesVals());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getStatusColorAction());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getClusterOrderHierarchy());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getRightClickedCluster());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getClearRightClickedCluster());
    datasetAndLinkerOptionsGroup->addAction(&_settingsAction.getTopSelectedHierarchyStatus());


    auto tsneOptionsGroup = new VerticalGroupAction(this, "Options");
    tsneOptionsGroup->setIcon(mv::util::StyledIcon("tools"));
    //tsneOptionsGroup->addAction(&_settingsAction.getUsePreComputedTSNE());
    //tsneOptionsGroup->addAction(&_settingsAction.getTsnePerplexity());
    //tsneOptionsGroup->addAction(&_settingsAction.getClusterCountSortingType());
    //tsneOptionsGroup->addAction(&_settingsAction.getScatterplotReembedColorOption());
    tsneOptionsGroup->addAction(&_settingsAction.getApplyLogTransformation());
    //tsneOptionsGroup->addAction(&_settingsAction.getTopHierarchyClusterNamesFrequencyInclusionList());
    //tsneOptionsGroup->addAction(&_settingsAction.getMiddleHierarchyClusterNamesFrequencyInclusionList());
    //tsneOptionsGroup->addAction(&_settingsAction.getBottomHierarchyClusterNamesFrequencyInclusionList());
    //tsneOptionsGroup->addAction(&_settingsAction.getComputeTreesToDisplayFromHierarchy());
    //tsneOptionsGroup->addAction(&_settingsAction.getPerformGeneTableTsneAction());
    //tsneOptionsGroup->addAction(&_settingsAction.getPerformGeneTableTsnePerplexity());
    //tsneOptionsGroup->addAction(&_settingsAction.getPerformGeneTableTsneKnn());
    //tsneOptionsGroup->addAction(&_settingsAction.getPerformGeneTableTsneDistance());
    //tsneOptionsGroup->addAction(&_settingsAction.getPerformGeneTableTsneTrigger());
    

    auto mainOptionsGroupLayout = new QVBoxLayout();
    auto mainOptionsGroup1 = new HorizontalGroupAction(this, "MainGroup1");
    auto mainOptionsGroup2 = new HorizontalGroupAction(this, "MainGroup2");
    mainOptionsGroup1->setIcon(mv::util::StyledIcon("database"));
    mainOptionsGroup2->setIcon(mv::util::StyledIcon("play"));

 
    mainOptionsGroup1->addAction(&_settingsAction.getStartComputationTriggerAction());
    mainOptionsGroup1->addAction(&_settingsAction.getTopNGenesFilter());
    mainOptionsGroup1->addAction(&_settingsAction.getTypeofTopNGenes());
    
    
    
    mainOptionsGroup2->addAction(&_settingsAction.getRemoveRowSelection());
    mainOptionsGroup2->addAction(&_settingsAction.getSpeciesExplorerInMapTrigger());
    mainOptionsGroup2->addAction(&_settingsAction.getRevertRowSelectionChangesToInitial());
    mainOptionsGroup2->addAction(&_settingsAction.getToggleScatterplotSelection()); 
    

    auto group1Widget = mainOptionsGroup1->createWidget(&getWidget());
    group1Widget->setMaximumWidth(450);
    mainOptionsGroupLayout->addWidget(group1Widget);

    auto group2Widget = mainOptionsGroup2->createWidget(&getWidget());
    group2Widget->setMaximumWidth(450);
    mainOptionsGroupLayout->addWidget(group2Widget);
    auto statusBarWiddget = _settingsAction.getStatusBarActionWidget();
    statusBarWiddget->setMaximumWidth(600);

    auto searchBoxWidget = _settingsAction.getSearchBox();
    searchBoxWidget->setMaximumWidth(600);

    

    auto startsANDSearchLayout = new QVBoxLayout();
    startsANDSearchLayout->addWidget(statusBarWiddget);
    startsANDSearchLayout->addWidget(searchBoxWidget);
    mainOptionsLayout->addLayout(startsANDSearchLayout);
    mainOptionsLayout->addLayout(mainOptionsGroupLayout);
    //mainOptionsLayout->addWidget(statusBarWiddget);
    //mainOptionsLayout->addLayout(mainOptionsGroupLayout);
    //mainOptionsLayout->addWidget(searchBoxWidget);



    auto linkerandtsneLayout = new QVBoxLayout();
    //auto linkerWidget = datasetAndLinkerOptionsGroup->createCollapsedWidget(&getWidget());
    //linkerWidget->setMaximumHeight(22);
    //linkerandtsneLayout->addWidget(linkerWidget);
    auto tsneWidget = tsneOptionsGroup->createCollapsedWidget(&getWidget());
    tsneWidget->setMaximumHeight(22);
    linkerandtsneLayout->addWidget(tsneWidget);
    
    mainOptionsLayout->addLayout(linkerandtsneLayout);

    auto downloadOptionsGroup = new VerticalGroupAction(this, "Save table as CSV");
    downloadOptionsGroup->setIcon(mv::util::StyledIcon("download"));
    downloadOptionsGroup->addAction(&_settingsAction.getSaveGeneTable());
    downloadOptionsGroup->addAction(&_settingsAction.getSaveSpeciesTable());

    auto downloadLayout = new QVBoxLayout();
    auto downloadWidget = downloadOptionsGroup->createCollapsedWidget(&getWidget());
    downloadWidget->setMaximumHeight(22);
    downloadLayout->addWidget(downloadWidget);

    mainOptionsLayout->addLayout(downloadLayout);

    //mainOptionsLayout->addWidget(tsneOptionsGroup->createCollapsedWidget(&getWidget()), 3);
    //mainOptionsLayout->addWidget(datasetAndLinkerOptionsGroup->createCollapsedWidget(&getWidget()), 2);
    //mainOptionsLayout->addWidget(extraOptionsGroup->createCollapsedWidget(&getWidget()), 1);

    auto fullSettingsLayout = new QVBoxLayout();
    fullSettingsLayout->addLayout(mainOptionsLayout);
    mainLayout->addLayout(fullSettingsLayout);

    connect(_settingsAction.getGeneTableView(), &QTableView::entered, [this](const QModelIndex& index) {
        if (index.isValid()) {
            //QString text = index.model()->data(index).toString();
            //QToolTip::showText(QCursor::pos(), text, _settingsAction.getGeneTableView());
            //alternative to above because of linux errors
            QAbstractItemModel* model = _settingsAction.getGeneTableView()->model();
            for (int row = 0; row < model->rowCount(); ++row) {
                for (int column = 0; column < model->columnCount(); ++column) {
                    QModelIndex index = model->index(row, column);
                    QString text = model->data(index).toString();
                    model->setData(index, text, Qt::ToolTipRole);
                }
            }

        }
        });



    QFrame* verticalLine = new QFrame();
    verticalLine->setFrameShape(QFrame::VLine);
    verticalLine->setFrameShadow(QFrame::Sunken);
    verticalLine->setLineWidth(1);

    _settingsAction.getGeneTableView()->setMaximumWidth(165);
    _settingsAction.getTableSplitter()->addWidget(_settingsAction.getGeneTableView());
    _settingsAction.getTableSplitter()->addWidget(verticalLine);
    _settingsAction.getTableSplitter()->addWidget(_settingsAction.getSelectionDetailsTable());

    // Add the splitter to the main layout
    mainLayout->addLayout(_settingsAction.getTableSplitter());

    mainLayout->addLayout(_settingsAction.getSelectedCellClusterInfoStatusBar());
    _settingsAction.getStatusColorAction().setString("M");

    getWidget().setLayout(mainLayout);





}

void CrossSpeciesComparisonGeneDetectPlugin::geneExplorer()
{
    std::vector<std::seed_seq::result_type> selectedSpeciesIndices;
    auto speciesDataset = _settingsAction.getSpeciesNamesDataset().getCurrentDataset();
    auto umapDataset = _settingsAction.getScatterplotEmbeddingPointsUMAPOption().getCurrentDataset();
    auto mainPointsDataset = _settingsAction.getMainPointsDataset().getCurrentDataset();
    std::vector<std::seed_seq::result_type> filtSelectInndx;
    QString gene = _settingsAction.getSelectedGeneAction().getString();
    QStringList finalsettingSpeciesNamesArray = _settingsAction.getSpeciesExplorerInMap().getSelectedOptions();

    if (!speciesDataset.isValid() || !umapDataset.isValid() || !mainPointsDataset.isValid() || !_settingsAction.getFilteredUMAPDatasetPoints().isValid() || !_settingsAction.getFilteredUMAPDatasetColors().isValid() || !_settingsAction.getFilteredUMAPDatasetClusters().isValid())
    {
        qDebug() << "One of the datasets is not valid";
        return;
    }
    if (gene == "")
    {
        qDebug() << "Gene is not selected";
        return;
    }


    if (finalsettingSpeciesNamesArray.isEmpty())
    {
        qDebug() << "Species names are not selected";
        return;
    }


    {
        auto speciesClusterDataset = mv::data().getDataset<Clusters>(speciesDataset.getDatasetId());
        auto umapPointsDataset = mv::data().getDataset<Points>(umapDataset.getDatasetId());
        auto fullMainPointsDataset = mv::data().getDataset<Points>(mainPointsDataset.getDatasetId());

        std::unordered_set<QString> speciesNamesSet(finalsettingSpeciesNamesArray.begin(), finalsettingSpeciesNamesArray.end());
        std::map<QString, std::pair<QColor, std::vector<int>>> selectedFilteredUMAPDatasetColorsMap;
        for (const auto& species : speciesClusterDataset->getClusters()) {
            if (speciesNamesSet.find(species.getName()) != speciesNamesSet.end()) {
                const auto& indices = species.getIndices();
                selectedSpeciesIndices.insert(selectedSpeciesIndices.end(), indices.begin(), indices.end());
                selectedFilteredUMAPDatasetColorsMap[species.getName()] = std::make_pair(species.getColor(), std::vector<int>(indices.begin(), indices.end()));
            }
        }


        std::vector<std::seed_seq::result_type>& selectedIndicesFromStorage = _settingsAction.getSelectedIndicesFromStorage();
        std::unordered_set<std::seed_seq::result_type> indicesSet(selectedIndicesFromStorage.begin(), selectedIndicesFromStorage.end());
        filtSelectInndx.reserve(selectedSpeciesIndices.size());
        for (int i = 0; i < selectedSpeciesIndices.size(); ++i) {
            if (indicesSet.find(selectedSpeciesIndices[i]) != indicesSet.end()) {
                filtSelectInndx.push_back(i);
            }
        }
        auto dimensionNamesUmap = umapPointsDataset->getDimensionNames();
        auto numDimensions = umapPointsDataset->getNumDimensions();
        std::vector<int> geneIndicesSpecies(numDimensions);
        std::iota(geneIndicesSpecies.begin(), geneIndicesSpecies.end(), 0);


        if (selectedSpeciesIndices.size() > 0)
        {
            std::vector<float> resultContainerSpeciesUMAP(selectedSpeciesIndices.size() * umapPointsDataset->getNumDimensions());
            umapPointsDataset->populateDataForDimensions(resultContainerSpeciesUMAP, geneIndicesSpecies, selectedSpeciesIndices);
            auto speciesDataId = _settingsAction.getFilteredUMAPDatasetPoints().getDatasetId();
            int tempnumPoints = static_cast<int>(selectedSpeciesIndices.size());
            int tempNumDimensions = static_cast<int>(geneIndicesSpecies.size());
            _settingsAction.populatePointData(speciesDataId, resultContainerSpeciesUMAP, tempnumPoints, tempNumDimensions, dimensionNamesUmap);



            std::vector<float> resultContainerSpeciesColors(selectedSpeciesIndices.size());
            std::vector<int> selectedGeneIndex;

            auto dimensionNames = fullMainPointsDataset->getDimensionNames();
            auto it = std::find(dimensionNames.begin(), dimensionNames.end(), gene);
            if (it != dimensionNames.end()) {
                selectedGeneIndex.push_back(static_cast<int>(std::distance(dimensionNames.begin(), it)));
            }


            fullMainPointsDataset->populateDataForDimensions(resultContainerSpeciesColors, selectedGeneIndex, selectedSpeciesIndices);
            auto speciesColorDataId = _settingsAction.getFilteredUMAPDatasetColors().getDatasetId();
            auto speciesClusterDataId = _settingsAction.getFilteredUMAPDatasetClusters().getDatasetId();
            int tempnumPointsColors = static_cast<int>(selectedSpeciesIndices.size());

            std::vector<QString> columnGeneColors = { gene };
            int tempNumDimensionsColors = static_cast<int>(columnGeneColors.size());
            if (_settingsAction.getApplyLogTransformation().isChecked())
            {
                applyLogTransformation(resultContainerSpeciesColors);
            }
            _settingsAction.populatePointData(speciesColorDataId, resultContainerSpeciesColors, tempnumPointsColors, tempNumDimensionsColors, columnGeneColors);
            //std::map<QString, std::pair<QColor, std::vector<int>>> selectedFilteredUMAPDatasetColorsMap;
            //TODO: populate the selectedFilteredUMAPDatasetColorsMap

            _settingsAction.populateClusterData(speciesClusterDataId, selectedFilteredUMAPDatasetColorsMap);

            auto scatterplotViewFactory = mv::plugins().getPluginFactory("Scatterplot View");
            mv::gui::DatasetPickerAction* colorDatasetPickerAction;
            mv::gui::DatasetPickerAction* pointDatasetPickerAction;
            mv::gui::ViewPluginSamplerAction* samplerActionAction;

            if (scatterplotViewFactory) {
                for (auto plugin : mv::plugins().getPluginsByFactory(scatterplotViewFactory)) {
                    if (plugin->getGuiName() == "Scatterplot Embedding View") {
                        //plugin->printChildren();
                        pointDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Position"));
                        if (pointDatasetPickerAction) {
                            pointDatasetPickerAction->setCurrentText("");

                            pointDatasetPickerAction->setCurrentDataset(_settingsAction.getFilteredUMAPDatasetPoints());

                            colorDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Color"));
                            if (colorDatasetPickerAction)
                            {
                                colorDatasetPickerAction->setCurrentText("");
                                colorDatasetPickerAction->setCurrentDataset(_settingsAction.getFilteredUMAPDatasetColors());
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
                                                legendDatasetPickerAction->setCurrentDataset(_settingsAction.getFilteredUMAPDatasetColors());
                                            }
                                            auto chartTitle = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Chart Title"));
                                            if (chartTitle)
                                            {
                                                chartTitle->setString("Gene expression");
                                            }
                                        }
                                    }
                                }

                            }

                            auto focusSelectionAction = dynamic_cast<ToggleAction*>(plugin->findChildByPath("Settings/Plot/Point/Focus selection"));

                            if (focusSelectionAction)
                            {
                                focusSelectionAction->setChecked(true);

                            }

                            auto opacityAction = dynamic_cast<DecimalAction*>(plugin->findChildByPath("Settings/Plot/Point/Point opacity/Point opacity"));
                            if (opacityAction)
                            {
                                opacityAction->setValue(20.0);
                            }
                            samplerActionAction = plugin->findChildByPath<mv::gui::ViewPluginSamplerAction>("Sampler");

                            if (samplerActionAction)
                            {
                                samplerActionAction->setHtmlViewGeneratorFunction([this](const ViewPluginSamplerAction::SampleContext& toolTipContext) -> QString {
                                    QString clusterDatasetId = _settingsAction.getFilteredUMAPDatasetClusters().getDatasetId();
                                    return _settingsAction.generateTooltip(toolTipContext, clusterDatasetId,true, "GlobalPointIndices");
                                    });
                            }
                        }
                    }
                }
            }



            _settingsAction.getFilteredUMAPDatasetPoints()->setSelectionIndices(filtSelectInndx);
            mv::events().notifyDatasetDataSelectionChanged(_settingsAction.getFilteredUMAPDatasetPoints());


        }


    }
}


void CrossSpeciesComparisonGeneDetectPlugin::geneExplorer(QString selectedSpecies)
{
    std::vector<std::seed_seq::result_type> selectedSpeciesIndices;
    auto speciesDataset = _settingsAction.getSpeciesNamesDataset().getCurrentDataset();
    auto umapDataset = _settingsAction.getScatterplotEmbeddingPointsUMAPOption().getCurrentDataset();
    auto mainPointsDataset = _settingsAction.getMainPointsDataset().getCurrentDataset();
    std::vector<std::seed_seq::result_type> filtSelectInndx;
    QString gene = _settingsAction.getSelectedGeneAction().getString();
    //QStringList finalsettingSpeciesNamesArray = _settingsAction.getSpeciesExplorerInMap().getSelectedOptions();

    if (!speciesDataset.isValid() || !umapDataset.isValid() || !mainPointsDataset.isValid() || !_settingsAction.getFilteredUMAPDatasetPoints().isValid() || !_settingsAction.getFilteredUMAPDatasetColors().isValid() || !_settingsAction.getFilteredUMAPDatasetClusters().isValid())
    {
        qDebug() << "One of the datasets is not valid";
        return;
    }
    if (gene == "")
    {
        qDebug() << "Gene is not selected";
        return;
    }


    if (selectedSpecies.isEmpty())
    {
        qDebug() << "Species name empty";
        return;
    }


    {
        auto speciesClusterDataset = mv::data().getDataset<Clusters>(speciesDataset.getDatasetId());
        auto umapPointsDataset = mv::data().getDataset<Points>(umapDataset.getDatasetId());
        auto fullMainPointsDataset = mv::data().getDataset<Points>(mainPointsDataset.getDatasetId());

        std::unordered_set<QString> speciesNamesSet;
        speciesNamesSet.insert(selectedSpecies);
        std::map<QString, std::pair<QColor, std::vector<int>>> selectedFilteredUMAPDatasetColorsMap;
        for (const auto& species : speciesClusterDataset->getClusters()) {
            if (speciesNamesSet.find(species.getName()) != speciesNamesSet.end()) {
                const auto& indices = species.getIndices();
                selectedSpeciesIndices.insert(selectedSpeciesIndices.end(), indices.begin(), indices.end());
                selectedFilteredUMAPDatasetColorsMap[species.getName()] = std::make_pair(species.getColor(), std::vector<int>(indices.begin(), indices.end()));
            }
        }


        std::vector<std::seed_seq::result_type>& selectedIndicesFromStorage = _settingsAction.getSelectedIndicesFromStorage();
        std::unordered_set<std::seed_seq::result_type> indicesSet(selectedIndicesFromStorage.begin(), selectedIndicesFromStorage.end());
        filtSelectInndx.reserve(selectedSpeciesIndices.size());
        for (int i = 0; i < selectedSpeciesIndices.size(); ++i) {
            if (indicesSet.find(selectedSpeciesIndices[i]) != indicesSet.end()) {
                filtSelectInndx.push_back(i);
            }
        }
        auto dimensionNamesUmap = umapPointsDataset->getDimensionNames();
        auto numDimensions = umapPointsDataset->getNumDimensions();
        std::vector<int> geneIndicesSpecies(numDimensions);
        std::iota(geneIndicesSpecies.begin(), geneIndicesSpecies.end(), 0);


        if (selectedSpeciesIndices.size() > 0)
        {
            std::vector<float> resultContainerSpeciesUMAP(selectedSpeciesIndices.size() * umapPointsDataset->getNumDimensions());
            umapPointsDataset->populateDataForDimensions(resultContainerSpeciesUMAP, geneIndicesSpecies, selectedSpeciesIndices);
            auto speciesDataId = _settingsAction.getFilteredUMAPDatasetPoints().getDatasetId();
            int tempnumPoints = static_cast<int>(selectedSpeciesIndices.size());
            int tempNumDimensions = static_cast<int>(geneIndicesSpecies.size());
            _settingsAction.populatePointData(speciesDataId, resultContainerSpeciesUMAP, tempnumPoints, tempNumDimensions, dimensionNamesUmap);



            std::vector<float> resultContainerSpeciesColors(selectedSpeciesIndices.size());
            std::vector<int> selectedGeneIndex;

            auto dimensionNames = fullMainPointsDataset->getDimensionNames();
            auto it = std::find(dimensionNames.begin(), dimensionNames.end(), gene);
            if (it != dimensionNames.end()) {
                selectedGeneIndex.push_back(std::distance(dimensionNames.begin(), it));
            }


            fullMainPointsDataset->populateDataForDimensions(resultContainerSpeciesColors, selectedGeneIndex, selectedSpeciesIndices);
            auto speciesColorDataId = _settingsAction.getFilteredUMAPDatasetColors().getDatasetId();
            auto speciesClusterDataId = _settingsAction.getFilteredUMAPDatasetClusters().getDatasetId();
            int tempnumPointsColors = static_cast<int>(selectedSpeciesIndices.size());

            std::vector<QString> columnGeneColors = { gene };
            int tempNumDimensionsColors = static_cast<int>(columnGeneColors.size());
            if (_settingsAction.getApplyLogTransformation().isChecked())
            {
                applyLogTransformation(resultContainerSpeciesColors);
            }
            _settingsAction.populatePointData(speciesColorDataId, resultContainerSpeciesColors, tempnumPointsColors, tempNumDimensionsColors, columnGeneColors);
            //std::map<QString, std::pair<QColor, std::vector<int>>> selectedFilteredUMAPDatasetColorsMap;
            //TODO: populate the selectedFilteredUMAPDatasetColorsMap

            _settingsAction.populateClusterData(speciesClusterDataId, selectedFilteredUMAPDatasetColorsMap);

            auto scatterplotViewFactory = mv::plugins().getPluginFactory("Scatterplot View");
            mv::gui::DatasetPickerAction* colorDatasetPickerAction;
            mv::gui::DatasetPickerAction* pointDatasetPickerAction;
            mv::gui::ViewPluginSamplerAction* samplerActionAction;

            if (scatterplotViewFactory) {
                for (auto plugin : mv::plugins().getPluginsByFactory(scatterplotViewFactory)) {
                    if (plugin->getGuiName() == "Scatterplot Embedding View") {
                        //plugin->printChildren();
                        pointDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Position"));
                        if (pointDatasetPickerAction) {
                            pointDatasetPickerAction->setCurrentText("");

                            pointDatasetPickerAction->setCurrentDataset(_settingsAction.getFilteredUMAPDatasetPoints());

                            colorDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Color"));
                            if (colorDatasetPickerAction)
                            {
                                colorDatasetPickerAction->setCurrentText("");
                                colorDatasetPickerAction->setCurrentDataset(_settingsAction.getFilteredUMAPDatasetColors());
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
                                                legendDatasetPickerAction->setCurrentDataset(_settingsAction.getFilteredUMAPDatasetColors());
                                            }
                                            auto chartTitle = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Chart Title"));
                                            if (chartTitle)
                                            {
                                                chartTitle->setString("Gene expression");
                                            }
                                        }
                                    }
                                }

                            }

                            auto focusSelectionAction = dynamic_cast<ToggleAction*>(plugin->findChildByPath("Settings/Plot/Point/Focus selection"));

                            if (focusSelectionAction)
                            {
                                focusSelectionAction->setChecked(true);

                            }

                            auto opacityAction = dynamic_cast<DecimalAction*>(plugin->findChildByPath("Settings/Plot/Point/Point opacity/Point opacity"));
                            if (opacityAction)
                            {
                                opacityAction->setValue(20.0);
                            }
                            samplerActionAction = plugin->findChildByPath<mv::gui::ViewPluginSamplerAction>("Sampler");

                            if (samplerActionAction)
                            {
                                samplerActionAction->setHtmlViewGeneratorFunction([this](const ViewPluginSamplerAction::SampleContext& toolTipContext) -> QString {
                                    QString clusterDatasetId = _settingsAction.getFilteredUMAPDatasetClusters().getDatasetId();
                                    return _settingsAction.generateTooltip(toolTipContext, clusterDatasetId, true, "GlobalPointIndices");
                                    });
                            }
                        }
                    }
                }
            }



            _settingsAction.getFilteredUMAPDatasetPoints()->setSelectionIndices(filtSelectInndx);
            mv::events().notifyDatasetDataSelectionChanged(_settingsAction.getFilteredUMAPDatasetPoints());


        }


    }
}

void CrossSpeciesComparisonGeneDetectPlugin::adjustTableWidths(const QString& value) {
   /*
    // Assuming _settingsAction.getHorizontalLayout() returns your QHBoxLayout
    QHBoxLayout* layout = _settingsAction.getTableSplitter();
    if (!layout) return;

    QWidget* parentWidget = layout->parentWidget();
    if (!parentWidget) return;

    int totalWidth = parentWidget->width();

    double tableViewRatio = 1;
    double selectionDetailsTableRatio = 2;

    if (value == "small") {
        tableViewRatio = 1.2;
        selectionDetailsTableRatio = 1.2;
    }

    int tableViewWidth = static_cast<int>(totalWidth * tableViewRatio / (tableViewRatio + selectionDetailsTableRatio));
    int selectionDetailsTableWidth = totalWidth - tableViewWidth;

    // Assuming the first two widgets in the layout are the ones we want to adjust
    if (layout->count() >= 2) {
        QWidget* tableViewWidget = layout->itemAt(0)->widget();
        QWidget* selectionDetailsTableWidget = layout->itemAt(1)->widget();

        if (tableViewWidget && selectionDetailsTableWidget) {
            tableViewWidget->setMinimumWidth(tableViewWidth);
            tableViewWidget->setMaximumWidth(tableViewWidth);

            selectionDetailsTableWidget->setMinimumWidth(selectionDetailsTableWidth);
            selectionDetailsTableWidget->setMaximumWidth(selectionDetailsTableWidth);
        }
    }
    */
}



QColor getColorFromValue(int value, int min, int max) {
    if (value < min) value = min;
    if (value > max) value = max;

    int range = max - min;
    if (range == 0) return QColor(Qt::gray);

    int blue = 255 * (value - min) / range;

    return QColor(255 - blue, 255 - blue, 255);
}



void CrossSpeciesComparisonGeneDetectPlugin::modifyListData()
{
    try {
    //qDebug() << "It's here";
        auto variant = _settingsAction.getListModelAction().getVariant();
        QStandardItemModel* model = qobject_cast<QStandardItemModel*>(variant.value<QAbstractItemModel*>());

        if (!_settingsAction.getGeneTableView()) {
            // qDebug() << "_settingsAction.getGeneTableView() is null";
            return;
        }

        if (!model) {
            // qDebug() << "Model is null";
            QAbstractItemModel* currentModel = _settingsAction.getGeneTableView()->model();
            if (currentModel) {
                currentModel->removeRows(0, currentModel->rowCount());
                _settingsAction.getGeneTableView()->update();
            }
            else {
                // qDebug() << "TableView model is null";
            }
            return;
        }
        QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(this);
        proxyModel->setSourceModel(model);
        proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        proxyModel->setFilterKeyColumn(0);
        _settingsAction.getGeneTableView()->setModel(proxyModel);
        // This method runs on every recomputation, so we clear earlier hooks
        // before wiring search and selection behavior again.
        disconnect(_settingsAction.getSearchBox(), nullptr, this, nullptr);
        connect(_settingsAction.getSearchBox(), &CustomLineEdit::textboxSelectedForTyping, this, [this, proxyModel, model]() {
            makeAllRowsVisible(_settingsAction.getGeneTableView(),proxyModel);
            });

        connect(_settingsAction.getSearchBox(), &CustomLineEdit::textboxDeselectedNotTypingAnymore, this, [this, proxyModel, model]() {
            proxyModel->setFilterRegularExpression(QRegularExpression());
            updateRowVisibility(_settingsAction.getUniqueReturnGeneList(), _settingsAction.getGeneTableView(),proxyModel);
            });

        connect(_settingsAction.getSearchBox(), &CustomLineEdit::textChanged, this, [this, proxyModel,model](const QString& text) {
            QTimer::singleShot(300, this, [this, proxyModel, text,model]() {
                if (!text.isEmpty()) {
                    //
                    // proxyModel->setFilterWildcard("*" + text + "*");

                    QRegularExpression regExp(text, QRegularExpression::CaseInsensitiveOption);
                    proxyModel->setFilterRegularExpression(regExp);

                }

                });
            });


        _settingsAction.getSearchBox()->setText("");

        updateRowVisibility(_settingsAction.getUniqueReturnGeneList(), _settingsAction.getGeneTableView(),proxyModel);

        // Hide columns not in the shown columns list
        auto shownColumns = _settingsAction.getHiddenShowncolumns().getSelectedOptions();
        for (int i = 0; i < _settingsAction.getGeneTableView()->model()->columnCount(); i++) {
            if (!shownColumns.contains(model->horizontalHeaderItem(i)->text())) {
                _settingsAction.getGeneTableView()->hideColumn(i);
            }
        }

        // Sort and update the table view
        proxyModel->sort(1, Qt::DescendingOrder);
        _settingsAction.getGeneTableView()->setColumnWidth(0, 85);
        _settingsAction.getGeneTableView()->setColumnWidth(1, 80);
        _settingsAction.getGeneTableView()->setColumnWidth(2, 220);
        _settingsAction.getGeneTableView()->setColumnWidth(3, 420);
        _settingsAction.getGeneTableView()->update();


        //disconnect(_settingsAction.getGeneTableView()->selectionModel(), &QItemSelectionModel::currentChanged, this, nullptr);

    disconnect(_settingsAction.getGeneTableView()->selectionModel(), nullptr, this, nullptr);
    connect(_settingsAction.getGeneTableView()->selectionModel(), &QItemSelectionModel::currentChanged, [this](const QModelIndex& current, const QModelIndex& previous) {
        if (!current.isValid()) return;

        QString gene = current.siblingAtColumn(0).data().toString();
        _settingsAction.getSelectedGeneAction().setString(gene);
        _settingsAction.getSelectedRowIndexAction().setString(QString::number(current.row()));
        _settingsAction.getRemoveRowSelection().setEnabled(true);
        //_settingsAction.enableDisableButtonsAutomatically();
        

        std::map<QString, SpeciesDetailsStats> speciesExpressionMap;
        QStringList finalsettingSpeciesNamesArray;
        QString finalSpeciesNameString;
        const auto* model = current.model();
        const int columnCount = model->columnCount();
        const auto initColumnNamesSize = _settingsAction.getInitColumnNames().size(); 
        for (int i = 0; i < columnCount; ++i) {
            const QString columnName = model->headerData(i, Qt::Horizontal).toString();
            const auto data = current.siblingAtColumn(i).data();

            if (columnName == "Gene Appearance Species Names") {
                finalsettingSpeciesNamesArray = data.toString().split(";");
                finalSpeciesNameString = finalsettingSpeciesNamesArray.join(" @%$,$%@ ");
            }
            else if (columnName == "Statistics") {

                if (!finalsettingSpeciesNamesArray.isEmpty()) {
                    std::map<QString, SpeciesDetailsStats> statisticsValues = convertToStatisticsMap(data.toString());
                    speciesExpressionMap = statisticsValues;
                    selectedCellCountStatusBarRemove();
                    selectedCellStatisticsStatusBarAdd(statisticsValues, finalsettingSpeciesNamesArray);
                }
                else {
    
                    qDebug() << "Warning: Gene Appearance Species Names must be processed before Statistics.";
                }
            }
        }



        std::vector<std::seed_seq::result_type> selectedSpeciesIndices;
        auto speciesDataset = _settingsAction.getSpeciesNamesDataset().getCurrentDataset();
        auto umapDataset = _settingsAction.getScatterplotEmbeddingPointsUMAPOption().getCurrentDataset();
        auto mainPointsDataset = _settingsAction.getMainPointsDataset().getCurrentDataset();
        std::vector<std::seed_seq::result_type> filtSelectInndx;


        if (!speciesDataset.isValid() || !umapDataset.isValid() || !mainPointsDataset.isValid() || !_settingsAction.getFilteredUMAPDatasetPoints().isValid() || !_settingsAction.getFilteredUMAPDatasetColors().isValid() || !_settingsAction.getFilteredUMAPDatasetClusters().isValid())
        {
            qDebug() << "One of the datasets is not valid";
            return;
        }


        {
            auto speciesClusterDataset = mv::data().getDataset<Clusters>(speciesDataset.getDatasetId());
            auto umapPointsDataset = mv::data().getDataset<Points>(umapDataset.getDatasetId());
            auto fullMainPointsDataset = mv::data().getDataset<Points>(mainPointsDataset.getDatasetId());
            std::unordered_set<QString> speciesNamesSet(finalsettingSpeciesNamesArray.begin(), finalsettingSpeciesNamesArray.end());
            std::map<QString, std::pair<QColor, std::vector<int>>> selectedFilteredUMAPDatasetColorsMap;
            for (const auto& species : speciesClusterDataset->getClusters()) {
                if (speciesNamesSet.find(species.getName()) != speciesNamesSet.end()) {
                    const auto& indices = species.getIndices();

                    selectedSpeciesIndices.insert(selectedSpeciesIndices.end(), indices.begin(), indices.end());
                    selectedFilteredUMAPDatasetColorsMap[species.getName()] = std::make_pair(species.getColor(), std::vector<int>(indices.begin(), indices.end()));
                }
            }

            
            std::vector<std::seed_seq::result_type>& selectedIndicesFromStorage = _settingsAction.getSelectedIndicesFromStorage();
            std::unordered_set<std::seed_seq::result_type> indicesSet(selectedIndicesFromStorage.begin(), selectedIndicesFromStorage.end());
            filtSelectInndx.reserve(selectedSpeciesIndices.size());
            for (int i = 0; i < selectedSpeciesIndices.size(); ++i) {
                if (indicesSet.find(selectedSpeciesIndices[i]) != indicesSet.end()) {
                    filtSelectInndx.push_back(i);
                }
            }
            auto dimensionNamesUmap = umapPointsDataset->getDimensionNames();
            auto numDimensions = umapPointsDataset->getNumDimensions();
            std::vector<int> geneIndicesSpecies(numDimensions);
            std::iota(geneIndicesSpecies.begin(), geneIndicesSpecies.end(), 0);


            if (selectedSpeciesIndices.size() > 0)
            {
                std::vector<float> resultContainerSpeciesUMAP(selectedSpeciesIndices.size()* umapPointsDataset->getNumDimensions());
                umapPointsDataset->populateDataForDimensions(resultContainerSpeciesUMAP, geneIndicesSpecies, selectedSpeciesIndices);
                auto speciesDataId= _settingsAction.getFilteredUMAPDatasetPoints().getDatasetId();
                int tempnumPoints = static_cast<int>(selectedSpeciesIndices.size());
                int tempNumDimensions = static_cast<int>(geneIndicesSpecies.size());
                _settingsAction.populatePointData(speciesDataId, resultContainerSpeciesUMAP, tempnumPoints, tempNumDimensions, dimensionNamesUmap);



                std::vector<float> resultContainerSpeciesColors(selectedSpeciesIndices.size());
                std::vector<int> selectedGeneIndex;

                auto dimensionNames = fullMainPointsDataset->getDimensionNames();
                auto it = std::find(dimensionNames.begin(), dimensionNames.end(), gene);
                if (it != dimensionNames.end()) {
                    selectedGeneIndex.push_back(std::distance(dimensionNames.begin(), it));
                }


                fullMainPointsDataset->populateDataForDimensions(resultContainerSpeciesColors, selectedGeneIndex, selectedSpeciesIndices);
                auto speciesColorDataId = _settingsAction.getFilteredUMAPDatasetColors().getDatasetId();
                auto speciesClusterDataId = _settingsAction.getFilteredUMAPDatasetClusters().getDatasetId();
                int tempnumPointsColors = static_cast<int>(selectedSpeciesIndices.size());
                
                std::vector<QString> columnGeneColors = { gene };
                int tempNumDimensionsColors = static_cast<int>(columnGeneColors.size());
                if (_settingsAction.getApplyLogTransformation().isChecked())
                {
                    applyLogTransformation(resultContainerSpeciesColors);
                }
                _settingsAction.populatePointData(speciesColorDataId, resultContainerSpeciesColors, tempnumPointsColors, tempNumDimensionsColors, columnGeneColors);

                //std::map<QString, std::pair<QColor, std::vector<int>>> selectedFilteredUMAPDatasetColorsMap;
                //TODO: populate the selectedFilteredUMAPDatasetColorsMap


                _settingsAction.populateClusterData(speciesClusterDataId, selectedFilteredUMAPDatasetColorsMap);


                auto scatterplotViewFactory = mv::plugins().getPluginFactory("Scatterplot View");
                mv::gui::DatasetPickerAction* colorDatasetPickerAction;
                mv::gui::DatasetPickerAction* pointDatasetPickerAction;
                mv::gui::ViewPluginSamplerAction* samplerActionAction;

                if (scatterplotViewFactory) {
                    for (auto plugin : mv::plugins().getPluginsByFactory(scatterplotViewFactory)) {
                        if (plugin->getGuiName() == "Scatterplot Embedding View") {
                            //plugin->printChildren();
                            pointDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Position"));
                            if (pointDatasetPickerAction) {
                                pointDatasetPickerAction->setCurrentText("");

                                pointDatasetPickerAction->setCurrentDataset(_settingsAction.getFilteredUMAPDatasetPoints());

                                colorDatasetPickerAction = dynamic_cast<DatasetPickerAction*>(plugin->findChildByPath("Settings/Datasets/Color"));
                                if (colorDatasetPickerAction)
                                {
                                    colorDatasetPickerAction->setCurrentText("");
                                    colorDatasetPickerAction->setCurrentDataset(_settingsAction.getFilteredUMAPDatasetColors());
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
                                                    legendDatasetPickerAction->setCurrentDataset(_settingsAction.getFilteredUMAPDatasetColors());
                                                }
                                                auto chartTitle = dynamic_cast<StringAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Chart Title"));
                                                if (chartTitle)
                                                {
                                                    chartTitle->setString("Gene expression");
                                                }
                                            }
                                        }
                                    }
                                }

                                auto focusSelectionAction = dynamic_cast<ToggleAction*>(plugin->findChildByPath("Settings/Plot/Point/Focus selection"));

                                if (focusSelectionAction)
                                {
                                    focusSelectionAction->setChecked(true);

                                }

                                auto opacityAction = dynamic_cast<DecimalAction*>(plugin->findChildByPath("Settings/Plot/Point/Point opacity/Point opacity"));
                                if (opacityAction)
                                {
                                    opacityAction->setValue(20.0);
                                }
                                
                                samplerActionAction = plugin->findChildByPath<mv::gui::ViewPluginSamplerAction>("Sampler");

                                if (samplerActionAction)
                                {
                                    samplerActionAction->setHtmlViewGeneratorFunction([this](const ViewPluginSamplerAction::SampleContext& toolTipContext) -> QString {
                                        QString clusterDatasetId = _settingsAction.getFilteredUMAPDatasetClusters().getDatasetId();
                                        return _settingsAction.generateTooltip(toolTipContext, clusterDatasetId,true, "GlobalPointIndices");
                                        });
                                }
                            }
                        }
                    }
                }

                

                _settingsAction.getFilteredUMAPDatasetPoints()->setSelectionIndices(filtSelectInndx);
                mv::events().notifyDatasetDataSelectionChanged(_settingsAction.getFilteredUMAPDatasetPoints());


            }


        }









        auto referenceTreeDataset = _settingsAction.getReferenceTreeDatasetAction().getCurrentDataset();
        if (referenceTreeDataset.isValid()) {
            auto referenceTree = mv::data().getDataset<CrossSpeciesComparisonTree>(referenceTreeDataset.getDatasetId());
            if (referenceTree.isValid()) {
                QJsonObject speciesDataJson = referenceTree->getTreeData();
                updateSpeciesData(speciesDataJson, speciesExpressionMap);
                referenceTree->setTreeData(speciesDataJson);
                events().notifyDatasetDataChanged(referenceTree);
            }
        }

        std::vector<std::seed_seq::result_type> selectedPoints;
        auto speciesColorClusterDataset = _settingsAction.getTsneDatasetSpeciesColors();
        auto tsneDataset = _settingsAction.getSelectedPointsTSNEDataset();
        if (speciesColorClusterDataset.isValid() && tsneDataset.isValid()) {
            for (const auto& species : speciesColorClusterDataset->getClusters()) {
                if (finalsettingSpeciesNamesArray.contains(species.getName())) {
                    const auto& indices = species.getIndices();
                    selectedPoints.insert(selectedPoints.end(), indices.begin(), indices.end());
                }
            }
            //below two lines causing problems TODO: Test
            //tsneDataset->setSelectionIndices(selectedPoints);
            //mv::events().notifyDatasetDataSelectionChanged(tsneDataset);
        }

        if (_settingsAction.getScatterplotReembedColorOption().getCurrentText() == "Expression"&& !_settingsAction.getPerformGeneTableTsneAction().isChecked()) {
            
            
            
            auto expressionColorPointDataset = _settingsAction.getTsneDatasetExpressionColors();
            
            auto selectedPointsMain = _settingsAction.getSelectedPointsDataset();

            if (expressionColorPointDataset.isValid() && selectedPointsMain.isValid()) {

                const int rowSize = expressionColorPointDataset->getNumPoints();

                if (rowSize == selectedPointsMain->getNumPoints())
                {
                    std::vector<float> resultContainerColorPoints(rowSize, -1.0);

                    QString datasetIdEmb = expressionColorPointDataset->getId();

                    std::vector<int> indexOfGene;
                    auto dimsValsTemp = selectedPointsMain->getDimensionNames();
                    auto it = std::find(dimsValsTemp.begin(), dimsValsTemp.end(), gene);
                    if (it != dimsValsTemp.end()) {
                        indexOfGene.push_back(static_cast<int>(it - dimsValsTemp.begin()));
                    }

                    std::vector<int> tempselectIndices(selectedPointsMain->getNumPoints());
                    std::iota(tempselectIndices.begin(), tempselectIndices.end(), 0);

                    if (indexOfGene.size() > 0 && tempselectIndices.size() > 0)
                    {
                        selectedPointsMain->populateDataForDimensions(resultContainerColorPoints, indexOfGene, tempselectIndices);



                        int rowSizeEmbd = rowSize;
                        int columnSizeEmbd = 1;
                        std::vector<QString> columnGeneEmbd = { gene };
                        if (_settingsAction.getApplyLogTransformation().isChecked())
                        {
                            applyLogTransformation(resultContainerColorPoints);
                        }
                        //
                        _settingsAction.populatePointData(datasetIdEmb, resultContainerColorPoints, rowSizeEmbd, columnSizeEmbd, columnGeneEmbd);

                    }
                }



            }
        }
        else
        {
            //TODO: Add selected gene point selection to scatterplot
            unsigned int selectedGeneIndex = -1;
            if (_settingsAction.getGeneOrder().size() > 0 && _settingsAction.getSelectedPointsTSNEDatasetForGeneTable().isValid())
            {
                auto it = std::find(_settingsAction.getGeneOrder().begin(), _settingsAction.getGeneOrder().end(), gene);
                if (it != _settingsAction.getGeneOrder().end()) {
                    selectedGeneIndex = it - _settingsAction.getGeneOrder().begin();
                }
                //gene
                unsigned int maxIndex = _settingsAction.getSelectedPointsTSNEDatasetForGeneTable()->getNumPoints();
                if (selectedGeneIndex <= maxIndex)
                {
                    _settingsAction.getSelectedPointsTSNEDatasetForGeneTable()->setSelectionIndices({ selectedGeneIndex });
                    mv::events().notifyDatasetDataSelectionChanged(_settingsAction.getSelectedPointsTSNEDatasetForGeneTable());

                }

            }
            


        }

        if (_settingsAction.getSelctedSpeciesVals().getString() == finalSpeciesNameString)
        {
            _settingsAction.getSelctedSpeciesVals().setString("");
        }
        _settingsAction.getSelctedSpeciesVals().setString(finalSpeciesNameString);

        });

    emit model->layoutChanged();

    

    selectedCellStatisticsStatusBarRemove();
    selectedCellCountStatusBarAdd();

        }
        catch (const std::exception& e) {
            qDebug() << "An exception occurred in modifyListData: " << e.what();
        }
        catch (...) {
            qDebug() << "An unknown exception occurred in modifyListData";
        }



}
void CrossSpeciesComparisonGeneDetectPlugin::selectedCellCountStatusBarAdd()
{
    if (!_settingsAction.getSelectedSpeciesCellCountMap().empty())
    {
        // Create a new model for the table view
        QStandardItemModel* model = new QStandardItemModel();


        QStringList middleSet = _settingsAction.getCurrentHierarchyItemsMiddleForTable();

        bool singleColumn;
        QString headerStringToAdd = "";

        if (middleSet.size() > 1)
        {
            headerStringToAdd = "Neuronal";
            singleColumn = true;
        }
        else if (middleSet.size() == 1)
        {

            
                headerStringToAdd = *middleSet.begin();
                singleColumn = false;
            


        }
        else
        {
            headerStringToAdd = "Abundance";
            singleColumn = true;
        }

        model->setHorizontalHeaderLabels({ "Species", "Fraction in Neuronal", "Fraction in " + headerStringToAdd , "Count of Selected", "Count of All" });
  
        float minValueTopAbundance = std::numeric_limits<float>::max();
        float maxValueTopAbundance = std::numeric_limits<float>::lowest();
        float minValueMiddleAbundance = std::numeric_limits<float>::max();
        float maxValueMiddleAbundance = std::numeric_limits<float>::lowest();
        float minValueSelectedCellsCount = std::numeric_limits<float>::max();
        float maxValueSelectedCellsCount = std::numeric_limits<float>::lowest();
        float minValueTotal = std::numeric_limits<float>::max();
        float maxValueTotal = std::numeric_limits<float>::lowest();
  

        for (const auto& [species, details] : _settingsAction.getSelectedSpeciesCellCountMap())
        { 
        
        float topAbundance = (static_cast<float>(details.countAbundanceNumerator) / static_cast<float>(details.abundanceTop)) * 100;
        float middleAbundance = (static_cast<float>(details.countAbundanceNumerator) / static_cast<float>(details.abundanceMiddle)) * 100;
        float selectedCellsCount = static_cast<float>(details.selectedCellsCount);
        float total = static_cast<float>(details.selectedCellsCount) + static_cast<float>(details.nonSelectedCellsCount);

        if (topAbundance < minValueTopAbundance)
        {
            minValueTopAbundance = topAbundance;
        }
        if (topAbundance > maxValueTopAbundance)
        {
            maxValueTopAbundance = topAbundance;
        }

        if (middleAbundance < minValueMiddleAbundance)
        {
            minValueMiddleAbundance = middleAbundance;
        }
        if (middleAbundance > maxValueMiddleAbundance)
        {
            maxValueMiddleAbundance = middleAbundance;
        }
        
        if (selectedCellsCount < minValueSelectedCellsCount)
        {
            minValueSelectedCellsCount = selectedCellsCount;
        }
        if (selectedCellsCount > maxValueSelectedCellsCount)
        {
            maxValueSelectedCellsCount = selectedCellsCount;
        }

        if (total < minValueTotal)
        {
            minValueTotal = total;
        }
        if (total > maxValueTotal)
        {
            maxValueTotal = total;
        }

        }







        //float value = 0.0f;

        //QColor color = getGrayscaleColor(value, minValue, maxValue);
        //qDebug() << "Grayscale color:" << color;



        for (const auto& [species, details] : _settingsAction.getSelectedSpeciesCellCountMap()) {
            //QColor backgroundColor = QColor(details.color); // Ensure color is converted to QColor
            //qreal brightness = backgroundColor.lightnessF();
            //QColor textColor = (brightness > 0.4) ? Qt::black : Qt::white;

            QList<QStandardItem*> rowItems;

            // Replace underscores in species name for display purposes
            QString speciesDisplayName = species;
            speciesDisplayName.replace("_", " ");
            QStandardItem* speciesItem = new QStandardItem(speciesDisplayName);

            // Check if the path ":/speciesicons" exists
            QDir speciesIconsDir(":/speciesicons");
            if (speciesIconsDir.exists()) {
                //qDebug() << "The path :/speciesicons exists.";
            } else {
                qDebug() << "The path :/speciesicons does not exist.";
            }

            QString iconPath = ":/speciesicons/SpeciesIcons/" + species + ".svg";

            // Check if the SVG file exists
            if (QFile::exists(iconPath)) {
                // Load the SVG icon
                QIcon svgIcon(iconPath);

                // Set the SVG icon as the decoration role (for display in item views)
                speciesItem->setData(svgIcon, Qt::DecorationRole);

                // Set tooltip to show the display name with the SVG icon
                QString tooltipHtml = QString("<img src='%1' width='16' height='16'/> %2").arg(iconPath, speciesDisplayName);
                speciesItem->setData(tooltipHtml, Qt::ToolTipRole);
            } else {
                qDebug() << "Path " << iconPath << " not found";

                // Set tooltip to show the display name without the SVG icon
                speciesItem->setData(speciesDisplayName, Qt::ToolTipRole);
            }

            // Store the original species name in UserRole for retrieval or further processing
            speciesItem->setData(species, Qt::UserRole);

            // Add the species item to the row items list
            rowItems << speciesItem;

            // Fraction of Neuronal column
            QStandardItem* item = new QStandardItem();
            float topAbundance = 0.0;
            if (details.abundanceTop != 0) {
                topAbundance = (static_cast<float>(details.countAbundanceNumerator) / static_cast<float>(details.abundanceTop)) * 100;
            }
            QString formattedValueTop = QString::number(topAbundance, 'f', 2);

            // Set the numeric value for sorting
            item->setData(QVariant(topAbundance), Qt::UserRole);

            // Set the formatted string for display (tooltip)
            item->setData(QVariant(formattedValueTop), Qt::DisplayRole);

            // Create a pixmap for the bar
            float lengthTop = getNormalizedSize(topAbundance, minValueTopAbundance, maxValueTopAbundance);
            QPixmap barPixmapTop(60, 20); // Width 100, Height 20
            barPixmapTop.fill(Qt::transparent);

            QPainter painterTop(&barPixmapTop);
            painterTop.setPen(Qt::NoPen);
            painterTop.setBrush(Qt::gray);
            painterTop.drawRect(0, 0, static_cast<int>(lengthTop), 20);
            painterTop.end();

            // Set the pixmap as the decoration
            item->setData(QVariant(barPixmapTop), Qt::DecorationRole);

            rowItems << item;


            // Fraction of Middle column
            item = new QStandardItem();
            float middleAbundance = 0.0;
            if (details.abundanceMiddle != 0)
            {
                middleAbundance = (static_cast<float>(details.countAbundanceNumerator) / static_cast<float>(details.abundanceMiddle)) * 100;
            }
            QString formattedValueMiddle = QString::number(middleAbundance, 'f', 2);

            // Set the numeric value for sorting
            item->setData(QVariant(middleAbundance), Qt::UserRole);

            // Set the formatted string for display
            item->setData(QVariant(formattedValueMiddle), Qt::DisplayRole);

            // Create a pixmap for the bar
            float lengthMiddle = getNormalizedSize(middleAbundance, minValueMiddleAbundance, maxValueMiddleAbundance);
            QPixmap barPixmapMiddle(80, 20); // Width 100, Height 20
            barPixmapMiddle.fill(Qt::transparent);

            QPainter painterMiddle(&barPixmapMiddle);
            painterMiddle.setPen(Qt::NoPen);
            painterMiddle.setBrush(Qt::gray);
            painterMiddle.drawRect(0, 0, static_cast<int>(lengthMiddle), 20);
            painterMiddle.end();

            // Set the pixmap as the decoration
            item->setData(QVariant(barPixmapMiddle), Qt::DecorationRole);


            rowItems << item;

            // Count Selected column
            item = new QStandardItem();
            int selectedCellsCount = details.selectedCellsCount;
            QString  formattedSelectedCellsCount = QString::number(selectedCellsCount);
            item->setData(QVariant(selectedCellsCount), Qt::UserRole);
            item->setData(QVariant(formattedSelectedCellsCount), Qt::DisplayRole);
            // Create a pixmap for the bar
            float lengthSelectedCellsCount = getNormalizedSize(selectedCellsCount, minValueSelectedCellsCount, maxValueSelectedCellsCount);
            QPixmap barPixmapSelectedCellsCount(60, 20); // Width 100, Height 20
            barPixmapSelectedCellsCount.fill(Qt::transparent);

            QPainter painterSelectedCellsCount(&barPixmapSelectedCellsCount);
            painterSelectedCellsCount.setPen(Qt::NoPen);
            painterSelectedCellsCount.setBrush(Qt::gray);
            painterSelectedCellsCount.drawRect(0, 0, static_cast<int>(lengthSelectedCellsCount), 20);
            painterSelectedCellsCount.end();

            // Set the pixmap as the decoration
            item->setData(QVariant(barPixmapSelectedCellsCount), Qt::DecorationRole);

            rowItems << item;

            // Count All column
            item = new QStandardItem();
            auto total = details.selectedCellsCount + details.nonSelectedCellsCount;
            QString formattedTotal = QString::number(total);
            item->setData(QVariant(total), Qt::UserRole);
            item->setData(QVariant(formattedTotal), Qt::DisplayRole);
            // Create a pixmap for the bar
            float lengthTotal = getNormalizedSize(total, minValueTotal, maxValueTotal);
            QPixmap barPixmapTotal(60, 20); // Width 100, Height 20
            barPixmapTotal.fill(Qt::transparent);

            QPainter painterTotal(&barPixmapTotal);
            painterTotal.setPen(Qt::NoPen);
            painterTotal.setBrush(Qt::gray);
            painterTotal.drawRect(0, 0, static_cast<int>(lengthTotal), 20);
            painterTotal.end();

            // Set the pixmap as the decoration
            item->setData(QVariant(barPixmapTotal), Qt::DecorationRole);

            rowItems << item;

            model->appendRow(rowItems);
        }

        // Enable sorting on the table view
        _settingsAction.getSelectionDetailsTable()->setSortingEnabled(true);

        // Set the model to the table view
        _settingsAction.getSelectionDetailsTable()->setModel(model);
        model->setSortRole(Qt::UserRole);
        // Sort the model by the fourth column (index 3) in descending order
        model->sort(1, Qt::DescendingOrder);

        // Configure the table view
        _settingsAction.getSelectionDetailsTable()->setSelectionMode(QAbstractItemView::NoSelection);
        _settingsAction.getSelectionDetailsTable()->verticalHeader()->hide();

        if (singleColumn)
        {
            _settingsAction.getSelectionDetailsTable()->hideColumn(2);
        }

        // Fixed widths are cheaper than recomputing content widths on every refresh.
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(1, 70);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(2, 90);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(3, 70);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(4, 70);

        // Update the view (layoutChanged signal is typically not required here)
        _settingsAction.getSelectionDetailsTable()->update();

    }

    adjustTableWidths("small");
}



void CrossSpeciesComparisonGeneDetectPlugin::selectedCellStatisticsStatusBarAdd(std::map<QString, SpeciesDetailsStats> statisticsValues, QStringList selectedSpecies)
{
    if (!_settingsAction.getSelectedSpeciesCellCountMap().empty())
    {
        // Create a new model for the table view
        QStandardItemModel* model = new QStandardItemModel();

        QStringList middleSet = _settingsAction.getCurrentHierarchyItemsMiddleForTable();

        bool singleColumn;
        QString headerStringToAdd = "";

        if (middleSet.size() > 1)
        {
            headerStringToAdd = "Neuronal";
            singleColumn = true;
        }
        else if (middleSet.size() == 1)
        {


            headerStringToAdd = *middleSet.begin();
            singleColumn = false;



        }
        else
        {
            headerStringToAdd = "Abundance";
            singleColumn = true;
        }

        model->setHorizontalHeaderLabels({ "Species", "Mean Gene Differential Expression", "Gene Appearance Rank", "Fraction in Neuronal", "Fraction in " + headerStringToAdd, "Count of Selected", "Mean Gene Expression of Selected", "Count of Non Selected", "Mean Gene Expression of Non Selected" });

        //auto colorValues = _settingsAction.getSystemModeColor();
        //auto systemColor = colorValues[0];
        //auto valuesColor = colorValues[1];

        float minDifference = std::numeric_limits<float>::max();
        float maxDifference = std::numeric_limits<float>::lowest();
        float minRank = std::numeric_limits<float>::max();
        float maxRank = std::numeric_limits<float>::lowest();
        float minTopAbundance = std::numeric_limits<float>::max();
        float maxTopAbundance = std::numeric_limits<float>::lowest();
        float minMiddleAbundance = std::numeric_limits<float>::max();
        float maxMiddleAbundance = std::numeric_limits<float>::lowest();
        float minSelectedCellsCount = std::numeric_limits<float>::max();
        float maxSelectedCellsCount = std::numeric_limits<float>::lowest();
        float minMeanSelected = std::numeric_limits<float>::max();
        float maxMeanSelected = std::numeric_limits<float>::lowest();
        float minNonSelectedCellsCount = std::numeric_limits<float>::max();
        float maxNonSelectedCellsCount = std::numeric_limits<float>::lowest();
        float minMeanNonSelected = std::numeric_limits<float>::max();
        float maxMeanNonSelected = std::numeric_limits<float>::lowest();
        int numofGenes = _settingsAction.getTopNGenesFilter().getMaximum();

        for (const auto& [species, details] : _settingsAction.getSelectedSpeciesCellCountMap())
        {
            auto it = statisticsValues.find(species);
            if (it != statisticsValues.end()) {
                float difference = (it->second.meanSelected - it->second.meanNonSelected);
                if (difference < minDifference)
                {
                    minDifference = difference;
                }
                if (difference > maxDifference)
                {
                    maxDifference = difference;
                }
                int temprank = numofGenes - it->second.rank;
                float rankVal = static_cast<float>(temprank);
                if (rankVal < minRank)
                {
                    minRank = rankVal;
                }
                if (rankVal > maxRank)
                {
                    maxRank = rankVal;
                }

                float topAbundance = 0.0;
                if (it->second.abundanceTop != 0) {
                    topAbundance = (static_cast<float>(it->second.countAbundanceNumerator) / static_cast<float>(it->second.abundanceTop)) * 100;
                }
                if (topAbundance < minTopAbundance)
                {
                    minTopAbundance = topAbundance;
                }
                if (topAbundance > maxTopAbundance)
                {
                    maxTopAbundance = topAbundance;
                }

                float middleAbundance = 0.0;
                if (it->second.abundanceMiddle != 0)
                {
                    middleAbundance = (static_cast<float>(it->second.countAbundanceNumerator) / static_cast<float>(it->second.abundanceMiddle)) * 100;
                }
                if (middleAbundance < minMiddleAbundance)
                {
                    minMiddleAbundance = middleAbundance;
                }
                if (middleAbundance > maxMiddleAbundance)
                {
                    maxMiddleAbundance = middleAbundance;
                }
                float selectedCellsCount = static_cast<float>(it->second.countSelected);
                if (selectedCellsCount < minSelectedCellsCount)
                {
                    minSelectedCellsCount = selectedCellsCount;
                }
                if (selectedCellsCount > maxSelectedCellsCount)
                {
                    maxSelectedCellsCount = selectedCellsCount;
                }

                if (it->second.meanSelected < minMeanSelected)
                {
                    minMeanSelected = it->second.meanSelected;
                }
                if (it->second.meanSelected > maxMeanSelected)
                {
                    maxMeanSelected = it->second.meanSelected;
                }
                float nonSelectedCellsCount = static_cast<float>(it->second.countNonSelected);
                if (nonSelectedCellsCount < minNonSelectedCellsCount)
                {
                    minNonSelectedCellsCount = nonSelectedCellsCount;
                }
                if (nonSelectedCellsCount > maxNonSelectedCellsCount)
                {
                    maxNonSelectedCellsCount = nonSelectedCellsCount;
                }

                if (it->second.meanNonSelected < minMeanNonSelected)
                {
                    minMeanNonSelected = it->second.meanNonSelected;
                }
                if (it->second.meanNonSelected > maxMeanNonSelected)
                {
                    maxMeanNonSelected = it->second.meanNonSelected;
                }


            }
            
        }
        

        // Populate the model with sorted data and statistics
        for (const auto& [species, details] : _settingsAction.getSelectedSpeciesCellCountMap()) {
            //QColor backgroundColor = QColor(details.color);

            // Calculate the brightness of the background color
            //qreal brightness = backgroundColor.lightnessF();

            // Choose text color based on the brightness of the background color
            //QColor textColor = (brightness > 0.4) ? Qt::black : Qt::white;

            QList<QStandardItem*> rowItems;
            QString speciesCopy = species;
            speciesCopy.replace("_", " ");
            QStandardItem* item = new QStandardItem(speciesCopy);

            // Check if the path ":/speciesicons" exists
            QDir speciesIconsDir(":/speciesicons");
            if (speciesIconsDir.exists()) {
                //qDebug() << "The path :/speciesicons exists.";
            }
            else {
                qDebug() << "The path :/speciesicons does not exist.";
            }

            QString iconPath = ":/speciesicons/SpeciesIcons/" + species + ".svg";

            // Check if the SVG file exists
            if (QFile::exists(iconPath)) {
                // Load the SVG icon
                QIcon svgIcon(iconPath);

                // Set the SVG icon as the decoration role (for display in item views)
                item->setData(svgIcon, Qt::DecorationRole);

                // Set tooltip to show the display name with the SVG icon
                QString tooltipHtml = QString("<img src='%1' width='16' height='16'/> %2").arg(iconPath, speciesCopy);
                item->setData(tooltipHtml, Qt::ToolTipRole);
            }
            else {
                qDebug() << "Path " << iconPath << " not found";

                // Set tooltip to show the display name without the SVG icon
                item->setData(speciesCopy, Qt::ToolTipRole);
            }

            // Store the original species name in UserRole for retrieval or further processing
            item->setData(species, Qt::UserRole);

            // Add the species item to the row items list
            rowItems << item;


            // Find statistics for the species
            auto it = statisticsValues.find(species);
            if (it != statisticsValues.end()) {
                QStandardItem* item;

                item = new QStandardItem();
                float difference = (it->second.meanSelected - it->second.meanNonSelected);
                QString formattedValue = QString::number(difference, 'f', 2);
                item->setData(QVariant(difference), Qt::UserRole);
                item->setData(QVariant(formattedValue), Qt::DisplayRole);

                // Create a pixmap for the bar
                float length = getNormalizedSizePosNeg(difference, minDifference, maxDifference);
                QPixmap barPixmap(75, 25); // Width 75, Height 25 to allow space for the axis line
                barPixmap.fill(Qt::transparent);
                QPainter painter(&barPixmap);
                painter.setPen(Qt::NoPen);
                painter.setBrush(Qt::gray);

                // Draw the bar from the middle
                int middle = barPixmap.width() / 2;
                int barHeight = 20; // Height of the bar
                int barY = (barPixmap.height() - barHeight) / 2; // Center the bar vertically
                if (difference >= 0) {
                    painter.drawRect(middle, barY, static_cast<int>(length), barHeight);
                }
                else {
                    painter.drawRect(middle - static_cast<int>(length), barY, static_cast<int>(length), barHeight);
                }

                // Draw the axis line in the middle to indicate 0
                painter.setPen(Qt::black);
                int axisHeight = barHeight + 4; // Extend the axis line height
                int axisYStart = (barPixmap.height() - axisHeight) / 2; // Center the axis line vertically
                painter.drawLine(middle, axisYStart, middle, axisYStart + axisHeight);

                painter.end();
                item->setData(QVariant(barPixmap), Qt::DecorationRole);
                rowItems << item;



                // Main logic for creating a rank item with a bar
                item = new QStandardItem();
                int rank = it->second.rank;

                QString formattedRank = QString::number(rank);
                item->setData(QVariant(rank), Qt::UserRole);
                item->setData(QVariant(formattedRank), Qt::DisplayRole);

                // Logarithmic calculations for the ranks
                float logOfNumofGenes = std::log(numofGenes) / std::log(_settingsAction.getTopNGenesFilter().getValue());
                float logOfRank = std::log(rank) / std::log(_settingsAction.getTopNGenesFilter().getValue());

                // Adjust the logarithmic values for the rank difference
                float logofChangeRank = logOfNumofGenes - logOfRank;

                // Ensure the min and max logarithmic values are calculated properly
                float logofMinRank = std::log(1) / std::log(_settingsAction.getTopNGenesFilter().getValue());
                float logofMaxRank = std::log(maxRank) / std::log(_settingsAction.getTopNGenesFilter().getValue());

                // Normalize the rank length for the bar
                float lengthRank = getNormalizedSizeRank(logofChangeRank, logofMinRank, logofMaxRank);

                // Ensure the length is within a reasonable range (0 to 100)
                lengthRank = qBound(0.0f, lengthRank, 100.0f); // Ensure the lengthRank is within bounds

                // Create the pixmap for the rank bar
                QPixmap barPixmapRank(75, 20); // Width 100, Height 20
                barPixmapRank.fill(Qt::transparent); // Start with a transparent background

                // Create a painter to draw on the pixmap
                QPainter painterRank(&barPixmapRank);
                painterRank.setPen(Qt::NoPen); // No outline
                painterRank.setBrush(Qt::gray); // Bar color
                painterRank.drawRect(0, 0, static_cast<int>(lengthRank), 20); // Draw the bar

                painterRank.end(); // End painting

                // Set the bar pixmap as the decoration for the item
                item->setData(QVariant(barPixmapRank), Qt::DecorationRole);

                rowItems << item;

                item = new QStandardItem();
                float topAbundance = 0.0;
                if (it->second.abundanceTop != 0) {
                    topAbundance = (static_cast<float>(it->second.countAbundanceNumerator) / static_cast<float>(it->second.abundanceTop)) * 100;
                }
                QString formattedValueTop = QString::number(topAbundance, 'f', 2);
                item->setData(QVariant(topAbundance), Qt::UserRole);
                item->setData(QVariant(formattedValueTop), Qt::DisplayRole);
                float lengthTop = getNormalizedSize(topAbundance, minTopAbundance, maxTopAbundance);
                QPixmap barPixmapTop(60, 20); // Width 100, Height 20
                barPixmapTop.fill(Qt::transparent);
                QPainter painterTop(&barPixmapTop);
                painterTop.setPen(Qt::NoPen);
                painterTop.setBrush(Qt::gray);
                painterTop.drawRect(0, 0, static_cast<int>(lengthTop), 20);
                painterTop.end();
                item->setData(QVariant(barPixmapTop), Qt::DecorationRole);
                rowItems << item;


                float middleAbundance = 0.0;
                if (it->second.abundanceMiddle != 0)
                {
                    middleAbundance = (static_cast<float>(it->second.countAbundanceNumerator) / static_cast<float>(it->second.abundanceMiddle)) * 100;
                }
                item = new QStandardItem();
                QString formattedValueMiddle = QString::number(middleAbundance, 'f', 2);
                item->setData(QVariant(middleAbundance), Qt::UserRole);
                item->setData(QVariant(formattedValueMiddle), Qt::DisplayRole);
                float lengthMiddle = getNormalizedSize(middleAbundance, minMiddleAbundance, maxMiddleAbundance);
                QPixmap barPixmapMiddle(80, 20); // Width 100, Height 20
                barPixmapMiddle.fill(Qt::transparent);
                QPainter painterMiddle(&barPixmapMiddle);
                painterMiddle.setPen(Qt::NoPen);
                painterMiddle.setBrush(Qt::gray);
                painterMiddle.drawRect(0, 0, static_cast<int>(lengthMiddle), 20);
                painterMiddle.end();
                item->setData(QVariant(barPixmapMiddle), Qt::DecorationRole);
                rowItems << item;




                item = new QStandardItem();
                int selectedCellsCount = it->second.countSelected;
                QString formattedSelectedCellsCount = QString::number(selectedCellsCount);
                item->setData(QVariant(selectedCellsCount), Qt::UserRole);
                item->setData(QVariant(formattedSelectedCellsCount), Qt::DisplayRole);
                float lengthSelectedCellsCount = getNormalizedSize(selectedCellsCount, minSelectedCellsCount, maxSelectedCellsCount);
                QPixmap barPixmapSelectedCellsCount(55, 20); // Width 100, Height 20
                barPixmapSelectedCellsCount.fill(Qt::transparent);
                QPainter painterSelectedCellsCount(&barPixmapSelectedCellsCount);
                painterSelectedCellsCount.setPen(Qt::NoPen);
                painterSelectedCellsCount.setBrush(Qt::gray);
                painterSelectedCellsCount.drawRect(0, 0, static_cast<int>(lengthSelectedCellsCount), 20);
                painterSelectedCellsCount.end();
                item->setData(QVariant(barPixmapSelectedCellsCount), Qt::DecorationRole);
                rowItems << item;

                

                item = new QStandardItem();
                float meanSelected = it->second.meanSelected;
                QString formattedMeanSelected = QString::number(meanSelected, 'f', 2);
                item->setData(QVariant(meanSelected), Qt::UserRole);
                item->setData(QVariant(formattedMeanSelected), Qt::DisplayRole);
                float lengthMeanSelected = getNormalizedSize(meanSelected, minMeanSelected, maxMeanSelected);
                QPixmap barPixmapMeanSelected(70, 20); // Width 100, Height 20
                barPixmapMeanSelected.fill(Qt::transparent);
                QPainter painterMeanSelected(&barPixmapMeanSelected);
                painterMeanSelected.setPen(Qt::NoPen);
                painterMeanSelected.setBrush(Qt::gray);
                painterMeanSelected.drawRect(0, 0, static_cast<int>(lengthMeanSelected), 20);
                painterMeanSelected.end();
                item->setData(QVariant(barPixmapMeanSelected), Qt::DecorationRole);
                rowItems << item;


                item = new QStandardItem();
                int nonSelectedCellsCount = it->second.countNonSelected;
                QString formattedNonSelectedCellsCount = QString::number(nonSelectedCellsCount);
                item->setData(QVariant(nonSelectedCellsCount), Qt::UserRole);
                item->setData(QVariant(formattedNonSelectedCellsCount), Qt::DisplayRole);
                float lengthNonSelectedCellsCount = getNormalizedSize(nonSelectedCellsCount, minNonSelectedCellsCount, maxNonSelectedCellsCount);
                QPixmap barPixmapNonSelectedCellsCount(60, 20); // Width 100, Height 20
                barPixmapNonSelectedCellsCount.fill(Qt::transparent);
                QPainter painterNonSelectedCellsCount(&barPixmapNonSelectedCellsCount);
                painterNonSelectedCellsCount.setPen(Qt::NoPen);
                painterNonSelectedCellsCount.setBrush(Qt::gray);
                painterNonSelectedCellsCount.drawRect(0, 0, static_cast<int>(lengthNonSelectedCellsCount), 20);
                painterNonSelectedCellsCount.end();
                item->setData(QVariant(barPixmapNonSelectedCellsCount), Qt::DecorationRole);
                rowItems << item;
                

                item = new QStandardItem();
                float meanNonSelected = it->second.meanNonSelected;
                QString formattedMeanNonSelected = QString::number(it->second.meanNonSelected, 'f', 2);
                item->setData(QVariant(meanNonSelected), Qt::UserRole);
                item->setData(QVariant(formattedMeanNonSelected), Qt::DisplayRole);
                float lengthMeanNonSelected = getNormalizedSize(meanNonSelected, minMeanNonSelected, maxMeanNonSelected);
                QPixmap barPixmapMeanNonSelected(75, 20); // Width 100, Height 20
                barPixmapMeanNonSelected.fill(Qt::transparent);
                QPainter painterMeanNonSelected(&barPixmapMeanNonSelected);
                painterMeanNonSelected.setPen(Qt::NoPen);
                painterMeanNonSelected.setBrush(Qt::gray);
                painterMeanNonSelected.drawRect(0, 0, static_cast<int>(lengthMeanNonSelected), 20);
                painterMeanNonSelected.end();
                item->setData(QVariant(barPixmapMeanNonSelected), Qt::DecorationRole);
                rowItems << item;

            }
            else {
                qDebug() << "Species: " + species + " not found in map auto it = statisticsValues.find(species);";
                for (auto it = statisticsValues.begin(); it != statisticsValues.end(); ++it)
                {
                    qDebug() << it->first;
                }
                rowItems << new QStandardItem("N/A");
                rowItems << new QStandardItem("N/A");
                rowItems << new QStandardItem("N/A");
                rowItems << new QStandardItem("N/A");
                rowItems << new QStandardItem("N/A");
                rowItems << new QStandardItem("N/A");
                rowItems << new QStandardItem("N/A");
                rowItems << new QStandardItem("N/A");
            }
            
            if (selectedSpecies.contains(species)) {
                for (int i = 0; i < rowItems.size(); ++i) {
                    rowItems[i]->setBackground(QBrush(QColor("#00A2ED")));
                    rowItems[i]->setForeground(QBrush(QColor("#000000")));
                }
            }
            else
            {
                for (int i = 0; i < rowItems.size(); ++i) {
                    rowItems[i]->setBackground(QBrush(QColor("#FFFFFF")));
                    rowItems[i]->setForeground(QBrush(QColor("#000000")));
                }
            }
            _settingsAction.getSpeciesExplorerInMap().setSelectedOptions(selectedSpecies);

            model->appendRow(rowItems);
        }
        _settingsAction.getSelectionDetailsTable()->setModel(model);
        model->setSortRole(Qt::UserRole);
        model->sort(2, _settingsAction.getTypeofTopNGenes().getCurrentText() == "Positive" || _settingsAction.getTypeofTopNGenes().getCurrentText() == "Absolute" ? Qt::AscendingOrder : Qt::DescendingOrder);

        _settingsAction.getSelectionDetailsTable()->setSelectionMode(QAbstractItemView::SingleSelection);
        _settingsAction.getSelectionDetailsTable()->verticalHeader()->hide();
        if (singleColumn) {
            _settingsAction.getSelectionDetailsTable()->hideColumn(4);
        }
        // Keep widths stable to avoid an extra full-table measurement pass.
        //_settingsAction.getSelectionDetailsTable()->setColumnWidth(0, 100);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(1, 85);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(2, 85);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(3, 70);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(4, 90);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(5, 65);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(6, 80);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(7, 70);
        _settingsAction.getSelectionDetailsTable()->setColumnWidth(8, 85);
        _settingsAction.getSelectionDetailsTable()->update();



        emit model->layoutChanged();

        // Selection handling code
        disconnect(_settingsAction.getSelectionDetailsTable()->selectionModel(), nullptr, this, nullptr);
        connect(_settingsAction.getSelectionDetailsTable()->selectionModel(), &QItemSelectionModel::selectionChanged, [this](const QItemSelection& selected, const QItemSelection& deselected) {
            static QModelIndex lastSelectedIndex;

            if (selected.isEmpty()) {
                return;
            }
            QModelIndexList selectedIndexes = selected.indexes();
            if (selectedIndexes.isEmpty()) {
                return;
            }
            QModelIndex selectedIndex = selectedIndexes.at(0);
            if (!selectedIndex.isValid()) {
                return;
            }

            lastSelectedIndex = selectedIndex;
            // Retrieve the original species value from the user role
            QString species = selectedIndex.siblingAtColumn(0).data(Qt::UserRole).toString();
            QStringList autoSpecies = { species };

            if (!(autoSpecies.size() == 1 && autoSpecies.first().isEmpty())) {
                if (_settingsAction.getSpeciesExplorerInMap().getNumberOfOptions() > 0) {
                    _settingsAction.getSpeciesExplorerInMap().setSelectedOptions(autoSpecies);
                    geneExplorer();
                }
            }
            _settingsAction.getClearRightClickedCluster().trigger();
            });
    }
    adjustTableWidths("large");
}

void CrossSpeciesComparisonGeneDetectPlugin::updatePhylogeneticTree()
{
    if (_settingsAction.getGeneTableView())
    {
        QString treeData = "";// = _settingsAction.getGeneTableView()->model()->index(0, 2).data().toString();
        QTableView* geneTableView = _settingsAction.getGeneTableView();
        QAbstractItemModel* model = geneTableView->model();
        if (model != nullptr)
        {
            int statisticsColumn = -1;
            for (int col = 0; col < _settingsAction.getGeneTableView()->model()->columnCount(); ++col) {
                if (_settingsAction.getGeneTableView()->model()->headerData(col, Qt::Horizontal).toString() == "Statistics") {
                    statisticsColumn = col;
                    break;
                }
            }

            if (statisticsColumn != -1) {
                QModelIndex index = _settingsAction.getGeneTableView()->model()->index(0, statisticsColumn);
                QVariant value = _settingsAction.getGeneTableView()->model()->data(index);
                // Use the value as needed
                std::map<QString, SpeciesDetailsStats> statisticsValues = convertToStatisticsMap(value.toString());



                auto referenceTreeDataset = _settingsAction.getReferenceTreeDatasetAction().getCurrentDataset();
                if (referenceTreeDataset.isValid()) {
                    auto referenceTree = mv::data().getDataset<CrossSpeciesComparisonTree>(referenceTreeDataset.getDatasetId());
                    if (referenceTree.isValid()) {
                        QJsonObject speciesDataJson = referenceTree->getTreeData();
                        updateTreeData(speciesDataJson, statisticsValues);
                        referenceTree->setTreeData(speciesDataJson);
                        events().notifyDatasetDataChanged(referenceTree);
                    }
                }

            }
            else {
                // Handle the case where the "Statistics" column is not found
                //std::cerr << "Statistics column not found in the model." << std::endl;
            }

        }

    }
}

void CrossSpeciesComparisonGeneDetectPlugin::selectedCellCountStatusBarRemove()
{
    _settingsAction.getSelectionDetailsTable()->setModel(new QStandardItemModel());
}

void CrossSpeciesComparisonGeneDetectPlugin::selectedCellStatisticsStatusBarRemove()
{
    _settingsAction.getSelectionDetailsTable()->setModel(new QStandardItemModel());
}
void CrossSpeciesComparisonGeneDetectPlugin::updateSpeciesData(QJsonObject& node, const std::map<QString, SpeciesDetailsStats>& speciesExpressionMap) {
    // Check if the "name" key exists in the current node
    if (node.contains("name")) {
        QString nodeName = node["name"].toString();
        auto it = speciesExpressionMap.find(nodeName);

        if (it != speciesExpressionMap.end()) {
            node["mean"] = std::round(it->second.meanSelected * 100.0) / 100.0; // Round to 2 decimal places
            float differentialVal = it->second.meanSelected - it->second.meanNonSelected;
            node["differential"] = std::round(differentialVal * 100.0) / 100.0; // Round to 2 decimal places
            int rank = it->second.rank;
            node["rank"] = rank;
            node["gene"] = _settingsAction.getSelectedGeneAction().getString();
            QString clusterNames = "N/A";
            if (_settingsAction.getTsneDatasetClusterColors().isValid())
            {
                //auto clusterDataset = mv::data().getDataset<Clusters>(_tsneDatasetClusterColors.getDatasetId());

                auto clusterValues = _settingsAction.getTsneDatasetClusterColors()->getClusters();

                if (!clusterValues.empty())
                {
                    clusterNames = "";
                    if (clusterValues.size() > 1)
                    {
                        clusterNames = "selected cells";
                    }
                    else
                    {
                        clusterNames = clusterValues.at(0).getName();
                    }

                }
            }
            node["clusterName"] = clusterNames;


            QStringList middleSet = _settingsAction.getCurrentHierarchyItemsMiddleForTable();

            bool singleColumn;
            QString headerStringToAdd = "";

            if (middleSet.size() > 1)
            {
                headerStringToAdd = "Neuronal";
                singleColumn = true;
            }
            else if (middleSet.size() == 1)
            {


                headerStringToAdd = *middleSet.begin();
                singleColumn = false;
            }
            else
            {
                headerStringToAdd = "Neuronal";
            }


            node["middleAbundanceClusterName"] = headerStringToAdd;

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

            node["abundanceTop"] = std::round(topAbundance * 100.0) / 100.0;
            node["abundanceMiddle"] = std::round(middleAbundance * 100.0) / 100.0;
        }
        if (it != speciesExpressionMap.end()) {
            node["cellCounts"] = it->second.countSelected;

        }
    }

    // If the node has "children", recursively update them as well
    if (node.contains("children")) {
        QJsonArray children = node["children"].toArray();
        for (int i = 0; i < children.size(); ++i) {
            QJsonObject child = children[i].toObject();
            updateSpeciesData(child, speciesExpressionMap); // Recursive call
            children[i] = child; // Update the modified object back into the array
        }
        node["children"] = children; // Update the modified array back into the parent JSON object
    }
}

void CrossSpeciesComparisonGeneDetectPlugin::updateTreeData(QJsonObject& node, const std::map<QString, SpeciesDetailsStats>& speciesExpressionMap) {
    // Check if the "name" key exists in the current node
    if (node.contains("name")) {
        QString nodeName = node["name"].toString();
        auto it = speciesExpressionMap.find(nodeName);

        if (it != speciesExpressionMap.end()) {
            node["mean"] = 0.0;
            node["differential"] = 0.0; 
            node["rank"] = 0.0;
            node["gene"] = "";
            QString clusterNames = "N/A";
            if (_settingsAction.getTsneDatasetClusterColors().isValid())
            {
                //auto clusterDataset = mv::data().getDataset<Clusters>(_tsneDatasetClusterColors.getDatasetId());

                auto clusterValues = _settingsAction.getTsneDatasetClusterColors()->getClusters();

                if (!clusterValues.empty())
                {
                    clusterNames = "";
                    if (clusterValues.size() > 1)
                    {
                        clusterNames = "selected cells";
                    }
                    else
                    {
                        clusterNames= clusterValues.at(0).getName();
                    }

                }
            }
            node["clusterName"] = clusterNames;


            QStringList middleSet = _settingsAction.getCurrentHierarchyItemsMiddleForTable();

            bool singleColumn;
            QString headerStringToAdd = "";

            if (middleSet.size() > 1)
            {
                headerStringToAdd = "Neuronal";
                singleColumn = true;
            }
            else if (middleSet.size() == 1)
            {


                headerStringToAdd = *middleSet.begin();
                singleColumn = false;
            }
            else
            {
                headerStringToAdd = "Neuronal";
            }


            node["middleAbundanceClusterName"] = headerStringToAdd;

            float topAbundance = 0.0;
            if (it->second.abundanceTop != 0)
            {
                topAbundance =  (static_cast<float>(it->second.countAbundanceNumerator) / static_cast<float>(it->second.abundanceTop)) * 100;
            }

            float middleAbundance = 0.0;
            if (it->second.abundanceMiddle != 0)
            {
                middleAbundance = (static_cast<float>(it->second.countAbundanceNumerator) / static_cast<float>(it->second.abundanceMiddle)) * 100;
            }

            node["abundanceTop"] = std::round(topAbundance * 100.0) / 100.0;
            node["abundanceMiddle"] =  std::round(middleAbundance * 100.0) / 100.0;
        }
        if (it != speciesExpressionMap.end()) {
            node["cellCounts"] = it->second.countSelected;

        }
    }

    // If the node has "children", recursively update them as well
    if (node.contains("children")) {
        QJsonArray children = node["children"].toArray();
        for (int i = 0; i < children.size(); ++i) {
            QJsonObject child = children[i].toObject();
            updateSpeciesData(child, speciesExpressionMap); // Recursive call
            children[i] = child; // Update the modified object back into the array
        }
        node["children"] = children; // Update the modified array back into the parent JSON object
    }
}
/*
void CrossSpeciesComparisonGeneDetectPlugin::onDataEvent(mv::DatasetEvent* dataEvent)
{
    // Get smart pointer to dataset that changed
    const auto changedDataSet = dataEvent->getDataset();

    // Get GUI name of the dataset that changed
    const auto datasetGuiName = changedDataSet->getGuiName();

    // The data event has a type so that we know what type of data event occurred (e.g. data added, changed, removed, renamed, selection changes)
    switch (dataEvent->getType()) {

        // A points dataset was added
        case EventType::DatasetAdded:
        {
            // Cast the data event to a data added event
            const auto dataAddedEvent = static_cast<DatasetAddedEvent*>(dataEvent);

            // Get the GUI name of the added points dataset and print to the console
            qDebug() << datasetGuiName << "was added";

            break;
        }

        // Points dataset data has changed
        case EventType::DatasetDataChanged:
        {
            // Cast the data event to a data changed event
            const auto dataChangedEvent = static_cast<DatasetDataChangedEvent*>(dataEvent);

            // Get the name of the points dataset of which the data changed and print to the console
            qDebug() << datasetGuiName << "data changed";

            break;
        }

        // Points dataset data was removed
        case EventType::DatasetRemoved:
        {
            // Cast the data event to a data removed event
            const auto dataRemovedEvent = static_cast<DatasetRemovedEvent*>(dataEvent);

            // Get the name of the removed points dataset and print to the console
            qDebug() << datasetGuiName << "was removed";

            break;
        }

        // Points dataset selection has changed
        case EventType::DatasetDataSelectionChanged:
        {
            // Cast the data event to a data selection changed event
            const auto dataSelectionChangedEvent = static_cast<DatasetDataSelectionChangedEvent*>(dataEvent);

            // Get the selection set that changed
            const auto& selectionSet = changedDataSet->getSelection<Points>();

            // Print to the console
            qDebug() << datasetGuiName << "selection has changed";

            break;
        }

        default:
            break;
    }
}
*/

void CrossSpeciesComparisonGeneDetectPlugin::fromVariantMap(const QVariantMap& variantMap)
{
    ViewPlugin::fromVariantMap(variantMap);

    mv::util::variantMapMustContain(variantMap, "CSCGDV:CrossSpeciesComparison Gene Detect Plugin Settings");
    _settingsAction.fromVariantMap(variantMap["CSCGDV:CrossSpeciesComparison Gene Detect Plugin Settings"].toMap());
   // modifyTableData();
   // _settingsAction.getStartComputationTriggerAction().trigger();

}

QVariantMap CrossSpeciesComparisonGeneDetectPlugin::toVariantMap() const
{
    QVariantMap variantMap = ViewPlugin::toVariantMap();

    _settingsAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
ViewPlugin* CrossSpeciesComparisonGeneDetectPluginFactory::produce()
{
    return new CrossSpeciesComparisonGeneDetectPlugin(this);
}

mv::DataTypes CrossSpeciesComparisonGeneDetectPluginFactory::supportedDataTypes() const
{
    DataTypes supportedTypes;

    // This example analysis plugin is compatible with points datasets
    supportedTypes.append(PointType);

    return supportedTypes;
}

mv::gui::PluginTriggerActions CrossSpeciesComparisonGeneDetectPluginFactory::getPluginTriggerActions(const mv::Datasets& datasets) const
{
    PluginTriggerActions pluginTriggerActions;
    /*
    const auto getPluginInstance = [this]() -> CrossSpeciesComparisonGeneDetectPlugin* {
        return dynamic_cast<CrossSpeciesComparisonGeneDetectPlugin*>(plugins().requestViewPlugin(getKind()));
    };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets >= 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType)) {
        auto pluginTriggerAction = new PluginTriggerAction(const_cast<CrossSpeciesComparisonGeneDetectPluginFactory*>(this), this, "CrossSpeciesComparisonGeneDetect View", "View gene data", getIcon(), [this, getPluginInstance, datasets](PluginTriggerAction& pluginTriggerAction) -> void {
            for (auto dataset : datasets)
                getPluginInstance();
        });

        pluginTriggerActions << pluginTriggerAction;
    }
    */
    return pluginTriggerActions;
}
