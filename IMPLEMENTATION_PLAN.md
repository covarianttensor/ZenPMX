# IMPLEMENTATION_PLAN.md — Master Development Roadmap & Completion Level

> **Notice:** This document is maintained strictly by the Lead Architect / Orchestrator.  
> Worker subagents (`pmx_core_engineer`, `pmx_ui_engineer`, `pmx_qa_adversary`, `pmx_devops_engineer`) MUST NOT read or modify this file.

---

## 1. Project Completion Level

```text
================================================================================
[████████████████████████░░░░░░░░░░░░░░░░] 60% COMPLETE
================================================================================
Project: ZenPMX (Native Linux PMX Editor)
Repository: https://github.com/covarianttensor/ZenPMX (Public)
Status: Milestone 1 & 2 VERIFIED & PUSHED (v0.1.0-milestone1, v0.2.0-milestone2)
Active Phase: Milestone 3 (Full MMD Shader Pipeline & Interactive 3D Edit Mode)
```

---

## 2. Milestone Summary Table

| Milestone ID | Title | Est. Tickets | Status | Gate Verdict | Release Tag |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Milestone 1** | **The Tracer Bullet (End-to-End Pipeline)** | 5 | `[DONE]` | **PASS** | `v0.1.0-milestone1` |
| **Milestone 2** | **Complete 8-Tab Spreadsheet Inspector & Kitbash Merger** | 5 | `[DONE]` | **PASS** | `v0.2.0-milestone2` |
| **Milestone 3** | **Full MMD Shader Pipeline & Interactive 3D Edit Mode** | 5 | `[IN_PROGRESS]` | Pending | `v0.3.0-milestone3` |
| **Milestone 4** | **Live TransformView (CCDIK, SDEF & Bullet3 Physics)** | 5 | `[TODO]` | Pending | `v0.4.0-milestone4` |
| **Milestone 5** | **The Top 8 Native C++ Community Power Tools** | 5 | `[TODO]` | Pending | `v0.5.0-milestone5` |
| **Milestone 6** | **External Mono PEPlugin Host & Full MCP Co-Pilot** | 4 | `[TODO]` | Pending | `v1.0.0` |

---

## 3. Milestone 1 Ticket Registry: The Tracer Bullet (`100% COMPLETE`)

| Ticket ID | Title | Target Subsystem | Assigned Subagent | Status | Dependencies | Commit |
| :--- | :--- | :--- | :--- | :---: | :--- | :---: |
| **Ticket 1.1** | Build Harness, Canonical PmxTypes.h & Header/Info Parser | `PmxCore` | `pmx_core_engineer` | `[DONE]` | None | `84d19ee` |
| **Ticket 1.2** | Complete Lossless PMX 2.0/2.1 Binary Reader/Writer | `PmxCore` | `pmx_core_engineer` | `[DONE]` | Ticket 1.1 | `a4e2ce8` |
| **Ticket 1.3** | PmxModels State Layer & Material/Bone Table Models | `PmxModels` | `pmx_core_engineer` | `[DONE]` | Ticket 1.2 | `d811c4b` |
| **Ticket 1.4** | PmxViewport & pmx-editor Tracer UI Shell | `PmxViewport` / `pmx-editor` | `pmx_ui_engineer` | `[DONE]` | Ticket 1.3 | `d6c5d7e` |
| **Ticket 1.5** | PmxMcpServer (pmx-mcp) Tracer Endpoint | `PmxMcpServer` | `pmx_core_engineer` | `[DONE]` | Ticket 1.3 | `b7fed90` |

---

## 4. Milestone 2 Ticket Registry: 8-Tab Inspector & Kitbash Merger (`100% COMPLETE`)

### Overview & Objective
Scale out the spreadsheet data engineering capabilities to all 8 canonical PMX tabs with high-performance virtualization (100,000+ vertices), multi-format import (`.pmd`, `.obj`, `.fbx`, `.gltf`), and non-destructive kitbash merging (`追加選択`) with index remapping.

| Ticket ID | Title | Target Subsystem | Assigned Subagent | Status | Dependencies | Commit |
| :--- | :--- | :--- | :--- | :---: | :--- | :---: |
| **Ticket 2.1** | **Remaining 5 Table Models (Vertex, Morph, Display, RigidBody, Joint)** | `PmxModels` | `pmx_core_engineer` | `[DONE]` | Ticket 1.3 | `9079cf6` |
| **Ticket 2.2** | **Spreadsheet UI Completion: 8 QML Tabs, Multi-Select & Search/Filter** | `PmxModels` / `pmx-editor` | `pmx_ui_engineer` | `[DONE]` | Ticket 2.1 | `f8d69e6` |
| **Ticket 2.3** | **Legacy PMD Binary Importer (`PmdImporter.cpp`)** | `PmxCore` | `pmx_core_engineer` | `[DONE]` | Ticket 1.2 | `6de48c9` |
| **Ticket 2.4** | **Kitbash Merger Engine (`PmxMerger.cpp` — 追加選択)** | `PmxCore` / `PmxModels` | `pmx_core_engineer` | `[DONE]` | Ticket 2.1, Ticket 2.3 | `44caf36` |
| **Ticket 2.5** | **Assimp Accessory Importer (`AssimpImporter.cpp`)** | `PmxCore` | `pmx_core_engineer` | `[DONE]` | Ticket 2.4 | `c87c34f` |

### Detailed Ticket Breakdown for Milestone 2:

#### Ticket 2.1: Remaining 5 Table Models (Vertex, Morph, Display, RigidBody, Joint)
* **Target:** `PmxModels` (`src/models/PmxVertexTableModel.h/.cpp`, `PmxMorphTableModel.h/.cpp`, `PmxDisplayFrameModel.h/.cpp`, `PmxRigidBodyTableModel.h/.cpp`, `PmxJointTableModel.h/.cpp`, `tests/tst_pmx_undo_models.cpp`)
* **Objective:** Implement virtualized `QAbstractTableModel` and `QAbstractListModel` for the remaining 5 tabs. `PmxVertexTableModel` must support virtualized reads for 100k+ rows with sub-millisecond scrolling, exposing positions, normals, UVs, weights (BDEF/SDEF/QDEF), and edge scales. All models must support `QUndoCommand` cell edits, JP/EN language toggling, bounds-safe lambdas, and numeric batch operations.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_undo_models`

#### Ticket 2.2: Spreadsheet UI Completion: 8 QML Tabs, Multi-Select & Search/Filter
* **Target:** `pmx-editor` (`src/app/qml/tabs/`, `src/app/qml/InspectorPanel.qml`)
* **Objective:** Create modular QML views for all 8 inspector tabs (`InfoTab`, `VertexTab`, `MaterialTab`, `BoneTab`, `MorphTab`, `DisplayTab`, `RigidBodyTab`, `JointTab`) embedded inside `InspectorPanel.qml`. Implement multi-row selection, regex name filtering, JP/EN toggle binding, and a batch calculation modal (`+`, `*`, `=`).
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 2.3: Legacy PMD Binary Importer (`PmdImporter.cpp`)
* **Target:** `PmxCore` (`src/core/PmdImporter.h/.cpp`, `tests/tst_pmd_import.cpp`)
* **Objective:** Lossless binary reader for legacy MMD `.pmd` files (v1.0), upgrading them into canonical `PmxDocument` structures. Converts Shift-JIS text strings to `QString` (UTF-8/16LE), normalizes BDEF2 vertex weights, maps PMD material properties and toon textures (`toon01.bmp`..`toon10.bmp`), maps PMD bones and IK chains, and upgrades PMD rigid bodies and joints.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmd_import`

#### Ticket 2.4: Kitbash Merger Engine (`PmxMerger.cpp` — 追加選択)
* **Target:** `PmxCore` / `PmxModels` (`src/core/PmxMerger.h/.cpp`, `tests/tst_pmx_merger.cpp`)
* **Objective:** Non-destructive merging of a secondary PMX model (e.g. outfit, hair, accessory) into an open base PMX model. Offsets all vertex, texture, material, bone, morph, rigid body, and joint indices with 100% mathematical precision. Provides options to merge identically named standard bones (e.g. `全ての親`, `センター`, `下半身`, `上半身`, `首`, `頭`). Returns an undoable `PmxDocumentSnapshotCommand`.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_merger`

#### Ticket 2.5: Assimp Accessory Importer (`AssimpImporter.cpp`)
* **Target:** `PmxCore` (`src/core/AssimpImporter.h/.cpp`, `CMakeLists.txt`, `tests/tst_assimp_import.cpp`)
* **Objective:** Integrates `libassimp-dev` into `CMakeLists.txt` to import standard 3D formats (`.obj`, `.fbx`, `.gltf`). Generates valid `PmxVertex` meshes (triangulating faces, generating smooth/flat normals, generating default BDEF1 weight to root bone), default materials with embedded/external texture path resolution, and adds a root display frame.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_assimp_import`

---

## 5. Milestone 3 Ticket Registry: Full MMD Shader Pipeline & Interactive 3D Edit Mode

### Overview & Objective
Transform the viewport from a flat diffuse renderer into a production-grade MMD render pipeline (`QRhi`), featuring toon ramp shading (`toon01..10.bmp`), sphere maps (`.sph`/`.spa`), inverted-hull outlines with vertex scaling, wireframe visualizers for bones/physics/joints, 3D translation/rotation/scale gizmos, and an interactive weight painting heatmap brush.

| Ticket ID | Title | Target Subsystem | Assigned Subagent | Status | Dependencies | Commit |
| :--- | :--- | :--- | :--- | :---: | :--- | :---: |
| **Ticket 3.1** | **MMD Toon Ramp Shading & Sphere Map (`.sph` / `.spa`) Multi-Pass QRhi Pipeline** | `PmxViewport` | `pmx_ui_engineer` | `[DONE]` | Ticket 2.5 | `2ab3af4` |
| **Ticket 3.2** | **Inverted-Hull Edge Outline Pass with Per-Vertex/Per-Material Width Scaling** | `PmxViewport` | `pmx_ui_engineer` | `[DONE]` | Ticket 3.1 | `d14666f` |
| **Ticket 3.3** | **Interactive 3D Transform Gizmos (Translate/Rotate/Scale) & Symmetric Editing** | `PmxViewport` / `PmxModels` | `pmx_ui_engineer` | `[TODO]` | Ticket 3.2 |
| **Ticket 3.4** | **Wireframe Visualizers for Bones, Rigid Bodies (Sphere/Box/Capsule) & 6DoF Joints** | `PmxViewport` | `pmx_ui_engineer` | `[TODO]` | Ticket 3.3 |
| **Ticket 3.5** | **Interactive Weight Painting Engine with Heatmap Visualization & Radius/Falloff Brush** | `PmxViewport` / `PmxModels` | `pmx_ui_engineer` | `[TODO]` | Ticket 3.4 |

### Detailed Ticket Breakdown for Milestone 3:

#### Ticket 3.1: MMD Toon Ramp Shading & Sphere Map (`.sph` / `.spa`) Multi-Pass QRhi Pipeline
* **Target:** `PmxViewport` (`src/viewport/shaders/pmx_mesh.frag`, `src/viewport/PmxViewportRenderer.cpp`, `src/viewport/PmxTextureCache.h/.cpp`)
* **Objective:** Implement full MMD lighting evaluation in GLSL 450: default MMD directional light ($(-0.5, -1.0, -0.5)$), half-lambert diffuse calculation, 1D toon ramp texture lookup (`toon01.bmp`..`toon10.bmp` embedded fallbacks or material custom toons), and sphere texture mapping (multiplicative `.sph` and additive `.spa` based on view-space reflected normal).
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.2: Inverted-Hull Edge Outline Pass with Per-Vertex/Per-Material Width Scaling
* **Target:** `PmxViewport` (`src/viewport/shaders/pmx_edge.vert`, `src/viewport/shaders/pmx_edge.frag`, `src/viewport/PmxViewportRenderer.cpp`)
* **Objective:** Implement the classic MMD inverted-hull silhouette outline rendering pass. Vertex shader extrudes vertices along their normals scaled by `material.edgeSize * vertex.edgeScale`. Front-face culling is used to render only the back-faces as solid outline geometry tinted by `material.edgeColor`.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.3: Interactive 3D Transform Gizmos (Translate/Rotate/Scale) & Symmetric Editing
* **Target:** `PmxViewport` / `PmxModels` (`src/viewport/PmxGizmo.h/.cpp`, `src/viewport/PmxViewportItem.cpp`)
* **Objective:** Interactive on-screen 3D gizmos for Translation (arrows), Rotation (rings), and Scaling (cubes). Includes screen-space raycasting/picking, world vs. local coordinate frame toggling, numerical snapping, symmetric editing (`対称編集`) mirroring transforms across the $X=0$ plane, and pushing undoable commands to `QUndoStack`.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.4: Wireframe Visualizers for Bones, Rigid Bodies (Sphere/Box/Capsule) & 6DoF Joints
* **Target:** `PmxViewport` (`src/viewport/shaders/pmx_wireframe.vert/.frag`, `src/viewport/PmxWireframeRenderer.cpp`)
* **Objective:** Render bone octahedral wireframes and parent-child connection lines; render Bullet physics rigid body geometric shapes (spheres, boxes, capsules) colored by collision group and type (Static=Blue, Dynamic=Orange, Bone-aligned=Yellow); render 6DoF spring joint coordinate crosses with angular limit arcs.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.5: Interactive Weight Painting Engine with Heatmap Visualization & Radius/Falloff Brush
* **Target:** `PmxViewport` / `PmxModels` (`src/viewport/PmxWeightPaintBrush.h/.cpp`, `src/viewport/shaders/pmx_weight.frag`)
* **Objective:** Viewport weight painting mode. Colors vertices using a classic gradient heatmap (Blue=0.0 $\to$ Cyan $\to$ Green $\to$ Yellow $\to$ Red=1.0) for the selected bone. Interactive spherical brush with configurable radius, strength, and falloff curves (linear/gaussian). Real-time normalization of BDEF2/BDEF4 weights pushed to `PmxDocumentController::undoStack()`.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

---

## 6. Milestone 4 through 6 Roadmap Preview

* **Milestone 4: Live TransformView (CCDIK, SDEF & Bullet3 Physics) (`v0.4.0-milestone4`)**
  - `PmxEvaluator` runtime scene with 60 Hz scheduler.
  - Deform Tier (`変形階層`) + After-Physics (`物理後`) evaluation order.
  - CCDIK solver with Euler angle limits and link hierarchies.
  - Append bone rotation/translation inheritance (`付与`).
  - CPU/GPU SDEF spherical skinning with volume preservation.
  - Real-time `Bullet3` rigid body & 6DoF spring constraint simulation.
  - `.vmd` motion and `.vpd` pose playback.
* **Milestone 5: The Top 8 Native C++ Community Power Tools (`v0.5.0-milestone5`)**
  - Tool 1: Curved Surface Auto-Rigging (`CurvedSurfaceRigGenerator` — `曲面自動設定`).
  - Tool 2: Semi-Standard Bone Generator (`SemiStandardBoneGenerator` — `準標準ボーン追加`).
  - Tool 3: Weight Transfer Engine (`WeightTransferEngine` — `ウェイト転送`).
  - Tool 4: SDEF Auto-Converter (`SdefConverter` — `ボーンウェイトSDEF化`).
  - Tool 5: Bone-Chain Hair/Tail Physics Wizard (`BoneChainPhysicsWizard`).
  - Tool 6: Anime Face Normal Editor (`FaceNormalEditor` — `法線編集`).
  - Tool 7: Multi-Stage Bone Splitter (`多段ボーン化`) & Material Morph Creator.
  - Tool 8: PMX Diagnostics & Crash Validator (`PmxValidator` — 1-click `autoFixAll`).
* **Milestone 6: External Mono PEPlugin Host & Full MCP Co-Pilot (`v1.0.0`)**
  - Clean-room `PEPlugin.dll` & `SlimDX.dll` CPU math shim for Linux Mono.
  - `PmxPluginHost` Unix socket bridge with live modeless WinForms support.
  - Full 10-tool MCP JSON-RPC 2.0 Co-Pilot server for AI assistants.
  - Release packaging (standalone AppImage / binary distribution).

