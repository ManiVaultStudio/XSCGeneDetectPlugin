// Included after ZoomablePlotWidget and HeatmapWidget.
class TraitPlotDialog : public QDialog
{
    using Point = ZoomablePlotWidget::Point;
    using Values = std::vector<double>;
    using Matrix = std::vector<Values>;
    enum Plot { Scatter, ExpressionHeatmap, Distribution, Ranking, Pca, Coexpression, Profile,
        ScoreHeatmap, SpeciesBoxes, GeneBoxes, Sign, Clustering, Variability, FocusScatter };

    struct Gene {
        QString name;
        size_t column;
        double correlation;
    };

public:
    std::function<bool(const QString&, bool)> onGeneClicked;
    std::function<void(const QString&, bool)> onSpeciesClicked;
    std::function<std::pair<std::vector<double>, size_t>(const std::vector<size_t>&, const std::vector<size_t>&)> subsetCorrelations;

    explicit TraitPlotDialog(QWidget* parent, SettingsAction& settings) : QDialog(parent)
    {
        setWindowTitle("Trait exploration");
        auto* shell = new QHBoxLayout(this);
        _plots = new ChartStatusSidebar(this);
        _plots->stateForRow = [this, &settings](int row) {
            const auto mode = static_cast<Plot>(_plots->item(row)->data(Qt::UserRole).toInt());
            const auto base = settings.traitChartState();
            if (mode == FocusScatter) {
                if (_focus.isEmpty()) return ChartUpdateState::Stale;
                const auto& focus = settings.getTraitFocus();
                if (focus.gene == _focus && focus.traitName == settings.getReverseSearchTraitSelectionString().getString()
                    && !focus.points.empty())
                    return ChartUpdateState::Ready;
                return base == ChartUpdateState::Updating ? base : ChartUpdateState::Stale;
            }
            if (base != ChartUpdateState::Ready) return base;
            if (!_geneOptions.hasSelectedOptions()) return ChartUpdateState::Stale;
            if (mode >= ScoreHeatmap && mode <= Variability && _method.contains("phylogenetic", Qt::CaseInsensitive))
                return ChartUpdateState::Stale;
            if (mode == Scatter || mode == Profile || mode == Ranking || mode == Sign) {
                const auto count = _speciesOptions.getSelectedOptions().size();
                const int minimum = mode != Ranking ? 1 : _method.contains("phylogenetic", Qt::CaseInsensitive) ? 3 : 2;
                if (count < minimum) return ChartUpdateState::Stale;
            }
            return ChartUpdateState::Ready;
        };
        _plots->setAccessibleName("Trait plots");
        _plots->setTextElideMode(Qt::ElideNone);
        _plots->setWordWrap(true);
        _plots->setMinimumWidth(215);
        _plots->setMaximumWidth(285);
        shell->addWidget(_plots);
        auto* content = new QWidget(this);
        auto* layout = new QVBoxLayout(content);
        shell->addWidget(content, 1);
        auto* controls = new QHBoxLayout();
        const QStringList names { "Expression vs trait", "Trait-ordered expression heatmap", "Correlation distribution",
            "Ranked correlations", "PCA colored by trait", "Gene co-expression heatmap", "Expression profiles",
            "Gene × species contribution heatmap", "Species contribution distributions", "Gene contribution distributions",
            "Contribution sign consistency", "Species clustering by contributions", "Contribution variability",
            "Selected gene vs trait" };
        const QStringList icons { "braille", "th", "chart-bar", "sort-amount-down", "project-diagram", "th-large", "chart-line",
            "table", "sliders-h", "sliders-h", "adjust", "sitemap", "wave-square", "crosshairs" };
        for (int i = 0; i < names.size(); ++i) {
            auto* item = new QListWidgetItem(mv::util::StyledIcon(icons[i]), names[i]);
            item->setData(Qt::UserRole, i);
            item->setToolTip(names[i] + "\n" + relevance(i));
            if (i == FocusScatter) _plots->insertItem(0, item);
            else _plots->addItem(item);
        }
        _plots->setCurrentRow(0);
        const auto addOptions = [&](OptionsAction& options, const QString& text, const QString& icon, const QString& help) {
            auto* button = new QToolButton(this);
            button->setIcon(mv::util::StyledIcon(icon));
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            button->setText(text);
            button->setToolTip(help);
            button->setPopupMode(QToolButton::InstantPopup);
            auto* menu = new QMenu(button);
            auto* action = new QWidgetAction(menu);
            auto* widget = options.createWidget(menu, OptionsAction::ListView | OptionsAction::Selection | OptionsAction::Filter);
            widget->setMinimumSize(330, 350);
            action->setDefaultWidget(widget);
            menu->addAction(action);
            button->setMenu(menu);
            controls->addWidget(button);
            return button;
        };
        _geneButton = addOptions(_geneOptions, "Genes", "list-check", "Choose genes for the multi-gene plots. "
            "Initially includes genes with positive species appearance in the gene table. "
            "The Selected gene vs trait plot follows the current gene selection.");
        _speciesButton = addOptions(_speciesOptions, "Species", "filter", "Choose which species appear in this plot. "
            "Follows Species Explorer until you customize the filter. Shows all species when the explorer selection is empty. "
            "Click a plotted species to update Species Explorer.");
        controls->addStretch();
        auto* reset = new QPushButton("Reset view", this);
        auto* exportData = new QPushButton("Export data", this);
        exportData->setToolTip("Save the chart explanation, analysis settings and underlying values.");
        auto* save = new QPushButton("Save image…", this);
        auto* guideToggle = new QToolButton(this);
        guideToggle->setText("Reading guide");
        guideToggle->setCheckable(true);
        guideToggle->setChecked(true);
        guideToggle->setToolTip("Show a starting point and mark examples in amber.");
        controls->addWidget(guideToggle);
        controls->addWidget(reset);
        controls->addWidget(exportData);
        controls->addWidget(save);
        layout->addLayout(controls);
        _description = new ChartTextLabel(ChartTextLabel::Section::Overview, this);
        layout->addWidget(_description);
        _relevance = new ChartTextLabel(ChartTextLabel::Section::Relevance, this);
        layout->addWidget(_relevance);
        _guide = new ChartTextLabel(ChartTextLabel::Section::Guide, this);
        layout->addWidget(_guide);
        auto* selectionHelp = new QLabel("Click to select. Ctrl-click to add or remove. Right-click to pin details.", this);
        selectionHelp->setWordWrap(true);
        layout->addWidget(selectionHelp);
        _plot = new ZoomablePlotWidget(this);
        _plot->setWidePlot(true);
        _plot->setLabelMode(ZoomablePlotWidget::LabelMode::Highlighted);
        _heatmap = new HeatmapWidget(this);
        controls->insertWidget(3, _heatmap->colorMap().createWidget(this, 0));
        _plot->setColorMapImage(_heatmap->colorMap().getColorMapImage());
        connect(&_heatmap->colorMap(), &ColorMapAction::imageChanged, _plot, &ZoomablePlotWidget::setColorMapImage);
        layout->addWidget(_plot, 1);
        layout->addWidget(_heatmap, 1);
        _plot->setDetailProvider([this](const Point& point) {
            if (point.series < 0) return tooltip(_details.value(point.id).toHtmlEscaped().replace('\n', "<br>"));
            const auto& gene = _genes[point.series];
            return tooltip(QString("<b>%1</b><br>%2 · %3: %4<br>Mean expression: %5<br>Trait correlation: %6")
                .arg(gene.name.toHtmlEscaped(), _matrix->species[point.species].toHtmlEscaped(),
                    _matrix->traitName.toHtmlEscaped(), number(_matrix->trait[point.species]), number(point.y), number(gene.correlation)));
        });
        _plot->setOnClick([this](int index) {
            const auto point = _plot->pointData(index);
            const auto gene = point.series < 0 ? _pointGenes.value(point.id) : _genes[point.series].name;
            const bool additive = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
            if (point.species >= 0 && onSpeciesClicked && !_plot->clickedOnLine()) {
                const auto name = currentMode() == FocusScatter ? _focusPlot->points[point.species].species : _matrix->species[point.species];
                onSpeciesClicked(name, additive);
            }
            if (point.species < 0 && gene.isEmpty() && currentMode() == Distribution)
                pinHoverDetails(_plot, tooltip(_details.value(point.id).toHtmlEscaped().replace('\n', "<br>")));
            selectGene(gene, additive);
        });
        _heatmap->setDetailProvider([this](int row, int column) {
            return tooltip(QString("<b>%1 · %2</b><br>Value: %3").arg(_rowLabels.value(row).toHtmlEscaped(),
                _heatmapGenes.value(column).toHtmlEscaped(), number(_heatmap->valueAt(row, column))));
        });
        _heatmap->setOnCellClicked([this](int row, int column) {
            const auto gene = _heatmapGenes.value(column);
            const bool additive = QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
            selectSpecies(row, additive);
            selectGene(gene, additive);
        });
        _heatmap->setOnColumnLabelClicked([this](int column) {
            selectGene(_heatmapGenes.value(column), QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier));
        });
        _heatmap->setOnRowLabelClicked([this](int row, bool additive) {
            if (currentMode() == Coexpression) selectGene(_heatmapGenes.value(row), additive);
            else selectSpecies(row, additive);
        });
        connect(_plots, &QListWidget::currentRowChanged, this, [this] { render(); });
        connect(&_geneOptions, &OptionsAction::selectedOptionsChanged, this, [this] {
            if (_updatingGeneOptions) return;
            _geneOptionsEdited = true;
            _checkedGenes = _geneOptions.getSelectedOptions();
            render();
        });
        connect(&_speciesOptions, &OptionsAction::selectedOptionsChanged, this, [this] {
            if (_updatingSpeciesOptions) return;
            _speciesFilterEdited = true;
            _checkedSpecies = _speciesOptions.getSelectedOptions();
            render();
        });
        connect(reset, &QPushButton::clicked, this, [this] {
            if (_plot->isVisible()) _plot->resetView();
            else _heatmap->resetView();
        });
        connect(exportData, &QPushButton::clicked, this, [this] { exportCurrentChart(); });
        connect(guideToggle, &QToolButton::toggled, this, [this](bool checked) {
            _showGuide = checked;
            updateReadingGuide();
        });
        connect(save, &QPushButton::clicked, this, [this] {
            const auto path = QFileDialog::getSaveFileName(this, "Save trait plot", "trait_plot.png", "PNG image (*.png)");
            if (!path.isEmpty())
                (_plot->isVisible() ? static_cast<QWidget*>(_plot) : static_cast<QWidget*>(_heatmap))->grab().save(path);
        });
        resize(fittingWindowSize(1100, 720));
    }

    void setResults(const TraitMatrix& matrix, const TraitMatrix& allSpecies, const TraitFocus& focusPlot,
        const std::unordered_map<QString, float>& scores,
        const std::unordered_map<QString, GeneTraitExtras>& extras, bool contribution, const QString& method,
        const QStringList& selectedGenes, const QString& focus, const QStringList& selectedSpecies,
        const QStringList& tableGenes)
    {
        _comparisonMatrix = &matrix;
        _allSpeciesMatrix = &allSpecies;
        _focusPlot = &focusPlot;
        _method = matrix.methodName.isEmpty() ? method : matrix.methodName;
        if (_focus != focus) _unlistedGene.clear();
        _focus = focus;
        const QSet<QString> nextGenes(selectedGenes.begin(), selectedGenes.end());
        const QSet<QString> nextSpecies(selectedSpecies.begin(), selectedSpecies.end());
        const bool speciesSelectionChanged = _selectedSpecies != nextSpecies;
        const bool geneSelectionChanged = _selectedGenes != nextGenes;
        if (_geneOptionsEdited && geneSelectionChanged)
            for (const auto& name : nextGenes - _selectedGenes)
                if (!_checkedGenes.contains(name)) _checkedGenes << name;
        if (_speciesFilterEdited && speciesSelectionChanged)
            for (const auto& name : nextSpecies - _selectedSpecies)
                if (!_checkedSpecies.contains(name)) _checkedSpecies << name;
        _selectedGenes = nextGenes;
        _selectedSpecies = nextSpecies;
        QStringList species;
        for (const auto& name : matrix.species) species << name;
        const bool speciesPoolChanged = _speciesOptions.getOptions() != species;
        if (speciesPoolChanged || speciesSelectionChanged) {
            _updatingSpeciesOptions = true;
            if (speciesPoolChanged) _speciesOptions.setOptions(species);
            QStringList defaults;
            for (const auto& name : species)
                if (_selectedSpecies.contains(name)) defaults << name;
            _speciesOptions.setSelectedOptions(_speciesFilterEdited ? _checkedSpecies : (defaults.isEmpty() ? species : defaults));
            _updatingSpeciesOptions = false;
        }
        _genes.clear();
        for (const auto& [name, column] : matrix.geneIndex) {
            const auto score = scores.find(name);
            const auto extra = extras.find(name);
            const double correlation = contribution && extra != extras.end() ? extra->second.overall
                : (!contribution && score != scores.end() ? score->second : std::numeric_limits<double>::quiet_NaN());
            if (std::all_of(matrix.expression.begin(), matrix.expression.end(),
                    [column](const auto& row) { return column < row.size() && std::isfinite(row[column]); }))
                _genes.push_back({ name, column, correlation });
        }
        std::sort(_genes.begin(), _genes.end(), [](const Gene& left, const Gene& right) {
            const bool finiteLeft = std::isfinite(left.correlation), finiteRight = std::isfinite(right.correlation);
            if (finiteLeft != finiteRight)
                return finiteLeft;
            if (finiteLeft && std::abs(left.correlation) != std::abs(right.correlation))
                return std::abs(left.correlation) > std::abs(right.correlation);
            return left.name < right.name;
        });
        QStringList available;
        for (const auto& gene : _genes) available << gene.name;
        QStringList defaultGenes = tableGenes;
        for (const auto& name : selectedGenes)
            if (!defaultGenes.contains(name)) defaultGenes << name;
        const bool genePoolChanged = _geneOptions.getOptions() != available;
        _updatingGeneOptions = true;
        if (genePoolChanged) _geneOptions.setOptions(available);
        if (genePoolChanged || (_geneOptionsEdited ? geneSelectionChanged : _defaultTableGenes != defaultGenes))
            _geneOptions.setSelectedOptions(_geneOptionsEdited ? _checkedGenes : defaultGenes);
        _defaultTableGenes = defaultGenes;
        _updatingGeneOptions = false;
        render();
    }

private:
    Plot currentMode() const
    {
        const auto* item = _plots->currentItem();
        return item ? static_cast<Plot>(item->data(Qt::UserRole).toInt()) : FocusScatter;
    }
    static QString relevance(int plot);
    QString tooltip(const QString& details) const
    {
        return details + "<br><br>" + _description->text().toHtmlEscaped()
            + "<br><br>" + _relevance->text().toHtmlEscaped();
    }

    static QString number(double value) { return std::isfinite(value) ? QString::number(value, 'g', 5) : QStringLiteral("n/a"); }

    bool geneSelected(const QString& gene) const
    {
        return !gene.isEmpty() && (_selectedGenes.contains(gene) || _unlistedGene == gene);
    }

    void selectGene(const QString& name, bool additive)
    {
        if (name.isEmpty()) return;
        if (onGeneClicked && onGeneClicked(name, additive)) {
            if (_unlistedGene.isEmpty()) return;
            _unlistedGene.clear();
        } else {
            _unlistedGene = additive && _unlistedGene == name ? QString() : name;
        }
        render();
    }

    void selectSpecies(int row, bool additive)
    {
        const auto species = _heatmapSpecies.value(row);
        if (!species.isEmpty() && onSpeciesClicked) onSpeciesClicked(species, additive);
    }
    static Values standardized(Values data, bool rank = false)
    {
        if (rank) {
            std::vector<size_t> order(data.size());
            std::iota(order.begin(), order.end(), 0);
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return data[a] < data[b]; });
            for (size_t first = 0; first < order.size();) {
                size_t end = first + 1;
                while (end < order.size() && data[order[first]] == data[order[end]]) ++end;
                for (size_t i = first; i < end; ++i) data[order[i]] = (first + end + 1.0) / 2;
                first = end;
            }
        }
        const double mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
        for (auto& value : data) value -= mean;
        const double sd = std::sqrt(std::inner_product(data.begin(), data.end(), data.begin(), 0.0) / data.size());
        if (sd > 0.0)
            for (auto& value : data) value /= sd;
        return data;
    }

    Values expression(size_t gene) const
    {
        Values values;
        values.reserve(_matrix->species.size());
        for (const auto& row : _matrix->expression) values.push_back(row[_genes[gene].column]);
        return values;
    }

    Matrix contributions(const std::vector<size_t>& genes) const
    {
        const bool rank = _method.contains("Spearman", Qt::CaseInsensitive);
        const auto trait = standardized(_matrix->trait, rank);
        Matrix result;
        for (auto gene : genes) {
            auto values = standardized(expression(gene), rank);
            for (size_t s = 0; s < trait.size(); ++s) values[s] *= trait[s];
            result.push_back(std::move(values));
        }
        return result;
    }

    int addPoint(QVector<Point>& points, double x, double y, const QString& label, const QString& detail,
        const QString& gene = {}, bool highlight = false, QColor color = {})
    {
        Point point;
        point.x = x; point.y = y; point.label = label; point.highlighted = highlight; point.color = color;
        point.id = _details.size();
        _details << detail;
        _pointGenes << gene;
        points.push_back(point);
        return point.id;
    }

    void box(QVector<Point>& points, Values values, int row, const QString& name, const QString& gene = {})
    {
        std::sort(values.begin(), values.end());
        const double low = ChartNumerics::linearQuantile(values, 0.25), median = ChartNumerics::linearQuantile(values, 0.5), high = ChartNumerics::linearQuantile(values, 0.75);
        addPoint(points, median, row, name, QString("%1\nMinimum %2 · Q1 %3 · Median %4 · Q3 %5 · Maximum %6")
            .arg(name, number(values.front()), number(low), number(median), number(high), number(values.back())), gene, geneSelected(gene));
        auto& point = points.back();
        point.low = values.front(); point.high = values.back(); point.boxLow = low; point.boxHigh = high;
    }

    void heatmap(const QStringList& rows, const QStringList& columns, const Matrix& values,
        const QStringList& geneNames, const QStringList& speciesNames)
    {
        QVector<QVector<float>> data;
        for (const auto& row : values)
            data.push_back(QVector<float>(row.begin(), row.end()));
        _rowLabels = rows;
        _heatmapGenes = geneNames;
        _heatmapSpecies = speciesNames;
        _heatmap->setData(rows, columns, data, QVector<int>(columns.size(), -1));
        QSet<int> rowsSelected, columnsSelected;
        for (int row = 0; row < speciesNames.size(); ++row)
            if (_selectedSpecies.contains(speciesNames[row])) rowsSelected.insert(row);
        for (int column = 0; column < geneNames.size(); ++column)
            if (geneSelected(geneNames[column])) columnsSelected.insert(column);
        _heatmap->setHighlight(rowsSelected, columnsSelected);
    }

    static std::vector<size_t> clusterOrder(const Matrix& scores, size_t count)
    {
        Matrix distances(count, Values(count));
        for (const auto& gene : scores)
            for (size_t i = 0; i < count; ++i)
                for (size_t j = i + 1; j < count; ++j) {
                    const double delta = gene[i] - gene[j];
                    distances[i][j] += delta * delta;
                }
        std::vector<std::vector<size_t>> groups;
        for (size_t i = 0; i < count; ++i) {
            groups.push_back({ i });
            for (size_t j = i + 1; j < count; ++j) distances[j][i] = distances[i][j] = std::sqrt(distances[i][j]);
        }
        while (groups.size() > 1) {
            size_t first = 0, second = 1;
            double closest = std::numeric_limits<double>::infinity();
            for (size_t i = 0; i < groups.size(); ++i)
                for (size_t j = i + 1; j < groups.size(); ++j) {
                    double sum = 0;
                    for (auto a : groups[i])
                        for (auto b : groups[j]) sum += distances[a][b];
                    const double average = sum / (groups[i].size() * groups[j].size());
                    if (average < closest) { closest = average; first = i; second = j; }
                }
            groups[first].insert(groups[first].end(), groups[second].begin(), groups[second].end());
            groups.erase(groups.begin() + second);
        }
        return groups.front();
    }

    void render();
    void exportCurrentChart();
    void updateReadingGuide();

    const TraitMatrix* _matrix = nullptr;
    const TraitMatrix* _comparisonMatrix = nullptr;
    const TraitMatrix* _allSpeciesMatrix = nullptr;
    const TraitFocus* _focusPlot = nullptr;
    QString _focus, _unlistedGene, _method;
    QSet<QString> _selectedGenes;
    QSet<QString> _selectedSpecies;
    std::vector<Gene> _genes;
    QStringList _details, _pointGenes, _heatmapGenes, _heatmapSpecies;
    QStringList _rowLabels;
    ChartStatusSidebar* _plots = nullptr;
    OptionsAction _geneOptions{ this, "Genes displayed in trait plots" };
    OptionsAction _speciesOptions{ this, "Species displayed in trait plots" };
    QToolButton* _speciesButton = nullptr;
    QToolButton* _geneButton = nullptr;
    QStringList _checkedSpecies;
    bool _updatingSpeciesOptions = false;
    bool _speciesFilterEdited = false;
    QStringList _defaultTableGenes;
    QStringList _checkedGenes;
    bool _updatingGeneOptions = false;
    bool _geneOptionsEdited = false;
    ChartTextLabel* _description = nullptr;
    ChartTextLabel* _relevance = nullptr;
    ChartTextLabel* _guide = nullptr;
    bool _showGuide = true;
    ZoomablePlotWidget* _plot = nullptr;
    HeatmapWidget* _heatmap = nullptr;
};
QString TraitPlotDialog::relevance(int plot)
{
    static const QStringList relevance {
        "Shows whether individual genes increase or decrease in expression with the trait.",
        "Reveals expression patterns across low-trait and high-trait species.",
        "Shows the overall balance of positive, negative, and weak associations.",
        "Identifies the strongest positively and negatively associated genes.",
        "Reveals whether global expression variation follows the trait gradient.",
        "Identifies groups of genes displaying coordinated expression patterns.",
        "Shows whether candidate gene expression changes consistently along the trait gradient.",
        "Shows whether associations are consistent, opposite, or variable across species.",
        "Compares the distributions of gene-level scores across species.",
        "Identifies genes with consistently high or low scores and genes whose scores vary across species.",
        "Shows the proportion of species with positive versus negative scores for each gene.",
        "Identifies species with similar gene-level correlation patterns.",
        "Identifies stable versus highly species-dependent gene associations.",
        "Shows whether the selected gene increases or decreases in expression with the trait across species."
    };
    return relevance.value(plot);
}

void TraitPlotDialog::exportCurrentChart()
{
    const auto mode = currentMode();
    const auto title = _plots->currentItem()->text();
    saveChartDataReport(this, title, [this, mode](ChartDataReport& report) {
        report.paragraph(_description->text());
        report.paragraph(_relevance->text());
        report.paragraph(_guide->text());
        const auto badge = _plots->currentItem()->toolTip().section("\nStatus: ", 1);
        if (!badge.isEmpty()) report.paragraph("Status: " + badge);
        if (_matrix && !_matrix->traitName.isEmpty())
            report.paragraph(QString("Trait: %1\nCorrelation method: %2").arg(_matrix->traitName, _method));
        if (mode != FocusScatter)
            report.paragraph("Checked genes: " + _geneOptions.getSelectedOptions().join(", "));
        if (!_speciesButton->isHidden())
            report.paragraph("Checked species: " + _speciesOptions.getSelectedOptions().join(", "));
        if (_heatmap->isVisible()) {
            if (mode == Coexpression && _heatmap->columnLabels().size() > 1000 && _matrix) {
                report.heading("Source expression matrix");
                report.paragraph("For a large co-expression matrix, this section exports its complete input. Each co-expression cell is the Pearson correlation between two gene columns in this matrix.");
                const auto& genes = _heatmap->columnLabels();
                QStringList header{ "Species" };
                header << genes;
                report.beginTable(header);
                std::vector<size_t> indexes;
                for (const auto& name : genes) indexes.push_back(_matrix->geneIndex.at(name));
                for (size_t i = 0; i < _matrix->species.size(); ++i) {
                    QStringList row{ _matrix->species[i] };
                    for (auto g : indexes) row << ChartDataReport::number(_matrix->expression[i][g]);
                    report.row(row);
                }
                report.endTable();
            } else {
                report.matrix(*_heatmap, mode == Coexpression ? "Pearson co-expression coefficients"
                    : mode == ExpressionHeatmap ? "Standardized mean expression" : "Species contribution values");
            }
            return;
        }
        if (mode == Distribution) {
            const auto checked = _geneOptions.getSelectedOptions();
            const QSet<QString> names(checked.begin(), checked.end());
            report.heading("Gene correlations used in the histogram");
            report.beginTable({ "Gene", "Trait correlation" });
            for (const auto& gene : _genes)
                if (names.contains(gene.name) && std::isfinite(gene.correlation))
                    report.row({ gene.name, ChartDataReport::number(gene.correlation) });
            report.endTable();
        }
        report.plot(*_plot, [this, mode](const Point& point) -> QStringList {
            if (mode == FocusScatter && _focusPlot && point.species >= 0)
                return { _focusPlot->gene, _focusPlot->points[point.species].species,
                    ChartDataReport::number(_focusPlot->points[point.species].trait), "Gene expression" };
            if (point.series >= 0 && _matrix && point.species >= 0) {
                const auto& gene = _genes[point.series];
                return { gene.name, _matrix->species[point.species], ChartDataReport::number(_matrix->trait[point.species]),
                    "Across-species trait correlation: " + ChartDataReport::number(gene.correlation) };
            }
            return { _pointGenes.value(point.id), _matrix && point.species >= 0 ? _matrix->species[point.species] : QString(),
                _matrix && point.species >= 0 ? ChartDataReport::number(_matrix->trait[point.species]) : QString(), _details.value(point.id) };
        }, { "Gene", "Species", "Raw trait value", "Details" });
    });
}

void TraitPlotDialog::updateReadingGuide()
{
    using Point = ZoomablePlotWidget::Point;
    static const QStringList tips{
        "Compare the highlighted median and middle half at opposite ends of the trait gradient. Inspect individual genes to see what drives the spread.",
        "Scan for a gene column that changes color from top to bottom. The outlined cell is an example to inspect.",
        "Start with the tallest bin, then compare how much of the distribution falls on either side of zero.",
        "Start at both ends of the ranking. The marked genes have the most positive and most negative correlations.",
        "Compare the species at the two ends of PC1. Check whether the trait colors change across the plot.",
        "Inspect an outlined off-diagonal pair of genes. Similar color blocks away from the diagonal suggest coordinated expression.",
        "Start with the highlighted high-expression endpoint. Follow its gene line across species and compare it with nearby profiles.",
        "Compare the outlined contribution with nearby species and genes. Opposite colors indicate opposite contributions.",
        "Start with the species that has the widest middle half of gene contributions. Compare the boxes and their centers.",
        "Inspect the gene with the widest middle half, then compare its median and whiskers with other genes.",
        "Start with the longest signed arm. Compare the two directions and check the zero-count share in the tooltip.",
        "Read adjacent species rows together. Compare their contribution profiles around the outlined cell.",
        "Inspect the gene with the greatest variation. Check its mean contribution to see whether variability accompanies a consistent direction.",
        "Compare expression at the marked low and high trait values. Check how the remaining species follow the fitted line."
    };
    const auto mode = currentMode();
    _guide->setText("Start here: " + tips.value(mode));
    _guide->setVisible(_showGuide);
    _plot->setReadingGuidePoints({});
    _heatmap->setReadingGuideCell({ -1, -1 });
    if (!_showGuide) return;
    if (!_heatmap->isHidden()) {
        const auto cell = strongestGuideCell(*_heatmap, mode == Coexpression);
        _heatmap->setReadingGuideCell(cell);
        _guide->setText(_guide->text() + guideCellNote(*_heatmap, cell));
        return;
    }
    QSet<int> marks;
    const auto mark = [&](const std::function<double(const Point&)>& score) {
        const int point = strongestGuidePoint(*_plot, score);
        if (point >= 0) marks.insert(point);
    };
    if (mode == Scatter || mode == FocusScatter || mode == Pca || mode == Ranking) {
        const auto eligible = [mode](const Point& p) { return mode != Scatter || std::isfinite(p.verticalLow); };
        mark([&](const Point& p) { return eligible(p) ? p.x : std::numeric_limits<double>::quiet_NaN(); });
        mark([&](const Point& p) { return eligible(p) ? -p.x : std::numeric_limits<double>::quiet_NaN(); });
    } else if (mode == SpeciesBoxes || mode == GeneBoxes) {
        mark([](const Point& p) { return p.boxHigh - p.boxLow; });
    } else if (mode == Sign) {
        mark([](const Point& p) { return std::abs(p.x); });
    } else {
        mark([](const Point& p) { return p.y; });
    }
    _guide->setText(_guide->text() + guidePointNote(*_plot, marks));
    _plot->setReadingGuidePoints(std::move(marks));
}

void TraitPlotDialog::render()
{
    const auto mode = currentMode();
    _matrix = mode == ExpressionHeatmap || mode == Pca || mode == ScoreHeatmap || mode == SpeciesBoxes || mode == Clustering
        ? _allSpeciesMatrix : _comparisonMatrix;
    const bool speciesAxis = mode == Scatter || mode == Profile || mode == Ranking || mode == Sign;
    _speciesButton->setVisible(speciesAxis);
    _geneButton->setVisible(mode != FocusScatter);
    _relevance->setText("Biological relevance: " + relevance(mode));
    const bool tiled = mode == ExpressionHeatmap || mode == Coexpression || mode == ScoreHeatmap || mode == Clustering;
    _plot->setVisible(!tiled);
    _heatmap->setVisible(tiled);
    _details.clear();
    _pointGenes.clear();
    _plot->setGuideLines({});
    _plot->setColumns({}, {}, {});
    _plot->setHistogramWidth(0.0);
    _plot->setConnectedSeries(mode == Profile);
    _plot->setShowTrendLine(mode == FocusScatter);
    const bool categories = mode == Ranking || mode == GeneBoxes || mode == SpeciesBoxes || mode == Sign;
    _plot->setGeneLabels(categories);
    _plot->setSpaceForCategories(categories);
    _plot->setPointSize(mode == Scatter || mode == Profile || categories ? 4 : 7);
    _plot->setShowGrid(!categories && mode != Profile);
    _plot->setLabelMode(mode == FocusScatter || mode == Pca
        ? ZoomablePlotWidget::LabelMode::Highlighted : ZoomablePlotWidget::LabelMode::None);
    _heatmap->setTraitLegend(mode == Coexpression ? "Pearson correlation" : mode == ExpressionHeatmap
        ? "Standardized expression" : "Species contribution");
    if (mode == FocusScatter) {
        QVector<Point> points;
        if (_focus.isEmpty()) {
            _description->setText("Select a gene in the gene table or another plot.");
        } else if (!_focusPlot || _focusPlot->gene != _focus) {
            _description->setText(QString("Preparing expression for %1 across all eligible species.").arg(_focus));
        } else {
            for (size_t i = 0; i < _focusPlot->points.size(); ++i) {
                const auto& row = _focusPlot->points[i];
                addPoint(points, row.trait, row.expression, row.species,
                    QString("%1\n%2\n%3: %4\nMean expression: %5")
                        .arg(_focus, row.species, _focusPlot->traitName, number(row.trait), number(row.expression)),
                    {}, _selectedSpecies.contains(row.species));
                points.back().species = static_cast<int>(i);
            }
            _description->setText(points.isEmpty() ? "No species have an eligible trait value and expression for this gene."
                : QString("%1 across all %2 eligible species. The dashed line is a descriptive linear fit.")
                    .arg(_focus).arg(points.size()));
        }
        _plot->setData(points, _focusPlot ? _focusPlot->traitName : QString(), "Mean expression");
        updateReadingGuide();
        return;
    }
    if (!_matrix || _matrix->empty() || _genes.empty()) {
        _description->setText("Choose a trait and cell types and finish the trait analysis to populate this plot.");
        if (tiled) heatmap({}, {}, {}, {}, {});
        else _plot->setData({}, {}, {});
        updateReadingGuide();
        return;
    }
    const bool contributionPlot = mode >= ScoreHeatmap && mode <= Variability;
    if (contributionPlot && _method.contains("Phylogenetic", Qt::CaseInsensitive)) {
        _description->setText("Species contributions are defined for Pearson and Spearman. Phylogenetic contrasts compare branches, so they do not yield a contribution for each species.");
        if (tiled) heatmap({}, {}, {}, {}, {});
        else _plot->setData({}, {}, {});
        updateReadingGuide();
        return;
    }

    std::vector<size_t> geneSelection;
    const bool needsCorrelation = mode == Distribution || contributionPlot;
    const auto checked = _geneOptions.getSelectedOptions();
    const QSet<QString> visibleGenes(checked.begin(), checked.end());
    for (size_t gene = 0; gene < _genes.size(); ++gene)
        if (visibleGenes.contains(_genes[gene].name) && (!needsCorrelation || std::isfinite(_genes[gene].correlation)))
            geneSelection.push_back(gene);
    if (geneSelection.empty()) {
        _description->setText(checked.isEmpty() ? "Select genes using the Genes options to populate this plot."
            : "No checked genes have valid values for this plot.");
        if (tiled) heatmap({}, {}, {}, {}, {});
        else _plot->setData({}, {}, {});
        updateReadingGuide();
        return;
    }
    const size_t speciesCount = _matrix->species.size();
    std::vector<size_t> speciesOrder(speciesCount);
    std::iota(speciesOrder.begin(), speciesOrder.end(), 0);
    std::stable_sort(speciesOrder.begin(), speciesOrder.end(), [&](size_t a, size_t b) { return _matrix->trait[a] < _matrix->trait[b]; });
    const auto checkedSpecies = _speciesOptions.getSelectedOptions();
    _speciesButton->setText(checkedSpecies.size() == static_cast<qsizetype>(speciesCount)
        ? "Species" : QString("Species (%1)").arg(checkedSpecies.size()));
    const QSet<QString> visibleSpecies(checkedSpecies.begin(), checkedSpecies.end());
    const auto filterSpecies = [&](std::vector<size_t>& order) {
        if (speciesAxis)
            order.erase(std::remove_if(order.begin(), order.end(),
                [&](size_t s) { return !visibleSpecies.contains(_matrix->species[s]); }), order.end());
    };
    const double traitMinimum = _matrix->trait[speciesOrder.front()], traitMaximum = _matrix->trait[speciesOrder.back()];
    filterSpecies(speciesOrder);
    if (speciesOrder.empty()) {
        _description->setText("Choose at least one species to display.");
        if (tiled) heatmap({}, {}, {}, {}, {});
        else _plot->setData({}, {}, {});
        updateReadingGuide();
        return;
    }
    const auto speciesSelected = [&](size_t s) { return _selectedSpecies.contains(_matrix->species[s]); };
    const auto speciesDetail = [&](size_t index) {
        return QString("%1\n%2: %3").arg(_matrix->species[index], _matrix->traitName, number(_matrix->trait[index]));
    };
    const auto geneDetail = [&](size_t index) {
        return QString("%1\nTrait correlation: %2").arg(_genes[index].name, number(_genes[index].correlation));
    };

    QStringList geneNames;
    for (auto gene : geneSelection) geneNames << _genes[gene].name;
    QVector<Point> points;
    QString xTitle, yTitle, description;

    if (mode == Scatter || mode == FocusScatter || mode == Profile) {
        xTitle = _matrix->traitName;
        yTitle = "Mean expression";
        if (mode == Scatter) {
            QStringList labels;
            QSet<int> highlighted;
            QVector<ZoomablePlotWidget::ColumnSummary> summaries;
            for (size_t column = 0; column < speciesOrder.size(); ++column) {
                const auto s = speciesOrder[column];
                QString name = _matrix->species[s];
                labels << name.replace('_', ' ');
                if (speciesSelected(s)) highlighted.insert(static_cast<int>(column));
                Values sorted;
                sorted.reserve(geneSelection.size());
                for (auto gene : geneSelection) sorted.push_back(_matrix->expression[s][_genes[gene].column]);
                std::sort(sorted.begin(), sorted.end());
                const double lower = ChartNumerics::linearQuantile(sorted, 0.25), median = ChartNumerics::linearQuantile(sorted, 0.5), upper = ChartNumerics::linearQuantile(sorted, 0.75);
                summaries.push_back({ static_cast<double>(column), lower, median, upper });
                addPoint(points, column + 0.34, median, _matrix->species[s],
                    speciesDetail(s) + QString("\nFirst quartile: %1\nMedian: %2\nThird quartile: %3")
                        .arg(number(lower), number(median), number(upper)));
                points.back().verticalLow = lower;
                points.back().verticalHigh = upper;
                points.back().species = static_cast<int>(s);
            }
            _plot->setColumns(std::move(labels), std::move(highlighted), std::move(summaries));
            xTitle = QString("Species ordered by %1").arg(_matrix->traitName);
        }
        for (size_t g = 0; g < geneSelection.size(); ++g) {
            const auto gene = geneSelection[g];
            const auto values = expression(gene);

            for (size_t column = 0; column < speciesOrder.size(); ++column) {
                const auto s = speciesOrder[column];
                const bool highlighted = speciesSelected(s);
                Point point;
                point.x = mode == Scatter ? column + (std::fmod(g * 0.61803398875, 1.0) - 0.5) * 0.5 : _matrix->trait[s];
                point.y = values[s];
                point.label = mode == Profile ? _genes[gene].name : _matrix->species[s];
                const bool selectedGene = geneSelected(_genes[gene].name);
                point.highlighted = (mode != Scatter && highlighted) || (mode != FocusScatter && selectedGene);
                point.muted = mode == Profile && geneSelection.size() > 6 && !geneSelected(_genes[gene].name);
                if (mode == Scatter) point.muted = !selectedGene;
                point.colorFraction = mode == Profile
                    ? std::fmod(g * 0.61803398875, 1.0)
                    : (std::isfinite(_genes[gene].correlation) ? (_genes[gene].correlation + 1.0) / 2.0 : 0.5);
                point.series = static_cast<int>(gene);
                point.species = static_cast<int>(s);
                points.push_back(std::move(point));
            }
        }
        description = mode == FocusScatter
            ? QString("%1 across %2 species. The dashed line is a descriptive linear fit.")
                .arg(_genes[geneSelection.front()].name).arg(speciesOrder.size())
            : mode == Scatter
            ? QString("%1 genes across %2 equally spaced species, ordered by trait. Dots show gene expression and color shows trait correlation. "
                      "Each side bar marks the middle 50% of expression values, with a tick at the median.")
                .arg(geneSelection.size()).arg(speciesOrder.size())
            : QString("Each line shows one of %1 genes across the displayed species. With more than six lines, selected genes stand out and other profiles are muted.")
                .arg(geneSelection.size());
    }
    else if (mode == Distribution) {
        constexpr int binCount = 20;
        int counts[binCount] = {};
        for (auto gene : geneSelection)
            ++counts[std::clamp(static_cast<int>((_genes[gene].correlation + 1) * 10), 0, binCount - 1)];
        for (int bin = 0; bin < binCount; ++bin) {
            const double left = -1.0 + bin / 10.0;
            addPoint(points, left + 0.05, counts[bin], {}, QString("%1 to %2\n%3 genes")
                .arg(number(left), number(left + 0.1)).arg(counts[bin]));
            points.back().colorFraction = (bin + 0.5) / binCount;
        }
        _plot->setHistogramWidth(0.1);
        _plot->setGuideLines({ { true, 0.0, {} }, { false, -1.0, {} }, { false, 1.0, {} } });
        xTitle = "Trait correlation";
        yTitle = "Gene count";
        description = QString("Distribution of %1 gene–trait correlations across species.").arg(geneSelection.size());
    }
    else if (mode == Ranking) {
        std::vector<size_t> columns;
        for (auto gene : geneSelection) columns.push_back(_genes[gene].column);
        const auto [correlations, used] = subsetCorrelations(speciesOrder, columns);
        std::vector<std::pair<size_t, double>> ranked;
        for (size_t i = 0; i < correlations.size(); ++i)
            if (std::isfinite(correlations[i])) ranked.emplace_back(geneSelection[i], correlations[i]);
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        for (size_t row = 0; row < ranked.size(); ++row) {
            const auto [gene, score] = ranked[row];
            const auto detail = QString("%1\n%2 correlation: %3\nSpecies used: %4")
                .arg(_genes[gene].name, _method, number(score)).arg(used);
            addPoint(points, score, row + 1, _genes[gene].name, detail, _genes[gene].name, geneSelected(_genes[gene].name),
                score >= 0 ? QColor("#237d9c") : QColor("#b95343"));
            points.back().low = std::min(0.0, score);
            points.back().high = std::max(0.0, score);
            points.back().colorFraction = (score + 1.0) / 2.0;
        }
        _plot->setGuideLines({ { false, -1, {} }, { false, 0, {} }, { false, 1, {} } });
        xTitle = "Trait correlation";
        description = ranked.empty() ? QString("No defined %1 correlations for these species. Check species count and variation in the trait and expression.").arg(_method)
            : QString("Genes ranked by signed %1 correlation calculated using %2 checked species.").arg(_method).arg(used);
    }
    else if (mode == Pca) {
        Matrix columns;
        for (auto gene : geneSelection) columns.push_back(standardized(expression(gene)));
        const auto components = ChartNumerics::principalComponents(columns, speciesCount);
        if (!components.valid) {
            _description->setText("PCA requires at least two eligible species and variation in at least one checked gene. No usable components were available.");
            _plot->setData({}, "PC1", "PC2");
            updateReadingGuide();
            return;
        }
        const auto& [pc1, pc2] = components.scores;
        for (auto s : speciesOrder) {
            const double fraction = traitMaximum > traitMinimum ? (_matrix->trait[s] - traitMinimum) / (traitMaximum - traitMinimum) : 0.5;

            addPoint(points, pc1[s], pc2[s], _matrix->species[s], speciesDetail(s) + QString("\nPC1: %1 · PC2: %2").arg(number(pc1[s]), number(pc2[s])),
                {}, speciesSelected(s));
            points.back().colorFraction = fraction;
            points.back().species = static_cast<int>(s);
        }
        xTitle = QString("PC1 (%1% of variance)").arg(100 * components.varianceFraction[0], 0, 'f', 1);
        yTitle = QString("PC2 (%1% of variance)").arg(100 * components.varianceFraction[1], 0, 'f', 1);
        description = QString("PCA of standardized expression for %1 genes. Each point is a species. Color encodes trait value from %2 to %3.")
            .arg(geneSelection.size()).arg(number(traitMinimum), number(traitMaximum));
    }
    else if (mode == Coexpression) {
        Matrix standardizedGenes;
        for (auto gene : geneSelection) standardizedGenes.push_back(standardized(expression(gene)));
        std::vector<bool> variable;
        for (auto& values : standardizedGenes) {
            const double norm = std::sqrt(std::inner_product(values.begin(), values.end(), values.begin(), 0.0));
            variable.push_back(norm > 0.0);
            if (norm > 0.0)
                for (auto& value : values) value /= norm;
        }
        _rowLabels = _heatmapGenes = geneNames;
        _heatmapSpecies.clear();
        _heatmap->setValueProvider(geneNames, [profiles = std::move(standardizedGenes), variable = std::move(variable)](int row, int column) {
            if (!variable[row] || !variable[column]) return std::numeric_limits<float>::quiet_NaN();
            const auto& a = profiles[row];
            const auto& b = profiles[column];
            return static_cast<float>(std::clamp(std::inner_product(a.begin(), a.end(), b.begin(), 0.0), -1.0, 1.0));
        });
        QSet<int> highlights;
        for (int gene = 0; gene < geneNames.size(); ++gene)
            if (geneSelected(geneNames[gene])) highlights.insert(gene);
        _heatmap->setHighlight(highlights, highlights);
        description = "Pearson correlation between gene expression profiles across species. Zoom in to inspect individual gene pairs. Grey cells indicate constant expression.";
    }
    else if (mode == ExpressionHeatmap || mode == ScoreHeatmap || mode == Clustering) {
        Matrix columns;
        if (mode == ExpressionHeatmap) {
            for (auto gene : geneSelection) columns.push_back(standardized(expression(gene)));
            description = "All eligible species are ordered from low to high trait value. Cell color shows mean expression standardized within each gene.";
        } else {
            columns = contributions(geneSelection);
            if (mode == Clustering) {
                speciesOrder = clusterOrder(columns, speciesCount);
                filterSpecies(speciesOrder);
            }
            description = mode == Clustering
                ? "Species ordered by average-linkage clustering using Euclidean distances between the gene contribution profiles."
                : "Species ordered by trait. Each cell shows that species’ signed contribution to the gene–trait association.";
        }
        Matrix cells;
        QStringList rowLabels, speciesNames;
        for (auto s : speciesOrder) {
            rowLabels << QString("%1 (%2)").arg(_matrix->species[s], number(_matrix->trait[s]));
            speciesNames << _matrix->species[s];
            auto& row = cells.emplace_back();
            for (const auto& column : columns) row.push_back(column[s]);
        }
        heatmap(rowLabels, geneNames, cells, geneNames, speciesNames);
    }
    else {
        const auto& genes = geneSelection;
        const auto scores = contributions(genes);
        if (mode == SpeciesBoxes) {
            for (size_t row = 0; row < speciesOrder.size(); ++row) {
                const auto s = speciesOrder[row];
                Values values;
                for (const auto& gene : scores) values.push_back(gene[s]);
                box(points, std::move(values), static_cast<int>(row + 1), _matrix->species[s]);
                points.back().highlighted = speciesSelected(s);
                points.back().species = static_cast<int>(s);
            }
            description = "Distribution across the checked genes for each species. Boxes show the interquartile range. Whiskers show the minimum and maximum.";
            xTitle = "Species contribution";
        }
        else {
            for (size_t row = 0; row < scores.size(); ++row) {
                const auto gene = genes[row];
                const auto& name = _genes[gene].name;
                const auto& values = scores[row];
                const bool focused = geneSelected(name);
                if (mode == GeneBoxes) {
                    box(points, values, static_cast<int>(row + 1), name, name);
                    for (size_t s = 0; s < speciesCount; ++s) {
                        if (!speciesSelected(s)) continue;
                        addPoint(points, values[s], row + 1.23, {}, name + "\n" + speciesDetail(s)
                            + "\nContribution: " + number(values[s]), name, true);
                        points.back().species = static_cast<int>(s);
                    }
                } else if (mode == Sign) {
                    const double positive = 100.0 * std::count_if(speciesOrder.begin(), speciesOrder.end(), [&](size_t s) { return values[s] > 0; }) / speciesOrder.size();
                    const double negative = 100.0 * std::count_if(speciesOrder.begin(), speciesOrder.end(), [&](size_t s) { return values[s] < 0; }) / speciesOrder.size();
                    const auto detail = QString("%1\nPositive: %2%\nNegative: %3%\nZero: %4%").arg(name, number(positive), number(negative), number(100 - positive - negative));
                    addPoint(points, positive, row + 1, name, detail, name, focused, QColor("#237d9c"));
                    points.back().colorFraction = 1.0;
                    points.back().low = 0; points.back().high = positive;
                    addPoint(points, -negative, row + 1, {}, detail, name, focused, QColor("#b95343"));
                    points.back().colorFraction = 0.0;
                    points.back().low = -negative; points.back().high = 0;
                } else {
                    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / speciesCount;
                    double squares = 0.0;
                    for (auto value : values) squares += (value - mean) * (value - mean);
                    const double sd = std::sqrt(squares / std::max<size_t>(1, speciesCount - 1));
                    addPoint(points, mean, sd, name, QString("%1\nMean contribution: %2\nSample SD: %3").arg(name, number(mean), number(sd)), name, focused);
                    points.back().colorFraction = (mean + 1.0) / 2.0;
                }
            }
            if (mode == GeneBoxes) {
                xTitle = "Species contribution";
                description = "Contribution distribution across species for each checked gene. Blue markers show the species selected in Species Explorer.";
            } else if (mode == Sign) {
                xTitle = "Species (%) · negative to positive";
                description = QString("Percentages use the %1 checked species. Contribution signs use the full analysis as the standardization reference. "
                    "Zero contributions are listed in the tooltip.").arg(speciesOrder.size());
                _plot->setGuideLines({ { false, 0, {} } });
            } else {
                xTitle = "Mean contribution";
                yTitle = "Contribution standard deviation";
                description = "Each point is a gene. Higher values indicate more variable contributions across species.";
            }
        }
    }

    if (contributionPlot)
        description += " Contributions are products of standardized expression and trait values (ranks for Spearman). They are signed scores and may exceed ±1.";
    if (categories)
        description += " Drag vertically or Ctrl-scroll to inspect more rows.";
    if (speciesAxis && speciesOrder.size() < speciesCount)
        description += QString(" Showing %1 of %2 species.").arg(speciesOrder.size()).arg(speciesCount);
    _description->setText(description);
    if (!tiled)
        _plot->setData(points, xTitle, yTitle);
    updateReadingGuide();
}
