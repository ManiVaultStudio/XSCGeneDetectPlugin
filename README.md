# XSCGeneDetectPlugin

`XSCGeneDetectPlugin` is a ManiVault view plugin for exploring cross-species gene expression differences from point-based single-cell style datasets.

The plugin is designed for workflows where a user:

- selects a subset of cells in an embedding or scatterplot,
- compares selected versus non-selected cells across species,
- ranks genes by differential expression,
- inspects per-species statistics in tables,
- and projects those statistics back onto phylogenetic tree and scatterplot views.

## What The Plugin Does

At a high level, the plugin combines five pieces of information:

- a main expression dataset (`Points`),
- a low-dimensional embedding dataset (`Points`),
- a species assignment dataset (`Cluster`),
- one or more hierarchy / cell-type cluster datasets (`Cluster`),
- and a reference phylogenetic tree dataset (`XSCTree`).

From these inputs, it computes:

- mean expression per gene and species,
- selected-versus-non-selected expression statistics,
- top-N ranked genes per species,
- species-level abundance summaries within selected hierarchy branches,
- table models for ranked genes and species summaries,
- optional scatterplot coloring datasets,
- and optional tree payloads for hierarchy-specific exploration.

## Repository Layout

- [CMakeLists.txt](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/CMakeLists.txt): CMake target definition and ManiVault/Qt integration.
- [src/XSCGeneDetectPlugin.h](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/src/XSCGeneDetectPlugin.h): plugin class declaration.
- [src/XSCGeneDetectPlugin.cpp](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/src/XSCGeneDetectPlugin.cpp): plugin wiring, view integration, table rendering, and scatterplot/tree coordination.
- [src/SettingsAction.h](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/src/SettingsAction.h): main state container and action declarations.
- [src/SettingsAction.cpp](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/src/SettingsAction.cpp): shared helpers and compilation unit for the split inline implementation.
- [src/SettingsAction.Ui.inl](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/src/SettingsAction.Ui.inl): UI/action wiring.
- [src/SettingsAction.Analysis.inl](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/src/SettingsAction.Analysis.inl): main analysis and ranking logic.
- [src/SettingsAction.Data.inl](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/src/SettingsAction.Data.inl): dataset creation, model generation, and export helpers.
- [src/SettingsAction.Tree.inl](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/src/SettingsAction.Tree.inl): tree-related logic.
- [src/SettingsAction.Serialization.inl](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/src/SettingsAction.Serialization.inl): serialization support.

## Build Requirements

The project expects a ManiVault development environment with:

- CMake 3.22 or newer,
- Qt 6 with `Widgets`, `WebEngineWidgets`, and `Concurrent`,
- ManiVault packages for `Core`, `PointData`, and `ClusterData`,
- the `XSCTreeData` plugin/library available at build or install time.

Relevant CMake variables:

- `ManiVault_INSTALL_DIR`: ManiVault installation root.
- `MV_CSCTD_INSTALL_DIR`: installation root for `XSCTreeData`.
- `XSCTREEDATA_LINK_LIBRARY`: optional explicit override for the tree data plugin library.

## Building

Typical CMake flow:

```powershell
cmake -S . -B build -DManiVault_INSTALL_DIR="C:\Path\To\ManiVault"
cmake --build build --config Release
```

On successful build, the plugin is installed into the ManiVault `Plugins` directory via the post-build install step defined in [CMakeLists.txt](E:/Coding/DevBundle/GenerateTHESISImages/source/XSCGeneDetectPlugin/CMakeLists.txt).

## Runtime Data Expectations

The plugin assumes:

- the selected species and hierarchy cluster datasets are children of the main points dataset,
- the embedding dataset is also a child of the main points dataset,
- point dimension names correspond to gene identifiers,
- the reference tree contains the same species names as the species cluster dataset.

If these relationships are missing or inconsistent, the plugin will refuse to compute and log diagnostic messages.

## Performance Notes

Recent optimization work focused on reducing full recomputation cost and UI overhead:

- chunked matrix reads are used instead of fetching one gene at a time,
- selected-cell statistics are aggregated in-memory after a single chunked read over the selected points,
- repeated linear lookups in selection remapping and hierarchy membership checks were replaced with hashed or linear-time helpers,
- forced waits around t-SNE stop actions were removed,
- expensive UI auto-sizing and duplicate signal connections were reduced.

Even with those changes, some operations can still be costly on very large datasets because the plugin may need to:

- rebuild large table models,
- update multiple derived ManiVault datasets,
- refresh scatterplot color/selection state,
- and serialize many tree payloads for hierarchy exploration.

## Maintenance Notes

If you extend the analysis logic, prefer:

- batched point reads over per-gene calls,
- explicit invalidation/caching boundaries,
- stable table widths over repeated `resizeColumnsToContents()` calls in hot paths,
- and avoiding repeated signal connections inside methods that run on every update.