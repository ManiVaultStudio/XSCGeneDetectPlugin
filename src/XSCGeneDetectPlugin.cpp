#include "StreamedPointData.h"
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
#include <functional>
#include <tuple>
#include <QFileDialog>
#include <QSignalBlocker>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolTip>
#include <QToolButton>
#include <QMenu>
#include <QWidgetAction>
#include <QGuiApplication>
#include <QScreen>
#include <QSizePolicy>
#include <QMenu>
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
    XSC_TRACE("applyLogTransformation");


#ifdef _WIN32
    std::transform(std::execution::par, values.begin(), values.end(), values.begin(),
        [](float value) { return std::log(value + 1); });
#else
    std::transform(values.begin(), values.end(), values.begin(),
        [](float value) { return std::log(value + 1); });
#endif

}
float getNormalizedSizeRank(float value, float min, float max) {
    XSC_TRACE("getNormalizedSizeRank");
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
    XSC_TRACE("getNormalizedSizePosNeg");
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
    XSC_TRACE("getNormalizedSize");
    if (value < min) value = min;
    if (value > max) value = max;

    if (max <= min) {
        return 0.0f; // flat range: avoid 0/0 -> NaN (casting NaN to int in the bar painting is undefined)
    }
    float normalizedValue = (value - min) / (max - min);
    return normalizedValue * 100.0f; // Return size as a percentage
}

float getPowNormalizedSize(float value, float min, float max, float alpha) {
    XSC_TRACE("getPowNormalizedSize");
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
    XSC_TRACE("getLogNormalizedSize");
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
    XSC_TRACE("getRelativeNormalizedSize");
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
    XSC_TRACE("getSegmentedNormalizedSize");
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
    XSC_TRACE("getShiftedNormalizedSize");
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
    XSC_TRACE("getFixedPointNormalizedSize");
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
    XSC_TRACE("getColorMAp");
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
    XSC_TRACE("convertToStatisticsMap");
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

/**
 * Hover text of the gene table cells that carry no tooltip of their own (gene, species count, species names, statistics and the
 * further statistics columns). It is built on demand from the cells of the row, so nothing is stored per gene.
 */
static QString geneTableCellTooltip(SettingsAction& settings, const QModelIndex& index)
{
    XSC_TRACE("geneTableCellTooltip");
    const int column = index.column();
    const bool supported = column == 0 || column == 1 || column == 2 || column == 3 || (column >= kBestPositionColumn && column <= kMeanExpressionColumn);
    if (!index.isValid() || !supported) {
        return QString();
    }

    const QString gene = index.siblingAtColumn(0).data().toString();
    const int topCount = index.siblingAtColumn(1).data().toInt();
    const auto statistics = convertToStatisticsMap(index.siblingAtColumn(3).data().toString());
    if (statistics.empty()) {
        return QString();
    }
    const int speciesTotal = static_cast<int>(statistics.size());
    const int topN = settings.weightedTopN();
    const bool negative = settings.weightedNegative();

    struct SpeciesLine {
        QString species;
        int position = 0;      // 1 = strongest; 0 = unknown
        int listLength = 0;
        double difference = 0.0;
        double meanSelected = 0.0;
        double meanNonSelected = 0.0;
        int cellsSelected = 0;
        int cellsNonSelected = 0;
        bool topGene = false;
    };
    QVector<SpeciesLine> lines;
    for (const auto& entry : statistics) {
        const SpeciesDetailsStats& stats = entry.second;
        SpeciesLine line;
        line.species = entry.first;
        line.listLength = settings.speciesGeneListLength(entry.first);
        line.position = GeneStats::positionFromRank(stats.rank, line.listLength, negative);
        line.meanSelected = stats.meanSelected;
        line.meanNonSelected = stats.meanNonSelected;
        line.difference = line.meanSelected - line.meanNonSelected;
        line.cellsSelected = stats.countSelected;
        line.cellsNonSelected = stats.countNonSelected;
        line.topGene = line.position >= 1 && line.position <= topN && stats.meanSelected > 0;
        lines.push_back(line);
    }
    // best positions first; species without a position by their differential expression
    std::sort(lines.begin(), lines.end(), [](const SpeciesLine& a, const SpeciesLine& b) {
        if ((a.position > 0) != (b.position > 0))
            return a.position > 0;
        if (a.position > 0)
            return a.position < b.position;
        return a.difference > b.difference;
        });

    const auto pretty = [](QString name) {
        name.replace("_", " ");
        return name.toHtmlEscaped();
        };
    const auto num = [](double value, int decimals = 3) { return QString::number(value, 'f', decimals); };
    const auto row = [](const QString& label, const QString& value) {
        return QString("<tr><td><b>%1</b>&nbsp;&nbsp;</td><td>%2</td></tr>").arg(label.toHtmlEscaped(), value);
        };
    const QString title = QString("<p style='white-space:pre'><b>%1</b></p>").arg(gene.toHtmlEscaped());

    // summary values shared by several cells
    const QModelIndex weightedIndex = index.siblingAtColumn(kWeightedAppearanceColumn);
    const double weighted = weightedIndex.data(Qt::UserRole).toDouble();
    const double meanDifferential = index.siblingAtColumn(kMeanDifferentialColumn).data(Qt::UserRole).toDouble();
    const double positivePercent = index.siblingAtColumn(kPositiveSpeciesColumn).data(Qt::UserRole).toDouble();
    const double meanExpression = index.siblingAtColumn(kMeanExpressionColumn).data(Qt::UserRole).toDouble();
    const bool havePosition = !lines.isEmpty() && lines.first().position > 0;
    const QString bestText = havePosition ? QString("%1 in %2").arg(lines.first().position).arg(pretty(lines.first().species)) : QString("none");

    const auto tableOf = [](const QStringList& headers, const QVector<QStringList>& rows) {
        QString html = "<table cellspacing='0' cellpadding='2'><tr>";
        for (const QString& header : headers)
            html += "<th align='left'>" + header.toHtmlEscaped() + "&nbsp;&nbsp;</th>";
        html += "</tr>";
        for (const QStringList& cells : rows) {
            html += "<tr>";
            for (const QString& cell : cells)
                html += "<td>" + cell + "&nbsp;&nbsp;</td>";
            html += "</tr>";
        }
        return html + "</table>";
        };

    if (column == 0) {
        QString html = title + "<table>";
        html += row("Top-N gene in", QString("%1 of %2 species (%3%)").arg(topCount).arg(speciesTotal).arg(GeneStats::percent(topCount, speciesTotal), 0, 'f', 0));
        html += row("Weighted appearance", num(weighted));
        html += row("Best position", bestText);
        html += row("Mean differential", QString("%1 (higher in the selection in %2% of the species)").arg(num(meanDifferential)).arg(positivePercent, 0, 'f', 0));
        html += row("Mean expression (selection)", num(meanExpression));
        html += row("Rank consistency", QString("p = %1, FDR = %2").arg(weightedIndex.data(kWeightedPValueRole).toDouble(), 0, 'g', 3).arg(weightedIndex.data(kWeightedFdrRole).toDouble(), 0, 'g', 3));
        html += "</table><p>Hover the other columns for details.</p>";
        return html;
    }

    if (column == 1 || column == 2) {
        QVector<QStringList> rows;
        for (const SpeciesLine& line : lines) {
            if (!line.topGene)
                continue;
            rows.push_back({ pretty(line.species), QString::number(line.position), num(GeneStats::rankWeight(line.position, topN)), num(line.difference) });
        }
        QString html = title + QString("<p>Top-N gene in <b>%1 of %2</b> species (%3%).<br>A gene counts as a top-N gene of a species when its position is within the top %4 and its mean "
            "in the selected cells is positive.</p>").arg(topCount).arg(speciesTotal).arg(GeneStats::percent(topCount, speciesTotal), 0, 'f', 0).arg(topN);
        if (rows.isEmpty()) {
            return html + "<p>It is a top-N gene in none of the species.</p>";
        }
        if (rows.size() > 30) {
            const int more = rows.size() - 30;
            rows = rows.mid(0, 30);
            return html + tableOf({ "Species", "Position", "Weight", "Differential" }, rows) + QString("<p>... and %1 more</p>").arg(more);
        }
        return html + tableOf({ "Species", "Position", "Weight", "Differential" }, rows);
    }

    if (column == 3) {
        QVector<QStringList> rows;
        for (const SpeciesLine& line : lines) {
            if (rows.size() >= 40)
                break;
            rows.push_back({ pretty(line.species), line.position > 0 ? QString::number(line.position) : QString("-"),
                             line.position > 0 ? num(GeneStats::percent(line.position, line.listLength), 2) + "%" : QString("-"),
                             num(line.difference), num(line.meanSelected), num(line.meanNonSelected),
                             QString::number(line.cellsSelected), line.topGene ? "yes" : "" });
        }
        QString html = title + "<p>Statistics of the gene in every species, best positions first.</p>"
            + tableOf({ "Species", "Position", "Percentile", "Differential", "Mean selected", "Mean other", "Selected cells", "Top-N" }, rows);
        if (lines.size() > 40)
            html += QString("<p>... and %1 more species</p>").arg(lines.size() - 40);
        return html;
    }

    if (column == kBestPositionColumn) {
        QString html = title + "<p>Best (smallest) position of the gene in the top-N ordering of any species. Position 1 = the strongest gene of that species.</p><table>";
        html += row("Best", bestText);
        int shown = 0;
        for (const SpeciesLine& line : lines) {
            if (line.position > 0 && shown++ > 0 && shown <= 4)
                html += row(QString("#%1").arg(shown), QString("%1 in %2").arg(line.position).arg(pretty(line.species)));
        }
        return html + "</table>";
    }

    if (column == kMeanDifferentialColumn || column == kPositiveSpeciesColumn) {
        const SpeciesLine* highest = nullptr;
        const SpeciesLine* lowest = nullptr;
        QStringList negativeSpecies;
        for (const SpeciesLine& line : lines) {
            if (!highest || line.difference > highest->difference)
                highest = &line;
            if (!lowest || line.difference < lowest->difference)
                lowest = &line;
            if (line.difference <= 0.0)
                negativeSpecies << pretty(line.species);
        }
        QString html = title + "<table>";
        html += row("Mean differential", QString("%1 over %2 species").arg(num(meanDifferential)).arg(speciesTotal));
        html += row("Higher in the selection in", QString("%1% of the species").arg(positivePercent, 0, 'f', 0));
        if (highest)
            html += row("Largest differential", QString("%1 in %2").arg(num(highest->difference)).arg(pretty(highest->species)));
        if (lowest)
            html += row("Smallest differential", QString("%1 in %2").arg(num(lowest->difference)).arg(pretty(lowest->species)));
        html += "</table>";
        if (!negativeSpecies.isEmpty()) {
            const QStringList shown = negativeSpecies.mid(0, 8);
            html += "<p>Not higher in the selection: " + shown.join(", ") + (negativeSpecies.size() > 8 ? QString(" and %1 more").arg(negativeSpecies.size() - 8) : QString()) + "</p>";
        }
        return html + "<p>Differential = mean expression in the selected cells minus the mean in the other cells of the species.</p>";
    }

    // column == kMeanExpressionColumn
    const SpeciesLine* highest = nullptr;
    for (const SpeciesLine& line : lines) {
        if (!highest || line.meanSelected > highest->meanSelected)
            highest = &line;
    }
    QString html = title + "<table>";
    html += row("Mean expression (selection)", QString("%1 averaged over %2 species").arg(num(meanExpression)).arg(speciesTotal));
    if (highest)
        html += row("Highest", QString("%1 in %2").arg(num(highest->meanSelected)).arg(pretty(highest->species)));
    return html + "</table>";
}



// Size the gene table to the columns that are actually visible. It used to be capped at a fixed
// 165 px, which only fits the first two columns, so the Trait Correlation column was cut off.
static void fitGeneTableWidth(QTableView* view)
{
    XSC_TRACE("fitGeneTableWidth");
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
    XSC_TRACE("setSelectionDetailsModel");
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
    XSC_TRACE("XSCGeneDetectPlugin::XSCGeneDetectPlugin");

}

void XSCGeneDetectPlugin::init()
{
    XSC_TRACE("XSCGeneDetectPlugin::init");
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
            // an explicit tooltip of the cell wins; otherwise the summary built from the row; otherwise the cell text
            QString text = index.data(Qt::ToolTipRole).toString();
            if (text.isEmpty()) {
                text = geneTableCellTooltip(_settingsAction, index);
            }
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
    // Right-click on the header of the species table: choose which columns are shown
    {
        auto* detailsHeader = _settingsAction.getSelectionDetailsTable()->horizontalHeader();
        detailsHeader->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(detailsHeader, &QWidget::customContextMenuRequested, this, [this, detailsHeader](const QPoint& position) {
            showDetailsColumnMenu(detailsHeader->mapToGlobal(position));
            });
    }

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
    traitCorrelationGroup->addAction(&_settingsAction.getTraitGeneScope());
    traitCorrelationGroup->addAction(&_settingsAction.getTraitFdrThreshold());
    traitCorrelationGroup->addAction(&_settingsAction.getTraitSignificantOnly());
    traitCorrelationGroup->addAction(&_settingsAction.getShowTraitPlot());
    connect(&_settingsAction.getShowTraitPlot(), &TriggerAction::triggered, this, [this]() { showTraitPlot(); });

    auto traitLayout = new QVBoxLayout();
    auto traitWidget = traitCorrelationGroup->createCollapsedWidget(&getWidget());
    traitWidget->setMaximumHeight(22);
    traitLayout->addWidget(traitWidget);
    mainOptionsLayout->addLayout(traitLayout);

    // Performance settings
    auto performanceGroup = new VerticalGroupAction(this, "Performance");
    performanceGroup->setIcon(mv::util::StyledIcon("tachometer-alt"));
    performanceGroup->addAction(&_settingsAction.getMaxWorkerThreads());
    performanceGroup->addAction(&_settingsAction.getMemoryBudgetMb());

    auto performanceLayout = new QVBoxLayout();
    auto performanceWidget = performanceGroup->createCollapsedWidget(&getWidget());
    performanceWidget->setMaximumHeight(22);
    performanceLayout->addWidget(performanceWidget);
    mainOptionsLayout->addLayout(performanceLayout);

    // Differential expression of every species against all the others
    auto speciesDeGroup = new VerticalGroupAction(this, "Species differential expression");
    speciesDeGroup->setIcon(mv::util::StyledIcon("chart-bar"));
    speciesDeGroup->addAction(&_settingsAction.getShowSpeciesDe());
    speciesDeGroup->addAction(&_settingsAction.getDeTopGenes());
    speciesDeGroup->addAction(&_settingsAction.getDeDirection());
    speciesDeGroup->addAction(&_settingsAction.getDeRankBy());
    speciesDeGroup->addAction(&_settingsAction.getDeMinDetection());
    speciesDeGroup->addAction(&_settingsAction.getDeMaxCells());
    speciesDeGroup->addAction(&_settingsAction.getDeMinCells());

    auto speciesDeLayout = new QVBoxLayout();
    auto speciesDeWidget = speciesDeGroup->createCollapsedWidget(&getWidget());
    speciesDeWidget->setMaximumHeight(22);
    speciesDeLayout->addWidget(speciesDeWidget);
    mainOptionsLayout->addLayout(speciesDeLayout);

    connect(&_settingsAction.getShowSpeciesDe(), &TriggerAction::triggered, this, [this]() { showSpeciesDe(); });
    // the selected species are highlighted in the window (same blue as everywhere)
    connect(&_settingsAction.getSpeciesExplorerInMap(), &OptionsAction::selectedOptionsChanged, this, [this]() { refreshSpeciesDeHighlights(); });

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
    XSC_TRACE("XSCGeneDetectPlugin::showGeneInSpeciesEmbedding");
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



            std::vector<float> resultContainerSpeciesColors(selectedSpeciesIndices.size(), 0.0f);

            auto dimensionNames = fullMainPointsDataset->getDimensionNames();
            auto it = std::find(dimensionNames.begin(), dimensionNames.end(), gene);
            if (it != dimensionNames.end()) {
                const int selectedGeneIndex = static_cast<int>(std::distance(dimensionNames.begin(), it));
                resultContainerSpeciesColors = PointStreaming::fetchColumnValues(fullMainPointsDataset.get(), selectedGeneIndex, selectedSpeciesIndices);
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
    XSC_TRACE("XSCGeneDetectPlugin::geneExplorer");
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
    XSC_TRACE("XSCGeneDetectPlugin::geneExplorer");
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
    XSC_TRACE("XSCGeneDetectPlugin::adjustTableWidths");
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
    XSC_TRACE("getColorFromValue");
    if (value < min) value = min;
    if (value > max) value = max;

    int range = max - min;
    if (range == 0) return QColor(Qt::gray);

    int blue = 255 * (value - min) / range;

    return QColor(255 - blue, 255 - blue, 255);
}



// ============================================================================================================
//  Interactive plots: zoom, pan, labels on hover
// ============================================================================================================

/**
 * Scatter plot with
 *  - zoom: mouse wheel around the pointer (Shift: x only, Ctrl: y only), buttons of the owner, double click = reset
 *  - pan: drag with the left mouse button
 *  - hover: the point under the pointer gets its label and a tooltip with details (so labels never have to overlap)
 *  - click: reported to the owner
 *  - labels: none / highlighted points only / all points; labels are placed without overlapping (a label that does not fit
 *    anywhere is left out; zoom in to see more)
 * (No Q_OBJECT: the widget has no signals or slots of its own; the owner is informed through callbacks.)
 */
/** Default window size that never exceeds 90% of the available screen area, so that the windows fit the screen. */
inline QSize fittingWindowSize(int width, int height)
{
    const QScreen* screen = QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    return QSize(std::min(width, available.width() * 9 / 10), std::min(height, available.height() * 9 / 10));
}

class ZoomablePlotWidget : public QWidget
{
public:
    struct Point {
        double x = 0.0;
        double y = 0.0;
        QString label;          // drawn next to the point (when labels are on); passed to the detail provider
        bool highlighted = false;
        QColor color;           // invalid: grey (blue when highlighted)
        int id = -1;            // free for the owner (index of a row, a species, ...)
    };
    struct GuideLine {
        bool horizontal = true;
        double value = 0.0;
        QString text;
    };
    enum class LabelMode { None = 0, Highlighted = 1, All = 2 };

    explicit ZoomablePlotWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(200, 160);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        updateMargins();
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
    }

    // ---- content ----
    void setData(const QVector<Point>& points, const QString& xTitle, const QString& yTitle, bool keepView = false)
    {
        _points = points;
        _xTitle = xTitle;
        _yTitle = yTitle;
        _hovered = -1;
        if (!keepView || !_viewValid)
            resetView();
        update();
    }
    void setGuideLines(const QVector<GuideLine>& guides) { _guides = guides; update(); }

    // ---- appearance ----
    void setLabelMode(LabelMode mode) { _labelMode = mode; update(); }
    void setPointSize(int size) { _pointSize = std::max(2, size); update(); }
    void setShowTrendLine(bool show) { _showTrend = show; update(); }
    void setShowGrid(bool show) { _showGrid = show; update(); }

    // ---- interaction ----
    void setDetailProvider(std::function<QString(const Point&)> provider) { _detailProvider = std::move(provider); }
    void setOnHover(std::function<void(int)> callback) { _onHover = std::move(callback); }
    void setOnClick(std::function<void(int)> callback) { _onClick = std::move(callback); }
    /** Highlight a point from outside (e.g. a selected table row). */
    void setHoveredPoint(int index) { _hovered = index; update(); }
    int pointCount() const { return _points.size(); }
    /** The point with the given index (an empty point when out of range). */
    Point pointData(int index) const { return (index >= 0 && index < _points.size()) ? _points[index] : Point(); }

    void zoomBy(double factor) { zoomAround(QPointF(plotRect().center()), factor, false, false); }

    void resetView()
    {
        double minX = -1.0, maxX = 1.0, minY = -1.0, maxY = 1.0;
        if (!_points.isEmpty()) {
            minX = maxX = _points[0].x;
            minY = maxY = _points[0].y;
            for (const Point& point : _points) {
                minX = std::min(minX, point.x);
                maxX = std::max(maxX, point.x);
                minY = std::min(minY, point.y);
                maxY = std::max(maxY, point.y);
            }
            for (const GuideLine& guide : _guides) {
                if (guide.horizontal) {
                    minY = std::min(minY, guide.value);
                    maxY = std::max(maxY, guide.value);
                }
                else {
                    minX = std::min(minX, guide.value);
                    maxX = std::max(maxX, guide.value);
                }
            }
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
        _minX = minX; _maxX = maxX; _minY = minY; _maxY = maxY;
        _viewValid = true;
        update();
    }

protected:
    /**
     * Margins from the font: room for the tick labels and the axis titles, so that nothing is cut off whatever the font
     * size or the widget size. (Cached: plotRect() is called for every point when the mouse moves.)
     */
    void updateMargins()
    {
        const QFontMetrics metrics(font());
        const int line = metrics.height();
        const int tickWidth = metrics.horizontalAdvance(QStringLiteral("-1.2345e-05"));
        _leftMargin = 6 + line + 6 + tickWidth + 10;
        _bottomMargin = 6 + line + 6 + line + 6;
    }

    void changeEvent(QEvent* event) override
    {
        if (event->type() == QEvent::FontChange) {
            updateMargins();
            update();
        }
        QWidget::changeEvent(event);
    }

    QRect plotRect() const { return QRect(_leftMargin, 16, std::max(10, width() - _leftMargin - 16), std::max(10, height() - 16 - _bottomMargin)); }

    QPointF toPixel(double x, double y) const
    {
        const QRect plot = plotRect();
        return QPointF(plot.left() + (x - _minX) / (_maxX - _minX) * plot.width(), plot.bottom() - (y - _minY) / (_maxY - _minY) * plot.height());
    }

    void zoomAround(const QPointF& pixel, double factor, bool xOnly, bool yOnly)
    {
        const QRect plot = plotRect();
        const double spanX = _maxX - _minX, spanY = _maxY - _minY;
        if (spanX <= 0.0 || spanY <= 0.0)
            return;
        const double centerX = _minX + (pixel.x() - plot.left()) / plot.width() * spanX;
        const double centerY = _minY + (plot.bottom() - pixel.y()) / plot.height() * spanY;
        if (!yOnly) {
            _minX = centerX - (centerX - _minX) * factor;
            _maxX = centerX + (_maxX - centerX) * factor;
        }
        if (!xOnly) {
            _minY = centerY - (centerY - _minY) * factor;
            _maxY = centerY + (_maxY - centerY) * factor;
        }
        update();
    }

    int pointAt(const QPointF& pixel) const
    {
        const double radius = _pointSize / 2.0 + 5.0;
        double best = radius * radius;
        int found = -1;
        for (int i = 0; i < _points.size(); ++i) {
            const QPointF p = toPixel(_points[i].x, _points[i].y);
            const double distance = (p.x() - pixel.x()) * (p.x() - pixel.x()) + (p.y() - pixel.y()) * (p.y() - pixel.y());
            if (distance <= best) {
                best = distance;
                found = i;
            }
        }
        return found;
    }

    // ---- events ----
    void wheelEvent(QWheelEvent* event) override
    {
        const double factor = event->angleDelta().y() > 0 ? 0.85 : 1.0 / 0.85;
        zoomAround(event->position(), factor, event->modifiers().testFlag(Qt::ShiftModifier), event->modifiers().testFlag(Qt::ControlModifier));
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            _dragging = true;
            _dragMoved = false;
            _lastPosition = event->position();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (_dragging) {
            const QPointF delta = event->position() - _lastPosition;
            if (std::abs(delta.x()) + std::abs(delta.y()) > 2.0)
                _dragMoved = true;
            const QRect plot = plotRect();
            const double shiftX = delta.x() / plot.width() * (_maxX - _minX);
            const double shiftY = delta.y() / plot.height() * (_maxY - _minY);
            _minX -= shiftX; _maxX -= shiftX;
            _minY += shiftY; _maxY += shiftY;
            _lastPosition = event->position();
            update();
            return;
        }

        const int hovered = plotRect().contains(event->position().toPoint()) ? pointAt(event->position()) : -1;
        if (hovered != _hovered) {
            _hovered = hovered;
            if (_onHover)
                _onHover(hovered);
            update();
            if (hovered >= 0 && _detailProvider)
                QToolTip::showText(event->globalPosition().toPoint(), _detailProvider(_points[hovered]), this);
            else
                QToolTip::hideText();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            if (_dragging && !_dragMoved && _hovered >= 0 && _onClick)
                _onClick(_hovered);
            _dragging = false;
        }
    }

    void mouseDoubleClickEvent(QMouseEvent*) override { resetView(); }

    void leaveEvent(QEvent*) override
    {
        _hovered = -1;
        if (_onHover)
            _onHover(-1);
        update();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal)
            zoomBy(0.8);
        else if (event->key() == Qt::Key_Minus)
            zoomBy(1.25);
        else if (event->key() == Qt::Key_0 || event->key() == Qt::Key_Home)
            resetView();
        else
            QWidget::keyPressEvent(event);
    }

    // ---- painting ----
    static double niceStep(double range, int targetTicks)
    {
        if (range <= 0.0)
            return 1.0;
        const double raw = range / targetTicks;
        const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
        const double residual = raw / magnitude;
        const double nice = residual < 1.5 ? 1.0 : (residual < 3.0 ? 2.0 : (residual < 7.0 ? 5.0 : 10.0));
        return nice * magnitude;
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), palette().color(QPalette::Base));

        const QColor textColor = palette().color(QPalette::Text);
        const QColor gridColor = palette().color(QPalette::Midlight);
        updateMargins();
        const QRect plot = plotRect();
        if (_points.isEmpty() || !_viewValid || _maxX <= _minX || _maxY <= _minY) {
            painter.setPen(textColor);
            painter.drawText(rect(), Qt::AlignCenter, "No data to plot");
            return;
        }

        // ---- grid, ticks and tick labels (sized from the font, see updateMargins) ----
        const QFontMetrics metrics(font());
        const double stepX = niceStep(_maxX - _minX, 6);
        const double stepY = niceStep(_maxY - _minY, 5);
        painter.setFont(font());
        const int yLabelBox = _leftMargin - metrics.height() - 16; // room for the numbers between the axis title and the plot
        // Labels that would overlap the previous one are skipped (their tick marks stay), so the numbers never collide.
        int tickGuard = 0; // (a runaway loop is impossible even for extreme zoom levels)
        double previousEnd = -1.0e9;
        for (double x = std::ceil(_minX / stepX) * stepX; x <= _maxX && tickGuard++ < 200; x += stepX) {
            const QPointF pixel = toPixel(x, _minY);
            if (_showGrid) {
                painter.setPen(gridColor);
                painter.drawLine(QPointF(pixel.x(), plot.top()), QPointF(pixel.x(), plot.bottom()));
            }
            painter.setPen(textColor);
            painter.drawLine(QPointF(pixel.x(), plot.bottom()), QPointF(pixel.x(), plot.bottom() + 4));
            const QString text = QString::number(x, 'g', 4);
            const double half = metrics.horizontalAdvance(text) / 2.0;
            if (pixel.x() - half >= previousEnd + 6.0) {
                painter.drawText(QRectF(pixel.x() - 40, plot.bottom() + 6, 80, metrics.height()), Qt::AlignHCenter | Qt::AlignTop, text);
                previousEnd = pixel.x() + half;
            }
        }
        tickGuard = 0;
        double previousTop = 1.0e9;
        for (double y = std::ceil(_minY / stepY) * stepY; y <= _maxY && tickGuard++ < 200; y += stepY) {
            const QPointF pixel = toPixel(_minX, y);
            if (_showGrid) {
                painter.setPen(gridColor);
                painter.drawLine(QPointF(plot.left(), pixel.y()), QPointF(plot.right(), pixel.y()));
            }
            painter.setPen(textColor);
            painter.drawLine(QPointF(plot.left() - 4, pixel.y()), QPointF(plot.left(), pixel.y()));
            if (pixel.y() + metrics.height() / 2.0 <= previousTop - 2.0) {
                painter.drawText(QRectF(plot.left() - 6 - yLabelBox, pixel.y() - metrics.height() / 2.0, yLabelBox, metrics.height()), Qt::AlignRight | Qt::AlignVCenter, QString::number(y, 'g', 4));
                previousTop = pixel.y() - metrics.height() / 2.0;
            }
        }

        // frame and axis titles (elided to the space they have)
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(plot);
        painter.setPen(textColor);
        painter.drawText(QRect(plot.left(), height() - metrics.height() - 6, plot.width(), metrics.height()), Qt::AlignCenter,
            metrics.elidedText(_xTitle, Qt::ElideRight, plot.width()));
        painter.save();
        painter.translate(4 + metrics.height() / 2.0, plot.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF(-plot.height() / 2.0, -metrics.height() / 2.0, plot.height(), metrics.height()), Qt::AlignCenter,
            metrics.elidedText(_yTitle, Qt::ElideRight, plot.height()));
        painter.restore();

        // ---- everything inside the plot is clipped to it (zoomed-in content must not leak into the margins) ----
        painter.save();
        painter.setClipRect(plot);

        // guide lines (e.g. the significance threshold)
        for (const GuideLine& guide : _guides) {
            painter.setPen(QPen(textColor, 1, Qt::DashLine));
            if (guide.horizontal) {
                const double y = toPixel(_minX, guide.value).y();
                painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
                painter.drawText(QPointF(plot.left() + 6, y - 4), guide.text);
            }
            else {
                const double x = toPixel(guide.value, _minY).x();
                painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
                painter.drawText(QPointF(x + 4, plot.top() + 14), guide.text);
            }
        }

        // least-squares trend (an illustration; it is not the statistic of the chosen method)
        if (_showTrend && _points.size() >= 2) {
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
            if (sxx > 0.0) {
                const double slope = sxy / sxx;
                const double intercept = meanY - slope * meanX;
                painter.setPen(QPen(textColor, 1, Qt::DashLine));
                painter.drawLine(toPixel(_minX, intercept + slope * _minX), toPixel(_maxX, intercept + slope * _maxX));
            }
        }

        // points (the hovered one last, so that it is on top)
        const double radius = _pointSize / 2.0;
        for (int i = 0; i < _points.size(); ++i) {
            if (i == _hovered)
                continue;
            const Point& point = _points[i];
            const QPointF pixel = toPixel(point.x, point.y);
            if (!plot.adjusted(-10, -10, 10, 10).contains(pixel.toPoint()))
                continue;
            painter.setPen(Qt::NoPen);
            painter.setBrush(point.color.isValid() ? point.color : (point.highlighted ? QColor("#00A2ED") : QColor(Qt::gray)));
            painter.drawEllipse(pixel, radius, radius);
        }
        if (_hovered >= 0 && _hovered < _points.size()) {
            const Point& point = _points[_hovered];
            painter.setPen(QPen(Qt::black, 2));
            painter.setBrush(point.color.isValid() ? point.color : (point.highlighted ? QColor("#00A2ED") : QColor(Qt::gray)));
            painter.drawEllipse(toPixel(point.x, point.y), radius + 2.0, radius + 2.0);
        }

        // ---- labels without overlaps: hovered point first, then highlighted points, then (mode All) the rest ----
        if (_labelMode != LabelMode::None || _hovered >= 0) {
            const QFontMetrics metrics(painter.font());
            QVector<QRectF> placed;
            const QRectF plotF(plot);
            const double offset = radius + 3.0;
            painter.setPen(textColor);

            const auto place = [&](int index) {
                const Point& point = _points[index];
                if (point.label.isEmpty())
                    return;
                const QPointF pixel = toPixel(point.x, point.y);
                if (!plotF.contains(pixel))
                    return;
                const QSizeF size(metrics.horizontalAdvance(point.label) + 4, metrics.height());
                const QPointF candidates[4] = { QPointF(offset, -size.height() - 1.0), QPointF(offset, 1.0),
                                                QPointF(-offset - size.width(), -size.height() - 1.0), QPointF(-offset - size.width(), 1.0) };
                for (const QPointF& shift : candidates) {
                    const QRectF rect(pixel + shift, size);
                    if (!plotF.contains(rect))
                        continue;
                    bool free = true;
                    for (const QRectF& other : placed) {
                        if (rect.intersects(other)) {
                            free = false;
                            break;
                        }
                    }
                    if (free) {
                        placed.push_back(rect);
                        painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, point.label);
                        return;
                    }
                }
                };

            if (_hovered >= 0 && _hovered < _points.size())
                place(_hovered);
            int budget = 400; // plenty for any readable plot; keeps painting fast for thousands of points
            for (int i = 0; i < _points.size() && budget > 0; ++i) {
                if (i != _hovered && _points[i].highlighted && _labelMode != LabelMode::None) {
                    place(i);
                    --budget;
                }
            }
            if (_labelMode == LabelMode::All) {
                for (int i = 0; i < _points.size() && budget > 0; ++i) {
                    if (i != _hovered && !_points[i].highlighted) {
                        place(i);
                        --budget;
                    }
                }
            }
        }
        painter.restore();
    }

private:
    QVector<Point> _points;
    QVector<GuideLine> _guides;
    QString _xTitle;
    QString _yTitle;
    double _minX = -1.0, _maxX = 1.0, _minY = -1.0, _maxY = 1.0;
    bool _viewValid = false;
    LabelMode _labelMode = LabelMode::Highlighted;
    int _pointSize = 10;
    bool _showTrend = true;
    bool _showGrid = true;
    int _leftMargin = 72;      /** see updateMargins() */
    int _bottomMargin = 52;
    int _hovered = -1;
    bool _dragging = false;
    bool _dragMoved = false;
    QPointF _lastPosition;
    std::function<QString(const Point&)> _detailProvider;
    std::function<void(int)> _onHover;
    std::function<void(int)> _onClick;
};

/**
 * The plots window with three tabs:
 *  1. Expression vs trait: one point per species for the selected gene, with a table of the species next to it
 *  2. Volcano: every analysed gene (trait correlation against significance); a click selects the gene in the gene table
 *  3. Conservation vs trait: weighted appearance against trait correlation of the listed genes
 * Every plot can be zoomed, panned, labelled and saved as an image.
 */
class TraitPlotDialog : public QDialog
{
public:
    using Point = ZoomablePlotWidget::Point;

    struct SpeciesRow {
        QString species;
        double trait = 0.0;
        double expression = 0.0;
        int cells = 0;
        bool selected = false;
    };

    /** Details of a gene row of the gene table (volcano / conservation tabs). */
    std::function<QString(int row)> geneDetailProvider;
    /** A gene point was clicked (its gene name). */
    std::function<void(const QString& gene)> onGeneClicked;
    /** The tab changed (the plugin refreshes the data of that tab). */
    std::function<void()> onTabChanged;

    explicit TraitPlotDialog(QWidget* parent) : QDialog(parent)
    {
        setWindowTitle("Trait plots");
        auto* layout = new QVBoxLayout(this);
        _tabs = new QTabWidget(this);
        layout->addWidget(_tabs, 1);

        // ---- tab 1: expression vs trait ----
        auto* speciesPage = new QWidget(_tabs);
        auto* speciesLayout = new QVBoxLayout(speciesPage);
        _speciesPlot = new ZoomablePlotWidget(speciesPage);
        _speciesPlot->setShowTrendLine(true);
        addToolbar(speciesLayout, _speciesPlot, true, ZoomablePlotWidget::LabelMode::Highlighted);

        _speciesTable = new QTableWidget(speciesPage);
        _speciesTable->setColumnCount(5);
        _speciesTable->setHorizontalHeaderLabels({ "Species", "Trait", "Mean expression", "Cells", "Selected" });
        _speciesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        _speciesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        _speciesTable->setSelectionMode(QAbstractItemView::SingleSelection);
        _speciesTable->verticalHeader()->hide();
        _speciesTable->horizontalHeader()->setStretchLastSection(true);
        _speciesTable->setMaximumWidth(380);

        auto* speciesRow = new QHBoxLayout();
        speciesRow->addWidget(_speciesPlot, 1);
        speciesRow->addWidget(_speciesTable);
        speciesLayout->addLayout(speciesRow, 1);
        _speciesInfo = new QLabel(speciesPage);
        _speciesInfo->setWordWrap(true);
        _speciesInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
        speciesLayout->addWidget(_speciesInfo);
        _tabs->addTab(speciesPage, "Expression vs trait");

        // hover <-> table
        _speciesPlot->setDetailProvider([this](const Point& point) {
            if (point.id < 0 || point.id >= _rows.size())
                return QString();
            const SpeciesRow& row = _rows[point.id];
            return QString("%1\nTrait: %2\nMean expression: %3\nCells used: %4\n%5")
                .arg(row.species).arg(row.trait, 0, 'g', 5).arg(row.expression, 0, 'g', 5).arg(row.cells)
                .arg(row.selected ? "Selected in the species explorer" : "Not selected");
            });
        _speciesPlot->setOnHover([this](int index) { selectTableRowOfPoint(index); });
        connect(_speciesTable, &QTableWidget::itemSelectionChanged, this, [this]() {
            const auto selected = _speciesTable->selectedItems();
            _speciesPlot->setHoveredPoint(selected.isEmpty() ? -1 : selected.first()->data(Qt::UserRole).toInt());
            });

        // ---- tab 2: volcano ----
        auto* volcanoPage = new QWidget(_tabs);
        auto* volcanoLayout = new QVBoxLayout(volcanoPage);
        _volcanoPlot = new ZoomablePlotWidget(volcanoPage);
        _volcanoPlot->setShowTrendLine(false);
        addToolbar(volcanoLayout, _volcanoPlot, false, ZoomablePlotWidget::LabelMode::Highlighted);
        volcanoLayout->addWidget(_volcanoPlot, 1);
        _volcanoInfo = new QLabel(volcanoPage);
        _volcanoInfo->setWordWrap(true);
        volcanoLayout->addWidget(_volcanoInfo);
        _tabs->addTab(volcanoPage, "Volcano (all analysed genes)");
        wireGenePlot(_volcanoPlot);

        // ---- tab 3: conservation vs trait ----
        auto* conservationPage = new QWidget(_tabs);
        auto* conservationLayout = new QVBoxLayout(conservationPage);
        _conservationPlot = new ZoomablePlotWidget(conservationPage);
        _conservationPlot->setShowTrendLine(false);
        addToolbar(conservationLayout, _conservationPlot, false, ZoomablePlotWidget::LabelMode::Highlighted);
        conservationLayout->addWidget(_conservationPlot, 1);
        _conservationInfo = new QLabel(conservationPage);
        _conservationInfo->setWordWrap(true);
        conservationLayout->addWidget(_conservationInfo);
        _tabs->addTab(conservationPage, "Conservation vs trait");
        wireGenePlot(_conservationPlot);

        connect(_tabs, &QTabWidget::currentChanged, this, [this](int) {
            if (onTabChanged)
                onTabChanged();
            });
        resize(fittingWindowSize(980, 640));
    }

    int currentTab() const { return _tabs->currentIndex(); }

    void setSpeciesData(const QVector<Point>& points, const QVector<SpeciesRow>& rows, const QString& xTitle, const QString& yTitle, const QString& info)
    {
        _rows = rows;
        _speciesPlot->setData(points, xTitle, yTitle, true);
        _speciesInfo->setText(info);

        const QSignalBlocker blocker(_speciesTable);
        _speciesTable->setSortingEnabled(false);
        _speciesTable->setRowCount(rows.size());
        for (int i = 0; i < rows.size(); ++i) {
            const SpeciesRow& row = rows[i];
            QString name = row.species;
            name.replace("_", " ");
            const auto numeric = [i](double value) {
                auto* item = new QTableWidgetItem();
                item->setData(Qt::DisplayRole, value);
                item->setData(Qt::UserRole, i);
                return item;
                };
            auto* nameItem = new QTableWidgetItem(name);
            nameItem->setData(Qt::UserRole, i);
            auto* selectedItem = new QTableWidgetItem(row.selected ? "yes" : "");
            selectedItem->setData(Qt::UserRole, i);
            _speciesTable->setItem(i, 0, nameItem);
            _speciesTable->setItem(i, 1, numeric(row.trait));
            _speciesTable->setItem(i, 2, numeric(row.expression));
            _speciesTable->setItem(i, 3, numeric(row.cells));
            _speciesTable->setItem(i, 4, selectedItem);
        }
        _speciesTable->setSortingEnabled(true);
        _speciesTable->resizeColumnsToContents();
    }

    void setVolcanoData(const QVector<Point>& points, double significanceLevel, const QString& info)
    {
        _volcanoPlot->setGuideLines({ { true, -std::log10(significanceLevel), QString("FDR = %1").arg(significanceLevel) },{ false, 0.0, QString() } });
        _volcanoPlot->setData(points, "Trait correlation", "-log10 FDR", true);
        _volcanoInfo->setText(info);
    }

    void setConservationData(const QVector<Point>& points, const QString& info)
    {
        _conservationPlot->setGuideLines({ { true, 0.0, QString() } });
        _conservationPlot->setData(points, "Weighted appearance", "Trait correlation", true);
        _conservationInfo->setText(info);
    }

private:
    void selectTableRowOfPoint(int pointIndex)
    {
        const QSignalBlocker blocker(_speciesTable); // (no feedback to the plot)
        if (pointIndex < 0) {
            _speciesTable->clearSelection();
            return;
        }
        for (int row = 0; row < _speciesTable->rowCount(); ++row) {
            const auto* item = _speciesTable->item(row, 0);
            if (item && item->data(Qt::UserRole).toInt() == pointIndex) {
                _speciesTable->selectRow(row);
                _speciesTable->scrollToItem(item);
                return;
            }
        }
    }

    void wireGenePlot(ZoomablePlotWidget* plot)
    {
        plot->setDetailProvider([this](const Point& point) { return geneDetailProvider ? geneDetailProvider(point.id) : QString(); });
        plot->setOnClick([this, plot](int index) {
            if (onGeneClicked && index >= 0)
                onGeneClicked(plot->pointData(index).label); // the label of a gene point is the gene name
            });
    }

    /** Controls above a plot: labels, point size, trend line, grid, zoom buttons and image export. */
    void addToolbar(QVBoxLayout* layout, ZoomablePlotWidget* plot, bool withTrend, ZoomablePlotWidget::LabelMode defaultLabels)
    {
        auto* bar = new QHBoxLayout();

        bar->addWidget(new QLabel("Labels:"));
        auto* labels = new QComboBox();
        labels->addItems({ "On hover only", "Highlighted points", "All points (no overlaps)" });
        labels->setCurrentIndex(defaultLabels == ZoomablePlotWidget::LabelMode::None ? 0 : (defaultLabels == ZoomablePlotWidget::LabelMode::Highlighted ? 1 : 2));
        bar->addWidget(labels);
        connect(labels, &QComboBox::currentIndexChanged, plot, [plot](int index) {
            plot->setLabelMode(index == 0 ? ZoomablePlotWidget::LabelMode::None : (index == 1 ? ZoomablePlotWidget::LabelMode::Highlighted : ZoomablePlotWidget::LabelMode::All));
            });
        plot->setLabelMode(defaultLabels);

        bar->addWidget(new QLabel("Point size:"));
        auto* size = new QSpinBox();
        size->setRange(2, 30);
        size->setValue(withTrend ? 11 : 6);
        bar->addWidget(size);
        connect(size, &QSpinBox::valueChanged, plot, [plot](int value) { plot->setPointSize(value); });
        plot->setPointSize(size->value());

        if (withTrend) {
            auto* trend = new QCheckBox("Trend line");
            trend->setChecked(true);
            bar->addWidget(trend);
            connect(trend, &QCheckBox::toggled, plot, [plot](bool on) { plot->setShowTrendLine(on); });
        }
        auto* grid = new QCheckBox("Grid");
        grid->setChecked(true);
        bar->addWidget(grid);
        connect(grid, &QCheckBox::toggled, plot, [plot](bool on) { plot->setShowGrid(on); });

        bar->addStretch(1);

        auto* zoomIn = new QPushButton("+");
        auto* zoomOut = new QPushButton(QString::fromUtf8("\xE2\x88\x92")); // minus sign
        auto* reset = new QPushButton("Reset view");
        auto* save = new QPushButton("Save image...");
        zoomIn->setToolTip("Zoom in (mouse wheel; Shift: x axis only, Ctrl: y axis only)");
        zoomOut->setToolTip("Zoom out");
        reset->setToolTip("Show everything again (double click on the plot)");
        for (QPushButton* button : { zoomIn, zoomOut }) {
            button->setFixedWidth(28);
        }
        for (QWidget* widget : { static_cast<QWidget*>(zoomIn), static_cast<QWidget*>(zoomOut), static_cast<QWidget*>(reset), static_cast<QWidget*>(save) }) {
            bar->addWidget(widget);
        }
        connect(zoomIn, &QPushButton::clicked, plot, [plot]() { plot->zoomBy(0.8); });
        connect(zoomOut, &QPushButton::clicked, plot, [plot]() { plot->zoomBy(1.25); });
        connect(reset, &QPushButton::clicked, plot, [plot]() { plot->resetView(); });
        connect(save, &QPushButton::clicked, plot, [plot]() {
            const QString path = QFileDialog::getSaveFileName(plot, "Save plot image", "trait_plot.png", "PNG image (*.png);;JPEG image (*.jpg)");
            if (!path.isEmpty())
                plot->grab().save(path);
            });

        layout->addLayout(bar);
        Q_UNUSED(layout);
    }

    QTabWidget* _tabs = nullptr;
    ZoomablePlotWidget* _speciesPlot = nullptr;
    ZoomablePlotWidget* _volcanoPlot = nullptr;
    ZoomablePlotWidget* _conservationPlot = nullptr;
    QTableWidget* _speciesTable = nullptr;
    QLabel* _speciesInfo = nullptr;
    QLabel* _volcanoInfo = nullptr;
    QLabel* _conservationInfo = nullptr;
    QVector<SpeciesRow> _rows;
};

// ============================================================================================================
//  Species differential expression: heatmap and window
// ============================================================================================================

/**
 * Heatmap with row/column labels, a diverging colour scale, zoom (wheel; Shift: columns only, Ctrl: rows only), pan (drag),
 * hover details and clicks on cells and labels. Selected rows and columns are outlined and labelled in the blue that marks
 * the selection everywhere in the plugin. (No Q_OBJECT: the owner is informed through callbacks.)
 */
class HeatmapWidget : public QWidget
{
public:
    explicit HeatmapWidget(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(240, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        updateLabelMetrics();
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
    }

    void setData(const QStringList& rowLabels, const QStringList& columnLabels, const QVector<QVector<float>>& values, const QVector<int>& columnGroup)
    {
        _rowLabels = rowLabels;
        _columnLabels = columnLabels;
        _values = values;
        _columnGroup = columnGroup;
        updateLabelMetrics();
        _rowHasValues = QVector<bool>(_rowLabels.size(), false);
        for (int row = 0; row < _values.size() && row < _rowHasValues.size(); ++row) {
            for (const float value : _values[row]) {
                if (!std::isnan(value)) {
                    _rowHasValues[row] = true;
                    break;
                }
            }
        }

        // colour scale: the 98th percentile of the absolute values, so that a few extreme genes do not wash out the rest
        QVector<float> magnitudes;
        for (const QVector<float>& row : _values) {
            for (const float value : row) {
                if (value != 0.0f && !std::isnan(value))
                    magnitudes.push_back(std::abs(value));
            }
        }
        _scale = 1.0;
        if (!magnitudes.isEmpty()) {
            std::sort(magnitudes.begin(), magnitudes.end());
            _scale = static_cast<double>(magnitudes[static_cast<int>(0.98 * (magnitudes.size() - 1))]);
        }
        if (_scale <= 0.0)
            _scale = 1.0;

        _hovered = QPoint(-1, -1);
        resetView();
    }

    void setHighlight(const QSet<int>& rows, const QSet<int>& columns) { _highlightRows = rows; _highlightColumns = columns; update(); }
    void setDetailProvider(std::function<QString(int row, int column)> provider) { _detailProvider = std::move(provider); }
    void setOnCellClicked(std::function<void(int row, int column)> callback) { _onCellClicked = std::move(callback); }
    void setOnRowLabelClicked(std::function<void(int row, bool additive)> callback) { _onRowLabelClicked = std::move(callback); }
    void setOnColumnLabelClicked(std::function<void(int column)> callback) { _onColumnLabelClicked = std::move(callback); }

    void resetView()
    {
        _col0 = 0.0;
        _col1 = std::max(1, static_cast<int>(_columnLabels.size()));
        _row0 = 0.0;
        _row1 = std::max(1, static_cast<int>(_rowLabels.size()));
        update();
    }

    void zoomBy(double factor) { zoomAround(QPointF(plotRect().center()), factor, false, false); }

protected:
    // Margins follow the labels, so the labels fit; long names take at most 40% of the width / height and are elided.
    void updateLabelMetrics()
    {
        const QFontMetrics metrics(font());
        _lineHeight = metrics.height();
        _rowLabelWidth = 0;
        for (const QString& label : _rowLabels) // (the selected species are drawn in bold: leave room for that)
            _rowLabelWidth = std::max(_rowLabelWidth, metrics.horizontalAdvance(label) + metrics.horizontalAdvance(QLatin1Char('M')));
        _columnLabelWidth = 0;
        for (const QString& label : _columnLabels)
            _columnLabelWidth = std::max(_columnLabelWidth, metrics.horizontalAdvance(label));
    }

    void changeEvent(QEvent* event) override
    {
        if (event->type() == QEvent::FontChange) {
            updateLabelMetrics();
            update();
        }
        QWidget::changeEvent(event);
    }

    int leftMargin() const { return std::max(40, std::min(_rowLabelWidth + 14, width() * 2 / 5)); }
    int bottomMargin() const { return _columnLabels.isEmpty() ? 20 : 14 + std::max(40, std::min(_columnLabelWidth + 8, height() * 2 / 5)); }
    QRect plotRect() const { return QRect(leftMargin(), 34, std::max(10, width() - leftMargin() - 16), std::max(10, height() - 34 - bottomMargin())); }

    /** Every how many rows a label is drawn: 1 when every row has room for its label (the labels never overlap). */
    int rowLabelStep() const
    {
        const double rowHeight = cellHeight();
        return rowHeight >= _lineHeight * 0.5 ? 1 : static_cast<int>(std::ceil(_lineHeight * 0.5 / std::max(rowHeight, 1e-3)));
    }
    /** Every how many columns a (rotated) label is drawn. */
    int columnLabelStep() const
    {
        const double columnWidth = cellWidth();
        return columnWidth >= _lineHeight ? 1 : static_cast<int>(std::ceil(_lineHeight / std::max(columnWidth, 1e-3)));
    }

    double cellWidth() const { return plotRect().width() / (_col1 - _col0); }
    double cellHeight() const { return plotRect().height() / (_row1 - _row0); }

    QColor colorFor(float value) const
    {
        if (std::isnan(value))
            return QColor(226, 226, 226); // no value (too few cells): grey
        const double t = std::max(-1.0, std::min(1.0, static_cast<double>(value) / _scale));
        // dark grey for higher, orange for lower than the other species (both already used in the tables); blue is reserved for the selection
        const QColor positive(64, 64, 64);
        const QColor negative(255, 93, 18);
        const QColor& target = t >= 0.0 ? positive : negative;
        const double amount = std::abs(t);
        return QColor(static_cast<int>(255 + (target.red() - 255) * amount), static_cast<int>(255 + (target.green() - 255) * amount), static_cast<int>(255 + (target.blue() - 255) * amount));
    }

    QPoint cellAt(const QPointF& pixel) const
    {
        const QRect plot = plotRect();
        if (!plot.contains(pixel.toPoint()) || _rowLabels.isEmpty() || _columnLabels.isEmpty())
            return QPoint(-1, -1);
        const int column = static_cast<int>(std::floor(_col0 + (pixel.x() - plot.left()) / cellWidth()));
        const int row = static_cast<int>(std::floor(_row0 + (pixel.y() - plot.top()) / cellHeight()));
        if (column < 0 || column >= _columnLabels.size() || row < 0 || row >= _rowLabels.size())
            return QPoint(-1, -1);
        return QPoint(column, row);
    }

    void zoomAround(const QPointF& pixel, double factor, bool columnsOnly, bool rowsOnly)
    {
        const QRect plot = plotRect();
        const double centerColumn = _col0 + (pixel.x() - plot.left()) / plot.width() * (_col1 - _col0);
        const double centerRow = _row0 + (pixel.y() - plot.top()) / plot.height() * (_row1 - _row0);
        const auto zoom = [](double& low, double& high, double center, double scale, double limit) {
            double newLow = center - (center - low) * scale;
            double newHigh = center + (high - center) * scale;
            if (newHigh - newLow < std::min(3.0, limit)) // never zoom in to less than three cells
                return;
            newLow = std::max(0.0, newLow);
            newHigh = std::min(limit, newHigh);
            if (newHigh > newLow) {
                low = newLow;
                high = newHigh;
            }
            };
        if (!rowsOnly)
            zoom(_col0, _col1, centerColumn, factor, std::max(1, static_cast<int>(_columnLabels.size())));
        if (!columnsOnly)
            zoom(_row0, _row1, centerRow, factor, std::max(1, static_cast<int>(_rowLabels.size())));
        update();
    }

    // ---- events ----
    void wheelEvent(QWheelEvent* event) override
    {
        zoomAround(event->position(), event->angleDelta().y() > 0 ? 0.85 : 1.0 / 0.85,
            event->modifiers().testFlag(Qt::ShiftModifier), event->modifiers().testFlag(Qt::ControlModifier));
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            _dragging = true;
            _dragMoved = false;
            _lastPosition = event->position();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (_dragging) {
            const QPointF delta = event->position() - _lastPosition;
            if (std::abs(delta.x()) + std::abs(delta.y()) > 2.0)
                _dragMoved = true;
            const QRect plot = plotRect();
            const double shiftColumns = delta.x() / plot.width() * (_col1 - _col0);
            const double shiftRows = delta.y() / plot.height() * (_row1 - _row0);
            const double columnLimit = std::max(1, static_cast<int>(_columnLabels.size()));
            const double rowLimit = std::max(1, static_cast<int>(_rowLabels.size()));
            if (_col0 - shiftColumns >= 0.0 && _col1 - shiftColumns <= columnLimit) {
                _col0 -= shiftColumns;
                _col1 -= shiftColumns;
            }
            if (_row0 - shiftRows >= 0.0 && _row1 - shiftRows <= rowLimit) {
                _row0 -= shiftRows;
                _row1 -= shiftRows;
            }
            _lastPosition = event->position();
            update();
            return;
        }

        const QPoint cell = cellAt(event->position());
        if (cell != _hovered) {
            _hovered = cell;
            update();
            if (cell.x() >= 0 && _detailProvider)
                QToolTip::showText(event->globalPosition().toPoint(), _detailProvider(cell.y(), cell.x()), this);
            else
                QToolTip::hideText();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton)
            return;
        const bool wasClick = _dragging && !_dragMoved;
        _dragging = false;
        if (!wasClick)
            return;

        const QRect plot = plotRect();
        const QPointF pixel = event->position();
        if (_hovered.x() >= 0 && _onCellClicked) {
            _onCellClicked(_hovered.y(), _hovered.x());
        }
        else if (pixel.x() < plot.left() && pixel.y() >= plot.top() && pixel.y() <= plot.bottom() && _onRowLabelClicked) {
            const int row = static_cast<int>(std::floor(_row0 + (pixel.y() - plot.top()) / cellHeight()));
            if (row >= 0 && row < _rowLabels.size())
                _onRowLabelClicked(row, event->modifiers().testFlag(Qt::ControlModifier));
        }
        else if (pixel.y() > plot.bottom() && pixel.x() >= plot.left() && pixel.x() <= plot.right() && _onColumnLabelClicked) {
            const int column = static_cast<int>(std::floor(_col0 + (pixel.x() - plot.left()) / cellWidth()));
            if (column >= 0 && column < _columnLabels.size())
                _onColumnLabelClicked(column);
        }
    }

    void mouseDoubleClickEvent(QMouseEvent*) override { resetView(); }

    void leaveEvent(QEvent*) override
    {
        _hovered = QPoint(-1, -1);
        update();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal)
            zoomBy(0.8);
        else if (event->key() == Qt::Key_Minus)
            zoomBy(1.25);
        else if (event->key() == Qt::Key_0 || event->key() == Qt::Key_Home)
            resetView();
        else
            QWidget::keyPressEvent(event);
    }

    // ---- painting ----
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Base));
        const QColor textColor = palette().color(QPalette::Text);
        const QColor selectionBlue("#00A2ED"); // the highlight colour of the whole plugin
        const QRect plot = plotRect();

        if (_rowLabels.isEmpty() || _columnLabels.isEmpty()) {
            painter.setPen(textColor);
            painter.drawText(rect(), Qt::AlignCenter, "No data to show");
            return;
        }

        const double cw = cellWidth();
        const double ch = cellHeight();
        const int firstColumn = std::max(0, static_cast<int>(std::floor(_col0)));
        const int lastColumn = std::min(static_cast<int>(_columnLabels.size()) - 1, static_cast<int>(std::ceil(_col1)));
        const int firstRow = std::max(0, static_cast<int>(std::floor(_row0)));
        const int lastRow = std::min(static_cast<int>(_rowLabels.size()) - 1, static_cast<int>(std::ceil(_row1)));
        const QFontMetrics metrics(font());

        // ---- cells ----
        painter.save();
        painter.setClipRect(plot);
        for (int row = firstRow; row <= lastRow; ++row) {
            for (int column = firstColumn; column <= lastColumn; ++column) {
                const float value = (row < _values.size() && column < _values[row].size()) ? _values[row][column] : 0.0f;
                painter.fillRect(QRectF(plot.left() + (column - _col0) * cw, plot.top() + (row - _row0) * ch, cw + 0.6, ch + 0.6), colorFor(value));
            }
        }

        // faint separators between the groups of columns (the genes of one species belong together)
        painter.setPen(QPen(palette().color(QPalette::Mid), 1));
        for (int column = std::max(1, firstColumn); column <= lastColumn; ++column) {
            if (column < _columnGroup.size() && _columnGroup[column] != _columnGroup[column - 1]) {
                const double x = plot.left() + (column - _col0) * cw;
                painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            }
        }

        // selected species: a light blue band over the whole row, so that the selection is visible from a distance
        for (const int row : _highlightRows) {
            if (row >= firstRow && row <= lastRow)
                painter.fillRect(QRectF(plot.left(), plot.top() + (row - _row0) * ch, plot.width(), ch), QColor(0, 162, 237, 60));
        }
        // selected species (rows) and gene (columns): outlined in blue
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(selectionBlue, 2));
        for (const int row : _highlightRows) {
            if (row >= firstRow && row <= lastRow)
                painter.drawRect(QRectF(plot.left(), plot.top() + (row - _row0) * ch, plot.width(), ch));
        }
        for (const int column : _highlightColumns) {
            if (column >= firstColumn && column <= lastColumn)
                painter.drawRect(QRectF(plot.left() + (column - _col0) * cw, plot.top(), cw, plot.height()));
        }
        if (_hovered.x() >= 0) {
            painter.setPen(QPen(Qt::black, 2));
            painter.drawRect(QRectF(plot.left() + (_hovered.x() - _col0) * cw, plot.top() + (_hovered.y() - _row0) * ch, cw, ch));
        }
        painter.restore();

        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(plot);

        // ---- row labels (species): every label that has room; the selected and the hovered one always ----
        QFont boldFont = font();
        boldFont.setBold(true);
        const int rowStep = rowLabelStep();
        const int columnStep = columnLabelStep();
        // species rows shrink their font before any label is dropped: every species keeps its label while it can be read
        QFont rowFont = font();
        const double rowScale = std::max(0.5, std::min(1.0, cellHeight() / std::max(1.0, static_cast<double>(_lineHeight))));
        if (rowScale < 1.0 && rowFont.pointSizeF() > 0.0)
            rowFont.setPointSizeF(rowFont.pointSizeF() * rowScale);
        const QFontMetrics rowMetrics(rowFont);
        QFont rowBold = rowFont;
        rowBold.setBold(true);
        for (int row = firstRow; row <= lastRow; ++row) {
            const bool selected = _highlightRows.contains(row);
            if (!selected && row != _hovered.y() && row % rowStep != 0)
                continue;
            const bool hasValues = row < _rowHasValues.size() && _rowHasValues[row];
            painter.setFont(selected ? rowBold : rowFont);
            painter.setPen(selected ? selectionBlue : (hasValues ? textColor : palette().color(QPalette::Mid)));
            const double centerY = plot.top() + (row + 0.5 - _row0) * ch;
            const QRectF area(4, centerY - rowMetrics.height() / 2.0, plot.left() - 8, rowMetrics.height());
            painter.drawText(area, Qt::AlignRight | Qt::AlignVCenter, rowMetrics.elidedText(_rowLabels[row], Qt::ElideRight, static_cast<int>(area.width())));
        }
        painter.setFont(font());

        // ---- column labels (genes), rotated: every label that has room; the selected and the hovered one always ----
        const int labelLength = bottomMargin() - 14;
        for (int column = firstColumn; column <= lastColumn; ++column) {
            const bool selected = _highlightColumns.contains(column);
            if (!selected && column != _hovered.x() && column % columnStep != 0)
                continue;
            painter.save();
            painter.setFont(selected ? boldFont : font());
            painter.setPen(selected ? selectionBlue : textColor);
            painter.translate(plot.left() + (column + 0.5 - _col0) * cw, plot.bottom() + 6);
            painter.rotate(90);
            painter.drawText(QPointF(0, (metrics.ascent() - metrics.descent()) / 2.0), metrics.elidedText(_columnLabels[column], Qt::ElideRight, labelLength));
            painter.restore();
        }

        // ---- colour legend ----
        const int legendWidth = std::max(60, std::min(180, plot.width() / 2));
        const int legendLeft = std::max(plot.left(), plot.right() - legendWidth);
        for (int x = 0; x < legendWidth; ++x) {
            const double value = (static_cast<double>(x) / (legendWidth - 1) * 2.0 - 1.0) * _scale;
            painter.setPen(colorFor(static_cast<float>(value)));
            painter.drawLine(legendLeft + x, 6, legendLeft + x, 16);
        }
        painter.setPen(textColor);
        painter.drawRect(legendLeft, 6, legendWidth - 1, 10);
        painter.drawText(QRect(legendLeft - 50, 4, 46, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number(-_scale, 'g', 3));
        painter.drawText(QRect(legendLeft + legendWidth + 4, 4, 60, 14), Qt::AlignLeft | Qt::AlignVCenter, QString::number(_scale, 'g', 3));
        const int captionWidth = std::max(10, legendLeft - 60 - plot.left());
        const QString caption = (rowStep > 1 || columnStep > 1)
            ? QStringLiteral("lower  <  difference  >  higher   (labels thinned out: zoom in to read every one)")
            : QStringLiteral("lower  <  difference  >  higher");
        if (captionWidth >= 140) // (too narrow for the caption: the colour legend alone)
            painter.drawText(QRect(plot.left(), 4, captionWidth, 14), Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(caption, Qt::ElideRight, captionWidth));
    }

private:
    QStringList _rowLabels;
    QStringList _columnLabels;
    QVector<QVector<float>> _values;
    QVector<int> _columnGroup;
    QVector<bool> _rowHasValues; /** per row: at least one value (rows without values are labelled grey) */
    int _rowLabelWidth = 0;      /** widest row label (see updateLabelMetrics) */
    int _columnLabelWidth = 0;   /** widest column label */
    int _lineHeight = 14;
    QSet<int> _highlightRows;
    QSet<int> _highlightColumns;
    double _scale = 1.0;
    double _col0 = 0.0, _col1 = 1.0, _row0 = 0.0, _row1 = 1.0;
    QPoint _hovered = QPoint(-1, -1);
    bool _dragging = false;
    bool _dragMoved = false;
    QPointF _lastPosition;
    std::function<QString(int, int)> _detailProvider;
    std::function<void(int, int)> _onCellClicked;
    std::function<void(int, bool)> _onRowLabelClicked;
    std::function<void(int)> _onColumnLabelClicked;
};

/**
 * Window for the species differential expression (every species against all the others):
 *  1. Overview: heatmap of species (rows, in the order of the species tree) against the union of their top genes (columns,
 *     grouped by the species they are top genes of). A diagonal block pattern shows species-specific genes; a gene that is
 *     coloured in several rows is shared.
 *  2. Details: one species against the others as a volcano plot (difference against significance) with its top genes in a table.
 * The selected species and the selected gene are highlighted in blue, like everywhere else in the plugin.
 */
class SpeciesDeDialog : public QDialog
{
public:
    using Point = ZoomablePlotWidget::Point;

    std::function<void(int direction, int topCount, int rank)> onParametersChanged;
    std::function<void(const QString& species)> onSpeciesChosen;
    std::function<void(const QString& gene, const QString& species)> onGeneClicked;   // species may be empty
    std::function<void(const QString& species, bool additive)> onSpeciesClicked;

    explicit SpeciesDeDialog(QWidget* parent) : QDialog(parent)
    {
        setWindowTitle("Species differential expression");
        auto* layout = new QVBoxLayout(this);

        // ---- parameters (they are the settings of the "Species differential expression" group) ----
        auto* bar = new QHBoxLayout();
        bar->addWidget(new QLabel("Genes:"));
        _direction = new QComboBox();
        _direction->addItems({ "Up in the species", "Down in the species", "Either direction" });
        bar->addWidget(_direction);
        bar->addWidget(new QLabel("Top per species:"));
        _topCount = new QSpinBox();
        _topCount->setRange(1, 100);
        bar->addWidget(_topCount);
        bar->addWidget(new QLabel("Rank by:"));
        _rank = new QComboBox();
        _rank->addItems({ "Effect size", "Moderated t" });
        bar->addWidget(_rank);
        bar->addStretch(1);
        _status = new QLabel();
        bar->addWidget(_status);
        layout->addLayout(bar);

        const auto emitParameters = [this]() {
            if (onParametersChanged)
                onParametersChanged(_direction->currentIndex(), _topCount->value(), _rank->currentIndex());
            };
        connect(_direction, &QComboBox::currentIndexChanged, this, emitParameters);
        connect(_rank, &QComboBox::currentIndexChanged, this, emitParameters);
        connect(_topCount, &QSpinBox::valueChanged, this, emitParameters);

        _tabs = new QTabWidget(this);
        layout->addWidget(_tabs, 1);

        // ---- tab 1: overview heatmap ----
        auto* overviewPage = new QWidget(_tabs);
        auto* overviewLayout = new QVBoxLayout(overviewPage);
        auto* overviewBar = new QHBoxLayout();
        overviewBar->addStretch(1);
        _heatmap = new HeatmapWidget(overviewPage);
        auto* zoomIn = new QPushButton("+");
        auto* zoomOut = new QPushButton(QString::fromUtf8("\xE2\x88\x92"));
        auto* reset = new QPushButton("Reset view");
        auto* save = new QPushButton("Save image...");
        zoomIn->setFixedWidth(28);
        zoomOut->setFixedWidth(28);
        zoomIn->setToolTip("Zoom in (mouse wheel; Shift: genes only, Ctrl: species only)");
        zoomOut->setToolTip("Zoom out");
        reset->setToolTip("Show everything again (double click on the heatmap)");
        for (QWidget* widget : { static_cast<QWidget*>(zoomIn), static_cast<QWidget*>(zoomOut), static_cast<QWidget*>(reset), static_cast<QWidget*>(save) })
            overviewBar->addWidget(widget);
        connect(zoomIn, &QPushButton::clicked, _heatmap, [this]() { _heatmap->zoomBy(0.8); });
        connect(zoomOut, &QPushButton::clicked, _heatmap, [this]() { _heatmap->zoomBy(1.25); });
        connect(reset, &QPushButton::clicked, _heatmap, [this]() { _heatmap->resetView(); });
        connect(save, &QPushButton::clicked, _heatmap, [this]() {
            const QString path = QFileDialog::getSaveFileName(_heatmap, "Save heatmap image", "species_differential_expression.png", "PNG image (*.png);;JPEG image (*.jpg)");
            if (!path.isEmpty())
                _heatmap->grab().save(path);
            });
        overviewLayout->addLayout(overviewBar);
        overviewLayout->addWidget(_heatmap, 1);
        _info = new QLabel(overviewPage);
        _info->setWordWrap(true);
        _info->setTextInteractionFlags(Qt::TextSelectableByMouse);
        overviewLayout->addWidget(_info);
        _tabs->addTab(overviewPage, "Overview (every species against the others)");

        _heatmap->setDetailProvider([this](int row, int column) { return heatmapTooltip(row, column); });
        _heatmap->setOnCellClicked([this](int row, int column) {
            if (onGeneClicked && column >= 0 && column < _result.heatmapGenes.size() && row >= 0 && row < _result.species.size())
                onGeneClicked(_result.heatmapGenes[column], _result.species[row]);
            });
        _heatmap->setOnColumnLabelClicked([this](int column) {
            if (onGeneClicked && column >= 0 && column < _result.heatmapGenes.size())
                onGeneClicked(_result.heatmapGenes[column], QString());
            });
        _heatmap->setOnRowLabelClicked([this](int row, bool additive) {
            if (onSpeciesClicked && row >= 0 && row < _result.species.size())
                onSpeciesClicked(_result.species[row], additive);
            });

        // ---- tab 2: one species against the others ----
        auto* detailPage = new QWidget(_tabs);
        auto* detailLayout = new QVBoxLayout(detailPage);
        auto* detailBar = new QHBoxLayout();
        detailBar->addWidget(new QLabel("Species:"));
        _speciesCombo = new QComboBox();
        _speciesCombo->setMinimumWidth(220);
        detailBar->addWidget(_speciesCombo);
        detailBar->addWidget(new QLabel("Labels:"));
        auto* labels = new QComboBox();
        labels->addItems({ "On hover only", "Top genes and selected gene", "All genes (no overlaps)" });
        labels->setCurrentIndex(1);
        detailBar->addWidget(labels);
        detailBar->addStretch(1);
        auto* volcanoReset = new QPushButton("Reset view");
        auto* volcanoSave = new QPushButton("Save image...");
        detailBar->addWidget(volcanoReset);
        detailBar->addWidget(volcanoSave);
        detailLayout->addLayout(detailBar);

        auto* splitter = new QSplitter(Qt::Vertical, detailPage);
        _volcano = new ZoomablePlotWidget(splitter);
        _volcano->setShowTrendLine(false);
        _volcano->setPointSize(6);
        _volcano->setLabelMode(ZoomablePlotWidget::LabelMode::Highlighted);
        splitter->addWidget(_volcano);
        _table = new QTableWidget(splitter);
        _table->setColumnCount(9);
        _table->setHorizontalHeaderLabels({ "Gene", "Difference", "Species", "Others", "Others SD", "Moderated t", "FDR", "Detected in species (%)", "Detected in others (%)" });
        _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        _table->setSelectionBehavior(QAbstractItemView::SelectRows);
        _table->setSelectionMode(QAbstractItemView::SingleSelection);
        _table->verticalHeader()->hide();
        _table->horizontalHeader()->setStretchLastSection(true);
        splitter->addWidget(_table);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);
        splitter->setChildrenCollapsible(false);
        detailLayout->addWidget(splitter, 1);
        _detailInfo = new QLabel(detailPage);
        _detailInfo->setWordWrap(true);
        detailLayout->addWidget(_detailInfo);
        _tabs->addTab(detailPage, "Details (one species against the others)");

        connect(labels, &QComboBox::currentIndexChanged, _volcano, [this](int index) {
            _volcano->setLabelMode(index == 0 ? ZoomablePlotWidget::LabelMode::None : (index == 1 ? ZoomablePlotWidget::LabelMode::Highlighted : ZoomablePlotWidget::LabelMode::All));
            if (index == 2)
                rebuildVolcano(true); // "all genes" needs a label for every gene
            else
                rebuildVolcano(false);
            });
        connect(volcanoReset, &QPushButton::clicked, _volcano, [this]() { _volcano->resetView(); });
        connect(volcanoSave, &QPushButton::clicked, _volcano, [this]() {
            const QString path = QFileDialog::getSaveFileName(_volcano, "Save plot image", "species_volcano.png", "PNG image (*.png);;JPEG image (*.jpg)");
            if (!path.isEmpty())
                _volcano->grab().save(path);
            });
        connect(_speciesCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
            if (index >= 0 && onSpeciesChosen)
                onSpeciesChosen(_speciesCombo->itemData(index).toString());
            });
        _volcano->setDetailProvider([this](const Point& point) { return volcanoTooltip(point.id); });
        _volcano->setOnClick([this](int index) {
            const Point point = _volcano->pointData(index);
            if (onGeneClicked && point.id >= 0 && point.id < _detail.genes.size())
                onGeneClicked(_detail.genes[point.id], _detail.species);
            });
        connect(_table, &QTableWidget::cellClicked, this, [this](int row, int) {
            const auto* item = _table->item(row, 0);
            if (item && onGeneClicked)
                onGeneClicked(item->text(), _detail.species);
            });

        resize(fittingWindowSize(1100, 700));
    }

    /** Show the current settings in the controls (without triggering a change). */
    void setControls(int direction, int topCount, int rank)
    {
        const QSignalBlocker b1(_direction), b2(_topCount), b3(_rank);
        _direction->setCurrentIndex(direction);
        _topCount->setValue(topCount);
        _rank->setCurrentIndex(rank);
    }

    void setStatus(const QString& text) { _status->setText(text); }

    QString currentSpecies() const { return _speciesCombo->currentIndex() >= 0 ? _speciesCombo->currentData().toString() : QString(); }

    void setResult(const SpeciesDeResult& result, const QSet<QString>& selectedSpecies, const QString& focusGene)
    {
        _result = result;
        _selectedSpecies = selectedSpecies;
        _focusGene = focusGene;

        QStringList rows;
        for (const QString& species : _result.species)
            rows << prettyName(species);
        _heatmap->setData(rows, _result.heatmapGenes, _result.heatmapDelta, _result.heatmapOwner);
        _info->setText(_result.message);
        int comparedCount = 0;
        for (int row = 0; row < _result.species.size(); ++row)
            comparedCount += (row >= _result.included.size() || _result.included[row]) ? 1 : 0;
        setStatus(_result.valid ? QString("%1 species compared, %2 genes").arg(comparedCount).arg(_result.heatmapGenes.size()) : QString());

        // species of the details tab: keep the chosen one when it is still there
        const QString previous = currentSpecies();
        {
            const QSignalBlocker blocker(_speciesCombo);
            _speciesCombo->clear();
            for (int row = 0; row < _result.species.size(); ++row) {
                if (row >= _result.included.size() || _result.included[row]) // (species with too few cells cannot be the focus)
                    _speciesCombo->addItem(prettyName(_result.species[row]), _result.species[row]);
            }
            const int keep = _speciesCombo->findData(previous);
            _speciesCombo->setCurrentIndex(keep >= 0 ? keep : (_speciesCombo->count() > 0 ? 0 : -1));
        }
        updateHighlights();
    }

    /** The species that are selected in the species explorer (rows shown in blue). */
    void setSelectedSpecies(const QSet<QString>& selectedSpecies)
    {
        _selectedSpecies = selectedSpecies;
        updateHighlights();
    }

    /** The gene that is selected (column, volcano point and table row shown in blue). */
    void setFocusGene(const QString& gene)
    {
        _focusGene = gene;
        updateHighlights();
        rebuildVolcano(_labelAll);
        rebuildTable();
    }

    void setDetail(const SpeciesDeDetail& detail, double significanceLevel)
    {
        _detail = detail;
        _level = significanceLevel;
        _detailInfo->setText(detail.valid
            ? QString("%1 against %2 other species: %3 genes tested, %4 genes shown. Hover a point for details; click a point or a table row to select the gene. "
                "Wheel: zoom, drag: move, double click: reset.").arg(prettyName(detail.species)).arg(detail.comparison.size()).arg(detail.genes.size()).arg(detail.top.size())
            : (detail.message.isEmpty() ? QString("No data.") : detail.message));
        rebuildVolcano(_labelAll);
        rebuildTable();
    }

private:
    static QString prettyName(QString name)
    {
        name.replace("_", " ");
        return name;
    }

    void updateHighlights()
    {
        QSet<int> rows;
        for (int i = 0; i < _result.species.size(); ++i) {
            if (_selectedSpecies.contains(_result.species[i]))
                rows.insert(i);
        }
        QSet<int> columns;
        const int column = _result.heatmapGenes.indexOf(_focusGene);
        if (column >= 0)
            columns.insert(column);
        _heatmap->setHighlight(rows, columns);
    }

    QString heatmapTooltip(int row, int column) const
    {
        if (row < 0 || column < 0 || row >= _result.species.size() || column >= _result.heatmapGenes.size())
            return QString();
        if (std::isnan(static_cast<double>(_result.heatmapDelta[row][column])))
            return QString("<b>%1</b>: too few cells of the selected cell type(s) for a comparison").arg(prettyName(_result.species[row]).toHtmlEscaped());
        const QString gene = _result.heatmapGenes[column].toHtmlEscaped();
        const QString species = prettyName(_result.species[row]).toHtmlEscaped();
        QString text = QString("<b>%1</b> in <b>%2</b><br>Difference to the other species: %3").arg(gene, species).arg(static_cast<double>(_result.heatmapDelta[row][column]), 0, 'f', 3);

        bool detailed = false;
        for (const SpeciesDeGene& g : _result.comparisons[row].topGenes) {
            if (g.gene == _result.heatmapGenes[column]) {
                text += QString("<br>Species: %1, others: %2 (SD %3)<br>Moderated t = %4, FDR = %5<br>Detected in %6% of the cells of the species, %7% of the others")
                    .arg(static_cast<double>(g.restMean + g.delta), 0, 'f', 3).arg(static_cast<double>(g.restMean), 0, 'f', 3).arg(static_cast<double>(g.restSd), 0, 'f', 3)
                    .arg(static_cast<double>(g.t), 0, 'f', 2).arg(static_cast<double>(g.fdr), 0, 'g', 3)
                    .arg(static_cast<double>(g.detection) * 100.0, 0, 'f', 0).arg(static_cast<double>(g.restDetection) * 100.0, 0, 'f', 0);
                detailed = true;
                break;
            }
        }
        const int owner = _result.heatmapOwner[column];
        if (!detailed && owner >= 0 && owner < _result.species.size())
            text += QString("<br>(a top gene of %1)").arg(prettyName(_result.species[owner]).toHtmlEscaped());
        return text;
    }

    QString volcanoTooltip(int gene) const
    {
        if (!_detail.valid || gene < 0 || gene >= _detail.genes.size())
            return QString();
        return QString("<b>%1</b> in <b>%2</b><br>Difference to the other species: %3<br>Species: %4, others: %5 (SD %6)<br>Moderated t = %7, p = %8, FDR = %9<br>"
            "Detected in %10% of the cells of the species, %11% of the others")
            .arg(_detail.genes[gene].toHtmlEscaped(), prettyName(_detail.species).toHtmlEscaped())
            .arg(static_cast<double>(_detail.delta[gene]), 0, 'f', 3)
            .arg(static_cast<double>(_detail.restMean[gene] + _detail.delta[gene]), 0, 'f', 3).arg(static_cast<double>(_detail.restMean[gene]), 0, 'f', 3).arg(static_cast<double>(_detail.restSd[gene]), 0, 'f', 3)
            .arg(static_cast<double>(_detail.t[gene]), 0, 'f', 2).arg(static_cast<double>(_detail.pValue[gene]), 0, 'g', 3).arg(static_cast<double>(_detail.fdr[gene]), 0, 'g', 3)
            .arg(static_cast<double>(_detail.detection[gene]) * 100.0, 0, 'f', 0).arg(static_cast<double>(_detail.restDetection[gene]) * 100.0, 0, 'f', 0);
    }

    void rebuildVolcano(bool labelAll)
    {
        _labelAll = labelAll;
        if (!_detail.valid) {
            _volcano->setData({}, "Difference to the other species", "-log10 FDR", true);
            return;
        }
        QVector<Point> points;
        points.reserve(_detail.genes.size());
        const QSet<int> topSet(_detail.top.begin(), _detail.top.end());
        for (int g = 0; g < _detail.genes.size(); ++g) {
            Point point;
            point.id = g;
            point.x = static_cast<double>(_detail.delta[g]);
            point.y = -std::log10(std::max(static_cast<double>(_detail.fdr[g]), 1e-12));
            const bool isFocus = _detail.genes[g] == _focusGene;
            if (isFocus) {
                point.color = QColor("#00A2ED"); // the selected gene: blue like everywhere in the plugin
                point.highlighted = true;
                point.label = _detail.genes[g];
            }
            else if (topSet.contains(g)) {
                point.color = QColor("#000000");
                point.highlighted = true;
                point.label = _detail.genes[g];
            }
            else {
                point.color = static_cast<double>(_detail.fdr[g]) < _level ? QColor("#8c8c8c") : QColor("#d0d0d0");
                if (labelAll)
                    point.label = _detail.genes[g];
            }
            points.push_back(point);
        }
        _volcano->setGuideLines({ { true, -std::log10(_level), QString("FDR = %1").arg(_level) },{ false, 0.0, QString() } });
        _volcano->setData(points, "Difference to the other species (higher in the species >)", "-log10 FDR", true);
    }

    void rebuildTable()
    {
        const QSignalBlocker blocker(_table);
        _table->setSortingEnabled(false);
        _table->setRowCount(_detail.valid ? _detail.top.size() : 0);
        for (int row = 0; row < _table->rowCount(); ++row) {
            const int g = _detail.top[row];
            const auto number = [](double value, char format, int decimals) {
                auto* item = new QTableWidgetItem();
                item->setData(Qt::DisplayRole, QString::number(value, format, decimals).toDouble());
                return item;
                };
            const double species = static_cast<double>(_detail.restMean[g] + _detail.delta[g]);
            QList<QTableWidgetItem*> items;
            items << new QTableWidgetItem(_detail.genes[g])
                << number(static_cast<double>(_detail.delta[g]), 'f', 3)
                << number(species, 'f', 3)
                << number(static_cast<double>(_detail.restMean[g]), 'f', 3)
                << number(static_cast<double>(_detail.restSd[g]), 'f', 3)
                << number(static_cast<double>(_detail.t[g]), 'f', 2)
                << number(static_cast<double>(_detail.fdr[g]), 'g', 3)
                << number(static_cast<double>(_detail.detection[g]) * 100.0, 'f', 0)
                << number(static_cast<double>(_detail.restDetection[g]) * 100.0, 'f', 0);
            const bool isFocus = _detail.genes[g] == _focusGene;
            for (int column = 0; column < items.size(); ++column) {
                if (isFocus) { // the selected gene: blue background like the selected species rows of the tables
                    items[column]->setBackground(QBrush(QColor("#00A2ED")));
                    items[column]->setForeground(QBrush(QColor("#000000")));
                }
                _table->setItem(row, column, items[column]);
            }
        }
        _table->setSortingEnabled(true);
        _table->resizeColumnsToContents();
    }

    SpeciesDeResult _result;
    SpeciesDeDetail _detail;
    QSet<QString> _selectedSpecies;
    QString _focusGene;
    double _level = 0.05;
    bool _labelAll = false;

    QComboBox* _direction = nullptr;
    QSpinBox* _topCount = nullptr;
    QComboBox* _rank = nullptr;
    QLabel* _status = nullptr;
    QTabWidget* _tabs = nullptr;
    HeatmapWidget* _heatmap = nullptr;
    QLabel* _info = nullptr;
    QComboBox* _speciesCombo = nullptr;
    ZoomablePlotWidget* _volcano = nullptr;
    QTableWidget* _table = nullptr;
    QLabel* _detailInfo = nullptr;

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

    /** FDR below which a gene counts as significant (follows the "Significance level" setting). */
    void setSignificanceLevel(double level)
    {
        if (_significanceLevel == level)
            return;
        _significanceLevel = level;
        if (_significantOnly)
            invalidateFilter();
    }

    /** List only genes with a significant trait correlation in normal mode. */
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
    /** Genes with the same number of species are ordered by their weighted appearance (better ranks first when sorted descending). */
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        if (left.column() == 1 && right.column() == 1) {
            const int leftCount = left.data().toInt();
            const int rightCount = right.data().toInt();
            if (leftCount != rightCount) {
                return leftCount < rightCount;
            }
            const QAbstractItemModel* source = sourceModel();
            return source->index(left.row(), kWeightedAppearanceColumn, left.parent()).data().toDouble()
                < source->index(right.row(), kWeightedAppearanceColumn, right.parent()).data().toDouble();
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }

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
            const QVariant fdr = source->index(sourceRow, kTraitCorrelationColumn, sourceParent).data(kTraitSignificanceFdrRole);
            return fdr.isValid() && fdr.toDouble() < _significanceLevel;
        }
        return true;
    }

private:
    double             _significanceLevel = 0.05;
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
        const QVariant fdr = index.data(kTraitSignificanceFdrRole); // phylogenetically corrected when a species tree is usable
        const QVariant level = widget ? widget->property("significanceLevel") : QVariant();
        const bool significant = fdr.isValid() && fdr.toDouble() < (level.isValid() ? level.toDouble() : 0.05);
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

/**
 * Draws the weighted appearance (0..1) like the other bar columns: a grey bar of fixed width growing from the left,
 * with a thin black outline when the rank consistency across the species is significant (FDR < 0.05).
 * The number is only printed when the column is wide enough.
 */
class WeightedAppearanceBarDelegate : public QStyledItemDelegate
{
public:
    static constexpr int kBarWidth = 60;
    static constexpr int kBarHeight = 20;
    static constexpr int kMargin = 4;
    static constexpr int kTextWidth = 40;

    static constexpr int defaultColumnWidth() { return kBarWidth + 2 * kMargin + 2; }

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.features &= ~QStyleOptionViewItem::HasDecoration;

        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        bool ok = false;
        const double value = index.data(Qt::UserRole).toDouble(&ok);
        if (!ok || std::isnan(value))
            return;

        const int barHeight = std::min(kBarHeight, option.rect.height() - 4);
        const int barTop = option.rect.top() + (option.rect.height() - barHeight) / 2;
        const int barLeft = option.rect.left() + kMargin;
        const int length = static_cast<int>(std::max(0.0, std::min(1.0, value)) * kBarWidth);

        const QVariant fdr = index.data(kWeightedFdrRole);
        const QVariant level = widget ? widget->property("significanceLevel") : QVariant();
        const bool significant = fdr.isValid() && fdr.toDouble() < (level.isValid() ? level.toDouble() : 0.05);

        painter->save();
        painter->setClipRect(option.rect);
        painter->setPen(significant ? QPen(Qt::black, 1) : QPen(Qt::NoPen));
        painter->setBrush(Qt::gray);
        painter->drawRect(barLeft, barTop, length, barHeight);

        if (option.rect.width() >= kBarWidth + kTextWidth + 2 * kMargin) {
            const QRect textArea(barLeft + kBarWidth + kMargin, option.rect.top(), kTextWidth, option.rect.height());
            painter->setPen((option.state & QStyle::State_Selected) ? option.palette.color(QPalette::HighlightedText) : option.palette.color(QPalette::Text));
            painter->drawText(textArea, Qt::AlignLeft | Qt::AlignVCenter, QString::number(value, 'f', 2));
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setWidth(defaultColumnWidth());
        return size;
    }
};

/** Shows numbers with a fixed number of decimals (and a suffix) while the model keeps the numeric value for sorting. */
class FixedDecimalDelegate : public QStyledItemDelegate
{
public:
    FixedDecimalDelegate(int decimals, const QString& suffix, QObject* parent) : QStyledItemDelegate(parent), _decimals(decimals), _suffix(suffix) {}

    QString displayText(const QVariant& value, const QLocale&) const override
    {
        bool ok = false;
        const double number = value.toDouble(&ok);
        return ok ? QString::number(number, 'f', _decimals) + _suffix : QString();
    }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
    }

private:
    int _decimals;
    QString _suffix;
};

// ============================================================================================================
//  Gene table installation
// ============================================================================================================

void XSCGeneDetectPlugin::modifyListData()
{
    XSC_TRACE("XSCGeneDetectPlugin::modifyListData");
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
    XSC_TRACE("XSCGeneDetectPlugin::installGeneTableModel");
    QTableView* geneTableView = _settingsAction.getGeneTableView();
    CustomLineEdit* searchBox = _settingsAction.getSearchBox();

    // ---- 1. Filter/sort proxy in front of the model -------------------------------------------------------
    auto* proxyModel = new GeneFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setAllowedGenes(_settingsAction.getUniqueReturnGeneList());
    proxyModel->setSignificantOnly(_settingsAction.getTraitSignificantOnly().isChecked());
    proxyModel->setSignificanceLevel(_settingsAction.significanceLevel());
    geneTableView->setProperty("significanceLevel", _settingsAction.significanceLevel()); // read by the bar delegates

    connect(&_settingsAction.getTraitFdrThreshold(), &DecimalAction::valueChanged, proxyModel, [this, proxyModel]() {
        proxyModel->setSignificanceLevel(_settingsAction.significanceLevel());
        _settingsAction.getGeneTableView()->setProperty("significanceLevel", _settingsAction.significanceLevel());
        _settingsAction.getGeneTableView()->viewport()->update();
        });

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
    // further statistics columns (hidden until chosen in the column chooser): fixed decimals, sorted numerically
    geneTableView->setColumnWidth(kBestPositionColumn, 70);
    for (const auto& columnFormat : { std::make_tuple(kMeanDifferentialColumn, 2, QString()), std::make_tuple(kPositiveSpeciesColumn, 1, QString("%")), std::make_tuple(kMeanExpressionColumn, 2, QString()) }) {
        const int formatColumn = std::get<0>(columnFormat);
        geneTableView->setColumnWidth(formatColumn, 80);
        if (!dynamic_cast<FixedDecimalDelegate*>(geneTableView->itemDelegateForColumn(formatColumn))) {
            geneTableView->setItemDelegateForColumn(formatColumn, new FixedDecimalDelegate(std::get<1>(columnFormat), std::get<2>(columnFormat), geneTableView));
        }
    }
    geneTableView->setColumnWidth(kWeightedAppearanceColumn, WeightedAppearanceBarDelegate::defaultColumnWidth());
    if (!dynamic_cast<WeightedAppearanceBarDelegate*>(geneTableView->itemDelegateForColumn(kWeightedAppearanceColumn))) {
        geneTableView->setItemDelegateForColumn(kWeightedAppearanceColumn, new WeightedAppearanceBarDelegate(geneTableView));
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
//  Species differential expression window
// ============================================================================================================

bool XSCGeneDetectPlugin::selectGeneInGeneTable(const QString& gene)
{
    XSC_TRACE("XSCGeneDetectPlugin::selectGeneInGeneTable");
    auto* view = _settingsAction.getGeneTableView();
    if (!view || !view->model() || gene.isEmpty())
        return false;

    QAbstractItemModel* viewModel = view->model();
    for (int row = 0; row < viewModel->rowCount(); ++row) {
        if (viewModel->index(row, 0).data().toString() == gene) {
            view->selectRow(row);
            view->setCurrentIndex(viewModel->index(row, 0));
            view->scrollTo(viewModel->index(row, 0));
            return true;
        }
    }
    return false;
}

void XSCGeneDetectPlugin::showSpeciesDe()
{
    XSC_TRACE("XSCGeneDetectPlugin::showSpeciesDe");
    if (!_speciesDeDialog) {
        auto* dialog = new SpeciesDeDialog(&getWidget());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowFlag(Qt::Window);

        // the controls of the window are the settings of the group
        dialog->onParametersChanged = [this](int direction, int topCount, int rank) {
            _settingsAction.getDeDirection().setCurrentIndex(direction);
            _settingsAction.getDeTopGenes().setValue(topCount);
            _settingsAction.getDeRankBy().setCurrentIndex(rank);
            };
        dialog->onSpeciesChosen = [this](const QString& species) { refreshSpeciesDeDetail(species); };

        // a gene was clicked: select it in the gene table (it may not be listed there: it is then only highlighted in the window)
        dialog->onGeneClicked = [this](const QString& gene, const QString& species) {
            auto* window = static_cast<SpeciesDeDialog*>(_speciesDeDialog.data());
            if (!window)
                return;
            window->setFocusGene(gene);
            window->setStatus(selectGeneInGeneTable(gene) ? QString() : QString("%1 is not in the gene table (it is not a top-N gene of the current selection)").arg(gene));
            if (!species.isEmpty())
                _settingsAction.setSpeciesExplorerSelection({ species });
            };

        // a species label was clicked: select it in the species explorer (Ctrl: add / remove)
        dialog->onSpeciesClicked = [this](const QString& species, bool additive) {
            if (!additive) {
                _settingsAction.setSpeciesExplorerSelection({ species });
                return;
            }
            QStringList selection = _settingsAction.getSpeciesExplorerInMap().getSelectedOptions();
            if (selection.contains(species))
                selection.removeAll(species);
            else
                selection << species;
            _settingsAction.setSpeciesExplorerSelection(selection);
            };

        _settingsAction.setSpeciesDeFinishedCallback([this]() { refreshSpeciesDe(); });
        // closing the window stops all work for it
        connect(dialog, &QObject::destroyed, this, [this]() { _settingsAction.setSpeciesDeActive(false); });
        _speciesDeDialog = dialog;
    }

    auto* dialog = static_cast<SpeciesDeDialog*>(_speciesDeDialog.data());
    dialog->setControls(_settingsAction.getDeDirection().getCurrentIndex(), _settingsAction.getDeTopGenes().getValue(), _settingsAction.getDeRankBy().getCurrentIndex());
    dialog->setStatus("Computing... (the first time the expression of the selected cell type has to be read)");
    dialog->show();
    dialog->raise();
    dialog->activateWindow();

    _speciesDeLastGene = QString();
    _settingsAction.setSpeciesDeActive(true);
}

void XSCGeneDetectPlugin::refreshSpeciesDe()
{
    XSC_TRACE("XSCGeneDetectPlugin::refreshSpeciesDe");
    if (!_speciesDeDialog)
        return;
    auto* dialog = static_cast<SpeciesDeDialog*>(_speciesDeDialog.data());

    const SpeciesDeResult& result = _settingsAction.getSpeciesDeResult();
    const QStringList selected = _settingsAction.getSpeciesExplorerInMap().getSelectedOptions();
    _speciesDeLastGene = _settingsAction.getSelectedGeneAction().getString();

    dialog->setControls(_settingsAction.getDeDirection().getCurrentIndex(), _settingsAction.getDeTopGenes().getValue(), _settingsAction.getDeRankBy().getCurrentIndex());
    dialog->setResult(result, QSet<QString>(selected.begin(), selected.end()), _speciesDeLastGene);
    if (!result.valid)
        dialog->setStatus(result.message);

    refreshSpeciesDeDetail(dialog->currentSpecies());
}

void XSCGeneDetectPlugin::refreshSpeciesDeDetail(const QString& species)
{
    XSC_TRACE("XSCGeneDetectPlugin::refreshSpeciesDeDetail");
    if (!_speciesDeDialog)
        return;
    auto* dialog = static_cast<SpeciesDeDialog*>(_speciesDeDialog.data());
    dialog->setDetail(species.isEmpty() ? SpeciesDeDetail() : _settingsAction.computeSpeciesDeDetail(species), _settingsAction.significanceLevel());
}

void XSCGeneDetectPlugin::refreshSpeciesDeHighlights()
{
    XSC_TRACE("XSCGeneDetectPlugin::refreshSpeciesDeHighlights");
    if (!_speciesDeDialog || !_speciesDeDialog->isVisible())
        return;
    auto* dialog = static_cast<SpeciesDeDialog*>(_speciesDeDialog.data());

    const QStringList selected = _settingsAction.getSpeciesExplorerInMap().getSelectedOptions();
    const QString gene = _settingsAction.getSelectedGeneAction().getString();

    dialog->setSelectedSpecies(QSet<QString>(selected.begin(), selected.end()));

    // A gene that was clicked in the window but is not listed in the gene table keeps its highlight until the gene
    // selection of the table changes.
    if (gene != _speciesDeLastGene) {
        _speciesDeLastGene = gene;
        dialog->setFocusGene(gene);
    }
}

// ============================================================================================================
//  Trait plot window
// ============================================================================================================

void XSCGeneDetectPlugin::showTraitPlot()
{
    XSC_TRACE("XSCGeneDetectPlugin::showTraitPlot");
    if (!_traitPlotDialog) {
        auto* dialog = new TraitPlotDialog(&getWidget());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowFlag(Qt::Window); // an independent window (movable, resizable, can be put beside the views)

        // details of a gene point = the tooltips of its cells in the gene table
        dialog->geneDetailProvider = [this](int row) {
            auto variant = _settingsAction.getListModelAction().getVariant();
            auto* model = qobject_cast<QStandardItemModel*>(variant.value<QAbstractItemModel*>());
            if (!model || row < 0 || row >= model->rowCount())
                return QString();
            QString text = model->item(row, 0)->text() + "\n\n";
            text += model->item(row, kTraitCorrelationColumn)->data(Qt::ToolTipRole).toString() + "\n\n";
            text += model->item(row, kWeightedAppearanceColumn)->data(Qt::ToolTipRole).toString();
            return text;
            };

        // a click selects the gene in the gene table (when it is listed there)
        dialog->onGeneClicked = [this](const QString& gene) {
            auto* view = _settingsAction.getGeneTableView();
            if (!view || !view->model() || gene.isEmpty())
                return;
            QAbstractItemModel* viewModel = view->model();
            for (int row = 0; row < viewModel->rowCount(); ++row) {
                if (viewModel->index(row, 0).data().toString() == gene) {
                    view->selectRow(row);
                    view->setCurrentIndex(viewModel->index(row, 0));
                    view->scrollTo(viewModel->index(row, 0));
                    return;
                }
            }
            };

        dialog->onTabChanged = [this]() { refreshTraitPlot(); };
        _traitPlotDialog = dialog;
    }

    _traitPlotDialog->show();
    _traitPlotDialog->raise();
    _traitPlotDialog->activateWindow();
    refreshTraitPlot();
}

void XSCGeneDetectPlugin::refreshTraitPlot()
{
    XSC_TRACE("XSCGeneDetectPlugin::refreshTraitPlot");
    if (!_traitPlotDialog || !_traitPlotDialog->isVisible()) {
        return;
    }
    auto* dialog = static_cast<TraitPlotDialog*>(_traitPlotDialog.data()); // created as a TraitPlotDialog in showTraitPlot()
    using Point = ZoomablePlotWidget::Point;

    const TraitMatrix& matrix = _settingsAction.getTraitMatrix();
    const QString gene = _settingsAction.getSelectedGeneAction().getString();
    auto variant = _settingsAction.getListModelAction().getVariant();
    auto* model = qobject_cast<QStandardItemModel*>(variant.value<QAbstractItemModel*>());
    const double level = _settingsAction.significanceLevel();
    const bool haveModel = model && model->columnCount() > kWeightedAppearanceColumn;

    // ---- tab 1: the selected gene across the species ----
    {
        QVector<Point> points;
        QVector<TraitPlotDialog::SpeciesRow> rows;
        QString info;

        if (gene.isEmpty()) {
            info = "Select a gene in the gene table.";
        }
        else if (matrix.empty()) {
            info = "No trait correlation has been computed yet: choose a trait and cell type(s).";
        }
        else {
            const auto geneIt = matrix.geneIndex.find(gene);
            if (geneIt == matrix.geneIndex.end()) {
                info = "The gene is not part of the trait matrix (the trait correlation covers the listed top-N genes; see \"Trait Correlation Genes\").";
            }
            else {
                for (size_t i = 0; i < matrix.species.size(); ++i) {
                    TraitPlotDialog::SpeciesRow row;
                    row.species = matrix.species[i];
                    row.trait = matrix.trait[i];
                    row.expression = static_cast<double>(matrix.expression[i][geneIt->second]);
                    row.cells = i < matrix.cells.size() ? matrix.cells[i] : 0;
                    row.selected = matrix.selected[i] != 0;
                    rows.push_back(row);

                    QString label = row.species;
                    label.replace("_", " ");
                    Point point;
                    point.x = row.trait;
                    point.y = row.expression;
                    point.label = label;
                    point.highlighted = row.selected;
                    point.id = static_cast<int>(i);
                    points.push_back(point);
                }

                // the statistics are the tooltip of the trait cell: method, number of species, correlation, interval, p, FDR, leave-one-out
                if (haveModel) {
                    for (int row = 0; row < model->rowCount(); ++row) {
                        if (model->item(row, 0)->text() == gene) {
                            info = model->item(row, kTraitCorrelationColumn)->data(Qt::ToolTipRole).toString();
                            break;
                        }
                    }
                }
                info += "\n\nHover a point for details. Wheel: zoom, drag: move, double click: reset. Blue: species selected in the species explorer.";
            }
        }
        dialog->setWindowTitle(gene.isEmpty() ? QString("Trait plots") : gene + " - trait plots");
        dialog->setSpeciesData(points, rows, matrix.traitName, "Mean expression of " + gene, info);
    }

    // ---- tabs 2 and 3: all analysed genes (only the visible tab is rebuilt) ----
    const int tab = dialog->currentTab();
    if ((tab == 1 || tab == 2) && haveModel) {
        const auto& values = _settingsAction.getGeneTraitValues();
        const auto& extras = _settingsAction.getGeneTraitExtras();

        QVector<Point> points;
        points.reserve(model->rowCount());
        int significantCount = 0;
        for (int row = 0; row < model->rowCount(); ++row) {
            const QString geneName = model->item(row, 0)->text();
            const auto valueIt = values.find(geneName);
            const auto extrasIt = extras.find(geneName);
            if (valueIt == values.end() || extrasIt == extras.end())
                continue;

            const double fdr = static_cast<double>(extrasIt->second.significanceFdr);
            const bool significant = fdr < level;
            significantCount += significant ? 1 : 0;

            Point point;
            point.id = row;
            point.label = geneName;
            point.highlighted = significant;
            if (tab == 1) {
                point.x = static_cast<double>(valueIt->second);
                point.y = -std::log10(std::max(fdr, 1e-12));
            }
            else {
                if (model->item(row, 1)->data(Qt::DisplayRole).toInt() < 1)
                    continue; // only the listed genes have a weighted appearance
                point.x = model->item(row, kWeightedAppearanceColumn)->data(Qt::UserRole).toDouble();
                point.y = static_cast<double>(valueIt->second);
            }
            points.push_back(point);
        }

        const QString basis = QString(" %1 genes are significant.").arg(significantCount);
        if (tab == 1) {
            dialog->setVolcanoData(points, level, QString("%1 genes analysed, %2 significant (FDR < %3; blue). Click a gene to select it in the gene table. "
                "Hover for details. Wheel: zoom, drag: move, double click: reset.").arg(points.size()).arg(significantCount).arg(level));
        }
        else {
            dialog->setConservationData(points, QString("Listed genes: weighted appearance across species (x) against the trait correlation (y). Genes in the upper right are "
                "conserved markers whose expression rises with the trait; blue: significant (FDR < %1).%2").arg(level).arg(basis));
        }

    }
}

// ============================================================================================================
//  Gene row click
// ============================================================================================================

void XSCGeneDetectPlugin::onGeneRowChanged(const QModelIndex& current)
{
    XSC_TRACE("XSCGeneDetectPlugin::onGeneRowChanged");
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
    refreshSpeciesDeHighlights(); // ... and so does the species differential expression window
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
    QStandardItem* makeBarItem(const QVariant& sortValue, const QString& text, const QPixmap& bar, const QString& tooltip = QString()) {
        auto* item = new QStandardItem();
        item->setData(sortValue, Qt::UserRole);
        item->setData(QVariant(text), Qt::DisplayRole);
        item->setData(QVariant(bar), Qt::DecorationRole);
        if (!tooltip.isEmpty()) {
            item->setData(tooltip, Qt::ToolTipRole);
        }
        return item;
    }

    /** Table cell with plain text: DisplayRole = text, UserRole = raw number (an invalid variant sorts first). */
    QStandardItem* makeTextItem(const QVariant& sortValue, const QString& text, const QString& tooltip = QString()) {
        auto* item = new QStandardItem();
        item->setData(sortValue, Qt::UserRole);
        item->setData(QVariant(text), Qt::DisplayRole);
        if (!tooltip.isEmpty()) {
            item->setData(tooltip, Qt::ToolTipRole);
        }
        return item;
    }

    /** Species cell: readable name, species icon (if there is one) and tooltip; UserRole keeps the raw species name. */
    QStandardItem* makeSpeciesItem(const QString& species, const QString& extraText = QString()) {
        QString displayName = species;
        displayName.replace("_", " ");

        auto* item = new QStandardItem(displayName);
        const QString iconPath = ":/speciesicons/SpeciesIcons/" + species + ".svg";
        if (QFile::exists(iconPath)) {
            item->setData(QIcon(iconPath), Qt::DecorationRole);
            QString html = QString("<img src='%1' width='16' height='16'/> <b>%2</b>").arg(iconPath, displayName.toHtmlEscaped());
            if (!extraText.isEmpty()) {
                html += "<br>" + extraText.toHtmlEscaped().replace("\n", "<br>");
            }
            item->setData(html, Qt::ToolTipRole);
        }
        else {
            item->setData(extraText.isEmpty() ? displayName : displayName + "\n" + extraText, Qt::ToolTipRole);
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

// ============================================================================================================
//  Species table: visible columns (header context menu)
// ============================================================================================================

void XSCGeneDetectPlugin::applyDetailsColumnVisibility()
{
    XSC_TRACE("XSCGeneDetectPlugin::applyDetailsColumnVisibility");
    QTableView* view = _settingsAction.getSelectionDetailsTable();
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model) {
        return;
    }

    // The two layouts of the table (counts / statistics) have their own choice, keyed by the number of columns.
    // The extra statistics columns of the statistics layout start hidden.
    const int count = model->columnCount();
    if (!_detailsHiddenColumns.contains(count)) {
        QSet<int> defaults;
        if (count == 13) {
            defaults = { 9, 10, 11, 12 };
        }
        _detailsHiddenColumns.insert(count, defaults);
    }
    const QSet<int>& hidden = _detailsHiddenColumns[count];
    for (int column = 0; column < count; ++column) {
        view->setColumnHidden(column, hidden.contains(column) || column == _detailsRedundantColumn);
    }
}

void XSCGeneDetectPlugin::showDetailsColumnMenu(const QPoint& globalPosition)
{
    XSC_TRACE("XSCGeneDetectPlugin::showDetailsColumnMenu");
    QTableView* view = _settingsAction.getSelectionDetailsTable();
    QAbstractItemModel* model = view ? view->model() : nullptr;
    if (!model || model->columnCount() < 2) {
        return;
    }
    applyDetailsColumnVisibility(); // makes sure the choice for this layout exists

    const int count = model->columnCount();
    QMenu menu;
    menu.addSection("Visible columns");
    for (int column = 1; column < count; ++column) { // the species column always stays
        if (column == _detailsRedundantColumn) {
            continue;
        }
        QString name = model->headerData(column, Qt::Horizontal).toString();
        name.replace("\n", " ");
        QAction* action = menu.addAction(name);
        action->setCheckable(true);
        action->setChecked(!_detailsHiddenColumns[count].contains(column));
        connect(action, &QAction::toggled, this, [this, count, column](bool visible) {
            if (visible) {
                _detailsHiddenColumns[count].remove(column);
            }
            else {
                _detailsHiddenColumns[count].insert(column);
            }
            applyDetailsColumnVisibility();
            });
    }
    menu.exec(globalPosition);
}

// ============================================================================================================
//  Species table: "count" view (shown while no gene is selected)
// ============================================================================================================

void XSCGeneDetectPlugin::selectedCellCountStatusBarAdd()
{
    XSC_TRACE("XSCGeneDetectPlugin::selectedCellCountStatusBarAdd");
    const auto& speciesCountMap = _settingsAction.getSelectedSpeciesCellCountMap();
    QTableView* detailsTable = _settingsAction.getSelectionDetailsTable();

    if (!speciesCountMap.empty())
    {
        auto* model = new QStandardItemModel();

        bool singleColumn = false;
        const QString middleTitle = middleFractionTitle(_settingsAction.getCurrentHierarchyItemsMiddleForTable(), singleColumn);
        model->setHorizontalHeaderLabels({ "Species", "Fraction in Neuronal", "Fraction in " + middleTitle, "Count of Selected", "Count of All",
                                           "Share of\nSelection (%)", "Selected Cells\nof Species (%)" });
        model->setHeaderData(0, Qt::Horizontal, "Species. Right-click a column header to show or hide columns.", Qt::ToolTipRole);
        model->setHeaderData(1, Qt::Horizontal, "Selected cells of the species inside the included top clusters, as a percentage of all its cells in those clusters.", Qt::ToolTipRole);
        model->setHeaderData(2, Qt::Horizontal, "Selected cells of the species inside the included top clusters, as a percentage of its cells in the " + middleTitle + " clusters that contain selected cells.", Qt::ToolTipRole);
        model->setHeaderData(3, Qt::Horizontal, "Number of selected cells of the species.", Qt::ToolTipRole);
        model->setHeaderData(4, Qt::Horizontal, "Number of cells of the species (selected and not selected).", Qt::ToolTipRole);
        model->setHeaderData(5, Qt::Horizontal, "Composition of the selection: the percentage of all selected cells that belong to the species.", Qt::ToolTipRole);
        model->setHeaderData(6, Qt::Horizontal, "Coverage: the percentage of the cells of the species that are selected.", Qt::ToolTipRole);

        // ---- pass 1: value range of every bar column and the size of the selection ----
        ValueRange topRange, middleRange, selectedRange, totalRange, shareRange, coverageRange;
        double allSelectedCells = 0.0;
        for (const auto& [species, details] : speciesCountMap)
        {
            topRange.include(percentOf(details.countAbundanceNumerator, details.abundanceTop));
            middleRange.include(percentOf(details.countAbundanceNumerator, details.abundanceMiddle));
            selectedRange.include(static_cast<float>(details.selectedCellsCount));
            totalRange.include(static_cast<float>(details.selectedCellsCount + details.nonSelectedCellsCount));
            allSelectedCells += details.selectedCellsCount;
        }
        // (the share needs the size of the whole selection, so these two ranges are collected after the loop above)
        for (const auto& [species, details] : speciesCountMap)
        {
            shareRange.include(static_cast<float>(GeneStats::percent(details.selectedCellsCount, allSelectedCells)));
            coverageRange.include(static_cast<float>(GeneStats::percent(details.selectedCellsCount, details.selectedCellsCount + details.nonSelectedCellsCount)));
        }

        // ---- pass 2: one row per species ----
        for (const auto& [species, details] : speciesCountMap)
        {
            const int selectedCellsCount = details.selectedCellsCount;
            const int total = details.selectedCellsCount + details.nonSelectedCellsCount;
            const double share = GeneStats::percent(selectedCellsCount, allSelectedCells);
            const double coverage = GeneStats::percent(selectedCellsCount, total);

            QList<QStandardItem*> rowItems;
            rowItems << makeSpeciesItem(species, QString("%1 cells, %2 selected (%3% of the species)").arg(total).arg(selectedCellsCount).arg(coverage, 0, 'f', 1));

            const float topAbundance = percentOf(details.countAbundanceNumerator, details.abundanceTop);
            rowItems << makeBarItem(topAbundance, QString::number(topAbundance, 'f', 2),
                makeBarPixmap(60, 20, getNormalizedSize(topAbundance, topRange.min, topRange.max)),
                QString("%1 selected cells of the species lie in the included top clusters, which hold %2 of its cells: %3%")
                .arg(details.countAbundanceNumerator).arg(details.abundanceTop).arg(topAbundance, 0, 'f', 2));

            const float middleAbundance = percentOf(details.countAbundanceNumerator, details.abundanceMiddle);
            rowItems << makeBarItem(middleAbundance, QString::number(middleAbundance, 'f', 2),
                makeBarPixmap(80, 20, getNormalizedSize(middleAbundance, middleRange.min, middleRange.max)),
                QString("%1 selected cells lie in the included top clusters; the %2 clusters that contain selected cells hold %3 cells of the species: %4%")
                .arg(details.countAbundanceNumerator).arg(middleTitle).arg(details.abundanceMiddle).arg(middleAbundance, 0, 'f', 2));

            rowItems << makeBarItem(selectedCellsCount, QString::number(selectedCellsCount),
                makeBarPixmap(60, 20, getNormalizedSize(static_cast<float>(selectedCellsCount), selectedRange.min, selectedRange.max)),
                QString("%1 cells of the species are selected: %2% of the species and %3% of the whole selection.")
                .arg(selectedCellsCount).arg(coverage, 0, 'f', 1).arg(share, 0, 'f', 1));

            rowItems << makeBarItem(total, QString::number(total),
                makeBarPixmap(60, 20, getNormalizedSize(static_cast<float>(total), totalRange.min, totalRange.max)),
                QString("%1 cells in total: %2 selected and %3 not selected.").arg(total).arg(selectedCellsCount).arg(details.nonSelectedCellsCount));

            rowItems << makeBarItem(share, QString::number(share, 'f', 1),
                makeBarPixmap(70, 20, getNormalizedSize(static_cast<float>(share), shareRange.min, shareRange.max)),
                QString("%1 of the %2 selected cells (%3%) belong to this species.").arg(selectedCellsCount).arg(static_cast<long long>(allSelectedCells)).arg(share, 0, 'f', 1));
            rowItems << makeBarItem(coverage, QString::number(coverage, 'f', 1),
                makeBarPixmap(80, 20, getNormalizedSize(static_cast<float>(coverage), coverageRange.min, coverageRange.max)),
                QString("%1 of the %2 cells of this species (%3%) are selected.").arg(selectedCellsCount).arg(total).arg(coverage, 0, 'f', 1));

            model->appendRow(rowItems);
        }

        detailsTable->setSortingEnabled(true);
        setSelectionDetailsModel(detailsTable, model);
        model->setSortRole(Qt::UserRole);
        model->sort(1, Qt::DescendingOrder);

        detailsTable->setSelectionMode(QAbstractItemView::NoSelection);
        detailsTable->verticalHeader()->hide();
        detailsTable->setColumnWidth(1, 70);
        detailsTable->setColumnWidth(2, 90);
        detailsTable->setColumnWidth(3, 70);
        detailsTable->setColumnWidth(4, 70);
        detailsTable->setColumnWidth(5, 80);   // bar 70 px
        detailsTable->setColumnWidth(6, 90);   // bar 80 px
        _detailsRedundantColumn = singleColumn ? 2 : -1;
        applyDetailsColumnVisibility();
        detailsTable->update();
    }

    adjustTableWidths("small");
}

// ============================================================================================================
//  Species table: "statistics" view (shown for the clicked gene)
// ============================================================================================================

void XSCGeneDetectPlugin::selectedCellStatisticsStatusBarAdd(std::map<QString, SpeciesDetailsStats> statisticsValues, QStringList selectedSpecies)
{
    XSC_TRACE("XSCGeneDetectPlugin::selectedCellStatisticsStatusBarAdd");
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
        model->setHorizontalHeaderLabels({ "Species", "Mean Gene Differential Expression", "Gene Appearance Rank", "Fraction in Neuronal", "Fraction in " + middleTitle,
                                           "Count of Selected", "Mean Gene Expression of Selected", "Count of Non Selected", "Mean Gene Expression of Non Selected",
                                           "Log2\nFold Change", "Selected\nCells (%)", "Rank\nPercentile (%)", "Rank\nWeight" });

        // header tooltips: what every column means
        const QStringList headerTips = {
            "Species. Right-click a column header to show or hide columns (the last four statistics are hidden by default).",
            "Mean expression of the gene in the selected cells minus its mean expression in the other cells of the species.",
            "Rank of the gene among all genes of the species by differential expression (1 = strongest). For the Negative type the rank counts from the other end.",
            "Selected cells of the species inside the included top clusters, as a percentage of all its cells in those clusters.",
            "Selected cells inside the included top clusters, as a percentage of the species' cells in the " + middleTitle + " clusters that contain selected cells.",
            "Number of selected cells of the species.",
            "Mean expression of the gene in the selected cells of the species.",
            "Number of cells of the species that are not selected.",
            "Mean expression of the gene in the cells of the species that are not selected.",
            "log2 of (mean in the selected cells / mean in the other cells). Undefined (n/a) when a mean is not positive. Meaningful for linear-scale expression; for log-transformed data read the differential instead.",
            "Percentage of the cells of the species that are selected.",
            "Where the gene stands among all genes of the species: position / number of genes in percent (small = strong, 1% = in the top 1%).",
            "Weight of the gene's position in the top-N list: 1 for position 1 down to 1/N for position N; 0 outside the top-N or when the mean in the selected cells is not positive."
        };
        for (int column = 0; column < headerTips.size(); ++column) {
            model->setHeaderData(column, Qt::Horizontal, headerTips[column], Qt::ToolTipRole);
        }

        const int numofGenes = _settingsAction.getTopNGenesFilter().getMaximum();
        const int topNValue = _settingsAction.getTopNGenesFilter().getValue();
        const int topN = _settingsAction.weightedTopN();
        const bool negativeType = _settingsAction.weightedNegative();
        const QString orderingName = _settingsAction.getTypeofTopNGenes().getCurrentText();

        // ---- pass 1: value range of every bar column ----
        ValueRange differenceRange, rankRange, topRange, middleRange, selectedCountRange, meanSelectedRange, nonSelectedCountRange, meanNonSelectedRange;
        ValueRange foldRange, selectedPercentRange, strengthRange, weightRange; // the four further statistics columns
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

            // further statistics (same formulas as in pass 2)
            const int rangeListLength = _settingsAction.speciesGeneListLength(species);
            const int rangePosition = GeneStats::positionFromRank(stats.rank, rangeListLength, negativeType);
            selectedPercentRange.include(static_cast<float>(GeneStats::percent(stats.countSelected, static_cast<double>(stats.countSelected) + stats.countNonSelected)));
            if (rangePosition > 0) {
                strengthRange.include(static_cast<float>(100.0 - GeneStats::percent(rangePosition, rangeListLength))); // strong = long bar, like the rank column
            }
            const bool rangeTopGene = rangePosition >= 1 && rangePosition <= topN && stats.meanSelected > 0;
            weightRange.include(static_cast<float>(rangeTopGene ? GeneStats::rankWeight(rangePosition, topN) : 0.0));
            double rangeFold = 0.0;
            if (GeneStats::log2FoldChange(stats.meanSelected, stats.meanNonSelected, rangeFold)) {
                foldRange.include(static_cast<float>(rangeFold));
            }
        }

        // ---- pass 2: one row per species ----
        for (const auto& [species, details] : speciesCountMap)
        {
            const auto it = statisticsValues.find(species);
            QList<QStandardItem*> rowItems;

            if (it != statisticsValues.end()) {
                const auto& stats = it->second;

                // values shared by several cells
                const double allCells = static_cast<double>(stats.countSelected) + static_cast<double>(stats.countNonSelected);
                const float difference = stats.meanSelected - stats.meanNonSelected;
                const int listLength = _settingsAction.speciesGeneListLength(species);
                const int position = GeneStats::positionFromRank(stats.rank, listLength, negativeType);
                const double percentile = GeneStats::percent(position, listLength);
                const bool topGene = position >= 1 && position <= topN && stats.meanSelected > 0;
                const double weight = topGene ? GeneStats::rankWeight(position, topN) : 0.0;
                double log2Fold = 0.0;
                const bool haveFold = GeneStats::log2FoldChange(stats.meanSelected, stats.meanNonSelected, log2Fold);
                const double selectedPercent = GeneStats::percent(stats.countSelected, allCells);

                rowItems << makeSpeciesItem(species, QString("%1 cells, %2 selected (%3%)").arg(static_cast<long long>(allCells)).arg(stats.countSelected).arg(selectedPercent, 0, 'f', 1));

                // Mean gene differential expression: centred bar (right = higher in the selection)
                const float differenceLength = getNormalizedSizePosNeg(difference, differenceRange.min, differenceRange.max);
                rowItems << makeBarItem(difference, QString::number(difference, 'f', 2),
                    makeCenteredBarPixmap(75, 25, differenceLength, difference >= 0),
                    QString("Mean differential expression = mean in the selected cells - mean in the other cells\n= %1 - %2 = %3\n%4")
                    .arg(stats.meanSelected, 0, 'f', 4).arg(stats.meanNonSelected, 0, 'f', 4).arg(difference, 0, 'f', 4)
                    .arg(difference > 0 ? "Higher in the selection." : (difference < 0 ? "Lower in the selection." : "No difference.")));

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
                QString rankTip;
                if (position > 0) {
                    rankTip = QString("Position %1 of %2 genes of the species (%3 ordering): the top %4% of its genes.\n").arg(position).arg(listLength).arg(orderingName).arg(percentile, 0, 'f', 2);
                    if (negativeType) {
                        rankTip += QString("(The rank shown, %1, counts from the other end for the Negative type.)\n").arg(rank);
                    }
                    rankTip += topGene ? QString("It is a top-N gene of this species (N = %1); weight %2.").arg(topN).arg(weight, 0, 'f', 3)
                        : QString("It is not a top-N gene of this species (N = %1, a positive mean in the selected cells is required).").arg(topN);
                }
                else {
                    rankTip = QString("Rank %1.").arg(rank);
                }
                rowItems << makeBarItem(rank, QString::number(rank), makeBarPixmap(75, 20, rankLength), rankTip);

                // Fractions of the species' cells
                const float topAbundance = percentOf(stats.countAbundanceNumerator, stats.abundanceTop);
                rowItems << makeBarItem(topAbundance, QString::number(topAbundance, 'f', 2),
                    makeBarPixmap(60, 20, getNormalizedSize(topAbundance, topRange.min, topRange.max)),
                    QString("%1 selected cells of the species lie in the included top clusters, which hold %2 of its cells: %3%")
                    .arg(stats.countAbundanceNumerator).arg(stats.abundanceTop).arg(topAbundance, 0, 'f', 2));

                const float middleAbundance = percentOf(stats.countAbundanceNumerator, stats.abundanceMiddle);
                rowItems << makeBarItem(middleAbundance, QString::number(middleAbundance, 'f', 2),
                    makeBarPixmap(80, 20, getNormalizedSize(middleAbundance, middleRange.min, middleRange.max)),
                    QString("%1 selected cells lie in the included top clusters; the %2 clusters that contain selected cells hold %3 cells of the species: %4%")
                    .arg(stats.countAbundanceNumerator).arg(middleTitle).arg(stats.abundanceMiddle).arg(middleAbundance, 0, 'f', 2));

                // Selected cells: count and mean expression
                rowItems << makeBarItem(stats.countSelected, QString::number(stats.countSelected),
                    makeBarPixmap(55, 20, getNormalizedSize(static_cast<float>(stats.countSelected), selectedCountRange.min, selectedCountRange.max)),
                    QString("%1 of the %2 cells of the species are selected (%3%).").arg(stats.countSelected).arg(static_cast<long long>(allCells)).arg(selectedPercent, 0, 'f', 1));

                QString meanSelectedTip = QString("Mean expression in the %1 selected cells: %2\nIn the %3 other cells: %4")
                    .arg(stats.countSelected).arg(stats.meanSelected, 0, 'f', 4).arg(stats.countNonSelected).arg(stats.meanNonSelected, 0, 'f', 4);
                if (stats.meanNonSelected > 0 && stats.meanSelected > 0) {
                    meanSelectedTip += QString("\nRatio selected / other: %1").arg(stats.meanSelected / stats.meanNonSelected, 0, 'f', 2);
                }
                rowItems << makeBarItem(stats.meanSelected, QString::number(stats.meanSelected, 'f', 2),
                    makeBarPixmap(70, 20, getNormalizedSize(stats.meanSelected, meanSelectedRange.min, meanSelectedRange.max)), meanSelectedTip);

                // Non selected cells: count and mean expression
                rowItems << makeBarItem(stats.countNonSelected, QString::number(stats.countNonSelected),
                    makeBarPixmap(60, 20, getNormalizedSize(static_cast<float>(stats.countNonSelected), nonSelectedCountRange.min, nonSelectedCountRange.max)),
                    QString("%1 cells of the species are not selected.").arg(stats.countNonSelected));

                rowItems << makeBarItem(stats.meanNonSelected, QString::number(stats.meanNonSelected, 'f', 2),
                    makeBarPixmap(75, 20, getNormalizedSize(stats.meanNonSelected, meanNonSelectedRange.min, meanNonSelectedRange.max)),
                    QString("Mean expression in the %1 cells that are not selected: %2").arg(stats.countNonSelected).arg(stats.meanNonSelected, 0, 'f', 4));

                // Further statistics (hidden by default; right-click the header to show them) - drawn like the other columns
                const QString foldTip = haveFold
                    ? QString("log2(%1 / %2) = %3: the gene is %4 times %5 in the selection.")
                    .arg(stats.meanSelected, 0, 'f', 3).arg(stats.meanNonSelected, 0, 'f', 3).arg(log2Fold, 0, 'f', 3)
                    .arg(std::pow(2.0, std::abs(log2Fold)), 0, 'f', 2).arg(log2Fold >= 0 ? "higher" : "lower")
                    : QString("Undefined: a mean is not positive, so the ratio cannot be taken.");
                if (haveFold) {
                    // centred bar like the differential: right = higher in the selection
                    const float foldLength = getNormalizedSizePosNeg(static_cast<float>(log2Fold), foldRange.min, foldRange.max);
                    rowItems << makeBarItem(log2Fold, QString::number(log2Fold, 'f', 2), makeCenteredBarPixmap(75, 25, foldLength, log2Fold >= 0), foldTip);
                }
                else {
                    rowItems << makeTextItem(QVariant(), "n/a", foldTip);
                }

                rowItems << makeBarItem(selectedPercent, QString::number(selectedPercent, 'f', 1),
                    makeBarPixmap(70, 20, getNormalizedSize(static_cast<float>(selectedPercent), selectedPercentRange.min, selectedPercentRange.max)),
                    QString("%1 of the %2 cells of the species are selected.").arg(stats.countSelected).arg(static_cast<long long>(allCells)));

                if (position > 0) {
                    // the bar shows the STRENGTH (100 - percentile): a gene near the top gets a long bar, like in the rank column
                    rowItems << makeBarItem(percentile, QString::number(percentile, 'f', 2),
                        makeBarPixmap(75, 20, getNormalizedSize(static_cast<float>(100.0 - percentile), strengthRange.min, strengthRange.max)),
                        QString("Position %1 of %2 genes: the top %3% of the genes of this species (long bar = strong).").arg(position).arg(listLength).arg(percentile, 0, 'f', 2));
                }
                else {
                    rowItems << makeTextItem(QVariant(), "n/a", "Position unknown.");
                }

                rowItems << makeBarItem(weight, QString::number(weight, 'f', 3),
                    makeBarPixmap(70, 20, getNormalizedSize(static_cast<float>(weight), weightRange.min, weightRange.max)),
                    topGene ? QString("(N - position + 1) / N = (%1 - %2 + 1) / %1 = %3").arg(topN).arg(position).arg(weight, 0, 'f', 3)
                    : QString("0: not a top-N gene of this species."));
            }
            else {
                // species without statistics for this gene
                rowItems << makeSpeciesItem(species);
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
        detailsTable->setColumnWidth(1, 85);
        detailsTable->setColumnWidth(2, 85);
        detailsTable->setColumnWidth(3, 70);
        detailsTable->setColumnWidth(4, 90);
        detailsTable->setColumnWidth(5, 65);
        detailsTable->setColumnWidth(6, 80);
        detailsTable->setColumnWidth(7, 70);
        detailsTable->setColumnWidth(8, 85);
        detailsTable->setColumnWidth(9, 85);    // centred bar 75 px
        detailsTable->setColumnWidth(10, 80);   // bar 70 px
        detailsTable->setColumnWidth(11, 85);   // bar 75 px
        detailsTable->setColumnWidth(12, 80);   // bar 70 px
        _detailsRedundantColumn = singleColumn ? 4 : -1;
        applyDetailsColumnVisibility();
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
    XSC_TRACE("XSCGeneDetectPlugin::updatePhylogeneticTree");
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
    XSC_TRACE("XSCGeneDetectPlugin::selectedCellCountStatusBarRemove");
    setSelectionDetailsModel(_settingsAction.getSelectionDetailsTable(), new QStandardItemModel());
}

void XSCGeneDetectPlugin::selectedCellStatisticsStatusBarRemove()
{
    XSC_TRACE("XSCGeneDetectPlugin::selectedCellStatisticsStatusBarRemove");
    setSelectionDetailsModel(_settingsAction.getSelectionDetailsTable(), new QStandardItemModel());
}
void XSCGeneDetectPlugin::updateSpeciesData(QJsonObject& node, const std::map<QString, SpeciesDetailsStats>& speciesExpressionMap) {
    XSC_TRACE("XSCGeneDetectPlugin::updateSpeciesData");
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
    XSC_TRACE("XSCGeneDetectPlugin::updateTreeData");
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
    XSC_TRACE("XSCGeneDetectPlugin::onDataEvent");
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
    XSC_TRACE("XSCGeneDetectPlugin::fromVariantMap");
    ViewPlugin::fromVariantMap(variantMap);

    mv::util::variantMapMustContain(variantMap, "CSCGDV:XSC Gene Detect Plugin Settings");
    _settingsAction.fromVariantMap(variantMap["CSCGDV:XSC Gene Detect Plugin Settings"].toMap());
    // modifyTableData();
    // _settingsAction.getStartComputationTriggerAction().trigger();

}

QVariantMap XSCGeneDetectPlugin::toVariantMap() const
{
    XSC_TRACE("XSCGeneDetectPlugin::toVariantMap");
    QVariantMap variantMap = ViewPlugin::toVariantMap();

    _settingsAction.insertIntoVariantMap(variantMap);

    return variantMap;
}
ViewPlugin* XSCGeneDetectPluginFactory::produce()
{
    XSC_TRACE("XSCGeneDetectPluginFactory::produce");
    return new XSCGeneDetectPlugin(this);
}

mv::DataTypes XSCGeneDetectPluginFactory::supportedDataTypes() const
{
    XSC_TRACE("XSCGeneDetectPluginFactory::supportedDataTypes");
    DataTypes supportedTypes;

    // This example analysis plugin is compatible with points datasets
    supportedTypes.append(PointType);

    return supportedTypes;
}

mv::gui::PluginTriggerActions XSCGeneDetectPluginFactory::getPluginTriggerActions(const mv::Datasets& datasets) const
{
    XSC_TRACE("XSCGeneDetectPluginFactory::getPluginTriggerActions");
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
