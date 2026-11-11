# GeneDetect workflow and state inventory

This inventory covers the plugin-owned C++ workflows and view interactions.
Third-party library internals and ManiVault's executor, storage and web-channel
implementations are outside this plugin's ownership.

## Execution rules

- GUI objects, dataset creation, workflow transitions and publication run on the
  GUI thread. Workers receive snapshots and report progress through atomics.
- DE requires its analysis window. A completed gene click requests correlation
  for that selected gene only; opening trait analysis explicitly requests listed
  or all genes. Update never starts a full-gene correlation scan automatically.
- Each analysis has an independent cancellation generation. Closing its window
  cancels that generation. Reopening creates a fresh token; it cannot revive an
  old worker. Update cancels both generations and suspends analysis until its
  publication finishes. Destruction cancels and joins all outstanding workers.
- Cancellation requests do not set a workflow to Idle. Only its worker-finished
  callback does that. A superseded worker may still own datasets or cache data.
- A finished worker is not automatically publishable. Its token must still be
  current, demand must still exist, and the latest input signature must match.
- Changed inputs invalidate displayed trait values even when its window is
  closed. The correlation column is hidden until the selected gene has a result,
  and respects the user's column chooser. A selected-gene request reports its
  single-gene FDR scope; broader requests retain their requested correction set.
- Completed numerical caches and published views are distinct. A new result is
  published as a whole; a partial cache write is never a completed result.
- DE and trait heavy pipelines share a one-worker, low-priority pool; each may
  use bounded parallel reads/math internally. Requests for the other pipeline
  are coalesced and resumed when the current phase ends.
- Gene clicks do not cancel or restart background analyses. Their faster cached
  reads no longer justify abandoning long-running reads; completed means are
  retained and callbacks re-snapshot the latest requested gene and settings.
- Each gene request has a generation token in addition to its row identity.
  A/B/A clicks cannot publish the first A as the last A. In-flight reads show
  neutral colors labeled Preparing; GUI callers never fall back to source reads.
- Tiled analysis uses cached narrow stripes for grouped sums and detection when
  available, otherwise one grouped physical scan, even for few/sparse genes.
  The sparse range compatibility reader is for non-tiled data and reports progress
  between batches. Detection-count requests use a reader that produces counts.
- Packed frames use the public codec `decodeTo` path with an owned output buffer;
  the plugin avoids the legacy allocating `decode` path and its context lifetime.
- Update's existing decoded tiles also prepare a lossless 64-gene-block layout
  with the bundled Zstandard library. This adds packing work to that scan, not
  another matrix read. Clicks read these narrow frames, with a safe source
  fallback for missing or quota-limited blocks. Completed vectors use bounded
  RAM when the block file is enabled, so there is only one expression cache file.
- Cache preparation uses Zstandard fast mode and batches all stripes of a tile
  into one append. This avoids a seek/flush/write cycle for every 64-gene stripe;
  completed tiles remain available while other tiles are packed.
- Packing and bulk unpacking use short row bands to keep memory access local.
  Narrow requests keep 64-gene frames. Dense DE requests read ahead one tile's
  encoded frames and aggregate stripes directly, avoiding a reconstructed
  full-width matrix tile in the interactive path.
- Repeated 32-bit values within a row use exact raw-bit dictionaries and one-byte
  references before compression. This is lossless, including signed zero and NaN
  payloads. High-entropy rows or unhelpful dictionaries use the original compressed
  values instead. All-zero stripes have no disk payload. Raw zero-mask packing was
  rejected because it can inflate the cache.
- Each worker owns a read-only handle to the same cache file. Published offsets
  are immutable; index lookup remains locked but reads/decompression run outside
  the writer mutex. Workers retain cache ownership until their handles close.
  Writer buffers are flushed before offsets are exposed to independent readers.
- Cached-stripe timings use hot-operation sampling rather than millions of CSV
  begin/end records. Full operation totals and bulk-tile scopes remain recorded;
  packing, writer wait, writing and raw/encoded byte counts have separate records.
- All plugin cache files and timing logs live under
  `<ManiVault configured temp root>/EvoViewer/GeneDetect-<PID>-<unique-key>/`,
  resolved using `Application::getTemporaryDirRoot()`. The plugin does not change
  that setting or touch the application's parent/session files. A process-lifetime `QLockFile`
  protects each session. A registry lock serializes creation with stale cleanup;
  cleanup checks the full key shape, skips live locks, and never follows nested
  directories or junctions. Shutdown removes owned files; next launch recovers
  sessions abandoned by a crash. No age limit can evict a still-running process.

## State inventory

| Workflow / owner | States and transitions | Invariant / reason to retain |
| --- | --- | --- |
| Main Update (`SettingsAction::updateButtonTriggered`) | Idle → prepare → index → baseline/selected means → publish → success, stale or failure | ManiVault `WorkflowPlan` owns the explicit stages. `_updateWorkflowRunning` is the re-entry guard. Original selection snapshots detect changed inputs before publication. |
| Overall exploration (`cancelExplorationAnalysis`, `prepareExplorationAnalysis`) | Available ↔ suspended for Update | Suspension is independent of window demand and worker lifetime; completing Update resumes only explicitly demanded analyses. |
| Trait analysis (`AnalysisWorkflow _traitWorkflow`) | Demand closed/open; `Idle → Reading → Idle → Computing → Idle`; one pending refresh | One phase replaces independent reading/computing flags. Changing trait, scope, cells or species recomputes from latest inputs only while demanded. Closing cancels its read/math token and stops its debounce timer. |
| DE overview (`AnalysisWorkflow _deWorkflow`) | Demand closed/open; `Idle → Reading → Idle → Computing → Idle`; one pending refresh | Read means, compute tests, then build overview in separate completed phases. Top-N, rank and detection changes reuse numerical caches. Closing cancels only DE. |
| DE details (`speciesDeDetail`) | Missing comparison → calculate requested comparison → cached | Species-versus-rest details are computed only when requested. Custom groups retain their own calculation path and historical statistical definitions. |
| Analysis cache validity (`TraitCache`, `DeCache`, statistics and published keys) | Missing → sorted membership built → means known → statistics keyed → view published | Input signatures, per-gene `known` bits and per-comparison detail flags are validity state, not worker-running state. Keep partial numerical caches separate from displayed result keys. |
| Gene click (`onGeneRowChanged`, `prepareGeneClick`) | Debounced row → read worker → outcome Pending/Ready/Failed → publish or discard | Latest persistent row wins. New row cancels old work; its completion starts the latest request. Outcome is read after the future completes; cancellation is atomic. Metadata can update before colors. |
| Expression vectors (`GeneExpressionCache`) | Absent → complete RAM value → bounded eviction; compatibility fallback may use Writing → DiskReady | RAM-only alongside the gene-block file. The compatibility vector file is used when no block layout is available. Partial disk vectors remain unpublished and canceled writes can be retried. |
| Gene blocks (`ColumnTileCache`) | Missing → one producer codes/compresses already-decoded tile → independently published stripes; quota/failure → source fallback | One bounded file, lossless 64-gene frames, metadata limited to 128 MiB. The disk allowance is at most 6,000 MB, 32× the configured memory budget, and one third of currently free space. This is a ceiling, not a target/preallocation. The actual bytes and dictionary-use counts are logged. Reads remain available while other tiles are written. |
| Temporary session (`EvoViewerTemp`) | Registry locked → stale sessions checked → unique PID/key folder locked → running → shutdown removal; crash → next-launch recovery | Locks use PID/host/process identity, never time-based eviction. Only owned keyed directories are cleaned. Tests exercise a real live process, normal exit and forced exit. |
| Tile read cache (`PackedTileAccess`) | Absent → Loading → Ready or Failed | Readers of the same tile wait for one producer. Failed entries wake waiters and are removed so future requests can retry. Leases retain storage/bytes; I/O runs outside the cache mutex. |
| Archive index (`StoredZipRanges`) | Missing → indexed for file identity → invalidated on size/time change or bounded eviction | Direct stored-entry reads preserve archive offsets without staging millions of files. Compressed/encrypted entries retain the compatible fallback. |
| Embedding geometry (`_embeddingCoordinates*`, `_embeddingVisible*`) | Missing → prepared coordinates → visible row mapping; dataset event → invalid | Reuse geometry across color changes. Dataset changes cancel affected reads and clear dependent geometry. Removal joins readers before storage disappears. |
| Progress (`OperationProgress`, `OperationProgressState`) | Queued/running → phase updates → finished or aborted | Workers never access widgets. GUI timer publishes progress; only successful view publication reports 100%. Canceling an already finished task leaves it finished. |
| Baseline/selected means and abundance | Missing/invalid → computed for source or selection key → reused | `_meanMapComputed` plus signature, selected-means key, hierarchy generation and abundance key invalidate independently. Baseline and selected means share a scan when both are needed. |
| Status and action enablement | Serialized `M` needs Update, `R` running, `C` complete, `E` error; `_pauseStatusUpdates` suppresses intermediate messages | Preserve serialized status codes and existing linked actions. Status is presentation, not permission to publish worker output. |
| Project load and temporary dataset ownership | Load blocked → restore exact dataset ID queues → release blocker → ready | `_projectOpened`, load blocker and queue-serialization marker support old project migration. Delete only registered temporary IDs; session-reusable datasets have a separate lifetime. |
| Selection / hierarchy controls | Main selection listener rebound on dataset replacement; cluster selection single/additive; hierarchy timers debounce rebuilds | Primary cell type, selected set, label styles and species selection stay coordinated. Signal blockers prevent feedback loops; hierarchy changes invalidate dependent cache generations. |
| Top-gene filtering and table models | Parameter changes → debounce → rebuild; model replacement → proxy replacement and old model disposal | Search mode, allowed-gene set, significance threshold and visible-column choices are independent view state. Preserve numeric sort roles and model connection lifetimes. |
| t-SNE and tree plugins | Optional analysis instance absent/running/attached; owned datasets removed → instance invalid | `QPointer` and dataset ownership govern external plugin lifetime. Retain both selected-cell and gene-table t-SNE, precomputed embedding, tree selection and export paths. |
| Trait/DE dialogs | Closed/open, selected tab; DE overview/focus/custom groups; automatic/manual group choice; debounced group comparison | Dialog lifetime owns demand. Tab/focus changes reuse results and preserve highlighting, exports and disjoint-group checks. |
| Plot and heatmap gestures | Invalid/valid viewport; idle → pressed → drag or click → idle; zoom/reset | Keep `_viewValid`, `_dragging`, `_dragMoved`, label/grid/trend options local to widgets. These describe orthogonal presentation choices, not background analysis phases. |
| Timing writer (`XSCDebug`) | Scope begin/end; asynchronous span queued/finished; buffered flush | Preserve operation timing and sampling of hot loops. Instrumentation must not introduce per-cell disk writes. |

## Source of truth and maintenance

`SettingsAction.cpp` includes the dotted responsibility files (`SettingsAction.Ui.inl`,
`SettingsAction.Analysis.inl`, `SettingsAction.Data.inl`, `SettingsAction.Tree.inl`,
`SettingsAction.Serialization.inl`). The obsolete underscore-named analysis copy
was not compiled and has been removed. Do not recreate parallel implementations.

Use `AnalysisWorkflow` for GUI-thread phase transitions. Worker lambdas capture
the cancellation token at launch; never look up the replacement token from a
worker. Do not infer completion from a canceled token or a progress percentage.
New asynchronous stages must join the same latest-input/publication checks.
Keep existing serialization names and statistical formulas stable when changing
execution order.

Regression checks live in `tests`: workflow cancellation across close/reopen and
Update, independent DE/trait tokens, complete cache publication during writes,
canceled-write retry, bounded RAM/disk, tile scheduling and progress monotonicity.
Build only `XSCGeneDetectPlugin`; link the existing bundled `QuaZip` target.

Runtime validation still requires a representative ManiVault project: Update,
rapid gene changes, open/close/reopen each analysis during a read, change selection
while work is running, custom DE groups, exports, and project reopen. Unit checks
and compilation cannot establish end-to-end dataset latency or every UI path.

`ColumnTileCacheTest` also benchmarks 7,754-column synthetic tile packing and a
2,326,579-cell cached gene with eight readers. These are read-path benchmarks,
not a substitute for the full `gene.click.total` timing including UI publication.
