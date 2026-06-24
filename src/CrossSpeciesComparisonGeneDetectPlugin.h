#pragma once

#include <ViewPlugin.h>

#include <Dataset.h>
#include <widgets/DropWidget.h>

#include <PointData/PointData.h>
#include "SettingsAction.h"
#include <QWidget>
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

class CrossSpeciesComparisonGeneDetectPlugin : public ViewPlugin
{
    Q_OBJECT

public:

    /**
     * Constructor
     * @param factory Pointer to the plugin factory
     */
    CrossSpeciesComparisonGeneDetectPlugin(const PluginFactory* factory);

    /** Destructor */
    ~CrossSpeciesComparisonGeneDetectPlugin() override = default;
    
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

};

/**
 * CrossSpeciesComparisonGeneDetect plugin factory class
 *
 * Note: Factory does not need to be altered (merely responsible for generating new plugins when requested)
 */
class CrossSpeciesComparisonGeneDetectPluginFactory : public ViewPluginFactory
{
    Q_INTERFACES(mv::plugin::ViewPluginFactory mv::plugin::PluginFactory)
    Q_OBJECT
    Q_PLUGIN_METADATA(IID   "studio.manivault.CrossSpeciesComparisonGeneDetectPlugin"
                      FILE  "CrossSpeciesComparisonGeneDetectPlugin.json")

public:

    /** Default constructor */
    CrossSpeciesComparisonGeneDetectPluginFactory() {}

    /** Destructor */
    ~CrossSpeciesComparisonGeneDetectPluginFactory() override {}
    
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
