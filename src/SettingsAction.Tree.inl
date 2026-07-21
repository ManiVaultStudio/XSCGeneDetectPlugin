QString SettingsAction::generateTooltip(const ViewPluginSamplerAction::SampleContext& toolTipContext, const QString& clusterDatasetId, bool showTooltip, QString indicesType) {
    // Extract and convert GlobalPointIndices and ColorDatasetID from toolTipContext
    auto raw_Global_Local_PointIndices = toolTipContext[indicesType].toList();

    // Convert the list of global point indices to a vector of integers
    std::vector<std::seed_seq::result_type> global_local_PointIndices;
    global_local_PointIndices.reserve(raw_Global_Local_PointIndices.size());
    for (const auto& global_local_PointIndex : raw_Global_Local_PointIndices) {
        global_local_PointIndices.push_back(global_local_PointIndex.toInt());
    }

    // If the global point indices list is empty, return an empty result
    if (global_local_PointIndices.empty()) {
        return {};
    }

    // If there is no cluster dataset ID, return a summary of total points
    if (clusterDatasetId.isEmpty()) {
        return QString("<table><tr><td><b>Total points: </b></td><td>%1</td></tr></table>").arg(global_local_PointIndices.size());
    }

    // Retrieve the cluster dataset
    auto clusterFullDataset = mv::data().getDataset<Clusters>(clusterDatasetId);

    // If the dataset is invalid, return a summary of total points
    if (!clusterFullDataset.isValid()) {
        return QString("<table><tr><td><b>Total points: </b></td><td>%1</td></tr></table>").arg(global_local_PointIndices.size());
    }

    // Get the clusters from the dataset
    auto clusterValuesData = clusterFullDataset->getClusters();

    // If the clusters data is empty, return a summary of total points
    if (clusterValuesData.isEmpty()) {
        return QString("<table><tr><td><b>Total points: </b></td><td>%1</td></tr></table>").arg(global_local_PointIndices.size());
    }
    
    // Process each cluster and find intersections with global point indices
    std::map<QString, std::pair<int, QColor>> clusterCountMap;
    for (const auto& cluster : clusterValuesData) {
        QString clusterName = cluster.getName();
        QColor clusterColor = cluster.getColor();
        auto clusterIndices = cluster.getIndices();

        // Sort the indices before performing the intersection
        std::sort(clusterIndices.begin(), clusterIndices.end());
        std::sort(global_local_PointIndices.begin(), global_local_PointIndices.end());

        std::vector<std::seed_seq::result_type> intersect;
        std::set_intersection(clusterIndices.begin(), clusterIndices.end(),
            global_local_PointIndices.begin(), global_local_PointIndices.end(),
            std::back_inserter(intersect));

        // If there is an intersection, store the result in the map
        if (!intersect.empty()) {
            clusterCountMap[clusterName] = std::make_pair(intersect.size(), clusterColor);
        }
    }

    // If no clusters were found, return a summary of total points
    if (clusterCountMap.empty()) {
        return QString("<table><tr><td><b>Total points: </b></td><td>%1</td></tr></table>").arg(global_local_PointIndices.size());
    }

    // Generate HTML output
    QString html = "<html><head><style>"
        "body { display: flex; flex-direction: column; align-items: flex-start; }"
        ".bar-container { display: flex; align-items: center; margin: 2px 0; }"
        ".bar { height: 20px; margin-right: 5px; }"
        ".label { font-size: 12px; }"
        ".chart { width: 100%; }"
        "</style></head><body>";

    // Convert the map to a vector of pairs for sorting
    std::vector<std::pair<QString, std::pair<int, QColor>>> clusterVector(clusterCountMap.begin(), clusterCountMap.end());

    // Sort the vector by count in descending order
    std::sort(clusterVector.begin(), clusterVector.end(), [](const auto& a, const auto& b) {
        return a.second.first > b.second.first;
    });

    // Find the maximum count for scaling the bars
    int maxCount = clusterVector.empty() ? 1 : clusterVector.front().second.first; // Default to 1 if no data.

    // Calculate the maximum width required for text and icon
    int maxTextIconWidth = 0;
    for (const auto& entry : clusterVector) {
        QString clusterName = entry.first;
        int textWidth = QFontMetrics(QFont()).horizontalAdvance(clusterName + ": " + QString::number(entry.second.first));
        int iconWidth = 16; // Assuming icon width is 16px
        maxTextIconWidth = std::max(maxTextIconWidth, textWidth + iconWidth);
    }
    maxTextIconWidth = maxTextIconWidth + 2;
    // Populate the divs with cluster data
    html += "<div class='chart'>";
    for (const auto& entry : clusterVector) {
        QString clusterName = entry.first;
        int count = entry.second.first;
        QString colorHex = "#a6a6a6"; // entry.second.second.name();
        QColor color(entry.second.second);

        QString textColor = "black";
        int barWidth = (maxCount > 0) ? static_cast<int>((static_cast<double>(count) / maxCount) * 100) : 0;
        barWidth = std::max(barWidth, 5); // Minimum width for visibility

        QString iconPath = ":/speciesicons/SpeciesIcons/" + clusterName + ".svg";
        QString iconHtml;
        if (QFile::exists(iconPath)) {
            QFile file(iconPath);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray iconData = file.readAll().toBase64();
                iconHtml = QString("<img src='data:image/svg+xml;base64,%1' width='16' height='16'/> ").arg(QString(iconData));
            } else {
                iconHtml = "<div style='width:16px; height:16px;'></div>"; // Placeholder
            }
        } else {
            iconHtml = "<div style='width:16px; height:16px;'></div>"; // Placeholder
        }

        html += "<div class='bar-container'>";
        html += "<div class='bar' style='width:" + QString::number(barWidth) + "%; background-color:" + colorHex + "; color:" + textColor + ";'></div>";
        html += "<div class='label' style='width:" + QString::number(maxTextIconWidth) + "px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;'>"
            + iconHtml + clusterName + ": " + QString::number(count) + "</div>";
        html += "</div>";
    }

    html += "</div>";

    html += "</body></html>";


    return html;
}


void SettingsAction::updateSelectedSpeciesCounts(QJsonObject& node, const std::map<QString, int>& speciesCountMap) {
    // Check if the "name" key exists in the current node
    if (node.contains("name")) {
        QString nodeName = node["name"].toString();
        auto it = speciesCountMap.find(nodeName);
        // If the "name" is found in the speciesExpressionMap, update "mean" if it exists or add "mean" if it doesn't exist
        if (it != speciesCountMap.end()) {
            node["cellCounts"] = it->second; // Use it->second to access the value in the map
        }
    }

    // If the node has "children", recursively update them as well
    if (node.contains("children")) {
        QJsonArray children = node["children"].toArray();
        for (int i = 0; i < children.size(); ++i) {
            QJsonObject child = children[i].toObject();
            updateSelectedSpeciesCounts(child, speciesCountMap); // Recursive call
            children[i] = child; // Update the modified object back into the array
        }
        node["children"] = children; // Update the modified array back into the parent JSON object
    }
}
/*
QString SettingsAction::createJsonTreeFromNewick(QString tree, std::vector<QString> leafnames, std::map <QString, Statistics> speciesMeanValues)
{
    int i = 0;
    std::string jsonString = "";
    std::stringstream jsonStream;
    std::string newick = tree.toStdString();
    while (i < newick.size()) {
        if (newick[i] == '(') {
            jsonStream << "{\n\"children\": [";
            i++;
        }
        else if (newick[i] == ',') {
            jsonStream << ",";
            i++;
        }
        else if (newick[i] == ')') {
            jsonStream << "],\n\"id\": 1,\n\"score\": 1,\n\"branchLength\": 1.0,\n\"width\": 1\n}";
            i++;
        }
        else if (newick[i] == ';') {
            break;
        }
        else {
            if (isdigit(newick[i])) {
                int skip = 1;
                std::string num = "";
                for (int j = i; j < newick.size(); j++) {
                    if (isdigit(newick[j])) {
                        continue;
                    }
                    else {
                        num = newick.substr(i, j - i);

                        skip = j - i;
                        break;
                    }
                }
                std::string species = leafnames[(std::stoi(num) - 1)].toStdString();
                //std::string meanValue = std::to_string(speciesMeanValues[QString::fromStdString(species)]);
                auto it = speciesMeanValues.find(QString::fromStdString(species));
                std::string meanValue;
                if (it != speciesMeanValues.end()) {
                    // Key found, use the corresponding value
                    meanValue = std::to_string(it->second.meanSelected);
                }
                else {
                    // Key not found, assign -1
                    meanValue = "-1";
                }

                jsonStream << "{\n\"color\": \"#000000\",\n\"hastrait\": true,\n\"iscollapsed\": false,\n\"branchLength\": 1.0,\n\"cellCounts\": " << 0 << ",\n\"mean\": "<< meanValue <<", \n\"name\": \"" << species << "\"\n}";
                i += skip;
            }
        }
    }

    jsonString = jsonStream.str();

    nlohmann::json json = nlohmann::json::parse(jsonString);
    std::string jsonStr = json.dump(4);
    //qDebug()<< "XSCClusterRankPlugin::createJsonTree: jsonStr: " << QString::fromStdString(jsonStr);
    QString formattedTree = QString::fromStdString(jsonStr);


    return  formattedTree;
}
*/
/*
std::string SettingsAction::mergeToNewick(int* merge, int numOfLeaves) {
    std::vector<std::string> labels(numOfLeaves);
    for (int i = 0; i < numOfLeaves; ++i) {
        labels[i] = std::to_string(i + 1);
    }

    std::stack<std::string> stack;

    for (int i = 0; i < 2 * (numOfLeaves - 1); i += 2) {
        int left = merge[i];
        int right = merge[i + 1];

        std::string leftStr;
        if (left < 0) {
            leftStr = labels[-left - 1];
        }
        else {
            leftStr = stack.top();
            stack.pop();
        }

        std::string rightStr;
        if (right < 0) {
            rightStr = labels[-right - 1];
        }
        else {
            rightStr = stack.top();
            stack.pop();
        }

        std::string merged = "(" + leftStr + "," + rightStr + ")";
        stack.push(merged);
    }

    return stack.top() + ";";
}
*/
double* SettingsAction::condensedDistanceMatrix(const std::vector<float>& items) {
    size_t n = items.size();
    double* distmat = new double[(n * (n - 1)) / 2];
    size_t k = 0;

#pragma omp parallel for schedule(dynamic) collapse(2) private(k)
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            k = ((n * (n - 1)) / 2) - ((n - i) * (n - i - 1)) / 2 + j - i - 1;
            distmat[k] = std::abs(items[i] - items[j]);
        }
    }

    return distmat;
}

SettingsAction::Widget::Widget(QWidget* parent, SettingsAction* SettingsAction) :
    WidgetActionWidget(parent, SettingsAction)
{ }

SettingsAction::OptionSelectionAction::Widget::Widget(QWidget* parent, OptionSelectionAction* optionSelectionAction) :
    WidgetActionWidget(parent, optionSelectionAction)
{ }

