# PROGRESS.md — Chronological Progress & Incident Tracker

> **Notice:** This document is maintained strictly by the Lead Architect / Orchestrator.  
> Worker subagents (`pmx_core_engineer`, `pmx_ui_engineer`, `pmx_qa_adversary`, `pmx_devops_engineer`) MUST NOT read or modify this file.

---

## 1. Project Status Overview
* **Project Name:** ZenPMX (Native Linux PMX Editor)
* **GitHub Repository:** [`https://github.com/covarianttensor/ZenPMX`](https://github.com/covarianttensor/ZenPMX) (Public)
* **Active Phase:** Milestone 1 & 2 Completed (`v0.1.0-milestone1`, `v0.2.0-milestone2` pushed to GitHub)
* **Next Target:** Milestone 3 (Full MMD Shader Pipeline & Interactive 3D Edit Mode)
* **Overall Completion Level:** 50%
* **Current Working Tree:** Clean (`main` branch commit `c87c34f`)
* **Test Suite Status:** 7/7 CTest suites passing (107 unit tests, 0 failures, 100% clean under ASan + UBSan in 1.32s)

---

## 2. Chronological Milestone & Ticket Log

### Milestone 1: The Tracer Bullet (Completed & Tagged `v0.1.0-milestone1`)

| Date / UTC | Ticket ID | Subsystem Target | Subagent | Commit | Test Status | Token Burn | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :---: | :--- |
| 2026-10-24 10:39 | **Phase-0** | Architecture / Specs | Lead Architect | `HEAD` | PASS | ~14k | Triple-Document Foundation ingested; initial subagents registered. |
| 2026-10-24 11:00 | **Ticket-1.1** | Core / Build (`PmxCore`) | `pmx_core_engineer` | `84d19ee` | 1/1 CTest (6/6 QtTest) | ~26k | Build harness, canonical `PmxTypes.h`, Little-Endian stream, header round-trip. |
| 2026-10-24 11:27 | **Ticket-1.2** | Core / Serialization (`PmxCore`) | `pmx_core_engineer` | `a4e2ce8` | 1/1 CTest (10/10 QtTest) | ~28k | Complete PMX 2.0/2.1 lossless IO, 10 sections, US-1 surgical edit isolation, fuzzing. |
| 2026-10-24 11:59 | **Ticket-1.3** | State / Models (`PmxModels`) | `pmx_core_engineer` | `d811c4b` | 2/2 CTest (15/15 QtTest) | ~38k | `PmxDocumentController`, `QUndoStack`, Material & Bone tables, JP/EN toggle. (1 retry: Strike 1 resolved). |
| 2026-10-24 16:05 | **Ticket-1.4** | Viewport / App (`PmxViewport`) | `pmx_ui_engineer` | `d6c5d7e` | 3/3 CTest (10/10 QtTest) | ~52k | `QQuickRhiItem` viewport, orbital camera, GLSL shaders, `Main.qml`. (Strike 2 rollback, fresh worker resolved). |
| 2026-10-24 18:51 | **Ticket-1.5** | AI Co-Pilot / MCP (`PmxMcpServer`) | `pmx_core_engineer` | `b7fed90` | 4/4 CTest (16/16 QtTest) | ~32k | `pmx-mcp` CLI, JSON-RPC 2.0, `get_model_summary`, undoable `rename_bone`. (1 retry: Strike 1 resolved). |

### Milestone 2: 8-Tab Spreadsheet Inspector & Kitbash Merger (Completed & Tagged `v0.2.0-milestone2`)

| Date / UTC | Ticket ID | Subsystem Target | Subagent | Commit | Test Status | Token Burn | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :---: | :--- |
| 2026-10-24 22:27 | **Ticket-2.1** | State / Models (`PmxModels`) | `pmx_core_engineer` | `9079cf6` | 4/4 CTest (25/25 QtTest) | ~36k | 5 table models (Vertex, Morph, Display, RigidBody, Joint). Strike 1 resolved JP/EN command ID collision & bounds checks. |
| 2026-10-24 22:54 | **Ticket-2.2** | UI / Viewport (`pmx-editor`) | `pmx_ui_engineer` | `f8d69e6` | 4/4 CTest (14/14 QtTest) | ~32k | 8 modular QML tabs, InspectorPanel, BatchOperationDialog, multi-select, search filter, ASan/UBSan clean. |
| 2026-10-24 23:45 | **Ticket-2.3** | Core / Serialization (`PmxCore`) | `pmx_core_engineer` | `6de48c9` | 5/5 CTest (14/14 QtTest) | ~35k | Legacy PMD 1.0 binary importer, Shift-JIS decoding, BDEF2 weights, texture/sphere split, English/physics extensions. |
| 2026-10-25 00:38 | **Ticket-2.4** | Core / Models (`PmxCore` / `PmxModels`) | `pmx_core_engineer` | `44caf36` | 6/6 CTest (15/15 QtTest) | ~35k | Kitbash merger engine, 7-entity index remapping, standard bone deduplication, PMX 2.1 elevation, undoable controller integration. |
| 2026-10-25 01:10 | **Ticket-2.5** | Core / Importer (`PmxCore`) | `pmx_core_engineer` | `c87c34f` | 7/7 CTest (12/12 QtTest) | ~34k | Assimp accessory importer, OBJ/FBX/glTF, LH coordinates, BDEF1 root bone, materials/textures, QA hardened. |

---

## 3. Incident & Regression Log

### Incident 1: Ticket 1.3 Command ID Collision & Double-Apply Bug (Strike 1)
* **Date:** 2026-10-24 11:48 UTC
* **Subsystem:** `PmxModels` (`src/models/PmxUndoCommands.h`)
* **Root Cause:**
  1. `makeCommandId` allocated only 12 bits for row and 12 bits for column (`(row & 0xFFF) << 12`), which wrapped to 0 at row 4096, causing catastrophic command collisions on models with $\ge 4096$ bones/materials.
  2. `PmxPropertyEditCommand::mergeWith()` re-called `applyValue()`, resulting in double setter execution and duplicate signals because Qt's `QUndoStack::push()` already executes `other->redo()`.
  3. `applyBatchOperation` did not restrict operations to numeric columns, corrupting material names with `"0.0"`.
* **Corrective Action:**
  - Lead Architect fed exact failure log back under Strike 1.
  - Refactored `makeCommandId` to 4 bits tableId, 6 bits col, and 21 bits row (supporting 2,097,151 rows).
  - Removed `applyValue()` from `mergeWith()`.
  - Added numeric column validation and safe bounds checks in setter lambdas.
  - QA Re-Audit PASSED; committed at `d811c4b`.

### Incident 2: Ticket 1.4 Prototype Mismatch & Strike 2 Rollback
* **Date:** 2026-10-24 13:01 UTC
* **Subsystem:** `PmxViewport` (`src/viewport/PmxViewportItem.cpp`)
* **Root Cause:**
  1. `pmx_ui_engineer` implemented `void PmxRhiRenderer::buildGrid()` missing the `QRhiCommandBuffer* cb` argument declared in the header, failing compilation.
  2. Test `testTriangleIndexBoundsValidation` was hollow (duplicated loop in test body rather than exercising production code).
* **Corrective Action:**
  - Invoked **Law of Cheap Rollback (The 2-Strike Rule)**: immediately executed `git checkout -- CMakeLists.txt && rm -rf CMakeFiles src/app src/viewport tests/tst_pmx_viewport.cpp` to return to clean commit `d811c4b`.
  - Refined ticket work order with explicit method signatures and static helper requirement (`PmxViewportItem::filterValidTriangleIndices`).
  - Dispatched fresh `pmx_ui_engineer`.
  - During audit, an ASan heap buffer over-read on model size transitions was discovered and patched in-place.
  - QA Audit PASSED; committed at `d6c5d7e`.

### Incident 3: Ticket 1.5 Lifetime / Use-After-Free Hazard in Command Lambdas (Strike 1)
* **Date:** 2026-10-24 18:41 UTC
* **Subsystem:** `PmxMcpServer` (`src/mcp/PmxMcpServer.cpp`)
* **Root Cause:**
  1. Command notify lambdas captured `[this]` (`PmxMcpServer*`). When `PmxMcpServer` was destroyed while `QUndoStack` persisted, calling `undo()` triggered a heap use-after-free under ASan.
  2. `boneIndex: -0.5` was truncated to `0` by `toInt()`, mutating bone 0 instead of returning error `-32602`.
* **Corrective Action:**
  - Captured `QPointer<PmxDocumentController> ctrl = m_controller;` by value instead of `[this]`.
  - Added strict floating-point integer validation (`d == std::floor(d) && d >= 0.0`).
  - Added strict JSON-RPC 2.0 version validation and input type checks.
### Incident 4: Ticket 2.1 JP/EN Name Command ID Collision & Coalescing Defect (Strike 1)
* **Date:** 2026-10-24 22:16 UTC
* **Subsystem:** `PmxModels` (`src/models/`)
* **Root Cause:**
  1. Japanese and English name edits across table models shared identical command IDs (`makeCommandId(tableId, row, ColName)`). Rapid sequential edits caused `QUndoCommand::mergeWith` to coalesce the English edit into the Japanese command, corrupting the undo stack.
  2. Missing input validation for `ColPanel` and `ColMorphType` in `PmxMorphTableModel::setData`.
  3. Missing batch operation unit tests for rigid bodies and joints in `tst_pmx_undo_models.cpp`.
* **Corrective Action:**
  - Distinct column indices/offsets allocated for JP vs EN name command IDs (e.g. `isJp ? ColName : (ColName + 32)` or `cmdId = -1` for non-coalescing text edits).
  - Bounds clamping on morph panel/type enum values.
  - Added finite checks `std::isfinite(newNum)` in batch arithmetic.
  - Added unit tests for RigidBody and Joint batch operations in `tst_pmx_undo_models.cpp`.

---

## 4. Milestone 1 Acceptance & Sign-Off Receipt
* **Release Tag:** `v0.1.0-milestone1`
* **Commit:** `b7fed90`
* **Verification Command:** `ctest --test-dir build --output-on-failure`
* **Test Pass Rate:** 4/4 test suites, 51 test functions, 0 failures, 0 ASan/UBSan errors.
* **Architect Sign-Off:** Milestone 1 Acceptance Gate passed. All target boundaries and invariants verified.

### Incident 5: Ticket 2.5 Vertex Bone Indices Without Root Bone & Corrupt Scene Safety (Strike 1)
* **Date:** 2026-10-25 01:10 UTC
* **Subsystem:** `PmxCore` (`src/core/AssimpImporter.cpp`)
* **Root Cause:**
  1. `createRootBone = false` still assigned `vert.boneIndices[0] = 0` when `doc->bones.empty()` was true, causing vertices to reference non-existent bone index 0.
  2. Hollow test omission in `testWithoutRootBone()` which asserted `doc->bones.empty()` but skipped checking `vert.boneIndices`.
  3. Unprotected null-pointer vectors on corrupt Assimp scenes (`mMeshes`, `mVertices`, `mNormals`, `mFaces`, `mMaterials`).
* **Corrective Action:**
  - Audited and caught by `pmx_qa_adversary`.
  - Replicated failure deterministically with `QCOMPARE(doc->vertices[0].boneIndices[0], -1)`.
  - Updated `AssimpImporter.cpp` to assign `boneIndices = {-1, -1, -1, -1}` and `weights = {0, 0, 0, 0}` when `createRootBone` is disabled.
  - Added full null guards across all Assimp mesh and material properties.
  - Added `testTextureImport()` and `testMultiMeshImport()` test cases.
  - Full suite passed 100% cleanly under ASan/UBSan; committed at `c87c34f`.

---

## 5. Milestone 2 Acceptance & Sign-Off Receipt
* **Release Tag:** `v0.2.0-milestone2`
* **Commit:** `c87c34f`
* **Verification Command:** `ctest --test-dir build --output-on-failure`
* **Test Pass Rate:** 7/7 test suites (107 unit tests), 0 failures, 0 ASan/UBSan errors in 1.32s.
* **Architect Sign-Off:** Milestone 2 (8-Tab Spreadsheet Inspector & Kitbash Merger) Acceptance Gate PASSED.
  - All 8 canonical PMX tabs fully implemented with virtualized table models, batch operations, multi-select, and search/filtering.
  - Multi-format ingestion operational: Lossless PMX 2.0/2.1 reader/writer, Legacy PMD 1.0 binary importer, Assimp accessory importer (.obj, .fbx, .gltf), and non-destructive kitbash merger with 7-entity mathematical index remapping.
  - Ready for Milestone 3 (Full MMD Shader Pipeline & Interactive 3D Edit Mode).

