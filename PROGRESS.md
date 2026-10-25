# PROGRESS.md — Chronological Progress & Incident Tracker

> **Notice:** This document is maintained strictly by the Lead Architect / Orchestrator.  
> Worker subagents (`pmx_core_systems_engineer`, `pmx_ui_engineer`, `pmx_qa_adversary`, `pmx_devops_engineer`) MUST NOT read or modify this file.

---

## 1. Project Status Overview
* **Project Name:** ZenPMX (Native Linux PMX Editor)
* **GitHub Repository:** [`https://github.com/covarianttensor/ZenPMX`](https://github.com/covarianttensor/ZenPMX) (Public)
* **Active Phase:** Milestone 5: The Top 8 Native C++ Community Power Tools
* **Active Target:** Milestone 5 Initialization & Ticket 5.1
* **Overall Completion Level:** 97%
* **Current Working Tree:** Clean (feature commit `7c0ce9a`, release tag `v0.4.0-milestone4`)
* **Test Suite Status:** 8/8 CTest suites passing (179 unit tests, 0 failures, 100% clean under ASan + UBSan in 1.68s)

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

### Milestone 3: Full MMD Shader Pipeline & Interactive 3D Edit Mode (Completed & Tagged `v0.3.0-milestone3`)

| Date / UTC | Ticket ID | Subsystem Target | Subagent | Commit | Test Status | Token Burn | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :---: | :--- |
| 2026-10-25 01:43 | **Ticket-3.1** | Viewport / Shaders (`PmxViewport`) | `pmx_ui_engineer` | `2ab3af4` | 7/7 CTest (19/19 QtTest) | ~42k | MMD toon ramp shading, vertical V sampling, .sph/.spa sphere maps, PmxTextureCache, clamp samplers. (Strike 1 resolved). |
| 2026-10-25 02:04 | **Ticket-3.2** | Viewport / Shaders (`PmxViewport`) | `pmx_ui_engineer` | `d14666f` | 7/7 CTest (22/22 QtTest) | ~38k | Inverted-hull outline pass, per-vertex/material width scaling, CW front-face culling, QSB deserialization test. (Strike 1 resolved). |
| 2026-10-25 02:35 | **Ticket-3.3** | Viewport / Models (`PmxViewport` / `PmxModels`) | `pmx_ui_engineer` | `e72f5d7` | 7/7 CTest (31/31 QtTest) | ~45k | Interactive 3D gizmos (Translate/Rotate/Scale), ray picking, snapping, symmetry, sparse undo commands. (Strike 1 resolved). |
| 2026-10-25 04:18 | **Ticket-3.4** | Viewport / Physics (`PmxViewport`) | `pmx_ui_engineer` | `1b6afef` | 7/7 CTest (36/36 QtTest) | ~44k | Wireframe visualizers for bones (octahedron), rigid bodies (sphere/box/capsule), 6DoF joints (cross/limit arcs/anchors). Strike 1 resolved handedness & selection redraw. |
| 2026-10-25 04:56 | **Ticket-3.5** | Viewport / Models (`PmxViewport` / `PmxModels`) | `pmx_ui_engineer` | `f9a5312` | 7/7 CTest (43/43 QtTest) | ~48k | Interactive weight painting engine, heatmap shaders, radius/falloff brush, multi-bone normalization, sparse undo. Strike 1 resolved test call & encapsulation. |
| 2026-10-25 07:07 | **Ticket-3.6** | Viewport / Shaders (`PmxViewport`) | `pmx_ui_engineer` | `3f175fb` | 7/7 CTest (47/47 QtTest) | ~55k | Regression bugfix: Shaded mesh distortion resolved by fixing pipeline binding order, zero normal guard in mesh.vert, de-hollowed tests. (Strike 1 resolved). |

### Milestone 4: Live TransformView (Completed & Tagged `v0.4.0-milestone4`)

| Date / UTC | Ticket ID | Subsystem Target | Subagent | Commit | Test Status | Token Burn | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :---: | :--- |
| 2026-10-25 14:01 | **Ticket-4.1** | Evaluator (`PmxEvaluator`) | `pmx_core_systems_engineer` | `d149e08` | 8/8 CTest (13/13 QtTest) | ~35k | Runtime scene node graph, deform tier topological sorting, append inheritance, morph accumulators. PASSED on Strike 0. |
| 2026-10-25 14:36 | **Ticket-4.2** | Evaluator (`PmxEvaluator`) | `pmx_core_systems_engineer` | `a242a1b` | 8/8 CTest (19/19 QtTest) | ~38k | MMD CCDIK solver, ZXY Euler decomposition/clamping, knee limits, iteration bounding. PASSED on Strike 0. |
| 2026-10-25 15:22 | **Ticket-4.3** | Evaluator (`PmxEvaluator`) | `pmx_core_systems_engineer` | `5a6101a` | 8/8 CTest (27/27 QtTest) | ~42k | High-Fidelity SDEF spherical volume skinning, BDEF1/2/4/QDEF, morph integration, safe normal guards. (Strike 1 resolved SDEF auxiliary bounds spec violation). |
| 2026-10-25 16:14 | **Ticket-4.4** | Evaluator (`PmxEvaluator`) | `pmx_core_systems_engineer` | `121ea4d` | 8/8 CTest (36/36 QtTest) | ~46k | Bullet3 discrete dynamics, 6DoF spring constraints, kinematic tracking, collision filtering. (Strike 1 resolved PhysicsAndBone translation preservation & ZXY Euler orientation). |
| 2026-10-25 17:06 | **Ticket-4.5** | Evaluator (`PmxEvaluator`) | `pmx_core_systems_engineer` | `8fa77f5` | 8/8 CTest (47/47 QtTest) | ~45k | VMD motion & VPD pose file importer, cubic Bézier spline & quaternion slerp playback engine. (Strike 1 resolved NaN over-read & untrusted buffer bounds checks). |
| 2026-10-25 17:57 | **Ticket-4.6** | Viewport / App (`PmxViewport` / `pmx-editor`) | `pmx_ui_engineer` | `7c0ce9a` | 8/8 CTest (53/53 QtTest) | ~48k | Live TransformView viewport integration, dynamic VBO stream, TimelineBar.qml. (Strike 1 resolved QML read-only property & rest-pose material sync). |

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

### Incident 6: Ticket 3.1 Sampler Inversion, Toon Orientation & Shared Index Collision (Strike 1)
* **Date:** 2026-10-25 01:33 UTC
* **Subsystem:** `PmxViewport` (`src/viewport/shaders/mesh.frag`, `PmxTextureCache.cpp`)
* **Root Cause:**
  1. Binding inversion: Binding 2 was sphere and Binding 3 was toon, deviating from MASTER_SPECIFICATION.md §6.2.
  2. Horizontal vs Vertical toon sampling: toon textures are vertical strips sampled along V; horizontal sampling caused flat constant shading across lighting angles.
  3. Shared toon index off-by-one (`idx = idx - 1`) caused collisions for preset 0 and 1, making preset 9 unreachable.
  4. Unnormalized view normals in `mesh.vert` and missing backface normal inversion for double-sided materials.
* **Corrective Action:**
  - QA adversary detected all defects and issued Strike 1 rejection.
  - `pmx_ui_engineer` remediated all 6 items: synchronized bindings (1=Diffuse, 2=Toon, 3=Sphere), sampled toon vertically at `vec2(0.5, toonV)`, direct mapped `0..9` to presets `0..9`, normalized view normals, inverted backface normals (`if (!gl_FrontFacing) N = -N;`), and applied `QRhiSampler::ClampToEdge`.
  - Non-hollow unit tests added in `tst_pmx_viewport.cpp`.
  - QA Re-Audit PASSED; committed at `2ab3af4`.

### Incident 7: Ticket 3.2 Audit Deficiencies & Hollow Test Assertions (Strike 1)
* **Date:** 2026-10-25 02:21 UTC
* **Subsystem:** `PmxViewport` (`src/viewport/PmxViewportItem.cpp`, `tests/tst_pmx_viewport.cpp`)
* **Root Cause:**
  1. `testShaderResourcesLoaded()` asserted `size() > 0` on QSB files rather than deserializing via `QShader::fromSerialized()` and verifying `shader.isValid()`.
  2. Missing negative `edgeScale` test coverage in `testEdgeExtrusionMath` and `testMeshVertexEdgeScalePropagation`.
  3. Missing zero `faceVertexCount` edge case on materials with `drawFlags & 0x10 != 0`.
  4. Missing non-finite checks `std::isfinite(mat.edgeSize)` in `partitionMaterialBatches`.
* **Corrective Action:**
  - Lead Architect fed exact failure log back to `pmx_ui_engineer` under Strike 1.
  - Subagent instructed to deserialize shaders into `QShader`, add negative edgeScale assertions, test zero-vertex edge materials, and add finite guards.
### Incident 8: Ticket 3.3 GLM Experimental & PmxBone Member Name Errors (Strike 1)
* **Date:** 2026-10-25 02:52 UTC
* **Subsystem:** `PmxViewport` / `tests` (`tests/tst_pmx_viewport.cpp`)
* **Root Cause:**
  1. `glm/gtx/...` experimental extensions included without `#define GLM_ENABLE_EXPERIMENTAL`.
  2. Member access error: `PmxBone` has `nameJp` / `nameEn`, but test accessed `b.name`.
* **Corrective Action:**
  - Lead Architect fed exact compiler diagnostic back to `pmx_ui_engineer` under Strike 1.
  - Subagent instructed to define `GLM_ENABLE_EXPERIMENTAL` and use `nameJp`.

### Incident 9: Ticket 3.3 Snapping Non-Finite Vulnerability & Boundary Test Gaps (Strike 1)
* **Date:** 2026-10-25 02:59 UTC
* **Subsystem:** `PmxViewport` (`src/viewport/PmxGizmo.cpp`, `tests/tst_pmx_viewport.cpp`)
* **Root Cause:**
  1. `m_translationSnap > 1e-4f` lacked `std::isfinite()` check, causing `Inf` snapping to evaluate to `0.0 * Inf = NaN`.
  2. Boundary test cases (zero/negative screen dimensions, scale = 0, Mode None, snap = Inf, out-of-bounds indices, null controller, `mergeWith` rejection) were omitted.
* **Corrective Action:**
  - Lead Architect fed exact audit report back to `pmx_ui_engineer` under Strike 1.
  - Subagent instructed to add `std::isfinite()` guards and expand test suite with all mandated boundary assertions.

### Incident 10: Ticket 3.4 Joint Handedness Inversion, Viewport Selection Redraw & Hollow Tests (Strike 1)
* **Date:** 2026-10-25 04:05 UTC
* **Subsystem:** `PmxViewport` (`src/viewport/PmxWireframeRenderer.cpp`, `src/viewport/PmxViewportItem.cpp`, `tests/tst_pmx_viewport.cpp`)
* **Root Cause:**
  1. Handedness inversion omitted for joint rotational limits in `PmxWireframeRenderer.cpp`: in RH coordinates, angular limits on X and Y must invert bounds ($\min X_{RH} = -\max X_{LH}$, $\max X_{RH} = -\min X_{LH}$, and similarly for Y).
  2. Viewport selection redraw omitted for rigid bodies and joints in `PmxViewportItem::updateGizmoPosition()`.
  3. Hollow tests in `tests/tst_pmx_viewport.cpp`: zero Euler rotations tested, missing exact bone diamond base coordinates, missing sphere radius points, box corners, capsule apexes, and asymmetric joint limit checks.
* **Corrective Action:**
  - Lead Architect fed exact audit report back to fresh `pmx_ui_engineer` under Strike 1.
  - Subagent instructed to fix handedness inversion on X and Y joint limits, trigger viewport redraw on all selection changes, and de-hollow the unit test suite.

### Incident 11: Ticket 3.5 Test Harness Compilation Error & Encapsulation Hardening (Strike 1)
* **Date:** 2026-10-25 04:54 UTC
* **Subsystem:** `PmxViewport` / `PmxModels` (`src/models/PmxDocumentController.h`, `src/viewport/PmxWeightPaintBrush.cpp`, `tests/tst_pmx_viewport.cpp`)
* **Root Cause:**
  1. Test harness called non-existent `controller.setDocument(doc)` instead of `controller.setDocumentInternal(doc)`.
  2. `PmxDocumentController` lacked `friend class PmxVertexWeightCommand;` declaration.
  3. Missing explicit `v.boneWeights[1]` initialization before BDEF4 elevation in `PmxWeightPaintBrush.cpp`.
* **Corrective Action:**
  - In strict accordance with Golden Law 5 (The 2-Strike Rule), worker subagent was NOT killed.
  - Architect dispatched targeted remediation feedback to the same worker instance via `send_message`.
  - Worker resolved all 3 items; QA verified and committed at `f9a5312`.

### Incident 12: Ticket 3.6 Shaded Mesh Distortion & Order of Operations (Strike 1)
* **Date:** 2026-10-25 06:25 UTC
* **Subsystem:** `PmxViewport` (`src/viewport/PmxViewportItem.cpp`, `src/viewport/shaders/mesh.vert`, `tests/tst_pmx_viewport.cpp`)
* **Root Cause:**
  1. Pipeline binding order defect in `PmxViewportItem.cpp`: `cb->setVertexInput` was invoked *before* `cb->setGraphicsPipeline(pipe)` in the shaded pass, and was omitted entirely in the edge pass. On OpenGL Core (`QRhiGles2`), switching pipelines binds a new VAO without the vertex buffer/attribute bindings, corrupting vertex fetches and stretching the right half of the mesh across the viewport.
  2. Degenerate normal division by zero in `src/viewport/shaders/mesh.vert` line 39: `normalize(mat3(u_normalMatrix) * inNormal)` causes `NaN` propagation on zero-length normals.
  3. Textbook hollow test in `tests/tst_pmx_viewport.cpp`: `testBarbaraModelLoadingAndDiagnostics` asserted CPU staging floats without performing offscreen GPU render verification.
### Incident 13: Ticket 4.3 SDEF Directional Bounds Spec Violation & Strike 1 Remediation
* **Date:** 2026-10-25 15:06 UTC
* **Subsystem:** `PmxEvaluator` (`src/evaluator/PmxVertexDeformer.cpp`, `tests/tst_pmx_evaluator.cpp`)
* **Root Cause:**
  1. Mathematical defect in `PmxVertexDeformer::deformSdef`: Directional bounds calculation used `deltaR = (rTotal - rRest) * 0.5f` with unrotated `rRest`, violating rigid body rotation invariance and causing spurious mesh tearing under limb rotations.
  2. Hollow test omission in `tests/tst_pmx_evaluator.cpp`: SDEF tests previously evaluated only zero directional bounds ($R_0, R_1 = 0$) or rest pose ($\mathbf{M} = \mathbf{I}$), allowing the defect to pass CI.
* **Corrective Action:**
  - In strict accordance with Golden Law 5 (The 2-Strike Rule), worker subagent `154ce97d-6dd0-4c92-9360-9520dd3359ea` was NOT killed.
  - Lead Architect dispatched exact QA audit logs and remediation directives to the active worker via `send_message`.
  - Worker corrected the formula to canonical spec:
    $$\mathbf{r}_0 = \text{mat3}(\mathbf{M}_{b0}) \mathbf{R}_0 - \mathbf{R}_{\text{sdef}} \mathbf{R}_0, \quad \mathbf{r}_1 = \text{mat3}(\mathbf{M}_{b1}) \mathbf{R}_1 - \mathbf{R}_{\text{sdef}} \mathbf{R}_1, \quad \Delta \mathbf{r} = w_0 \mathbf{r}_0 + w_1 \mathbf{r}_1$$
    and added two non-hollow tests (`testSdefRigidRotationInvariance` and `testSdefDifferentialRotationWithBounds`).
  - Adversarial QA Re-Audit PASSED; committed at `5a6101a`.

### Incident 14: Ticket 4.4 PhysicsAndBone Translation & ZXY Euler Orientation (Strike 1)
* **Date:** 2026-10-25 16:01 UTC
* **Subsystem:** `PmxEvaluator` (`src/evaluator/PmxPhysicsEngine.cpp`, `tests/tst_pmx_evaluator.cpp`)
* **Root Cause:**
  1. Spec violation: `syncDynamicBones` overwrote translation for `physicsMode == 2` instead of preserving parent hierarchy translation while applying only physics rotation.
  2. Coordinate handedness / Euler order mismatch in `createBtTransform`: `setEulerZYX` used intrinsic ZYX / extrinsic XYZ instead of canonical extrinsic $ZXY$ ($R = R_y R_x R_z$).
  3. Move assignment operator lacked safe `clear()` teardown before moving member unique pointers.
  4. Collision shape size dimensions lacked clamping against non-positive floats.
* **Corrective Action:**
  - In strict accordance with Golden Law 5 (The 2-Strike Rule), worker subagent `352b7f9d-b69b-477a-a0f4-33c613871087` was NOT killed.
  - Lead Architect dispatched exact QA audit logs and remediation directives to the active worker via `send_message`.
  - Worker resolved all items: preserved translation on `boneWorld[3]` for mode 2, constructed canonical $ZXY$ quaternion `qy * qx * qz`, implemented custom move assignment calling `clear()`, clamped dimensions with `std::max(dim, 1e-4f)`, and added non-hollow tests `testBulletPhysicsAndBoneMode` and `testBulletCompoundEulerRotation`.
  - Adversarial QA Re-Audit PASSED; committed at `121ea4d`.

### Incident 15: Ticket 4.5 NaN Heap Iterator Read & Untrusted Buffer Allocation (Strike 1)
* **Date:** 2026-10-25 16:55 UTC
* **Subsystem:** `PmxEvaluator` (`src/evaluator/PmxAnimationPlayer.cpp`, `src/evaluator/VmdImporter.cpp`, `tests/tst_pmx_evaluator.cpp`)
* **Root Cause:**
  1. `std::upper_bound` on track keyframes with `frame = NaN` returned `track.end()`, dereferencing an out-of-bounds heap element under ASan.
  2. Untrusted counts (`boneCount`, `morphCount`, `ikCount`) in VMD 2.0 importer called `.reserve(count)` without verifying available buffer size, exposing memory exhaustion DOS on corrupted files.
  3. Desynchronization on truncated camera/light/shadow sub-records.
* **Corrective Action:**
  - In strict accordance with Golden Law 5 (The 2-Strike Rule), worker subagent was NOT killed.
  - Lead Architect dispatched exact QA audit logs and remediation directives to the active worker via `send_message`.
  - Worker resolved all items: added `std::isnan(frame)` guards in `setFrame()`, `evaluateBezier()`, and track search routines (`it == track.end()` and `it == track.begin()` fallback); added division-based bounds checking before all `.reserve()` calls in `VmdImporter.cpp`; added `testAnimationPlayerNanHandling` and `testVmdMalformedBufferRejection`.
  - Adversarial QA Re-Audit PASSED; committed at `8fa77f5`.

### Incident 16: Ticket 4.6 QML Read-Only implicitWidth & Rest-Pose Material Sync (Strike 1)
* **Date:** 2026-10-25 17:47 UTC
* **Subsystem:** `pmx-editor` / `PmxViewport` (`src/app/qml/TimelineBar.qml`, `src/viewport/PmxViewportItem.cpp`, `tests/tst_pmx_viewport.cpp`)
* **Root Cause:**
  1. `TimelineBar.qml` assigned `implicitWidth: 50` on `Label`, which is read-only in Qt 6 Quick Controls, causing `QQmlApplicationEngine failed to load component` and crashing `pmx-editor` on launch with exit code 255.
  2. In `PmxViewportItem::synchronize()`, `m_stagedMaterials` was only re-partitioned on `docDirty || m_stagedMaterials.empty()`. When `transformViewMode` was toggled off, `m_stagedMaterials` retained mutated material morph parameters indefinitely.
  3. Missing test coverage for material morph synchronization and rest-pose restoration on mode exit.
* **Corrective Action:**
  - In strict accordance with Golden Law 5 (The 2-Strike Rule), worker subagent was NOT killed.
  - Lead Architect dispatched exact QA audit logs and remediation directives to the active worker via `send_message`.
  - Worker resolved all items: used `Layout.preferredWidth` in `TimelineBar.qml`, added `transformViewChanged` trigger in `synchronize()` line 349, and added `testTransformViewMaterialMorphSyncAndRestoration` in `tests/tst_pmx_viewport.cpp`.
  - Adversarial QA Re-Audit PASSED; committed at `7c0ce9a`.

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

---

## 6. Milestone 3 Acceptance & Sign-Off Receipt
* **Release Tag:** `v0.3.0-milestone3`
* **Commit:** `f9a5312`
* **Verification Command:** `ctest --test-dir build --output-on-failure`
* **Test Pass Rate:** 7/7 test suites (126 unit tests), 0 failures, 0 ASan/UBSan errors in 1.32s.
* **Architect Sign-Off:** Milestone 3 (Full MMD Shader Pipeline & Interactive 3D Edit Mode) Acceptance Gate PASSED.
  - Production-grade MMD render pipeline (`QRhi`): Toon ramp shading, `.sph`/`.spa` sphere maps, inverted-hull silhouette edge outlines with per-vertex scaling.
  - Interactive 3D Edit Mode: 3D translate/rotate/scale gizmos, raycast picking, numerical snapping, symmetric editing across $X=0$.
  - Wireframe Visualizers: 3D octahedral bone pyramids, Bullet3 rigid bodies (Sphere, Box, Capsule) color-coded by physics mode, 6DoF spring joint coordinate crosses, limit arcs, and body connection lines.
  - Weight Painting Engine: Continuous heatmap gradient shader, 3D spherical falloff brush (Linear, Gaussian, Constant), multi-bone weight normalization, sparse reversible undo commands.
  - Ready for Milestone 4 (Live TransformView: CCDIK, SDEF & Bullet3 Physics).

---

## 7. Milestone 4 Acceptance & Sign-Off Receipt
* **Release Tag:** `v0.4.0-milestone4`
* **Commit:** `7c0ce9a`
* **Verification Command:** `ctest --test-dir build --output-on-failure`
* **Test Pass Rate:** 8/8 test suites (179 unit tests), 0 failures, 0 ASan/UBSan errors in 1.68s.
* **Architect Sign-Off:** Milestone 4 (Live TransformView: CCDIK, SDEF & Bullet3 Physics) Acceptance Gate PASSED.
  - Headless `PmxEvaluator` static runtime engine with deform tier topological sorting, append bone inheritance (`付与`), and morph weight accumulators.
  - Analytical MMD CCDIK constraint solver with Euler ZXY decomposition, knee angle limits, and iteration bounding.
  - High-fidelity SDEF spherical skinning and BDEF1/2/4 vertex deformer with zero rigid-body distortion and safe normal guards.
  - Real-time 60 Hz Bullet3 discrete dynamics simulation with 6DoF spring constraints, kinematic bone tracking, and `PhysicsAndBone` mode.
  - Lossless VMD 2.0 motion and VPD ASCII pose file importers with cubic Bézier spline interpolation and quaternion slerp.
  - Live TransformView viewport integration with dynamic GPU VBO streaming, dynamic bone and physics wireframes, and interactive QML `TimelineBar` transport controls.
  - Ready for Milestone 5 (The Top 8 Native C++ Community Power Tools).

