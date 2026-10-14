#include "XSCGeneDetectPlugin.h"

#include <util/Serialization.h>

#include <event/Event.h>
#include <XSCTreeData/XSCTreeData.h>
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
#include <QStyledItemDelegate>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QDialog>
#include <QLabel>
#include <QPointer>
#include <QPainter>
#include <QApplication>

#ifdef _WIN32
#include <execution>
#endif
Q_PLUGIN_METADATA(IID "studio.manivault.XSCGeneDetectPlugin")

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
    if (maxAbs <= 0.0f) {
        return 0.0f; // all values are zero: avoid 0/0 -> NaN (casting NaN to int is undefined)
    }

    if (value >= 0) {
        // Normalize positive values with respect to the maximum absolute value
        return (value / maxAbs) * 50.0f;
    }
    else {
        // Normalize negative values with respect to the maximum absolute value
        return (value / -maxAbs) * 50.0f;
    }
}





float getNormalizedSize(float value, float min, float max) {
    if (value < min) value = min;
    if (value > max) value = max;

    if (max <= min) {
        return 0.0f; // flat range: avoid 0/0 -> NaN (casting NaN to int in the bar painting is undefined)
    }
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
    }
    else if (colorMap == "bupu") {
        int blueValue = static_cast<int>((1 - normalizedValue) * 255);
        int purpleValue = static_cast<int>(normalizedValue * 255);
        color = QColor(blueValue, 0, purpleValue);
        textColor = (blueValue + purpleValue > 255) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    }
    else if (colorMap == "gnpu") {
        int greenValue = static_cast<int>((1 - normalizedValue) * 255);
        int purpleValue = static_cast<int>(normalizedValue * 255);
        color = QColor(0, greenValue, purpleValue);
        textColor = (greenValue + purpleValue > 255) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    }
    else {
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
    QRegularExpression regex(R"(Species: (.*), Rank: (\d+), AbundanceTop: (\d+),  AbundanceMiddle: (\d+),  CountAbundanceNumerator: (\d+), MeanSelected: (-?[\d.]+), CountSelected: (\d+), MeanNotSelected: (-?[\d.]+), CountNotSelected: (\d+))");

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



// Size the gene table to the columns that are actually visible. It used to be capped at a fixed
// 165 px, which only fits the first two columns, so the Trait Correlation column was cut off.
static void fitGeneTableWidth(QTableView* view)
{
    if (!view || !view->model()) {
        return;
    }
    int width = 0;
    for (int column = 0; column < view->model()->columnCount(); ++column) {
        if (!view->isColumnHidden(column)) {
            width += view->columnWidth(column);
        }
    }
    width += view->verticalHeader()->isVisible() ? view->verticalHeader()->width() : 0;
    width += view->verticalScrollBar()->sizeHint().width() + 2 * view->frameWidth() + 4;
    view->setMaximumWidth(std::min(width, 640));
}

// Replace the model of the (proxy-less) selection details table and free the previous one.
static void setSelectionDetailsModel(QTableView* view, QStandardItemModel* newModel)
{
    QAbstractItemModel* oldModel = view->model();
    view->setModel(newModel);
    // Only delete models we created: QTableView hands out a static empty model when none is set.
    if (oldModel && oldModel != newModel && qobject_cast<QStandardItemModel*>(oldModel)) {
        oldModel->deleteLater();
    }
}

XSCGeneDetectPlugin::XSCGeneDetectPlugin(const PluginFactory* factory) :
    ViewPlugin(factory),
    _settingsAction(*this)
{

}

void XSCGeneDetectPlugin::init()
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
                auto treeDataset = mv::data().getDataset<XSCTree>(_settingsAction.getFilteringEditTreeDatasetAction().getCurrentDataset().getDatasetId());

                QStringList selectedRowsStrList = _settingsAction.getSelectedRowIndexAction().getString().split(",");
                QList<int> selectedRows;
                for (const QString& str : selectedRowsStrList) {
                    selectedRows << str.toInt();
                }

                if (selectedRows.size() == 1)
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
                if (auto* tableModel = _settingsAction.getGeneTableView()->model()) {
                    for (int row : selectedRows) {
                        firstColumnValues << tableModel->index(row, 0).data().toString();
                    }
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
                    mv::gui::DatasetPickerAction* colorDatasetPickerAction = nullptr;
                    mv::gui::DatasetPickerAction* pointDatasetPickerAction = nullptr;
                    mv::gui::ViewPluginSamplerAction* samplerActionAction = nullptr;


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
                                            return _settingsAction.generateTooltip(toolTipContext, clusterDatasetId, true, "GlobalPointIndices");
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



            if (_settingsAction.getSelectedPointsTSNEDatasetForGeneTable().isValid())
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
            qDebug() << "XSCGeneDetectPlugin:Clearing Species Explorer selection at line 477";
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
                            qDebug() << "XSCGeneDetectPlugin:Clearing Species Explorer selection at line 511";
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

        auto* view = _settingsAction.getGeneTableView();
        QAbstractItemModel* viewModel = view ? view->model() : nullptr;

        // modifyListData() installs a QSortFilterProxyModel, so look through it to the item model
        QAbstractItemModel* sourceModel = viewModel;
        if (auto* proxy = qobject_cast<QSortFilterProxyModel*>(viewModel)) {
            sourceModel = proxy->sourceModel();
        }
        auto* model = qobject_cast<QStandardItemModel*>(sourceModel);

        if (model) {
            for (int i = 0; i < model->columnCount(); i++) {
                auto* headerItem = model->horizontalHeaderItem(i);
                if (!headerItem || !shownColumns.contains(headerItem->text())) {
                    view->hideColumn(i);
                }
                else
                {
                    view->showColumn(i);
                }
            }
            fitGeneTableWidth(view);
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
            mv::gui::DatasetPickerAction* colorDatasetPickerAction = nullptr;
            mv::gui::DatasetPickerAction* pointDatasetPickerAction = nullptr;
            mv::gui::ViewPluginSamplerAction* samplerActionAction = nullptr;

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
                                        auto colormapLegend = dynamic_cast<ColorMap1DAction*>(legendPlugin->findChildByPath("ChartLegendViewPlugin Chart/Color Options/Color map"));
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

    connect(&mv::projects(), &AbstractProjectManager::projectOpened, this, triggerInit);



    //change height of headers



    //make long strings in the cells visible and not ...shortened
    _settingsAction.getGeneTableView()->setTextElideMode(Qt::ElideNone);
    _settingsAction.getGeneTableView()->setWordWrap(true);
    _settingsAction.getGeneTableView()->setAlternatingRowColors(false);
    _settingsAction.getGeneTableView()->setSortingEnabled(true);

    //on hovering a cell, show the full text available in a tooltip
    connect(_settingsAction.getGeneTableView(), &QTableView::entered, [this](const QModelIndex& index) {
        if (index.isValid()) {
            // an explicit tooltip of the cell wins over its display text
            QString text = index.data(Qt::ToolTipRole).toString();
            if (text.isEmpty()) {
                text = index.data().toString();
            }
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
            // an explicit tooltip of the cell wins over its display text
            QString text = index.data(Qt::ToolTipRole).toString();
            if (text.isEmpty()) {
                text = index.data().toString();
            }
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
    // (the trait correlation settings live in their own group below)



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

    // All settings of the gene/trait correlation in one group (same style as "Options" and "Save table as CSV")
    auto traitCorrelationGroup = new VerticalGroupAction(this, "Trait correlation");
    traitCorrelationGroup->setIcon(mv::util::StyledIcon("chart-line"));
    traitCorrelationGroup->addAction(&_settingsAction.getTraitCorrelationMethod());
    traitCorrelationGroup->addAction(&_settingsAction.getTraitSpeciesScope());
    traitCorrelationGroup->addAction(&_settingsAction.getTraitPoolingMode());
    traitCorrelationGroup->addAction(&_settingsAction.getTraitMinCells());
    traitCorrelationGroup->addAction(&_settingsAction.getTraitSignificantOnly());
    traitCorrelationGroup->addAction(&_settingsAction.getShowTraitPlot());
    connect(&_settingsAction.getShowTraitPlot(), &TriggerAction::triggered, this, [this]() { showTraitPlot(); });

    auto traitLayout = new QVBoxLayout();
    auto traitWidget = traitCorrelationGroup->createCollapsedWidget(&getWidget());
    traitWidget->setMaximumHeight(22);
    traitLayout->addWidget(traitWidget);
    mainOptionsLayout->addLayout(traitLayout);

    auto downloadOptionsGroup = new VerticalGroupAction(this, "Save table as CSV");
    downloadOptionsGroup->setIcon(mv::util::StyledIcon("download"));
    downloadOptionsGroup->addAction(&_settingsAction.getSaveGeneTable());
    downloadOptionsGroup->addAction(&_settingsAction.getSaveSpeciesTable());
    downloadOptionsGroup->addAction(&_settingsAction.getSaveTraitMatrix());
    downloadOptionsGroup->addAction(&_settingsAction.getSaveIncludeParameters());

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

    // (Tooltips are already set per hovered cell by the handler registered above. The previous handler
    //  here rewrote the ToolTipRole of EVERY cell in the model on every mouse-enter, which is O(rows x cols)
    //  per hover and fired dataChanged for the whole table.)

    QFrame* verticalLine = new QFrame();
    verticalLine->setFrameShape(QFrame::VLine);
    verticalLine->setFrameShadow(QFrame::Sunken);
    verticalLine->setLineWidth(1);

    // width is fitted to the visible columns in fitGeneTableWidth() once a model is available
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

// ============================================================================================================
//  Gene explorer: expression of one gene in a set of species
// ============================================================================================================

bool XSCGeneDetectPlugin::showGeneInSpeciesEmbedding(const QString& gene, const QStringList& finalsettingSpeciesNamesArray)
{
    std::vector<std::seed_seq::result_type> selectedSpeciesIndices;
    auto speciesDataset = _settingsAction.getSpeciesNamesDataset().getCurrentDataset();
    auto umapDataset = _settingsAction.getScatterplotEmbeddingPointsUMAPOption().getCurrentDataset();
    auto mainPointsDataset = _settingsAction.getMainPointsDataset().getCurrentDataset();
    std::vector<std::seed_seq::result_type> filtSelectInndx;


    if (!speciesDataset.isValid() || !umapDataset.isValid() || !mainPointsDataset.isValid() || !_settingsAction.getFilteredUMAPDatasetPoints().isValid() || !_settingsAction.getFilteredUMAPDatasetColors().isValid() || !_settingsAction.getFilteredUMAPDatasetClusters().isValid())
    {
        qDebug() << "One of the datasets is not valid";
        return false;
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
                selectedGeneIndex.push_back(std::distance(dimensionNames.begin(), it));
            }


            if (!selectedGeneIndex.empty()) {
                fullMainPointsDataset->populateDataForDimensions(resultContainerSpeciesColors, selectedGeneIndex, selectedSpeciesIndices);
            }
            else {
                qDebug() << "Gene" << gene << "was not found in the main dataset";
            }
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
            mv::gui::DatasetPickerAction* colorDatasetPickerAction = nullptr;
            mv::gui::DatasetPickerAction* pointDatasetPickerAction = nullptr;
            mv::gui::ViewPluginSamplerAction* samplerActionAction = nullptr;

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

    return true;
}

void XSCGeneDetectPlugin::geneExplorer()
{
    const QString gene = _settingsAction.getSelectedGeneAction().getString();
    const QStringList speciesNames = _settingsAction.getSpeciesExplorerInMap().getSelectedOptions();

    if (gene.isEmpty())
    {
        qDebug() << "Gene is not selected";
        return;
    }
    if (speciesNames.isEmpty())
    {
        qDebug() << "Species names are not selected";
        return;
    }
    showGeneInSpeciesEmbedding(gene, speciesNames);
}

void XSCGeneDetectPlugin::geneExplorer(QString selectedSpecies)
{
    const QString gene = _settingsAction.getSelectedGeneAction().getString();

    if (gene.isEmpty())
    {
        qDebug() << "Gene is not selected";
        return;
    }
    if (selectedSpecies.isEmpty())
    {
        qDebug() << "Species name empty";
        return;
    }
    showGeneInSpeciesEmbedding(gene, QStringList{ selectedSpecies });
}

void XSCGeneDetectPlugin::adjustTableWidths(const QString& value) {
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



// ============================================================================================================
//  Plot: mean expression of one gene against the trait (one point per species)
// ============================================================================================================

/**
 * Scatter plot of the trait value (x) against the mean expression of a gene (y), one labelled point per species.
 * Species that are selected in the species explorer are blue, the others grey. A dashed least-squares line shows the trend.
 * (No Q_OBJECT: the widget has no signals or slots of its own.)
 */
class TraitPlotWidget : public QWidget
{
public:
    struct Point {
        double x = 0.0;
        double y = 0.0;
        QString label;
        bool highlighted = false;
    };

    explicit TraitPlotWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(420, 320);
    }

    void setData(const QVector<Point>& points, const QString& xTitle, const QString& yTitle)
    {
        _points = points;
        _xTitle = xTitle;
        _yTitle = yTitle;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), palette().color(QPalette::Base));

        const QColor textColor = palette().color(QPalette::Text);
        const QRect plot(70, 20, width() - 70 - 24, height() - 20 - 56);
        if (_points.isEmpty() || plot.width() < 40 || plot.height() < 40) {
            painter.setPen(textColor);
            painter.drawText(rect(), Qt::AlignCenter, "No data to plot");
            return;
        }

        // value ranges with a small margin
        double minX = _points[0].x, maxX = minX, minY = _points[0].y, maxY = minY;
        for (const Point& point : _points) {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }
        const auto pad = [](double& low, double& high) {
            const double span = high - low;
            if (span <= 0.0) {
                low -= 1.0;
                high += 1.0;
            }
            else {
                low -= 0.06 * span;
                high += 0.06 * span;
            }
            };
        pad(minX, maxX);
        pad(minY, maxY);

        const auto toPixel = [&](double x, double y) {
            return QPointF(plot.left() + (x - minX) / (maxX - minX) * plot.width(), plot.bottom() - (y - minY) / (maxY - minY) * plot.height());
            };

        // frame, ticks and tick labels
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(plot);
        painter.setPen(textColor);
        constexpr int kTicks = 4;
        for (int i = 0; i <= kTicks; ++i) {
            const double fraction = static_cast<double>(i) / kTicks;
            const double xValue = minX + fraction * (maxX - minX);
            const double yValue = minY + fraction * (maxY - minY);
            const QPointF xPixel = toPixel(xValue, minY);
            const QPointF yPixel = toPixel(minX, yValue);
            painter.drawLine(QPointF(xPixel.x(), plot.bottom()), QPointF(xPixel.x(), plot.bottom() + 4));
            painter.drawLine(QPointF(plot.left() - 4, yPixel.y()), QPointF(plot.left(), yPixel.y()));
            painter.drawText(QRectF(xPixel.x() - 30, plot.bottom() + 6, 60, 16), Qt::AlignHCenter | Qt::AlignTop, QString::number(xValue, 'g', 3));
            painter.drawText(QRectF(plot.left() - 66, yPixel.y() - 8, 60, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(yValue, 'g', 3));
        }

        // axis titles
        painter.drawText(QRect(plot.left(), height() - 22, plot.width(), 18), Qt::AlignCenter, _xTitle);
        painter.save();
        painter.translate(14, plot.center().y());
        painter.rotate(-90);
        painter.drawText(QRect(-plot.height() / 2, -8, plot.height(), 16), Qt::AlignCenter, _yTitle);
        painter.restore();

        // least-squares line (an illustration of the trend; it is not the statistic of the chosen method)
        double meanX = 0.0, meanY = 0.0;
        for (const Point& point : _points) {
            meanX += point.x;
            meanY += point.y;
        }
        meanX /= _points.size();
        meanY /= _points.size();
        double sxx = 0.0, sxy = 0.0;
        for (const Point& point : _points) {
            sxx += (point.x - meanX) * (point.x - meanX);
            sxy += (point.x - meanX) * (point.y - meanY);
        }
        if (_points.size() >= 2 && sxx > 0.0) {
            const double slope = sxy / sxx;
            const double intercept = meanY - slope * meanX;
            painter.save();
            painter.setClipRect(plot);
            painter.setPen(QPen(textColor, 1, Qt::DashLine));
            painter.drawLine(toPixel(minX, intercept + slope * minX), toPixel(maxX, intercept + slope * maxX));
            painter.restore();
        }

        // points with the species name
        for (const Point& point : _points) {
            const QPointF pixel = toPixel(point.x, point.y);
            painter.setPen(Qt::NoPen);
            painter.setBrush(point.highlighted ? QColor("#00A2ED") : QColor(Qt::gray));
            painter.drawEllipse(pixel, 5.0, 5.0);
            painter.setPen(textColor);
            painter.drawText(QPointF(pixel.x() + 8.0, pixel.y() - 6.0), point.label);
        }
    }

private:
    QVector<Point> _points;
    QString _xTitle;
    QString _yTitle;
};

// ============================================================================================================
//  Gene table: filter model and Trait Correlation bar delegate
// ============================================================================================================

/**
 * Filter model of the gene table.
 *  - normal mode: only genes that are top-N genes of at least one species are listed
 *  - search mode (search box has focus): every gene is listed, narrowed down by the typed (regular) expression
 * Filtering in the model replaces the former per-row hideRow()/showRow() calls, which re-laid out the whole view
 * once per row.
 */
class GeneFilterProxyModel : public QSortFilterProxyModel
{
public:
    explicit GeneFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent)
    {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        // The trait column is the one whose values change after the model was built. Declaring it as the filter column makes
        // the proxy re-evaluate the rows when those values are refreshed (needed for "significant genes only").
        setFilterKeyColumn(kTraitCorrelationColumn);
    }

    /** List only genes with a significant trait correlation (FDR < kSignificanceLevel) in normal mode. */
    void setSignificantOnly(bool significantOnly)
    {
        if (_significantOnly == significantOnly)
            return;
        _significantOnly = significantOnly;
        invalidateFilter();
    }

    /** Genes that are listed in normal mode. */
    void setAllowedGenes(const QSet<QString>& genes)
    {
        _allowedGenes = genes;
        invalidateFilter();
    }

    /** Switch between normal mode and search mode. */
    void setSearchMode(bool searchMode)
    {
        if (_searchMode == searchMode)
            return;
        _searchMode = searchMode;
        invalidateFilter();
    }

    /** Pattern used in search mode (an empty pattern lists every gene). */
    void setSearchPattern(const QRegularExpression& pattern)
    {
        _searchPattern = pattern;
        if (_searchMode)
            invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        const QAbstractItemModel* source = sourceModel();
        if (!source)
            return false;

        const QString gene = source->index(sourceRow, 0, sourceParent).data().toString();

        if (_searchMode)
            return _searchPattern.pattern().isEmpty() || gene.contains(_searchPattern);

        if (!_allowedGenes.contains(gene))
            return false;

        // column 1 = number of species in which the gene is a top-N gene
        if (source->index(sourceRow, 1, sourceParent).data().toInt() < 1)
            return false;

        if (_significantOnly) {
            const QVariant fdr = source->index(sourceRow, kTraitCorrelationColumn, sourceParent).data(kTraitFdrRole);
            return fdr.isValid() && fdr.toDouble() < kSignificanceLevel;
        }
        return true;
    }

private:
    static constexpr double kSignificanceLevel = 0.05;

    QSet<QString>      _allowedGenes;
    QRegularExpression _searchPattern;
    bool               _searchMode = false;
    bool               _significantOnly = false;
};

/**
 * Draws the Trait Correlation (range -1..1) like the other bar columns of the tables:
 * a grey bar of FIXED width (kBarWidth) at the left of the cell, growing right for positive and left for negative
 * values from a black axis line in the middle. Bars of significant genes (FDR < 0.05) have a thin black outline.
 * The number is printed behind the bar only if the column is wide enough; in a narrow column it is hidden
 * (it stays available in the cell tooltip together with the p-value and FDR).
 */
class TraitCorrelationBarDelegate : public QStyledItemDelegate
{
public:
    static constexpr int kBarWidth = 60;   // same width as the bars of the other columns
    static constexpr int kBarHeight = 20;  // same height as the bars of the other columns
    static constexpr int kMargin = 4;
    static constexpr int kTextWidth = 40;

    /** Column width at which the bar fits exactly (default width of the column). */
    static constexpr int defaultColumnWidth() { return kBarWidth + 2 * kMargin + 2; }

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        // 1. background / selection highlight, without the default text
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.features &= ~QStyleOptionViewItem::HasDecoration;

        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        // 2. value (numeric UserRole, see createModelFromData)
        bool ok = false;
        const double value = index.data(Qt::UserRole).toDouble(&ok);
        if (!ok || std::isnan(value))
            return;

        // 3. fixed-size bar area at the left edge of the cell
        const int barHeight = std::min(kBarHeight, option.rect.height() - 4);
        const int barTop = option.rect.top() + (option.rect.height() - barHeight) / 2;
        const int barLeft = option.rect.left() + kMargin;
        const int middle = barLeft + kBarWidth / 2;
        const double clamped = std::max(-1.0, std::min(1.0, value));
        const int length = static_cast<int>(std::abs(clamped) * (kBarWidth / 2));

        painter->save();
        painter->setClipRect(option.rect); // never paint outside a narrow column

        // grey bar, like every other bar column; a thin black outline marks a significant correlation (FDR < 0.05)
        const QVariant fdr = index.data(kTraitFdrRole);
        const bool significant = fdr.isValid() && fdr.toDouble() < 0.05;
        painter->setPen(significant ? QPen(Qt::black, 1) : QPen(Qt::NoPen));
        painter->setBrush(Qt::gray);
        if (clamped >= 0)
            painter->drawRect(middle, barTop, length, barHeight);
        else
            painter->drawRect(middle - length, barTop, length, barHeight);

        // black zero axis, like the "Mean Gene Differential Expression" column
        painter->setPen(Qt::black);
        painter->drawLine(middle, barTop - 2, middle, barTop + barHeight + 2);

        // value text: only when the column is wide enough to show it completely next to the bar
        if (option.rect.width() >= kBarWidth + kTextWidth + 2 * kMargin) {
            const QRect textArea(barLeft + kBarWidth + kMargin, option.rect.top(), kTextWidth, option.rect.height());
            painter->setPen((option.state & QStyle::State_Selected) ? option.palette.color(QPalette::HighlightedText) : option.palette.color(QPalette::Text));
            painter->drawText(textArea, Qt::AlignLeft | Qt::AlignVCenter, QString::number(value, 'f', 2));
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        // fixed: the bar defines the size, the column width is chosen by the user
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setWidth(defaultColumnWidth());
        return size;
    }
};

// ============================================================================================================
//  Gene table installation
// ============================================================================================================

void XSCGeneDetectPlugin::modifyListData()
{
    try {
        auto variant = _settingsAction.getListModelAction().getVariant();
        QStandardItemModel* model = qobject_cast<QStandardItemModel*>(variant.value<QAbstractItemModel*>());

        if (!_settingsAction.getGeneTableView()) {
            return;
        }

        // No model (cleared): empty the table that is currently shown
        if (!model) {
            QAbstractItemModel* currentModel = _settingsAction.getGeneTableView()->model();
            if (currentModel) {
                currentModel->removeRows(0, currentModel->rowCount());
                _settingsAction.getGeneTableView()->update();
            }
            return;
        }

        installGeneTableModel(model);

        // Secondary views that depend on the new table
        selectedCellStatisticsStatusBarRemove();
        selectedCellCountStatusBarAdd();

        // The new model starts with the correlation values it was created with: refresh them for the current
        // selection (debounced, runs in the background of the event loop).
        _settingsAction.updateTraitReverseSearchFeature();
    }
    catch (const std::exception& e) {
        qDebug() << "An exception occurred in modifyListData: " << e.what();
    }
    catch (...) {
        qDebug() << "An unknown exception occurred in modifyListData";
    }
}

void XSCGeneDetectPlugin::installGeneTableModel(QStandardItemModel* model)
{
    QTableView* geneTableView = _settingsAction.getGeneTableView();
    CustomLineEdit* searchBox = _settingsAction.getSearchBox();

    // ---- 1. Filter/sort proxy in front of the model -------------------------------------------------------
    auto* proxyModel = new GeneFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setAllowedGenes(_settingsAction.getUniqueReturnGeneList());
    proxyModel->setSignificantOnly(_settingsAction.getTraitSignificantOnly().isChecked());

    // (the proxy is the receiver context, so the connection disappears together with it)
    connect(&_settingsAction.getTraitSignificantOnly(), &ToggleAction::toggled, proxyModel, [proxyModel](bool significantOnly) {
        proxyModel->setSignificantOnly(significantOnly);
        });

    // ---- 2. Swap the models and release the previous ones (they used to leak on every recomputation) ---------
    QAbstractItemModel* previousViewModel = geneTableView->model();
    QAbstractItemModel* previousSourceModel = nullptr;
    if (auto* previousProxy = qobject_cast<QSortFilterProxyModel*>(previousViewModel)) {
        previousSourceModel = previousProxy->sourceModel();
    }

    geneTableView->setModel(proxyModel);

    if (qobject_cast<QSortFilterProxyModel*>(previousViewModel) && previousViewModel != proxyModel) {
        previousViewModel->deleteLater();
    }
    if (previousSourceModel && previousSourceModel != model && qobject_cast<QStandardItemModel*>(previousSourceModel)) {
        previousSourceModel->deleteLater();
    }

    // ---- 3. Search box: this runs on every recomputation, so drop the hooks of the previous proxy first -------
    QPointer<GeneFilterProxyModel> proxyGuard(proxyModel);
    disconnect(searchBox, nullptr, this, nullptr);

    // focus in: list every gene so that any gene can be searched
    connect(searchBox, &CustomLineEdit::textboxSelectedForTyping, this, [proxyGuard]() {
        if (proxyGuard)
            proxyGuard->setSearchMode(true);
        });

    // focus out with empty text: back to the top-N genes only
    connect(searchBox, &CustomLineEdit::textboxDeselectedNotTypingAnymore, this, [proxyGuard]() {
        if (proxyGuard) {
            proxyGuard->setSearchPattern(QRegularExpression());
            proxyGuard->setSearchMode(false);
        }
        });

    // typing: apply the (regular) expression after a short pause. The pointer guard protects against the proxy being
    // replaced during the 300 ms debounce window.
    connect(searchBox, &CustomLineEdit::textChanged, this, [this, proxyGuard](const QString& text) {
        QTimer::singleShot(300, this, [proxyGuard, text]() {
            if (!proxyGuard)
                return;

            if (text.isEmpty()) {
                proxyGuard->setSearchPattern(QRegularExpression()); // text erased while still focused: drop the filter
                return;
            }

            QRegularExpression regExp(text, QRegularExpression::CaseInsensitiveOption);
            if (regExp.isValid())
                proxyGuard->setSearchPattern(regExp);
            });
        });

    searchBox->setText("");

    // ---- 4. Visible columns (the view may still have columns hidden from a previous model) -------------------
    const auto shownColumns = _settingsAction.getHiddenShowncolumns().getSelectedOptions();
    for (int i = 0; i < proxyModel->columnCount(); i++) {
        auto* headerItem = model->horizontalHeaderItem(i);
        if (!headerItem || !shownColumns.contains(headerItem->text())) {
            geneTableView->hideColumn(i);
        }
        else {
            geneTableView->showColumn(i);
        }
    }

    // ---- 5. Sorting, widths, Trait Correlation bar delegate ------------------------------------------------------
    proxyModel->sort(1, Qt::DescendingOrder);
    geneTableView->setColumnWidth(0, 85);
    geneTableView->setColumnWidth(1, 80);
    geneTableView->setColumnWidth(2, 220);
    geneTableView->setColumnWidth(3, 420);
    geneTableView->setColumnWidth(kTraitCorrelationColumn, TraitCorrelationBarDelegate::defaultColumnWidth()); // bar fits, number hidden until widened

    // (findChild<>() cannot be used here: the delegate has no Q_OBJECT macro, so it is looked up through the view)
    if (!dynamic_cast<TraitCorrelationBarDelegate*>(geneTableView->itemDelegateForColumn(kTraitCorrelationColumn))) {
        geneTableView->setItemDelegateForColumn(kTraitCorrelationColumn, new TraitCorrelationBarDelegate(geneTableView));
    }
    fitGeneTableWidth(geneTableView);
    geneTableView->update();

    // ---- 6. Gene row click -----------------------------------------------------------------------------------------
    // The selection model is new for every model, so the connection cannot be duplicated.
    connect(geneTableView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex&) {
        onGeneRowChanged(current);
        });

    // the open trait plot follows the correlation values (they are published as one dataChanged signal)
    connect(model, &QAbstractItemModel::dataChanged, this, [this]() { refreshTraitPlot(); });

    emit model->layoutChanged();
}

// ============================================================================================================
//  Trait plot window
// ============================================================================================================

void XSCGeneDetectPlugin::showTraitPlot()
{
    if (!_traitPlotDialog) {
        auto* dialog = new QDialog(&getWidget());
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        auto* canvas = new TraitPlotWidget(dialog);
        auto* info = new QLabel(dialog);
        info->setWordWrap(true);
        info->setTextInteractionFlags(Qt::TextSelectableByMouse);

        auto* layout = new QVBoxLayout(dialog);
        layout->addWidget(canvas, 1);
        layout->addWidget(info);
        dialog->resize(700, 560);

        _traitPlotDialog = dialog;
        _traitPlotCanvas = canvas;
        _traitPlotInfo = info;
    }

    refreshTraitPlot();
    _traitPlotDialog->show();
    _traitPlotDialog->raise();
    _traitPlotDialog->activateWindow();
}

void XSCGeneDetectPlugin::refreshTraitPlot()
{
    if (!_traitPlotDialog || !_traitPlotCanvas || !_traitPlotInfo) {
        return;
    }
    auto* canvas = static_cast<TraitPlotWidget*>(_traitPlotCanvas.data()); // created as a TraitPlotWidget in showTraitPlot()

    const QString gene = _settingsAction.getSelectedGeneAction().getString();
    const TraitMatrix& matrix = _settingsAction.getTraitMatrix();

    QVector<TraitPlotWidget::Point> points;
    QString infoText;

    if (gene.isEmpty()) {
        infoText = "Select a gene in the gene table.";
    }
    else if (matrix.empty()) {
        infoText = "No trait correlation has been computed yet: choose a trait and cell type(s).";
    }
    else {
        const auto geneIt = matrix.geneIndex.find(gene);
        if (geneIt == matrix.geneIndex.end()) {
            infoText = "The gene is not part of the trait matrix.";
        }
        else {
            for (size_t i = 0; i < matrix.species.size(); ++i) {
                QString label = matrix.species[i];
                label.replace("_", " ");
                points.push_back({ matrix.trait[i], static_cast<double>(matrix.expression[i][geneIt->second]), label, matrix.selected[i] != 0 });
            }

            // the statistics are the tooltip of the trait cell: method, number of species, correlation, interval, p, FDR, leave-one-out
            auto variant = _settingsAction.getListModelAction().getVariant();
            auto* model = qobject_cast<QStandardItemModel*>(variant.value<QAbstractItemModel*>());
            if (model && model->columnCount() > kTraitCorrelationColumn) {
                for (int row = 0; row < model->rowCount(); ++row) {
                    auto* idItem = model->item(row, 0);
                    auto* traitItem = model->item(row, kTraitCorrelationColumn);
                    if (idItem && traitItem && idItem->text() == gene) {
                        infoText = traitItem->data(Qt::ToolTipRole).toString();
                        break;
                    }
                }
            }
            infoText += "\n\nDashed line: least-squares trend (illustration). Blue points: species selected in the species explorer.";
        }
    }

    canvas->setData(points, matrix.traitName, "Mean expression of " + gene);
    _traitPlotInfo->setText(infoText);
    _traitPlotDialog->setWindowTitle(gene.isEmpty() ? QString("Gene expression vs trait") : gene + " - expression vs trait");
}

// ============================================================================================================
//  Gene row click
// ============================================================================================================

void XSCGeneDetectPlugin::onGeneRowChanged(const QModelIndex& current)
{
    if (!current.isValid()) return;

    QString gene = current.siblingAtColumn(0).data().toString();
    _settingsAction.getSelectedGeneAction().setString(gene);
    _settingsAction.getSelectedRowIndexAction().setString(QString::number(current.row()));
    _settingsAction.getRemoveRowSelection().setEnabled(true);
    //_settingsAction.enableDisableButtonsAutomatically();


    // ---- 1. Read the species names and the statistics of the clicked gene from its row ----
    QStringList finalsettingSpeciesNamesArray;
    QString finalSpeciesNameString;
    QString statisticsText;
    const auto* model = current.model();
    for (int i = 0; i < model->columnCount(); ++i) {
        const QString columnName = model->headerData(i, Qt::Horizontal).toString();
        const auto data = current.siblingAtColumn(i).data();

        if (columnName == "Gene Appearance Species Names") {
            // skip empty parts: a gene without top-N species must give an EMPTY list, not a list holding ""
            finalsettingSpeciesNamesArray = data.toString().split(";", Qt::SkipEmptyParts);
            finalSpeciesNameString = finalsettingSpeciesNamesArray.join(" @%$,$%@ ");
        }
        else if (columnName == "Statistics") {
            statisticsText = data.toString();
        }
    }

    // ---- 2. Per-species statistics table (also for genes that are no top-N gene of any species: the statistics
    //         text holds the numbers of every species) ----
    std::map<QString, SpeciesDetailsStats> speciesExpressionMap;
    if (!statisticsText.isEmpty()) {
        speciesExpressionMap = convertToStatisticsMap(statisticsText);
        selectedCellCountStatusBarRemove();
        selectedCellStatisticsStatusBarAdd(speciesExpressionMap, finalsettingSpeciesNamesArray);
    }
    else {
        qDebug() << "Warning: the row of gene" << gene << "has no statistics.";
    }

    // Embedding scatterplot: expression of the clicked gene in the species it appears in
    // (returns false when the embedding datasets are not set up yet; the tree, the expression colors and the selection
    //  below do not depend on it and are still updated)
    showGeneInSpeciesEmbedding(gene, finalsettingSpeciesNamesArray);

    auto referenceTreeDataset = _settingsAction.getReferenceTreeDatasetAction().getCurrentDataset();
    if (referenceTreeDataset.isValid()) {
        auto referenceTree = mv::data().getDataset<XSCTree>(referenceTreeDataset.getDatasetId());
        if (referenceTree.isValid()) {
            QJsonObject speciesDataJson = referenceTree->getTreeData();
            updateSpeciesData(speciesDataJson, speciesExpressionMap);
            referenceTree->setTreeData(speciesDataJson);
            events().notifyDatasetDataChanged(referenceTree);
        }
    }

    if (_settingsAction.getScatterplotReembedColorOption().getCurrentText() == "Expression" && !_settingsAction.getPerformGeneTableTsneAction().isChecked()) {



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
            if (selectedGeneIndex < maxIndex)
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

    // an open trait plot shows the newly selected gene
    refreshTraitPlot();
}

// ============================================================================================================
//  Selection details tables (bottom right): shared building blocks
// ============================================================================================================

namespace {

    /** Running minimum / maximum of one table column; used to scale the bars of that column. */
    struct ValueRange {
        float min = std::numeric_limits<float>::max();
        float max = std::numeric_limits<float>::lowest();

        void include(float value) {
            min = std::min(min, value);
            max = std::max(max, value);
        }
    };

    /** numerator / denominator in percent; 0 when the denominator is 0 (the old code produced NaN/inf here). */
    inline float percentOf(int numerator, int denominator) {
        return denominator != 0 ? (static_cast<float>(numerator) / static_cast<float>(denominator)) * 100.0f : 0.0f;
    }

    /** Grey bar that grows from the left edge. \p length is in pixels (the 0..100 percentage is used directly). */
    QPixmap makeBarPixmap(int width, int height, float length) {
        QPixmap pixmap(width, height);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::gray);
        painter.drawRect(0, 0, static_cast<int>(length), height);
        painter.end();
        return pixmap;
    }

    /** Grey bar around a black axis line: positive values grow to the right, negative values to the left. */
    QPixmap makeCenteredBarPixmap(int width, int height, float length, bool positive) {
        QPixmap pixmap(width, height);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::gray);

        const int middle = width / 2;
        const int barHeight = 20;
        const int barY = (height - barHeight) / 2;
        if (positive) {
            painter.drawRect(middle, barY, static_cast<int>(length), barHeight);
        }
        else {
            painter.drawRect(middle - static_cast<int>(length), barY, static_cast<int>(length), barHeight);
        }

        painter.setPen(Qt::black);
        const int axisHeight = barHeight + 4;
        const int axisYStart = (height - axisHeight) / 2;
        painter.drawLine(middle, axisYStart, middle, axisYStart + axisHeight);
        painter.end();
        return pixmap;
    }

    /** Table cell with a bar: DisplayRole = text, UserRole = raw number (used for sorting), DecorationRole = bar. */
    QStandardItem* makeBarItem(const QVariant& sortValue, const QString& text, const QPixmap& bar) {
        auto* item = new QStandardItem();
        item->setData(sortValue, Qt::UserRole);
        item->setData(QVariant(text), Qt::DisplayRole);
        item->setData(QVariant(bar), Qt::DecorationRole);
        return item;
    }

    /** Species cell: readable name, species icon (if there is one) and tooltip; UserRole keeps the raw species name. */
    QStandardItem* makeSpeciesItem(const QString& species) {
        QString displayName = species;
        displayName.replace("_", " ");

        auto* item = new QStandardItem(displayName);
        const QString iconPath = ":/speciesicons/SpeciesIcons/" + species + ".svg";
        if (QFile::exists(iconPath)) {
            item->setData(QIcon(iconPath), Qt::DecorationRole);
            item->setData(QString("<img src='%1' width='16' height='16'/> %2").arg(iconPath, displayName), Qt::ToolTipRole);
        }
        else {
            item->setData(displayName, Qt::ToolTipRole);
        }
        item->setData(species, Qt::UserRole);
        return item;
    }

    /**
     * Title of the "middle" fraction column and whether that column is redundant.
     * Several middle clusters -> "Neuronal" (column hidden), exactly one -> its name, none -> "Abundance" (column hidden).
     */
    QString middleFractionTitle(const QStringList& middleSet, bool& singleColumn) {
        if (middleSet.size() > 1) {
            singleColumn = true;
            return "Neuronal";
        }
        if (middleSet.size() == 1) {
            singleColumn = false;
            return *middleSet.begin();
        }
        singleColumn = true;
        return "Abundance";
    }

    /** Apply a uniform foreground/background to a whole table row. */
    void colorRow(const QList<QStandardItem*>& rowItems, const QColor& background) {
        for (QStandardItem* item : rowItems) {
            item->setBackground(QBrush(background));
            item->setForeground(QBrush(QColor("#000000")));
        }
    }

} // namespace

// ============================================================================================================
//  Selection details table: "count" view (shown while no gene is selected)
// ============================================================================================================

void XSCGeneDetectPlugin::selectedCellCountStatusBarAdd()
{
    const auto& speciesCountMap = _settingsAction.getSelectedSpeciesCellCountMap();
    QTableView* detailsTable = _settingsAction.getSelectionDetailsTable();

    if (!speciesCountMap.empty())
    {
        auto* model = new QStandardItemModel();

        bool singleColumn = false;
        const QString middleTitle = middleFractionTitle(_settingsAction.getCurrentHierarchyItemsMiddleForTable(), singleColumn);
        model->setHorizontalHeaderLabels({ "Species", "Fraction in Neuronal", "Fraction in " + middleTitle, "Count of Selected", "Count of All" });

        // ---- pass 1: value range of every bar column ----
        ValueRange topRange, middleRange, selectedRange, totalRange;
        for (const auto& [species, details] : speciesCountMap)
        {
            topRange.include(percentOf(details.countAbundanceNumerator, details.abundanceTop));
            middleRange.include(percentOf(details.countAbundanceNumerator, details.abundanceMiddle));
            selectedRange.include(static_cast<float>(details.selectedCellsCount));
            totalRange.include(static_cast<float>(details.selectedCellsCount + details.nonSelectedCellsCount));
        }

        // ---- pass 2: one row per species ----
        for (const auto& [species, details] : speciesCountMap)
        {
            QList<QStandardItem*> rowItems;
            rowItems << makeSpeciesItem(species);

            const float topAbundance = percentOf(details.countAbundanceNumerator, details.abundanceTop);
            rowItems << makeBarItem(topAbundance, QString::number(topAbundance, 'f', 2),
                makeBarPixmap(60, 20, getNormalizedSize(topAbundance, topRange.min, topRange.max)));

            const float middleAbundance = percentOf(details.countAbundanceNumerator, details.abundanceMiddle);
            rowItems << makeBarItem(middleAbundance, QString::number(middleAbundance, 'f', 2),
                makeBarPixmap(80, 20, getNormalizedSize(middleAbundance, middleRange.min, middleRange.max)));

            const int selectedCellsCount = details.selectedCellsCount;
            rowItems << makeBarItem(selectedCellsCount, QString::number(selectedCellsCount),
                makeBarPixmap(60, 20, getNormalizedSize(static_cast<float>(selectedCellsCount), selectedRange.min, selectedRange.max)));

            const int total = details.selectedCellsCount + details.nonSelectedCellsCount;
            rowItems << makeBarItem(total, QString::number(total),
                makeBarPixmap(60, 20, getNormalizedSize(static_cast<float>(total), totalRange.min, totalRange.max)));

            model->appendRow(rowItems);
        }

        detailsTable->setSortingEnabled(true);
        setSelectionDetailsModel(detailsTable, model);
        model->setSortRole(Qt::UserRole);
        model->sort(1, Qt::DescendingOrder);

        detailsTable->setSelectionMode(QAbstractItemView::NoSelection);
        detailsTable->verticalHeader()->hide();
        if (singleColumn)
        {
            detailsTable->hideColumn(2);
        }
        detailsTable->setColumnWidth(1, 70);
        detailsTable->setColumnWidth(2, 90);
        detailsTable->setColumnWidth(3, 70);
        detailsTable->setColumnWidth(4, 70);
        detailsTable->update();
    }

    adjustTableWidths("small");
}

// ============================================================================================================
//  Selection details table: "statistics" view (shown for the clicked gene)
// ============================================================================================================

void XSCGeneDetectPlugin::selectedCellStatisticsStatusBarAdd(std::map<QString, SpeciesDetailsStats> statisticsValues, QStringList selectedSpecies)
{
    const auto& speciesCountMap = _settingsAction.getSelectedSpeciesCellCountMap();
    QTableView* detailsTable = _settingsAction.getSelectionDetailsTable();

    if (!speciesCountMap.empty())
    {
        // Which species are highlighted / selected in the species explorer? Follows the clicked gene unless the user
        // chose species by hand (see SettingsAction::resolveExplorerSelectionForGene). Decided ONCE, up front.
        const QStringList explorerSelection = _settingsAction.resolveExplorerSelectionForGene(selectedSpecies);

        auto* model = new QStandardItemModel();

        bool singleColumn = false;
        const QString middleTitle = middleFractionTitle(_settingsAction.getCurrentHierarchyItemsMiddleForTable(), singleColumn);
        model->setHorizontalHeaderLabels({ "Species", "Mean Gene Differential Expression", "Gene Appearance Rank", "Fraction in Neuronal", "Fraction in " + middleTitle, "Count of Selected", "Mean Gene Expression of Selected", "Count of Non Selected", "Mean Gene Expression of Non Selected" });

        const int numofGenes = _settingsAction.getTopNGenesFilter().getMaximum();
        const int topNValue = _settingsAction.getTopNGenesFilter().getValue();

        // ---- pass 1: value range of every bar column ----
        ValueRange differenceRange, rankRange, topRange, middleRange, selectedCountRange, meanSelectedRange, nonSelectedCountRange, meanNonSelectedRange;
        for (const auto& [species, details] : speciesCountMap)
        {
            const auto it = statisticsValues.find(species);
            if (it == statisticsValues.end()) {
                continue;
            }
            const auto& stats = it->second;
            differenceRange.include(stats.meanSelected - stats.meanNonSelected);
            rankRange.include(static_cast<float>(numofGenes - stats.rank));
            topRange.include(percentOf(stats.countAbundanceNumerator, stats.abundanceTop));
            middleRange.include(percentOf(stats.countAbundanceNumerator, stats.abundanceMiddle));
            selectedCountRange.include(static_cast<float>(stats.countSelected));
            meanSelectedRange.include(stats.meanSelected);
            nonSelectedCountRange.include(static_cast<float>(stats.countNonSelected));
            meanNonSelectedRange.include(stats.meanNonSelected);
        }

        // ---- pass 2: one row per species ----
        for (const auto& [species, details] : speciesCountMap)
        {
            QList<QStandardItem*> rowItems;
            rowItems << makeSpeciesItem(species);

            const auto it = statisticsValues.find(species);
            if (it != statisticsValues.end()) {
                const auto& stats = it->second;

                // Mean gene differential expression: centred bar (right = higher in the selection)
                const float difference = stats.meanSelected - stats.meanNonSelected;
                const float differenceLength = getNormalizedSizePosNeg(difference, differenceRange.min, differenceRange.max);
                rowItems << makeBarItem(difference, QString::number(difference, 'f', 2),
                    makeCenteredBarPixmap(75, 25, differenceLength, difference >= 0));

                // Gene appearance rank: logarithmic bar (rank 1 = longest)
                const int rank = stats.rank;
                float rankLength = 0.0f;
                const float logBase = std::log(static_cast<float>(topNValue));
                if (logBase > 0.0f && rank > 0) {
                    const float logOfNumofGenes = std::log(static_cast<float>(numofGenes)) / logBase;
                    const float logOfRank = std::log(static_cast<float>(rank)) / logBase;
                    const float logofMinRank = std::log(1.0f) / logBase;
                    const float logofMaxRank = std::log(rankRange.max) / logBase;
                    rankLength = qBound(0.0f, getNormalizedSizeRank(logOfNumofGenes - logOfRank, logofMinRank, logofMaxRank), 100.0f);
                }
                rowItems << makeBarItem(rank, QString::number(rank), makeBarPixmap(75, 20, rankLength));

                // Fractions of the species' cells
                const float topAbundance = percentOf(stats.countAbundanceNumerator, stats.abundanceTop);
                rowItems << makeBarItem(topAbundance, QString::number(topAbundance, 'f', 2),
                    makeBarPixmap(60, 20, getNormalizedSize(topAbundance, topRange.min, topRange.max)));

                const float middleAbundance = percentOf(stats.countAbundanceNumerator, stats.abundanceMiddle);
                rowItems << makeBarItem(middleAbundance, QString::number(middleAbundance, 'f', 2),
                    makeBarPixmap(80, 20, getNormalizedSize(middleAbundance, middleRange.min, middleRange.max)));

                // Selected cells: count and mean expression
                rowItems << makeBarItem(stats.countSelected, QString::number(stats.countSelected),
                    makeBarPixmap(55, 20, getNormalizedSize(static_cast<float>(stats.countSelected), selectedCountRange.min, selectedCountRange.max)));

                rowItems << makeBarItem(stats.meanSelected, QString::number(stats.meanSelected, 'f', 2),
                    makeBarPixmap(70, 20, getNormalizedSize(stats.meanSelected, meanSelectedRange.min, meanSelectedRange.max)));

                // Non selected cells: count and mean expression
                rowItems << makeBarItem(stats.countNonSelected, QString::number(stats.countNonSelected),
                    makeBarPixmap(60, 20, getNormalizedSize(static_cast<float>(stats.countNonSelected), nonSelectedCountRange.min, nonSelectedCountRange.max)));

                rowItems << makeBarItem(stats.meanNonSelected, QString::number(stats.meanNonSelected, 'f', 2),
                    makeBarPixmap(75, 20, getNormalizedSize(stats.meanNonSelected, meanNonSelectedRange.min, meanNonSelectedRange.max)));
            }
            else {
                // species without statistics for this gene
                for (int column = 1; column < model->columnCount(); ++column) {
                    rowItems << new QStandardItem("N/A");
                }
            }

            // highlight the species that are currently selected
            colorRow(rowItems, explorerSelection.contains(species) ? QColor("#00A2ED") : QColor("#FFFFFF"));
            model->appendRow(rowItems);
        }

        setSelectionDetailsModel(detailsTable, model);
        model->setSortRole(Qt::UserRole);
        model->sort(2, _settingsAction.getTypeofTopNGenes().getCurrentText() == "Positive" || _settingsAction.getTypeofTopNGenes().getCurrentText() == "Absolute" ? Qt::AscendingOrder : Qt::DescendingOrder);

        detailsTable->setSelectionMode(QAbstractItemView::SingleSelection);
        detailsTable->verticalHeader()->hide();
        if (singleColumn) {
            detailsTable->hideColumn(4);
        }
        detailsTable->setColumnWidth(1, 85);
        detailsTable->setColumnWidth(2, 85);
        detailsTable->setColumnWidth(3, 70);
        detailsTable->setColumnWidth(4, 90);
        detailsTable->setColumnWidth(5, 65);
        detailsTable->setColumnWidth(6, 80);
        detailsTable->setColumnWidth(7, 70);
        detailsTable->setColumnWidth(8, 85);
        detailsTable->update();
        emit model->layoutChanged();

        // Clicking a species row: explore the clicked gene in that species only
        disconnect(detailsTable->selectionModel(), nullptr, this, nullptr);
        connect(detailsTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection& selected, const QItemSelection&) {
            if (selected.isEmpty()) {
                return;
            }
            const QModelIndexList selectedIndexes = selected.indexes();
            if (selectedIndexes.isEmpty() || !selectedIndexes.at(0).isValid()) {
                return;
            }

            const QString species = selectedIndexes.at(0).siblingAtColumn(0).data(Qt::UserRole).toString();
            if (!species.isEmpty() && _settingsAction.getSpeciesExplorerInMap().getNumberOfOptions() > 0) {
                _settingsAction.setSpeciesExplorerSelection({ species });
                geneExplorer();
            }
            _settingsAction.getClearRightClickedCluster().trigger();
            });

        // Apply the explorer selection exactly once, now that the new table is in place (it used to be re-applied
        // for every species row, and every call re-ran the table recolor and the trait correlation).
        _settingsAction.setSpeciesExplorerSelection(explorerSelection);
    }

    adjustTableWidths("large");
}

void XSCGeneDetectPlugin::updatePhylogeneticTree()
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
                    auto referenceTree = mv::data().getDataset<XSCTree>(referenceTreeDataset.getDatasetId());
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

void XSCGeneDetectPlugin::selectedCellCountStatusBarRemove()
{
    setSelectionDetailsModel(_settingsAction.getSelectionDetailsTable(), new QStandardItemModel());
}

void XSCGeneDetectPlugin::selectedCellStatisticsStatusBarRemove()
{
    setSelectionDetailsModel(_settingsAction.getSelectionDetailsTable(), new QStandardItemModel());
}
void XSCGeneDetectPlugin::updateSpeciesData(QJsonObject& node, const std::map<QString, SpeciesDetailsStats>& speciesExpressionMap) {
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

void XSCGeneDetectPlugin::updateTreeData(QJsonObject& node, const std::map<QString, SpeciesDetailsStats>& speciesExpressionMap) {
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
            updateTreeData(child, speciesExpressionMap); // Recursive call
            children[i] = child; // Update the modified object back into the array
        }
        node["children"] = children; // Update the modified array back into the parent JSON object
    }
}
/*
void XSCGeneDetectPlugin::onDataEvent(mv::DatasetEvent* dataEvent)
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

void XSCGeneDetectPlugin::fromVariantMap(const QVariantMap& variantMap)
{
    ViewPlugin::fromVariantMap(variantMap);

    mv::util::variantMapMustContain(variantMap, "CSCGDV:XSC Gene Detect Plugin Settings");
    _settingsAction.fromVariantMap(variantMap["CSCGDV:XSC Gene Detect Plugin Settings"].toMap());
    // modifyTableData();
    // _settingsAction.getStartComputationTriggerAction().trigger();

}

QVariantMap XSCGeneDetectPlugin::toVariantMap() const
{
    QVariantMap variantMap = ViewPlugin::toVariantMap();

    _settingsAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
ViewPlugin* XSCGeneDetectPluginFactory::produce()
{
    return new XSCGeneDetectPlugin(this);
}

mv::DataTypes XSCGeneDetectPluginFactory::supportedDataTypes() const
{
    DataTypes supportedTypes;

    // This example analysis plugin is compatible with points datasets
    supportedTypes.append(PointType);

    return supportedTypes;
}

mv::gui::PluginTriggerActions XSCGeneDetectPluginFactory::getPluginTriggerActions(const mv::Datasets& datasets) const
{
    PluginTriggerActions pluginTriggerActions;
    /*
    const auto getPluginInstance = [this]() -> XSCGeneDetectPlugin* {
        return dynamic_cast<XSCGeneDetectPlugin*>(plugins().requestViewPlugin(getKind()));
    };

    const auto numberOfDatasets = datasets.count();

    if (numberOfDatasets >= 1 && PluginFactory::areAllDatasetsOfTheSameType(datasets, PointType)) {
        auto pluginTriggerAction = new PluginTriggerAction(const_cast<XSCGeneDetectPluginFactory*>(this), this, "XSCGeneDetect View", "View gene data", getIcon(), [this, getPluginInstance, datasets](PluginTriggerAction& pluginTriggerAction) -> void {
            for (auto dataset : datasets)
                getPluginInstance();
        });

        pluginTriggerActions << pluginTriggerAction;
    }
    */
    return pluginTriggerActions;
}
