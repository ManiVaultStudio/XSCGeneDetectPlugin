const QStringList& SpeciesDeDialog::dePlotNames()
{
    static const QStringList names{ "Species × gene DE heatmap", "Volcano", "Detection vs effect",
        "DE gene counts", "DE intersections (UpSet)", "Ranked DE genes", "Gene expression dot plot",
        "MA plot", "Gene effect across species", "DE effect distributions", "Trait-ordered expression heatmap",
        "DE gene expression vs trait" };
    return names;
}

const QStringList& SpeciesDeDialog::dePlotIcons()
{
    static const QStringList icons{ "th", "braille", "braille", "chart-bar", "project-diagram", "sort-amount-down",
        "circle", "braille", "chart-line", "sliders-h", "th", "crosshairs" };
    return icons;
}

const QStringList& SpeciesDeDialog::dePlotQuestions()
{
    static const QStringList questions{
        "Which genes show relatively higher or lower expression in each species?",
        "Which genes show the largest statistically supported expression differences?",
        "Are expression differences associated with the fraction of expressing cells?",
        "Which species have the most upregulated and downregulated genes?",
        "Which genes are identified as differentially expressed in multiple species comparisons?",
        "Which genes show the strongest expression changes between the selected groups?",
        "How do mean expression and the fraction of expressing cells vary across species?",
        "Are large expression differences concentrated among lowly or highly expressed genes?",
        "How does one gene’s relative expression difference vary across species?",
        "Do species differ in the overall distribution of expression effects?",
        "Do genes show systematic expression patterns along the phenotypic gradient?",
        "Are candidate DE genes also associated with variation in the biological trait?" };
    return questions;
}

QString SpeciesDeDialog::deDataExplanation(int mode)
{
    static const QString groupInput = "Input: Mean expression for each gene and species in Groups A and B, from the selected cell types.";
    static const QString groupTest = "Each species has equal weight. Where significance can be estimated, a moderated species-level test stabilizes variance estimates and the Benjamini–Hochberg correction adjusts p-values across genes.";
    static const QStringList explanations{
        "Input: Per-gene species-versus-rest expression differences, detection fractions, and test results.\n"
        "Processing: Apply the analysis filters and ranking to choose candidate genes across species.\n"
        "Output: One cell per species and candidate gene, colored by mean expression in that species minus the mean of the other species.",
        groupInput + "\nProcessing: " + groupTest + "\n"
        "Output: Each gene is plotted at its mean difference (A minus B) and negative base-10 logarithm of its adjusted p-value (FDR). Smaller adjusted p-values appear higher.",
        groupInput + " This chart also uses the fraction of cells expressing each gene in each species.\n"
        "Processing: Average expression means and detection fractions separately within each group, then subtract Group B from Group A.\n"
        "Output: Each point shows a gene’s mean expression difference and detection difference in percentage points (pp).",
        "Input: Per-gene expression differences and adjusted p-values (FDR) from each species-versus-rest comparison.\n"
        "Processing: Keep nonzero effects below the FDR threshold and count positive and negative effects separately.\n"
        "Output: Two gene counts per species, shown as opposing bars.",
        "Input: Per-gene expression differences and adjusted p-values (FDR) from each species-versus-rest comparison.\n"
        "Processing: Group genes by the exact set of species in which their nonzero effects pass the FDR threshold.\n"
        "Output: Up to 20 largest intersections, shown as gene counts and a species membership matrix. UpSet is the name of this set-intersection display.",
        groupInput + "\nProcessing: " + groupTest + " Keep genes with a nonzero difference and an adjusted p-value below the FDR threshold, then sort by signed difference.\n"
        "Output: Ranked genes and their Group A minus Group B mean expression differences.",
        "Input: Mean expression and fraction of expressing cells by species and gene, for candidate genes from the differential expression heatmap and the selected gene.\n"
        "Processing: Pair each mean with its detection fraction.\n"
        "Output: One dot per species and gene, colored by mean expression and sized by detection fraction.",
        groupInput + "\nProcessing: " + groupTest + " Calculate M as mean A minus mean B, and A as the average of the two group means.\n"
        "Output: One point per gene, with average expression on the horizontal axis and expression difference on the vertical axis. MA refers to these two quantities. The plotted values use the analysis expression scale.",
        "Input: Species-versus-rest expression differences and adjusted p-values (FDR) for the selected gene.\n"
        "Processing: Arrange species in tree order and flag effects that pass the FDR threshold.\n"
        "Output: One signed expression difference per species.",
        "Input: Species-versus-rest expression differences for all tested genes.\n"
        "Processing: For each species, calculate the median, first and third quartiles, minimum, and maximum of finite differences.\n"
        "Output: One box per species. Its box spans the middle half and its whiskers span the full range.",
        "Input: Species mean expression for candidate genes and the selected gene, plus numeric trait values.\n"
        "Processing: Match species to valid trait values and sort them from low to high.\n"
        "Output: A species-by-gene heatmap colored by mean expression.",
        "Input: Species mean expression and species-versus-rest test results for the selected gene, plus numeric trait values.\n"
        "Processing: Match species to valid trait values and flag nonzero expression differences below the FDR threshold.\n"
        "Output: One point per species, positioned by trait value and mean expression."
    };
    return explanations.value(mode) + "\nTerms: DE means differential expression, or a difference in gene expression between groups. "
        "FDR means false discovery rate, the expected proportion of false positives among results called significant. "
        "The chart labels per-gene adjusted p-values as FDR.";
}

static bool significantDeEffect(float effect, float fdr, double threshold)
{
    return std::isfinite(effect) && effect != 0 && std::isfinite(fdr) && fdr >= 0 && fdr < threshold;
}

void SpeciesDeDialog::updateReadingGuide()
{
    static const QStringList tips{
        "Read down one gene column to see which species have higher or lower expression relative to their own comparison groups. A value near zero means the species mean is close to the mean of the other species. Check the cell tooltip for its adjusted p-value before treating a large difference as statistically supported.",
        "Read horizontally for the size and direction of the expression difference. Points right of zero favor Group A. Read vertically for statistical evidence, with higher points representing smaller adjusted p-values. The horizontal line marks the selected false discovery rate threshold.",
        "Find a point far above or below zero. It marks a gene with a large change in the percentage of cells expressing it. Then check the horizontal axis. A strong detection shift with a small mean-expression difference can occur when a gene is expressed weakly in many cells.",
        "Compare bars on the same side across species. Left counts genes with lower mean expression and right counts genes with higher mean expression. Counts depend on the false discovery rate threshold. Click a bar to see the genes behind it.",
        "Start with the tallest bar and find the column with the same intersection number. A filled cell marks a species included in that exact set. For example, an intersection containing two species excludes genes that also passed the threshold in a third species. Click the bar to see the gene list.",
        "Compare genes at the two ends of the ranking. Positive differences favor Group A and negative differences favor Group B. Ranking is by signed expression difference among genes that pass the adjusted p-value filter. Hover to check the adjusted p-value and group comparison.",
        "Compare dot size and color separately. A large dot means many cells express the gene. Its color shows the mean expression, so a large dot can still have a low mean. Compare the same gene column across species to see which quantity changes.",
        "Compare genes near and far from the horizontal zero line. Distance from zero shows the size of the difference, and its sign shows which group has the higher mean. Scan left to right to see whether the biggest differences occur at low or high average expression. Color marks results that pass the adjusted p-value threshold.",
        "Compare species above and below zero for the same gene. Each point uses the other species as its reference, so every point has a different comparison group. The horizontal order follows the tree. Hover for the adjusted p-value behind each effect.",
        "Compare the centers and widths of the boxes. The center is the median across tested genes, the box contains the middle half, and the whiskers extend to the smallest and largest effects. A wide box means gene effects vary within that species comparison.",
        "Read down one gene column from lower to higher trait values. Look for a gradual color change or a change limited to a few species. Hover to check the original mean expression and trait value. The ordering helps reveal a pattern, while the cells show expression values.",
        "Follow the points from low to high trait values and look for a consistent expression pattern. Check whether a single species drives the pattern. Point color summarizes the species-versus-rest differential expression test for that gene. The horizontal and vertical positions show the trait and mean expression."
    };
    const int mode = _navigation->currentRow();
    if (mode < 0) return;
    auto* label = mode == 0 ? _overviewGuide : mode <= 2 ? _detailGuide : _extraGuide;
    label->setText("Start here: " + tips.value(mode));
    auto* heatmap = mode == 0 ? _heatmap : _extraHeatmap;
    auto* plot = mode == 1 ? _volcano : mode == 2 ? _detectionPlot : _extraPlot;
    heatmap->setReadingGuideCell({ -1, -1 });
    plot->setReadingGuidePoints({});
    if (!_showGuide) return;

    if (mode == 0 || mode == 6 || mode == 10) {
        const auto cell = strongestGuideCell(*heatmap);
        heatmap->setReadingGuideCell(cell);
        label->setText(label->text() + guideCellNote(*heatmap, cell));
        return;
    }
    QSet<int> marks;
    const auto mark = [&](const std::function<double(const Point&)>& score) {
        const int index = strongestGuidePoint(*plot, score);
        if (index >= 0) marks.insert(index);
        return index;
    };
    const auto noScore = std::numeric_limits<double>::quiet_NaN();
    if (mode == 3 || mode == 5 || mode == 11 || mode == 8) {
        if (mode == 8) {
            mark([](const Point& point) { return point.y; });
            mark([](const Point& point) { return -point.y; });
        } else {
            const int positive = mark([&](const Point& point) { return mode == 11 || point.x > 0 ? point.x : noScore; });
            const int negative = mark([&](const Point& point) { return mode == 11 || point.x < 0 ? -point.x : noScore; });
            if (mode == 3) {
                if (positive >= 0) {
                    const auto point = plot->pointData(positive);
                    label->setText(label->text() + QString(" Most upregulated: %1 (%2).").arg(_extraSpecies.value(point.id)).arg(point.x));
                }
                if (negative >= 0) {
                    const auto point = plot->pointData(negative);
                    label->setText(label->text() + QString(" Most downregulated: %1 (%2).").arg(_extraSpecies.value(point.id)).arg(-point.x));
                }
            }
        }
    } else if (mode == 2) {
        mark([](const Point& point) { return std::abs(point.y); });
    } else if (mode == 7) {
        mark([&](const Point& point) { return point.colorFraction > 0.5 ? std::abs(point.y) : noScore; });
    } else if (mode == 9) {
        mark([](const Point& point) { return point.boxHigh - point.boxLow; });
    } else {
        const int top = mark([](const Point& point) { return point.y; });
        if (mode == 4 && top >= 0) {
            const int column = plot->pointData(top).id;
            for (int row = 0; row < heatmap->rowLabels().size(); ++row) {
                if (heatmap->valueAt(row, column) > 0) {
                    heatmap->setReadingGuideCell({ column, row });
                    break;
                }
            }
        }
    }
    if (mode != 3) label->setText(label->text() + guidePointNote(*plot, marks));
    plot->setReadingGuidePoints(std::move(marks));
}

void SpeciesDeDialog::exportCurrentChart()
{
    const int mode = _navigation->currentRow();
    if (mode < 0) return;
    saveChartDataReport(this, dePlotNames().value(mode), [this, mode](ChartDataReport& report) {
        const auto* overview = mode == 0 ? _info : mode <= 2 ? _detailInfo : _extraInfo;
        report.heading("Description");
        report.paragraph(overview->descriptionText());
        report.heading("Data explanation");
        report.paragraph(overview->dataExplanation());
        report.heading("Why it matters");
        report.paragraph(overview->relevanceText());
        report.heading("Reading guide");
        report.paragraph((mode == 0 ? _overviewGuide : mode <= 2 ? _detailGuide : _extraGuide)->text());
        const auto badge = _navigation->item(mode)->toolTip().section("\nStatus: ", 1);
        if (!badge.isEmpty()) report.paragraph("Status: " + badge);
        report.paragraph(QString("Cell types: %1\nFDR threshold: %2")
            .arg(_result.cellTypes.join(", "), ChartDataReport::number(_level)));
        if (mode == 0 || mode == 6 || mode == 10)
            report.paragraph(QString("Candidate direction: %1\nMaximum genes per species: %2\nRank by: %3")
                .arg(_direction->currentText()).arg(_topCount->value()).arg(_rank->currentText()));
        if (mode == 0) {
            report.matrix(*_heatmap, "Mean-expression difference versus the other species");
            return;
        }
        if (mode == 1 || mode == 2 || mode == 5 || mode == 7) {
            const auto& comparison = mode <= 2 ? _detail : _extraComparison;
            const auto groupA = mode <= 2 ? _groupA->selected() : _extraComparisonA;
            report.paragraph(QString("Group A: %1\nGroup B: %2").arg(groupA.join(", "), (mode <= 2 ? comparison.comparison : _extraComparisonB).join(", ")));
            if (!comparison.valid) return;
            report.heading("All tested genes in the comparison");
            report.beginTable({ "Gene", "Mean A", "Mean B", "Difference (A minus B)", "Detection A (%)",
                "Detection B (%)", "Moderated t", "p-value", "FDR" });
            for (int g = 0; g < comparison.genes.size(); ++g) {
                const double difference = comparison.delta[g], meanB = comparison.restMean[g];
                report.row({ comparison.genes[g], ChartDataReport::number(meanB + difference), ChartDataReport::number(meanB),
                    ChartDataReport::number(difference), ChartDataReport::number(100.0 * comparison.detection[g]),
                    ChartDataReport::number(100.0 * comparison.restDetection[g]), ChartDataReport::number(comparison.t[g]),
                    ChartDataReport::number(comparison.pValue[g]), ChartDataReport::number(comparison.fdr[g]) });
            }
            report.endTable();
            if (mode <= 2) return;
        }
        if (mode == 4) {
            report.matrix(*_extraHeatmap, "Species membership (1 = included, 0 = absent)");
            report.heading("Complete gene lists for the plotted intersections");
            report.beginTable({ "Intersection ID", "Species", "Gene" });
            for (int i = 0; i < _intersectionGenes.size(); ++i) {
                const auto members = _intersectionSpecies.value(i).join(", ");
                for (const auto& gene : _intersectionGenes[i])
                    report.row({ QString::number(i + 1), members, gene });
            }
            report.endTable();
        }
        if (mode == 6 || mode == 10) {
            report.matrix(*_extraHeatmap, "Mean expression");
            return;
        }
        if (mode == 3 && plotData) {
            const auto data = plotData();
            report.heading("Genes represented by the count bars");
            report.beginTable({ "Species", "Direction", "Gene", "Mean expression difference", "FDR" });
            if (data.genes) for (const auto& row : data.rows) {
                for (size_t g = 0; g < data.genes->size(); ++g) {
                    const float effect = (*row.difference)[g], fdr = (*row.fdr)[g];
                    if (significantDeEffect(effect, fdr, _level))
                        report.row({ row.species, effect > 0 ? "Upregulated" : "Downregulated", (*data.genes)[g],
                            ChartDataReport::number(effect), ChartDataReport::number(fdr) });
                }
            }
            report.endTable();
        }
        report.plot(*_extraPlot, [this](const Point& point) -> QStringList {
            const auto details = _extraTips.value(point.id).section("<br><br>", 0, 0);
            return { _extraGenes.value(point.id), _extraSpecies.value(point.id), QTextDocumentFragment::fromHtml(details).toPlainText() };
        }, { "Gene", "Species", "Details" });
    });
}

void SpeciesDeDialog::showIntersectionGenes(int column)
{
    if (column < 0 || column >= _intersectionGenes.size()) return;
    auto genes = _intersectionGenes[column];
    genes.sort(Qt::CaseInsensitive);
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QString("Genes in intersection %1").arg(column + 1));
    auto* layout = new QVBoxLayout(dialog);
    auto* description = new QLabel(QString("%1 genes at FDR < %2.\nSpecies: %3")
        .arg(genes.size()).arg(_level).arg(_intersectionSpecies.value(column).join(", ")), dialog);
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* list = new QListWidget(dialog);
    list->addItems(genes);
    layout->addWidget(list, 1);
    auto* copy = new QPushButton("Copy all genes", dialog);
    layout->addWidget(copy, 0, Qt::AlignRight);
    connect(copy, &QPushButton::clicked, dialog, [genes] { QGuiApplication::clipboard()->setText(genes.join('\n')); });
    connect(list, &QListWidget::itemDoubleClicked, dialog, [this](QListWidgetItem* item) { selectExtra(item->text(), {}); });
    dialog->resize(550, 460);
    dialog->show();
}

void SpeciesDeDialog::showCountGenes(const QString& species, bool upregulated)
{
    const auto data = plotData ? plotData() : SpeciesDePlotData{};
    const auto row = std::find_if(data.rows.begin(), data.rows.end(), [&](const auto& item) { return item.species == species; });
    if (!data.genes || row == data.rows.end()) return;

    const QString direction = upregulated ? "Upregulated" : "Downregulated";
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QString("%1 genes in %2").arg(direction, prettyName(species)));
    auto* layout = new QVBoxLayout(dialog);
    auto* description = new QLabel(dialog);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* table = new QTableWidget(dialog);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({ "Gene", "Mean expression difference", "FDR" });
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table, 1);
    for (size_t g = 0; g < data.genes->size(); ++g) {
        const float effect = (*row->difference)[g], fdr = (*row->fdr)[g];
        if (!significantDeEffect(effect, fdr, _level) || (effect > 0) != upregulated) continue;
        const int index = table->rowCount();
        table->insertRow(index);
        table->setItem(index, 0, new QTableWidgetItem((*data.genes)[g]));
        auto* effectItem = new QTableWidgetItem();
        effectItem->setData(Qt::DisplayRole, effect);
        table->setItem(index, 1, effectItem);
        auto* fdrItem = new QTableWidgetItem();
        fdrItem->setData(Qt::DisplayRole, fdr);
        table->setItem(index, 2, fdrItem);
    }
    description->setText(QString("%1 genes at FDR < %2. Difference is the species mean minus the mean of the other species. "
        "Double-click a gene to select it.").arg(table->rowCount()).arg(_level));
    table->setSortingEnabled(true);
    table->sortItems(1, upregulated ? Qt::DescendingOrder : Qt::AscendingOrder);
    new TableHoverDetails(table, [](const QModelIndex& index) {
        return QString("<b>%1</b><br>Mean expression difference: %2<br>FDR: %3")
            .arg(index.siblingAtColumn(0).data().toString().toHtmlEscaped(),
                index.siblingAtColumn(1).data().toString(), index.siblingAtColumn(2).data().toString());
    });
    connect(table, &QTableWidget::cellDoubleClicked, dialog, [this, table](int clickedRow, int) {
        selectExtra(table->item(clickedRow, 0)->text(), {});
    });
    dialog->resize(610, 470);
    dialog->show();
}

const SpeciesDeDetail& SpeciesDeDialog::extraComparison(const SpeciesDePlotData& data)
{
    QStringList available, defaults;
    for (const auto& row : data.rows) {
        available << row.species;
        if (_selectedSpecies.contains(row.species)) defaults << row.species;
    }
    if (!available.isEmpty() && (defaults.isEmpty() || defaults.size() == available.size()))
        defaults = { available.first() };
    QStringList remainder;
    for (const auto& name : available)
        if (!defaults.contains(name)) remainder << name;
    _extraGroupA->setSpecies(available, _extraGroupA->selected());
    _extraGroupB->setSpecies(available, _extraGroupB->selected());
    if (!_extraSpeciesEdited) {
        _extraGroupA->setSelectedSpecies(defaults);
        _extraGroupB->setSelectedSpecies(remainder);
    }
    const auto groupA = _extraGroupA->selected();
    _extraGroupB->setDisabledSpecies(groupA);
    const auto groupB = _extraGroupB->selected();
    if (_extraComparisonA == groupA && _extraComparisonB == groupB && !groupA.isEmpty()) return _extraComparison;
    _extraComparisonA = groupA;
    _extraComparisonB = groupB;
    _extraComparison = {};
    if (groupA.isEmpty() || groupB.isEmpty()) {
        _extraComparison.message = "Select at least one species in each group. Species outside both groups are excluded.";
    } else if (plotComparison) {
        _extraComparison = plotComparison(groupA, groupB);
    }
    return _extraComparison;
}

void SpeciesDeDialog::renderExtra()
{
    const int mode = _navigation->currentRow();
    if (mode < 3) {
        updateReadingGuide();
        return;
    }
    const bool hasGroups = mode == 5 || mode == 7;
    _extraGroupButton->setVisible(hasGroups);
    _extraGroupControls->setVisible(hasGroups && _extraGroupButton->isChecked());
    _extraInfo->setDataExplanation(deDataExplanation(mode));
    _extraHeatmap->setShowAllRowLabels(mode == 4 || mode == 6);
    const auto data = plotData ? plotData() : SpeciesDePlotData{};
    _extraTips.clear(); _extraGenes.clear(); _extraSpecies.clear();
    _extraRows.clear(); _extraColumns.clear(); _extraCellTips.clear();
    _countBars.clear();
    _intersectionGenes.clear();
    _intersectionSpecies.clear();
    _extraPlot->setGuideLines({});
    _extraPlot->setColumns({}, {}, {});
    _extraPlot->setHistogramWidth(0);
    _extraPlot->setHorizontalBars(mode == 3);
    const bool categories = mode == 3 || mode == 5 || mode == 9;
    _extraPlot->setGeneLabels(categories);
    _extraPlot->setSpaceForCategories(categories && mode != 3);
    _extraPlot->setConnectedSeries(mode == 8);
    const bool heatmap = mode == 4 || mode == 6 || mode == 10;
    _extraPlot->setVisible(!heatmap || mode == 4);
    _extraHeatmap->setVisible(heatmap);
    QVector<Point> points;
    QString xTitle, yTitle, info;
    const QString question = dePlotQuestions().value(mode);
    const auto number = [](double value) { return QString::number(value, 'g', 5); };
    const auto detail = [&](const QString& gene, const QString& species, double difference, double fdr) {
        return QString("<b>%1</b><br>%2<br>Mean-expression difference: %3<br>FDR: %4")
            .arg(gene.toHtmlEscaped(), species.toHtmlEscaped(), number(difference), number(fdr));
    };
    const auto add = [&](double x, double y, const QString& label, const QString& text, const QString& gene = {},
        const QString& species = {}, double color = 0.5) -> Point& {
        Point point;
        point.x = x; point.y = y; point.label = label;
        point.id = _extraTips.size();
        point.colorFraction = color;
        point.highlighted = (!gene.isEmpty() && gene == _focusGene) || _selectedSpecies.contains(species);
        _extraTips << text;
        _extraGenes << gene;
        _extraSpecies << species;
        points.push_back(point);
        return points.back();
    };
    const auto setHeatmap = [&](const QStringList& rows, const QStringList& columns,
        const QVector<QVector<float>>& values, const QString& units) {
        _extraHeatmap->setTraitLegend(units);
        _extraHeatmap->setData(rows, columns, values, QVector<int>(columns.size(), -1));
        QSet<int> selectedRows, selectedColumns;
        for (int i = 0; i < _extraRows.size(); ++i) if (_selectedSpecies.contains(_extraRows[i])) selectedRows.insert(i);
        const int gene = _extraColumns.indexOf(_focusGene);
        if (gene >= 0) selectedColumns.insert(gene);
        _extraHeatmap->setHighlight(selectedRows, selectedColumns);
    };
    if (!data.genes || data.rows.empty()) {
        _extraInfo->setText("Waiting for differential-expression results.\nBiological relevance: " + question);
        _extraPlot->setEmptyMessage("Run differential expression analysis to populate this chart.");
        _extraPlot->setData({}, {}, {});
        if (heatmap) setHeatmap({}, {}, {}, "Differential expression");
        updateReadingGuide();
        return;
    }
    const auto& genes = *data.genes;
    const auto index = std::find(genes.begin(), genes.end(), _focusGene);
    const size_t focus = static_cast<size_t>(index - genes.begin());
    const auto pass = [&](const SpeciesDePlotData::Row& row, size_t g) {
        return significantDeEffect((*row.difference)[g], (*row.fdr)[g], _level);
    };
    const auto candidateGenes = [&] {
        std::vector<size_t> selected;
        const QSet<QString> names(_result.heatmapGenes.begin(), _result.heatmapGenes.end());
        for (size_t g = 0; g < genes.size(); ++g)
            if (names.contains(genes[g]) || g == focus) selected.push_back(g);
        return selected;
    };
    if (mode == 3) {
        for (size_t i = 0; i < data.rows.size(); ++i) {
            const auto& row = data.rows[i];
            int up = 0, down = 0;
            for (size_t g = 0; g < genes.size(); ++g) {
                if (!pass(row, g)) continue;
                if ((*row.difference)[g] > 0.0) ++up; else ++down;
            }
            const auto tip = QString("<b>%1</b><br>Upregulated: %2<br>Downregulated: %3<br>FDR threshold: %4")
                .arg(row.species.toHtmlEscaped()).arg(up).arg(down).arg(_level);
            auto& positive = add(up, i + 1, row.species, tip + "<br>Click for the upregulated genes.", {}, row.species, 0.9);
            if (up > 0) _countBars.insert(positive.id, { row.species, true });
            positive.low = 0; positive.high = up;
            auto& negative = add(-down, i + 1, {}, tip + "<br>Click for the downregulated genes.", {}, row.species, 0.1);
            if (down > 0) _countBars.insert(negative.id, { row.species, false });
            negative.low = -down; negative.high = 0;
        }
        xTitle = "Significant genes (downregulated to upregulated)";
        info = QString("All %1 tested species are shown at FDR < %2.")
            .arg(data.rows.size()).arg(_level);
    } else if (mode == 4) {
        struct Intersection { QString mask; QStringList genes; };
        QHash<QString, Intersection> grouped;
        for (size_t g = 0; g < genes.size(); ++g) {
            QString mask;
            for (const auto& row : data.rows) mask += pass(row, g) ? '1' : '0';
            if (!mask.contains('1')) continue;
            auto& group = grouped[mask];
            group.mask = mask;
            group.genes << genes[g];
        }
        auto groups = grouped.values();
        std::sort(groups.begin(), groups.end(), [](const Intersection& a, const Intersection& b) {
            return a.genes.size() != b.genes.size() ? a.genes.size() > b.genes.size() : a.mask < b.mask;
        });
        if (groups.size() > 20) groups.resize(20);
        QStringList columns;
        QVector<QVector<float>> membership(static_cast<qsizetype>(data.rows.size()));
        _extraCellTips.resize(static_cast<qsizetype>(data.rows.size()));
        for (int j = 0; j < groups.size(); ++j) {
            const auto& group = groups[j];
            const auto label = QString::number(j + 1);
            columns << label;
            QStringList members;
            for (int i = 0; i < group.mask.size(); ++i)
                if (group.mask[i] == '1') members << data.rows[i].species;
            _intersectionGenes.push_back(group.genes);
            _intersectionSpecies.push_back(members);
            const auto tip = QString("<b>Intersection %1</b><br>%2 genes<br>Species: %3<br>Examples: %4<br>Click to see all genes.")
                .arg(label).arg(group.genes.size()).arg(members.join(", ").toHtmlEscaped(), group.genes.mid(0, 10).join(", ").toHtmlEscaped());
            add(j + 1, group.genes.size(), label, tip);
            for (int i = 0; i < group.mask.size(); ++i) {
                membership[i].push_back(group.mask[i] == '1' ? 1.0f : 0.0f);
                _extraCellTips[i] << tip;
            }
        }
        for (const auto& row : data.rows) _extraRows << row.species;
        setHeatmap(_extraRows, columns, membership, "Intersection membership (1 = included)");
        _extraPlot->setHistogramWidth(0.65);
        _extraPlot->setGuideLines({ { true, 0.0, {} } });
        xTitle = "Intersection ID";
        yTitle = "Gene count";
        info = QString("Up to 20 largest exact intersections at FDR < %1.").arg(_level);
    } else if (mode == 5 || mode == 7) {
        const auto& comparison = extraComparison(data);
        if (!comparison.valid) {
            info = comparison.message;
        } else {
            const auto& group = _extraComparisonA;
            const QString scope = group.size() == 1 ? group.front() : QString("%1 species in Group A").arg(group.size());
            const QString label = scope + QString(" vs %1 species in Group B").arg(comparison.groupBCount);
            if (mode == 5) {
                std::vector<int> ranked;
                for (int g = 0; g < comparison.genes.size(); ++g)
                    if (std::isfinite(comparison.delta[g]) && comparison.delta[g] != 0 && comparison.fdr[g] < _level)
                        ranked.push_back(g);
                std::sort(ranked.begin(), ranked.end(), [&](int a, int b) { return comparison.delta[a] < comparison.delta[b]; });
                for (size_t row = 0; row < ranked.size(); ++row) {
                    const int g = ranked[row];
                    const auto effect = comparison.delta[g];
                    const auto& gene = comparison.genes[g];
                    auto& point = add(effect, row + 1, gene, detail(gene, label, effect, comparison.fdr[g]),
                        gene, {}, effect > 0 ? 0.9 : 0.1);
                    point.low = std::min(0.0f, effect);
                    point.high = std::max(0.0f, effect);
                }
                _extraPlot->setGuideLines({ { false, 0.0, {} } });
                xTitle = "Mean expression difference (Group A minus Group B)";
                info = QString("%1. %2 genes pass FDR < %3.").arg(label).arg(ranked.size()).arg(_level);
            } else {
                for (int g = 0; g < comparison.genes.size(); ++g) {
                    const double effect = comparison.delta[g];
                    const double meanB = comparison.restMean[g], meanA = meanB + effect;
                    const double average = (meanA + meanB) / 2.0;
                    if (!std::isfinite(effect) || !std::isfinite(average)) continue;
                    const auto& gene = comparison.genes[g];
                    const auto tip = detail(gene, label, effect, comparison.fdr[g])
                        + QString("<br>Group A mean: %1<br>Group B mean: %2").arg(number(meanA), number(meanB));
                    add(average, effect, gene, tip, gene, {}, comparison.fdr[g] < _level ? 0.9 : 0.3);
                }
                _extraPlot->setGuideLines({ { true, 0.0, {} } });
                xTitle = "Average of the two expression means";
                yTitle = "Mean expression difference (Group A minus Group B)";
                info = QString("%1. Color highlights genes passing FDR < %2.").arg(label).arg(_level);
            }
            if (comparison.groupACount + comparison.groupBCount == 2)
                info += " One species per group supports expression differences only.";
        }
    } else if (mode == 8) {
        QStringList labels;
        QSet<int> highlights;
        if (focus < genes.size()) for (size_t i = 0; i < data.rows.size(); ++i) {
            const auto& row = data.rows[i];
            labels << row.species;
            if (_selectedSpecies.contains(row.species)) highlights.insert(static_cast<int>(i));
            const auto effect = (*row.difference)[focus];
            auto& point = add(i, effect, row.species, detail(_focusGene, row.species, effect, (*row.fdr)[focus]),
                _focusGene, row.species, pass(row, focus) ? 0.9 : 0.3);
            point.series = 0;
        }
        _extraPlot->setColumns(labels, highlights, {});
        _extraPlot->setGuideLines({ { true, 0.0, {} } });
        xTitle = "Species in tree order";
        yTitle = "Mean-expression difference";
        info = focus < genes.size() ? QString("%1 across species-versus-rest comparisons. Color indicates whether the effect passes the FDR threshold.").arg(_focusGene)
            : "Select a gene in the table or another plot.";
    } else if (mode == 9) {
        for (size_t i = 0; i < data.rows.size(); ++i) {
            const auto& row = data.rows[i];
            auto values = *row.difference;
            values.erase(std::remove_if(values.begin(), values.end(), [](float v) { return !std::isfinite(v); }), values.end());
            if (values.empty()) continue;
            std::sort(values.begin(), values.end());
            const auto q = [&](double fraction) { return ChartNumerics::linearQuantile(values, fraction); };
            auto& point = add(q(0.5), i + 1, row.species,
                QString("<b>%1</b><br>Median: %2<br>Middle 50%: %3 to %4<br>Range: %5 to %6")
                    .arg(row.species.toHtmlEscaped(), number(q(0.5)), number(q(0.25)), number(q(0.75)), number(values.front()), number(values.back())),
                {}, row.species);
            point.low = values.front(); point.high = values.back(); point.boxLow = q(0.25); point.boxHigh = q(0.75);
        }
        xTitle = "Mean-expression difference";
        info = "One box per species, using all tested genes. Boxes show the middle 50%; whiskers show the range.";
    } else {
        const auto [trait, values] = mode == 6 || !traitData ? std::pair<QString, QJsonObject>{} : traitData();
        std::vector<std::pair<size_t, double>> ordered;
        for (size_t i = 0; i < data.rows.size(); ++i) {
            bool ok = false;
            const double value = values.value(data.rows[i].species).toVariant().toString().toDouble(&ok);
            if (mode == 6 || (ok && std::isfinite(value))) ordered.emplace_back(i, value);
        }
        if (mode != 6)
            std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        auto candidates = candidateGenes();
        if (mode == 11) {
            candidates.clear();
            if (focus < genes.size()) candidates.push_back(focus);
        }
        QStringList labels;
        QVector<QVector<float>> expression, detection;
        for (auto g : candidates) _extraColumns << genes[g];
        for (const auto& [s, value] : ordered) {
            const auto& row = data.rows[s];
            labels << (mode == 6 ? row.species : QString("%1 (%2)").arg(row.species, number(value)));
            _extraRows << row.species;
            QVector<float> means, fractions;
            QStringList tips;
            for (auto g : candidates) {
                const double mean = (*row.mean)[g], detected = (*row.detection)[g];
                const auto tip = detail(genes[g], row.species, (*row.difference)[g], (*row.fdr)[g])
                    + QString("<br>Mean expression: %1<br>Detected cells: %2%").arg(number(mean), number(100 * detected))
                    + (mode == 6 ? QString() : QString("<br>%1: %2").arg(trait.toHtmlEscaped(), number(value)));
                if (mode == 11) add(value, mean, row.species, tip, genes[g], row.species, pass(row, g) ? 0.9 : 0.3);
                else { means.push_back(mean); fractions.push_back(detected); tips << tip; }
            }
            expression.push_back(means);
            detection.push_back(fractions);
            _extraCellTips.push_back(tips);
        }
        if (heatmap) {
            setHeatmap(labels, _extraColumns, expression, "Mean expression");
            if (mode == 6) _extraHeatmap->setDotSizes(std::move(detection));
        }
        xTitle = trait;
        yTitle = "Mean expression";
        if (mode == 6) info = "Dot color encodes mean expression. Dot area encodes the fraction of expressing cells.";
        else if (ordered.empty()) info = trait.isEmpty()
            ? "Select a numeric trait in the Gene Identification settings."
            : "No tested species have a numeric value for this trait. Check the trait metadata.";
        else if (mode == 10) info = "Species with numeric trait values are ordered by trait. Colors represent mean expression of candidate DE genes.";
        else info = focus < genes.size() ? QString("%1 across species with numeric trait values.").arg(_focusGene) : "Select a gene in the gene table or another plot.";
    }
    _extraInfo->setText(info + "\nBiological relevance: " + question);
    if (!heatmap || mode == 4) {
        const bool showExplanation = points.isEmpty() && (mode == 5 || mode == 7 || mode == 8 || mode == 11);
        _extraPlot->setEmptyMessage(showExplanation ? info : QString());
        _extraPlot->setData(points, xTitle, yTitle);
    }
    updateReadingGuide();
}
