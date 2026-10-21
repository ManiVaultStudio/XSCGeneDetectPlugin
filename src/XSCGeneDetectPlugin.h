#pragma once

#include <ViewPlugin.h>

#include <Dataset.h>
#include <widgets/DropWidget.h>

#include <PointData/PointData.h>
#include "SettingsAction.h"
#include <QWidget>
#include <QModelIndex>
#include <QPointer>
#include <QHash>
#include <QSet>
#include <QDialog>
#include <QLabel>
#include <QStandardItemModel>
#include <actions/HorizontalToolbarAction.h>
#include <actions/VerticalToolbarAction.h>
#include <actions/ToolbarAction.h>
/** All plugin related classes are in the ManiVault plugin namespace */
using namespace mv::plugin;

/** Drop widget used in this plugin is located in the ManiVault gui namespace */
using namespace mv::gui;

/** Dataset reference used in this plugin is located in the ManiVault util namespace */
using namespace mv::util;

class QLabel;

class XSCGeneDetectPlugin : public ViewPlugin
{
    Q_OBJECT

public:

    /**
     * Constructor
     * @param factory Pointer to the plugin factory
     */
    XSCGeneDetectPlugin(const PluginFactory* factory);

    /** Destructor */
    ~XSCGeneDetectPlugin() override = default;

    /** This function is called by the core after the view plugin has been created */
    void init() override;
    void modifyListData();
    void updateSpeciesData(QJsonObject& node, const std::map<QString, SpeciesDetailsStats>& speciesExpressionMap);
    void updateTreeData(QJsonObject& node, const std::map<QString, SpeciesDetailsStats>& speciesExpressionMap);
    void updatePhylogeneticTree();
    void adjustTableWidths(const QString& value);
    void selectedCellCountStatusBarAdd();
    void selectedCellCountStatusBarRemove();
    void selectedCellStatisticsStatusBarRemove();
    void selectedCellStatisticsStatusBarAdd(std::map<QString, SpeciesDetailsStats> statisticsValues, QStringList selectedSpecies);
    void geneExplorer();
    void geneExplorer(QString selectedSpecies);

    /** Build the proxy/filter/delegates for a freshly computed gene table model and show it. */
    void installGeneTableModel(QStandardItemModel* model);

    /** Called when the current row of the gene table changes (gene clicked). */
    void onGeneRowChanged(const QModelIndex& current);

    /**
     * Show the expression of \p gene in the cells of \p speciesNames in the embedding scatterplot.
     * @return false when the required datasets are not available (nothing was changed)
     */
    bool showGeneInSpeciesEmbedding(const QString& gene, const QStringList& speciesNames);

    /** Show/hide the columns of the species table according to the user's choice (header context menu). */
    void applyDetailsColumnVisibility();

    /** Context menu of the species table header: choose the visible columns. */
    void showDetailsColumnMenu(const QPoint& globalPosition);

    /** Open (or bring to front) the plot of the expression of the selected gene against the trait. */
    void showTraitPlot();

    /** Update the open plot (selected gene, new correlation values); does nothing when it is not open. */
    void refreshTraitPlot();
    /**
     * Invoked when a data event occurs
     * @param dataEvent Data event which occurred
     */
     //void onDataEvent(mv::DatasetEvent* dataEvent);

    SettingsAction& getSettingsAction() { return _settingsAction; }

public: // Serialization

    /**
     * Load plugin from variant map
     * @param Variant map representation of the plugin
     */
    Q_INVOKABLE void fromVariantMap(const QVariantMap& variantMap) override;

    /**
     * Save plugin to variant map
     * @return Variant map representation of the plugin
     */
    Q_INVOKABLE QVariantMap toVariantMap() const override;

protected:

    SettingsAction _settingsAction;
    //HorizontalToolbarAction     _toolbarAction;    // Toolbar action that is shown in the main window
    //VerticalToolbarAction         _verticalGroupAction; // Vertical group action that is shown in the main window
    mv::Dataset<Points> _pointsDataset;
    mv::Dataset<Clusters> _clusterDataset;
    mv::Dataset<Points> _lowDimTSNEDataset;

    QHash<int, QSet<int>> _detailsHiddenColumns;   /** hidden columns of the species table, per layout (keyed by its number of columns) */
    int _detailsRedundantColumn = -1;              /** column hidden because it repeats another one (middle fraction), -1: none */

    QPointer<QDialog> _traitPlotDialog;   /** the plots window (a TraitPlotDialog, see the .cpp; created on first use) */

};

/**
 * XSCGeneDetect plugin factory class
 *
 * Note: Factory does not need to be altered (merely responsible for generating new plugins when requested)
 */
class XSCGeneDetectPluginFactory : public ViewPluginFactory
{
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
        Q_OBJECT
        Q_PLUGIN_METADATA(IID   "studio.manivault.XSCGeneDetectPlugin"
            FILE  "XSCGeneDetectPlugin.json")

public:

    /** Default constructor */
    XSCGeneDetectPluginFactory() {}

    /** Destructor */
    ~XSCGeneDetectPluginFactory() override {}

    /** Creates an instance of the example view plugin */
    ViewPlugin* produce() override;

    /** Returns the data types that are supported by the example view plugin */
    mv::DataTypes supportedDataTypes() const override;

    /**
     * Get plugin trigger actions given \p datasets
     * @param datasets Vector of input datasets
     * @return Vector of plugin trigger actions
     */
    PluginTriggerActions getPluginTriggerActions(const mv::Datasets& datasets) const override;
};
