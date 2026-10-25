# WORK_ORDER_LOG.md — Master Work Order & Ticket Revision Ledger

> **Notice:** This document is maintained strictly by the Lead Architect / Orchestrator.  
> Worker subagents (`pmx_core_systems_engineer`, `pmx_ui_engineer`, `pmx_qa_adversary`, `pmx_devops_engineer`) MUST NOT read or modify this file.

---

## Milestone 1: The Tracer Bullet

### Ticket 1.1: Build Harness, Canonical PmxTypes.h, Minimal PMX Header/Info Parser & Headless Sanity Test
* **Assigned Subagent:** `pmx_core_engineer`
* **Target Subsystem:** Build System & `PmxCore` (Target 1)
* **Date Issued:** 2026-10-24 10:47 UTC
* **Status:** `[DONE]` (Committed at `84d19ee`)
* **Specification:**
  - Create root `CMakeLists.txt` configuring C++20, `Qt6::Core`, `Qt6::Test`, `glm`, and AddressSanitizer/UndefinedBehaviorSanitizer flags in Debug builds.
  - Implement `src/core/PmxTypes.h` & `src/core/PmxDocument.h` with exact canonical C++20 structs in `namespace pmx`.
  - Implement `PmxReader` and `PmxWriter` Little-Endian stream primitives, variable-width index helpers obeying unsigned vertex vs. signed non-vertex index laws (`-1` preservation), UTF-16LE/UTF-8 codecs, and header/modelInfo parser/serializer.
  - Implement `tests/tst_pmx_roundtrip.cpp` verifying byte-for-byte round-trips and adversarial header fuzzing.
* **Verification Command:** `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`

---

### Ticket 1.2: Complete PmxCore Lossless Binary Reader/Writer (PMX 2.0 & 2.1) & Round-Trip Suite
* **Assigned Subagent:** `pmx_core_engineer`
* **Target Subsystem:** `PmxCore` (Target 1)
* **Date Issued:** 2026-10-24 11:07 UTC
* **Status:** `[DONE]` (Committed at `a4e2ce8`)
* **Specification:**
  - Enforce minimum wire-byte bounds checking (`checkCountBounds`) prior to every `vector::resize()` / `reserve()` across all 10 PMX sections and nested arrays to prevent OOM on `INT32_MAX` counts.
  - Complete 100% lossless binary IO for all 10 PMX sections: Vertices (`BDEF1/2/4`, `SDEF`, `QDEF`), Faces, Textures, Materials (with shared toon branching), Bones (all 6 flag blocks), Morphs (all 11 types, including 28-float material morphs), Display Frames, Rigid Bodies, Joints, and PMX 2.1 SoftBodies.
  - Expand `tests/tst_pmx_roundtrip.cpp` to verify full-model PMX 2.0 and PMX 2.1 byte-identical round-trips, US-1 surgical single-field edit isolation, and mid-record stream truncation fuzzing.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_roundtrip`

---

### Ticket 1.3: PmxModels State Layer — PmxDocumentController, QUndoStack & Material/Bone Table Models
* **Assigned Subagent:** `pmx_core_engineer`
* **Target Subsystem:** `PmxModels` (Target 3)
* **Date Issued:** 2026-10-24 11:30 UTC
* **Status:** `[DONE]` (Committed at `d811c4b`)
* **Original Specification:**
  - Add `Qt6::Gui` to `CMakeLists.txt`, define `PmxModels` static library (strictly isolated from Qt Quick), and add `tst_pmx_undo_models`.
  - Implement `PmxSelectionManager`, `PmxUndoCommands` (`PmxPropertyEditCommand`, `PmxDocumentSnapshotCommand`), `PmxDocumentController` (encapsulating mutable `PmxDocument` behind `QUndoStack`), and virtualized table models (`PmxInfoModel`, `PmxMaterialTableModel`, `PmxBoneTableModel`) with JP/EN toggle and batch math.
  - Implement `tests/tst_pmx_undo_models.cpp`.

#### Ticket 1.3 — Revision 2 (Strike 1 Fix)
* **Date & Timestamp:** 2026-10-24 11:48 UTC
* **Reason for Revision:**
  - Strike 1 audit failure. The QA Inspector detected:
    1. `makeCommandId` allocated only 12 bits for row, wrapping to 0 for row $\ge 4096$ and corrupting multi-thousand bone/material models.
    2. `PmxPropertyEditCommand::mergeWith()` double-executed setters and signals because Qt already runs `other->redo()`.
    3. Batch math on string columns corrupted material names/memos with `"0.0"`.
    4. Missing `applyBatchOperation` in `PmxBoneTableModel`.
* **Revision Directives:**
  - Reallocate `makeCommandId`: 4 bits tableId, 6 bits col, 21 bits row (supporting 2,097,151 rows).
  - Remove `applyValue()` from `mergeWith()`.
  - Restrict `applyBatchOperation` to numeric columns, reject `NaN`/`Inf`, and return false on no-op deltas.
  - Implement `applyBatchOperation` on `PmxBoneTableModel`.
  - Add safe vector bounds guards in setter lambdas.
  - Add unit tests for command ID non-collision at row 4096, batch math protection, and out-of-bounds safety.

---

### Ticket 1.4: PmxViewport & pmx-editor Tracer UI — Unified QML Shell, Orbital Camera & 3D Mesh/Bone Overlay Renderer
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystems:** `PmxViewport` (Target 4) & `pmx-editor` (Target 8)
* **Date Issued:** 2026-10-24 12:06 UTC
* **Status:** `[DONE]` (Committed at `d6c5d7e`)
* **Original Specification:**
  - Update `CMakeLists.txt` for `Qt6::Quick`, `QuickControls2`, `ShaderTools`, defining `PmxViewport` and `pmx-editor`.
  - Implement `PmxCamera` (orbital math, projection matrices, LH $\to$ RH coordinate conversion).
  - Implement `PmxViewportItem` on `QQuickRhiItem` with GLSL 450 mesh and line shaders.
  - Implement `Main.qml` unified workspace and `tst_pmx_viewport.cpp`.

#### Ticket 1.4 — Revision 2 (Strike 2 Rollback & Fresh Subagent Dispatch)
* **Date & Timestamp:** 2026-10-24 13:01 UTC
* **Reason for Revision:**
  - Strike 2 failure. The worker introduced a prototype mismatch on `void PmxRhiRenderer::buildGrid()` missing the `QRhiCommandBuffer* cb` argument, failing compilation. Additionally, the test suite duplicated filtering logic in the test body instead of exercising production code, mouse panning was inverted on both axes, and `undoStack()->indexChanged` was not connected to viewport updates.
  - In accordance with the **2-Strike Compute Governor**, the Architect immediately rolled back the working tree (`git checkout .`) to clean commit `d811c4b` and prepared a refined work order.
* **Revision Directives:**
  - Enforce `void buildGrid(QRhiCommandBuffer* cb)` prototype match in header and `.cpp`.
  - Declare public static helper `PmxViewportItem::filterValidTriangleIndices` and call it from production `synchronize()` and tests.
  - Correct mouse panning: `m_camera->pan(dx, -dy)` and reset `m_isPanning = false;` on `Qt::LeftButton` release.
  - Implement bidirectional handedness conversions (`toLeftHandedEuler`, `toLeftHandedQuat`).
  - Connect `undoStack()->indexChanged` to `modelRevision` in `PmxViewportItem::setController`.
  - Require `ShaderTools` in `CMakeLists.txt`.
  - Fresh worker dispatched; QA audit patched a heap buffer over-read; re-audit PASSED.

---

### Ticket 1.5: PmxMcpServer (pmx-mcp) Tracer Endpoint — get_model_summary & rename_bone via QUndoStack
* **Assigned Subagent:** `pmx_core_engineer`
* **Target Subsystem:** `PmxMcpServer` (Target 7)
* **Date Issued:** 2026-10-24 18:21 UTC
* **Status:** `[DONE]` (Committed at `b7fed90`)
* **Original Specification:**
  - Update `CMakeLists.txt` for `Qt6::Network`, defining `PmxMcpServer`, `pmx-mcp`, and `tst_pmx_mcp`.
  - Implement MCP JSON-RPC 2.0 protocol handler with `initialize`, `tools/list`, and `tools/call` for `get_model_summary` and undoable `rename_bone`.
  - Support direct headless in-memory mode and live GUI socket mode.
  - Implement `tst_pmx_mcp.cpp`.

#### Ticket 1.5 — Revision 2 (Strike 1 Fix)
* **Date & Timestamp:** 2026-10-24 18:41 UTC
* **Reason for Revision:**
  - Strike 1 audit failure. The QA Inspector detected:
    1. Severe use-after-free hazard: notify lambdas captured `[this]` (`PmxMcpServer*`). Calling `undo()` after the server was destroyed crashed under ASan.
    2. Fractional negative index bypass: `-0.5` truncated to `0`, mutating bone 0.
    3. Missing JSON-RPC 2.0 `"jsonrpc": "2.0"` validation.
    4. Non-string name parameters coerced to empty strings instead of error `-32602`.
    5. `main_mcp.cpp` unhandled `std::stoi` exceptions on oversized/invalid headers.
* **Revision Directives:**
  - Capture `QPointer<PmxDocumentController> ctrl = m_controller;` by value in notify lambdas.
  - Validate `d == std::floor(d) && d >= 0.0 && d < bones.size()`.
  - Validate `nameJp` / `nameEn` are strings.
  - Return `-32600` if `jsonrpc` is missing or not `"2.0"`.
  - Wrap `std::stoi` in `try-catch` with $[0, 64\text{MB}]$ bounds checking.
  - Add `testUafLifetimeAfterServerDestruction` and socket mutation tests.
  - QA Re-Audit PASSED; committed at `b7fed90`.

---

## Milestone 2: 8-Tab Spreadsheet Inspector & Kitbash Merger

### Ticket 2.1: Remaining 5 Table Models (Vertex, Morph, DisplayFrame, RigidBody, Joint)
* **Assigned Subagent:** `pmx_core_engineer`
* **Target Subsystem:** `PmxModels` (Target 3)
* **Date Issued:** 2026-10-24 21:57 UTC
* **Status:** `[DONE]` (Committed at `9079cf6`)
* **Specification:**
  - Implement `PmxVertexTableModel` (`tableId = 4`): Virtualized reads for 100k+ vertices exposing PosX/Y/Z, NormX/Y/Z, UV U/V, WeightType (BDEF1/2/4, SDEF, QDEF), Bones (0..3), Weights (0..3), EdgeScale. Cell edit routing via `PmxPropertyEditCommand`, batch math operations on numeric columns, dynamic JP/EN header switching.
  - Implement `PmxMorphTableModel` (`tableId = 5`): Exposing Name (JP/EN), Panel category (System, Eyebrow, Eye, Lip, Other), Morph Type (Group, Vertex, Bone, UV, ExtUV1..4, Material, Flip, Impulse), and sub-item offset count. Cell edit routing via `PmxPropertyEditCommand`, dynamic JP/EN header switching.
  - Implement `PmxDisplayFrameModel` (`tableId = 6`): Exposing Frame Name (JP/EN), Special flag (0: Normal, 1: Special/Root), and frame item count.
  - Implement `PmxRigidBodyTableModel` (`tableId = 7`): Exposing Name (JP/EN), BoneIndex, Group (0..15), Mask, Shape (Sphere, Box, Capsule), Size X/Y/Z, Pos X/Y/Z, Rot X/Y/Z, Mass, Linear/Angular Damping, Restitution, Friction, PhysicsMode (Static, Dynamic, Dynamic+BoneAlign). Cell edit routing via `PmxPropertyEditCommand`, batch math operations on numeric columns.
  - Implement `PmxJointTableModel` (`tableId = 8`): Exposing Name (JP/EN), JointType, RigidBody A/B, Pos X/Y/Z, Rot X/Y/Z, Pos Limits Min/Max X/Y/Z, Rot Limits Min/Max X/Y/Z, Spring Pos/Rot X/Y/Z. Cell edit routing via `PmxPropertyEditCommand`, batch math operations on numeric columns.
  - Expose `vertexModel()`, `morphModel()`, `displayFrameModel()`, `rigidBodyModel()`, and `jointModel()` on `PmxDocumentController` and update `resetAllModels()`.
  - Update `CMakeLists.txt` adding all new headers/sources to `PmxModels`.
  - Expand `tests/tst_pmx_undo_models.cpp` with comprehensive test cases verifying undo/redo, dynamic JP/EN toggling, bounds safety, no-op rejection, and numeric batch math across all 5 models.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_undo_models`

#### Ticket 2.1 — Revision 2 (Strike 1 Fix)
* **Date & Timestamp:** 2026-10-24 22:18 UTC
* **Reason for Revision:**
  - Strike 1 audit failure. The QA Inspector detected:
    1. Command ID collision / unwanted coalescing between Japanese and English name edits across table models. Both used `makeCommandId(tableId, row, ColName)` (`col = 0`). Sequential edits of Japanese and English names coalesced into a single command under `QUndoStack`, dropping the English edit upon `undo()`.
    2. Missing input validation on `ColPanel` (`[0, 4]`) and `ColMorphType` (`[0, 10]`) in `PmxMorphTableModel::setData`.
    3. Missing batch arithmetic finite check (`std::isfinite(newNum)`) in `PmxRigidBodyTableModel` and `PmxJointTableModel`.
    4. Missing unit tests for rigid body and joint batch operations in `tst_pmx_undo_models.cpp`.
* **Revision Directives:**
  - In all table models (`PmxMaterialTableModel`, `PmxBoneTableModel`, `PmxMorphTableModel`, `PmxDisplayFrameModel`, `PmxRigidBodyTableModel`, `PmxJointTableModel`), ensure name edit commands cannot collide across languages by passing distinct column identifiers (e.g., `isJp ? ColName : (ColName + 32)`) or passing `cmdId = -1` for `QString` name edits so they never coalesce.
  - Add input validation for `ColPanel` and `ColMorphType` in `PmxMorphTableModel::setData`.
  - Add `std::isfinite(newNum)` checks in batch operations for rigid bodies and joints.
  - Add explicit unit tests for rigid body and joint batch operations in `tst_pmx_undo_models.cpp`.

---

### Ticket 2.2: Spreadsheet UI Completion: 8 QML Tabs, Multi-Select & Search/Filter
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystem:** `PmxModels` / `pmx-editor` (Target 8)
* **Date Issued:** 2026-10-24 22:35 UTC
* **Status:** `[DONE]` (Committed at `f8d69e6`)
* **Specification:**
  - Expand `PmxAppBridge` (`src/app/PmxAppBridge.h/.cpp`) exposing all 8 models as QML properties (`infoModel`, `materialModel`, `boneModel`, `vertexModel`, `morphModel`, `displayFrameModel`, `rigidBodyModel`, `jointModel`). Add `Q_INVOKABLE bool applyBatchOperation(int tableId, const QList<int>& rows, int column, int operation, double operand);`.
  - Create modular QML spreadsheet views in `src/app/qml/tabs/`:
    - `InfoTab.qml`, `VertexTab.qml`, `MaterialTab.qml`, `BoneTab.qml`, `MorphTab.qml`, `DisplayFrameTab.qml`, `RigidBodyTab.qml`, `JointTab.qml`.
  - Create `src/app/qml/InspectorPanel.qml`:
    - TabBar for all 8 canonical tabs with JP/EN text binding.
    - Search/Filter bar for name filtering.
    - Multi-row selection tracking.
    - Batch operation trigger button opening `BatchOperationDialog.qml`.
  - Create `src/app/qml/BatchOperationDialog.qml`:
    - Modal dialog allowing users to choose column, operation (`+`, `-`, `*`, `=`), and numeric operand, invoking `bridge.applyBatchOperation`.
  - Update `src/app/qml/Main.qml` to host the modular `InspectorPanel.qml`.
  - Update `CMakeLists.txt` `qt_add_qml_module` with all new `.qml` files.
  - Expand `tests/tst_pmx_viewport.cpp` (or add test cases) verifying `PmxAppBridge` model accessors, batch operation dispatching, and language toggle bindings.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

---

### Ticket 2.3: Legacy PMD Binary Importer (`PmdImporter.cpp`)
* **Assigned Subagent:** `pmx_core_engineer`
* **Target Subsystem:** `PmxCore` (Target 1)
* **Date Issued:** 2026-10-24 23:25 UTC
* **Status:** `[DONE]` (Committed at `6de48c9`)
* **Specification:**
  - Implement `src/core/PmdImporter.h` and `src/core/PmdImporter.cpp` reading legacy MMD `.pmd` v1.0 binary files and upgrading them into canonical `PmxDocument` structures.
  - Enforce minimum wire-byte bounds checking (`checkCountBounds`) prior to every vector allocation across all PMD sections.
  - Verify magic bytes `"Pmd"` and version `1.0f`.
  - Decode Shift-JIS text strings (model names, comments, bone/morph names) using `QStringConverter::Encoding::ShiftJIS` to `QString`.
  - Convert PMD vertices: positions, normals, UVs, 2-bone indices, and 1-byte bone weight (0..100) normalized into floating-point `BDEF2` ($w_0 = w / 100.0f$, $w_1 = 1.0f - w_0$).
  - Convert PMD materials: diffuse, specular, ambient, shininess, toon index (`toon01.bmp`..`toon10.bmp`), edge flag, face vertex counts, and parse texture/sphere paths (delimited by `*`).
  - Convert PMD bones, IK chains (mapping target, loop iterations, angle limits, links), morphs (extracting base morph vertex indices and mapping skin offsets), and display frames.
  - Parse optional PMD English extension (English model name, comments, bone/skin/group names) if present.
  - Parse optional PMD Bullet Physics extension (rigid bodies and 6DoF spring constraints) if present.
  - Update `CMakeLists.txt` adding `PmdImporter.h/.cpp` to `PmxCore` and create test suite `tests/tst_pmd_import.cpp`.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmd_import`

---

### Ticket 2.4: Kitbash Merger Engine (`PmxMerger.cpp` — 追加選択)
* **Assigned Subagent:** `pmx_core_engineer`
* **Target Subsystem:** `PmxCore` (Target 1) & `PmxModels` (Target 3)
* **Date Issued:** 2026-10-25 00:20 UTC
* **Status:** `[DONE]` (Committed at `44caf36`)
* **Specification:**
  - Implement `src/core/PmxMerger.h` and `src/core/PmxMerger.cpp`:
    - Non-destructive merging of a secondary PMX/PMD model (`incomingDoc`) into an existing base model (`baseDoc`).
    - Define `MergeOptions`: `mergeIdenticalBones` (bool, default true), `offsetPosition` (`glm::vec3`, default 0), `importPhysics` (bool, default true), `importMorphs` (bool, default true), `importDisplayFrames` (bool, default true).
    - 100% mathematical index remapping across all 7 entity types:
      - Vertices & Faces: `faceIndex += baseVertexCount`.
      - Textures: Deduplicate identical texture paths or append.
      - Materials: Remap `textureIndex` and `sphereTextureIndex`.
      - Bones: Deduplicate identical bone names (`nameJp` / `nameEn`), re-parent child bones to existing base bones, remap `parentBoneIndex`, `tailBoneIndex`, `appendBoneIndex`, and IK link bones. Remap vertex bone weights (BDEF1, BDEF2, BDEF4, SDEF, QDEF).
      - Morphs: Remap vertex morph offsets (`offset.vertexIndex += baseVertexCount`), bone morphs (`boneMap`), material morphs (`baseMaterialCount + matIndex`), and group morphs (`baseMorphCount + morphIndex`).
      - Display Frames: Remap frame item indices (bones and morphs), merge into existing frames with matching names or create new frames.
      - Bullet Physics: Remap rigid body `boneIndex`, joint `rigidBodyIndexA` and `rigidBodyIndexB`.
    - Recalculate header index sizes dynamically based on final vector counts.
  - Integrate with `PmxDocumentController`:
    - Add `bool mergeModel(const PmxDocument& incomingDoc, const MergeOptions& options = {}, QString* errorMessage = nullptr);` which pushes an undoable `PmxDocumentSnapshotCommand` onto `undoStack()`, enabling instant full undo and redo.
  - Create test suite `tests/tst_pmx_merger.cpp`:
    - Test kitbashing hair/accessories onto base model (vertices, materials, textures).
    - Test standard bone deduplication and re-parenting.
    - Test physics remapping (rigid body bone indices and joint body indices).
    - Test morph remapping across vertex, bone, and material morphs.
    - Test complete Undo and Redo cycle via `PmxDocumentController::mergeModel` with zero state drift.
  - Update `CMakeLists.txt` adding `PmxMerger.h/.cpp` to `PmxCore`, and registering `tst_pmx_merger`.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_merger`

---

### Ticket 2.5: Assimp Accessory Importer (`AssimpImporter.cpp`)
* **Assigned Subagent:** `pmx_core_engineer`
* **Target Subsystem:** `PmxCore` (Target 1)
* **Date Issued:** 2026-10-25 00:48 UTC
* **Status:** `[DONE]` (Committed at `c87c34f`)
* **Specification:**
  - Build System:
    - Update `CMakeLists.txt` to find and link `assimp` (`find_package(assimp REQUIRED)` or pkg-config / library lookup). Add `src/core/AssimpImporter.h` and `src/core/AssimpImporter.cpp` to `PmxCore`.
  - Core Implementation (`src/core/AssimpImporter.h/.cpp`):
    - Implement `AssimpImporter` class in namespace `Pmx`:
      - `struct AssimpImportOptions { float scale = 1.0f; bool generateNormals = true; bool flipUVs = true; bool createRootBone = true; QString rootBoneNameJp = QString::fromUtf8("全ての親"); QString rootBoneNameEn = "Root"; };`
      - `static std::unique_ptr<PmxDocument> importFile(const QString& filePath, const AssimpImportOptions& options = {}, QString* errorMessage = nullptr);`
      - `static std::unique_ptr<PmxDocument> importFromMemory(const void* data, size_t length, const QString& hintFormat = "obj", const AssimpImportOptions& options = {}, QString* errorMessage = nullptr);`
    - Configure Assimp importer with post-processing flags:
      - `aiProcess_Triangulate`: Ensure all polygons are converted to triangles.
      - `aiProcess_GenSmoothNormals` / `aiProcess_GenNormals`: Ensure every mesh has normals.
      - `aiProcess_JoinIdenticalVertices`: Deduplicate identical vertices.
      - `aiProcess_SortByPType`: Remove non-triangle primitives (lines/points).
      - Coordinate system conversion: Convert Assimp coordinates to PMX native Left-Handed Y-Up ($p_{LH} = (x, y, -z)$ or `aiProcess_MakeLeftHanded` / `aiProcess_FlipWindingOrder`).
    - Mesh & Vertex Conversion:
      - Accumulate submeshes into `PmxDocument::vertices` and `PmxDocument::faces`.
      - Assign BDEF1 skinning to default root bone (`全ての親` / `Root`) at `(0, 0, 0)` with weight 1.0.
      - UV coordinates: `texCoords.x` and `texCoords.y` (flipped if requested for MMD UV convention).
    - Material Conversion:
      - Convert Assimp materials to `PmxMaterial`: diffuse color, specular color, specular strength, ambient, alpha, flags (double-sided / edge / shadow).
      - Extract diffuse texture paths (`aiTextureType_DIFFUSE`), deduplicate and append to `doc->textures`, setting `mat.textureIndex`.
    - Hierarchy & Default Structures:
      - Add root center bone if `createRootBone` is true (`全ての親` / `Root`, index 0).
      - Add default `Root` and `表情` display frames in `doc->displayFrames`.
      - Set header: PMX 2.0, proper index sizes based on element counts.
  - Test Suite (`tests/tst_assimp_import.cpp`):
    - Register `tst_assimp_import` in `CMakeLists.txt` and CTest.
    - Test synthetic OBJ loading (e.g. quad cube, textured mesh) via `importFromMemory` and file.
    - Assert triangle count, vertex count, normal normalization ($|\vec{n}| \approx 1$), UV bounds, material diffuse/texture mapping, root bone presence, and lossless roundtrip export/import via `PmxWriter` and `PmxReader`.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_assimp_import`

#### Ticket 2.5 — Revision 2 (Adversarial QA Audit Hardening)
* **Date & Timestamp:** 2026-10-25 01:10 UTC
* **Reason for Revision:**
  - Strike 1 audit finding by `pmx_qa_adversary`:
    1. Schema Bug: `createRootBone = false` still assigned `boneIndices[0] = 0` when `doc->bones` was empty. Corrected to assign `boneIndices = {-1, -1, -1, -1}` and `weights = {0.0f, 0.0f, 0.0f, 0.0f}`.
    2. Test Coverage: Added `testTextureImport()` verifying Windows backslash normalization and texture deduplication, and `testMultiMeshImport()` verifying vertex offsets and multi-object partitioning.
    3. Defensive Null Safety: Hardened against null pointers in corrupt Assimp mesh buffers (`mMeshes`, `mVertices`, `mNormals`, `mFaces`, `mMaterials`).

---

## Milestone 3: Full MMD Shader Pipeline & Interactive 3D Edit Mode

### Ticket 3.1: MMD Toon Ramp Shading & Sphere Map (`.sph` / `.spa`) Multi-Pass QRhi Pipeline
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystem:** `PmxViewport` (Target 4)
* **Date Issued:** 2026-10-25 01:15 UTC
* **Status:** `[DONE]` (Committed at `2ab3af4`)
* **Specification:**
  - Shader Pipeline Updates (`src/viewport/shaders/mesh.vert`, `src/viewport/shaders/mesh.frag`):
    - In `mesh.vert`:
      - Compute view-space normal `vNormalView = mat3(u_normalMatrix) * inNormal;`.
      - Compute MMD sphere map texture coordinates:
        `vSphereUV = vec2(0.5 + 0.5 * vNormalView.x, 0.5 - 0.5 * vNormalView.y);`.
      - Pass `vNormal`, `vUV`, `vSphereUV`, `vViewPos` to fragment shader.
    - In `mesh.frag`:
      - Add samplers:
        `layout(binding = 1) uniform sampler2D u_diffuseTex;`
        `layout(binding = 2) uniform sampler2D u_sphereTex;`
        `layout(binding = 3) uniform sampler2D u_toonTex;`
      - Evaluate Diffuse Color:
        Sample `u_diffuseTex` at `vUV`. Apply material `u_diffuse` color & alpha.
      - Evaluate MMD Toon Ramp:
        Directional light dot $N \cdot L = \text{dot}(N, L)$.
        Toon coordinate: $u = \text{clamp}(0.5 - 0.5 \times (N \cdot L), 0.0, 1.0)$.
        Sample `u_toonTex` at `vec2(u, 0.5)` to obtain toon shading ramp factor.
      - Evaluate Sphere Map:
        Sample `u_sphereTex` at `vSphereUV`.
        If `sphereMode == 1` (Multiplication / `.sph`): multiply base color by sphere RGB.
        If `sphereMode == 2` (Addition / `.spa`): add sphere RGB to base color.
        If `sphereMode == 0` (None) or 3 (SubTexture): bypass sphere effect.
      - Evaluate Specular:
        Blinn-Phong specular based on half-vector $H = \text{normalize}(L + V)$.
      - Final composite color with alpha blending and premultiplied / standard output.
  - Viewport Renderer & Texture Cache (`src/viewport/PmxTextureCache.h/.cpp`, `src/viewport/PmxViewportItem.cpp`):
    - Implement `PmxTextureCache`:
      - Manages `QRhiTexture` and `QRhiSampler` allocations.
      - Generates default 1x1 white texture for missing/untextured materials.
      - Generates embedded fallback 1D toon ramps (`toon01.bmp`..`toon10.bmp`) programmatically (smooth steps / color transitions) when external files are absent.
      - Loads model textures (`.png`, `.jpg`, `.bmp`, `.tga`) from disk using `QImage`.
    - Per-Material Draw Call Batching:
      - In `PmxRhiRenderer::render`, iterate over `doc.materials`.
      - Allocate / bind per-material `QRhiShaderResourceBindings` (SRB) binding uniform buffer, diffuse texture, sphere texture, and toon texture.
      - Support double-sided rendering (`QRhiGraphicsPipeline::CullNone` if `mat.drawFlags & 0x01`, otherwise `CullBack`).
      - Call `cb->drawIndexed(mat.faceVertexCount, 1, indexOffset, 0);`
      - Increment `indexOffset += mat.faceVertexCount;`.
  - Verification & Tests (`tests/tst_pmx_viewport.cpp`):
    - Add tests for:
      - Sphere map coordinate generation math.
      - Toon ramp lookup calculations.
      - Texture cache fallbacks (1x1 white, 10 embedded toon ramps).
      - Material submesh draw range partitioning without index overflow.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.1 — Revision 2 (Strike 1 Remediation & Alignment)
* **Date & Timestamp:** 2026-10-25 01:40 UTC
* **Reason for Revision:**
  - Strike 1 audit failure detected by `pmx_qa_adversary`:
    1. Spec Deviation: Inverted sampler bindings (Binding 2 was sphere, Binding 3 was toon). Corrected to Binding 2 = Toon (`u_toonTex`), Binding 3 = Sphere (`u_sphereTex`).
    2. Toon Ramp Orientation: Toon ramp was sampled horizontally along U instead of vertically along V. Standard MMD textures are vertical strips. Fixed to sample along V at `vec2(0.5, toonV)`.
    3. Shared Toon Index Collision: `idx = idx - 1` caused preset 0 and 1 collision and made preset 9 unreachable. Removed offset; directly mapped `0..9` to presets `0..9`.
    4. View Normal Normalization: Normalized view-space normal before computing sphere UVs in `mesh.vert`.
    5. Backface Normal Inversion: Added `if (!gl_FrontFacing) N = -N;` in `mesh.frag` for double-sided materials.
    6. Sampler Address Mode: Applied `QRhiSampler::ClampToEdge` for toon and sphere textures to prevent seam bleeding.

---

### Ticket 3.2: Inverted-Hull Edge Outline Pass with Per-Vertex/Per-Material Width Scaling
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystem:** `PmxViewport` (Target 4)
* **Date Issued:** 2026-10-25 02:04 UTC
* **Status:** `[DONE]` (Committed at `d14666f`)
* **Specification:**
  - Shaders (`src/viewport/shaders/pmx_edge.vert`, `src/viewport/shaders/pmx_edge.frag`, `src/viewport/shaders/mesh.vert`):
    - `pmx_edge.vert`: Extrudes vertex along its normal scaled by `material.edgeSize * vertex.edgeScale * 0.01`. Uses uniform block `FrameAndMaterialUbo` (binding 0).
    - `pmx_edge.frag`: Outputs solid `u_edgeColor` (RGBA).
    - Update `mesh.vert` to declare location 3 `in float inEdgeScale` to match shared `MeshVertex` layout.
  - Vertex Layout & Staging (`src/viewport/PmxViewportItem.h/.cpp`):
    - Extend `MeshVertex` to include `float edgeScale = 1.0f`.
    - Set attribute location 3 in `QRhiVertexInputLayout`.
    - Copy `mv.edgeScale = v.edgeScale` in `PmxRhiRenderer::synchronize()`.
  - Pipeline & Render Pass:
    - Add `m_edgePipeline` with front-face culling (`CullFront`), depth test `LessOrEqual`, depth write enabled, alpha blending.
    - Filter `StagedMaterialBatch` with `hasEdge = ((mat.drawFlags & 0x10) != 0) && (mat.edgeSize > 0.0f) && (mat.edgeColor.a > 0.0f)`.
    - Render edge outline pass for all batches with `hasEdge == true`.
  - Unit Tests (`tests/tst_pmx_viewport.cpp`):
    - Verify edge shader resource loading, extrusion math, `hasEdge` filtering, and vertex edgeScale staging.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.2 — Revision 2 (Strike 1 Remediation)
* **Date & Timestamp:** 2026-10-25 02:21 UTC
* **Reason for Revision:**
  - Strike 1 audit failure detected by `pmx_qa_adversary`:
    1. Hollow test assertion in `testShaderResourcesLoaded`: validated `size() > 0` instead of deserializing with `QShader::fromSerialized()` and verifying `shader.isValid()`.
    2. Missing negative `edgeScale` test coverage in `testEdgeExtrusionMath` and `testMeshVertexEdgeScalePropagation`.
    3. Missing zero `faceVertexCount` edge case on materials with `drawFlags & 0x10 != 0`.
    4. Defense-in-depth: non-finite `std::isfinite(mat.edgeSize)` checks missing in `partitionMaterialBatches`.
* **Revision Directives:**
  - Deserialize all shaders via `QShader::fromSerialized()` and assert `shader.isValid()`.
  - Add explicit assertions for negative `edgeScale` (`-1.0f`).
  - Add test asserting `hasEdge == true` and `indexCount == 0` for empty materials (`faceVertexCount == 0`).
  - Add `std::isfinite(mat.edgeSize)` and `std::isfinite(mat.edgeColor.a)` checks in `partitionMaterialBatches`.

---

### Ticket 3.3: Interactive 3D Transform Gizmos (Translate/Rotate/Scale) & Symmetric Editing
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystem:** `PmxViewport` / `PmxModels` (Target 4 & Target 3)
* **Date Issued:** 2026-10-25 02:35 UTC
* **Status:** `[DONE]` (Committed at `e72f5d7`)
* **Specification:**
  - `PmxGizmo` Engine (`src/viewport/PmxGizmo.h/.cpp`):
    - `GizmoMode`: `None`, `Translate`, `Rotate`, `Scale`.
    - `GizmoAxis`: `None`, `AxisX`, `AxisY`, `AxisZ`, `PlaneXY`, `PlaneYZ`, `PlaneXZ`.
    - `CoordSpace`: `World`, `Local`.
    - Raycasting / Picking: Given screen ray $(\mathbf{o}, \mathbf{d})$, test collision against axis shafts/planes and return hovered/selected handle.
    - Transform calculation: Project mouse delta along active axis/plane; compute position delta $\Delta\mathbf{p}$, rotation $\Delta\mathbf{q}$, or scale $\Delta\mathbf{s}$. Support numeric snapping (distance, angle, scale).
    - Symmetric Editing (`対称編集`): When enabled, mirror deltas across $X=0$: $(-\Delta x, \Delta y, \Delta z)$ for symmetric vertices (pairing $|x_A + x_B| < \epsilon, |y_A - y_B| < \epsilon, |z_A - z_B| < \epsilon$) or symmetric bones (`左` $\leftrightarrow$ `右`).
    - Dynamic scale: Scale gizmo size by camera distance so on-screen pixel size remains constant.
    - Overlay geometry: Generate colored line/mesh vertices for active gizmo (Red=X, Green=Y, Blue=Z, Yellow=Active).
  - Undo Commands (`src/models/PmxUndoCommands.h/.cpp`):
    - Implement `PmxVertexTransformCommand`: sparse deltas `(vertexIndex, oldPos, newPos, oldNorm, newNorm)` with `id()`, `mergeWith()` during continuous dragging, `redo()`, and `undo()`.
    - Implement `PmxBoneTransformCommand`: sparse deltas `(boneIndex, oldPos, newPos, oldRot, newRot)`.
  - Viewport Integration (`src/viewport/PmxViewportItem.h/.cpp`):
    - Connect mouse press, move, release to gizmo picking and dragging.
    - On drag release, commit transform command to `controller()->undoStack()`.
    - Render gizmo lines/handles on top of the scene with depth test disabled or depth bias so gizmo is always visible.
  - Test Suite (`tests/tst_pmx_viewport.cpp`):
    - Verify gizmo ray-axis hit testing.
    - Verify translation, rotation, and scaling delta math.
    - Verify numerical snapping.
    - Verify symmetric vertex mirroring ($X \to -X$).
    - Verify `PmxVertexTransformCommand` and `PmxBoneTransformCommand` undo/redo with `QUndoStack`.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.3 — Revision 2 (Strike 1 Fix)
* **Date & Timestamp:** 2026-10-25 02:52 UTC
* **Reason for Revision:**
  - Strike 1 compilation failure in `tests/tst_pmx_viewport.cpp`:
    1. GLM experimental extensions (`glm/gtx/quaternion.hpp`, `norm.hpp`) included without `#define GLM_ENABLE_EXPERIMENTAL`.
    2. Member access error: `PmxBone` has `nameJp` / `nameEn`, not `name`.
* **Revision Directives:**
  - Define `GLM_ENABLE_EXPERIMENTAL` before including GLM GTX headers.
  - Fix bone name field references to `nameJp`.

#### Ticket 3.3 — Revision 3 (Strike 1 QA Remediation)
* **Date & Timestamp:** 2026-10-25 02:59 UTC
* **Reason for Revision:**
  - Strike 1 audit failure detected by `pmx_qa_adversary`:
    1. Non-finite snapping vulnerability in `PmxGizmo.cpp`: extreme `Inf` snapping caused `0.0 * Inf = NaN` in translation/rotation/scale deltas.
    2. Missing boundary test coverage in `tests/tst_pmx_viewport.cpp` (zero-length rays, out-of-bounds indices, empty selections, null controller, `mergeWith` rejection).
* **Revision Directives:**
  - Add `std::isfinite()` guards before rounding with snapping parameters in `PmxGizmo.cpp`.
  - Add explicit assertions for zero/negative screen dimensions, `scale = 0`, `GizmoMode::None`, `snap = Inf`, out-of-bounds indices, null controllers, and `mergeWith` rejection in `tests/tst_pmx_viewport.cpp`.

---

### Ticket 3.4: Wireframe Visualizers for Bones, Rigid Bodies (Sphere/Box/Capsule) & 6DoF Joints
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystem:** `PmxViewport` (Target 4: `src/viewport/`)
* **Date Issued:** 2026-10-25 03:38 UTC
* **Status:** `[DONE]` (Committed at `1b6afef`)
* **Specification:**
  - Modular Wireframe Visualizer Engine (`src/viewport/PmxWireframeRenderer.h/.cpp`):
    - Create `PmxWireframeRenderer` utility class generating `std::vector<LineVertex>` for bones, physics, and joints.
    - Bone Wireframes:
      - Render 3D octahedral wireframe pyramids from bone head to tail:
        - If `flagConnection == 0` (tail is bone) and `tailBoneIndex` is valid, apex at child bone position; base ring at 15-20% length along the bone shaft with 4 diamond edges connecting to origin and 4 edges connecting to apex.
        - If `flagConnection == 1` (tail offset) and `length(tailOffset) > 0.001f`, apex at `position + tailOffset`.
        - If unlinked / zero-length tail, draw a small local octahedron (radius 0.2 units).
      - Connection lines to child bones and parent bones.
      - Bone coordinate crosses at joints.
    - Bullet3 Rigid Body Wireframes:
      - Geometric wireframes matching PMX `shapeType`:
        - **Sphere (`shapeType == 0`):** 3 orthogonal wireframe circles (XY, XZ, YZ) of radius `shapeSize.x`.
        - **Box (`shapeType == 1`):** 12 wireframe edge lines forming rectangular box of dimensions `shapeSize.x * 2, shapeSize.y * 2, shapeSize.z * 2` (or full extent matching PMX spec).
        - **Capsule (`shapeType == 2`):** Wireframe cylinder (radius `shapeSize.x`, length `shapeSize.y` between dome centers) plus 2 hemispherical wireframe dome rings/arcs at top and bottom.
      - World transformation:
        - Position: convert to RH `(p.x, p.y, -p.z)`.
        - Rotation: apply PMX Euler angles converted to RH rotation matrix.
      - Color coding by `operationMode`:
        - Mode 0 (Static / Kinematic bone-follow): Sky Blue `glm::vec4(0.2f, 0.55f, 1.0f, 0.85f)`
        - Mode 1 (Dynamic / Bullet simulated): Bright Orange `glm::vec4(1.0f, 0.5f, 0.05f, 0.85f)`
        - Mode 2 (Dynamic + Bone alignment): Golden Yellow `glm::vec4(1.0f, 0.85f, 0.1f, 0.85f)`
    - 6DoF Spring Joint Wireframes:
      - Coordinate cross at joint position `(p.x, p.y, -p.z)` oriented by joint Euler rotation (X=Red, Y=Green, Z=Blue).
      - Circular limit arcs showing angular constraint range (`rotationMin` to `rotationMax` around each axis).
      - Joint connection lines: Dotted/solid lines connecting joint position to the centers of linked `rigidBodyA` and `rigidBodyB` (if indices valid).
  - Viewport & UI Integration (`src/viewport/PmxViewportItem.h/.cpp`, `src/app/Main.qml`):
    - Expose QML boolean properties: `showBones` (existing), `showRigidBodies` (new), `showJoints` (new).
    - Synchronize dirty state when document or visibility toggles change.
    - Render overlays using `m_linePipeline`.
    - Update `Main.qml` with viewport toolbar toggles for Bones, Physics (Rigid Bodies), and Joints.
  - Test Suite (`tests/tst_pmx_viewport.cpp`):
    - Unit tests for bone octahedron generation and tail linkage.
    - Unit tests for Sphere, Box, and Capsule wireframe vertex counts and color assignments.
    - Unit tests for 6DoF joint crosses, limit arcs, and body connection lines.
    - Edge cases & boundary safety: non-finite coordinates, NaN/Inf sizes, negative dimensions, invalid bone/body indices, empty documents.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.4 — Revision 2 (Strike 1 Remediation)
* **Date & Timestamp:** 2026-10-25 04:05 UTC
* **Reason for Revision:**
  - Strike 1 audit failure detected by `pmx_qa_adversary`:
    1. **Spec Drift (Critical):** Handedness inversion omitted for joint rotational limits in `src/viewport/PmxWireframeRenderer.cpp#L346-L397`. In RH coordinates, Euler rotations around X and Y invert sign ($rx_{RH} = -rx_{LH}$, $ry_{RH} = -ry_{LH}$). For limits, bounds must invert:
       $$\min X_{RH} = -\max X_{LH}, \quad \max X_{RH} = -\min X_{LH}$$
       $$\min Y_{RH} = -\max Y_{LH}, \quad \max Y_{RH} = -\min Y_{LH}$$
       $$\min Z_{RH} = \min Z_{LH}, \quad \max Z_{RH} = \max Z_{LH}$$
    2. **Selection Refresh Defect:** In `src/viewport/PmxViewportItem.cpp#L863-L901`, `updateGizmoPosition()` only called `update()` if bones or vertices were selected. Selecting rigid bodies or joints in the inspector failed to trigger a viewport repaint.
    3. **Hollow Test Coverage:** `tests/tst_pmx_viewport.cpp` omitted assertions for exact bone diamond base points, non-zero Euler rotation on rigid bodies and joints, sphere radius offsets, box corner extents, capsule dome apex points, and asymmetric joint angular limits.
* **Revision Directives:**
  - Apply handedness inversion to X and Y angular limit ranges in `src/viewport/PmxWireframeRenderer.cpp`.
  - Ensure `PmxViewportItem` always calls `update()` on selection change so rigid body and joint selection highlights redraw immediately.
  - De-hollow unit tests in `tests/tst_pmx_viewport.cpp`: assert exact bone diamond points, non-zero Euler rotation matrices, sphere radius points, box corner extents, capsule dome apexes, and asymmetric joint limit arcs reflecting RH inverted limits.

---

### Ticket 3.5: Interactive Weight Painting Engine with Heatmap Visualization & Radius/Falloff Brush
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystems:** `PmxViewport` / `PmxModels` (Target 4 & Target 3: `src/viewport/`, `src/models/`, `src/app/qml/`, `tests/`)
* **Date Issued:** 2026-10-25 04:28 UTC
* **Status:** `[DONE]` (Committed at `f9a5312`)
* **Specification:**
  - Weight Heatmap Visualization (`src/viewport/PmxWeightPaintBrush.h/.cpp`, `src/viewport/shaders/pmx_mesh.frag`):
    - Color ramp gradient helper `weightToColor(float weight)`:
      - $w \le 0.0$: Blue `vec4(0.0, 0.0, 1.0, 1.0)`
      - $0.0 < w \le 0.25$: Blue $\to$ Cyan `vec4(0.0, 4*w, 1.0, 1.0)`
      - $0.25 < w \le 0.5$: Cyan $\to$ Green `vec4(0.0, 1.0, 1.0 - 4*(w - 0.25), 1.0)`
      - $0.5 < w \le 0.75$: Green $\to$ Yellow `vec4(4*(w - 0.5), 1.0, 0.0, 1.0)`
      - $0.75 < w \le 1.0$: Yellow $\to$ Red `vec4(1.0, 1.0 - 4*(w - 0.75), 0.0, 1.0)`
      - $w > 1.0$: Red `vec4(1.0, 0.0, 0.0, 1.0)`
    - Evaluate active bone weight across BDEF1, BDEF2, BDEF4, SDEF. When in WeightPaint mode, tint or replace mesh diffuse color with the heatmap color.
  - Interactive 3D Weight Painting Brush (`src/viewport/PmxWeightPaintBrush.h/.cpp`):
    - Brush operations: `Add`, `Subtract`, `Set`, `Smooth`.
    - Falloff curves: `Linear` ($1 - d/r$), `Gaussian` ($\exp(-3(d/r)^2)$), `Constant` ($1.0$).
    - Surface raycasting from mouse cursor against mesh triangles to locate hit point and surface normal.
    - Multi-bone weight normalization:
      - Enforce $\sum w_i = 1.0$ across all referenced bones for each affected vertex.
      - Support BDEF1, BDEF2, BDEF4, and SDEF without floating-point accumulation drift.
    - 3D wireframe brush cursor: circle geometry visualizing brush radius at intersection point.
  - Sparse Undo Command (`src/models/PmxUndoCommands.h/.cpp`):
    - `PmxVertexWeightCommand`: records sparse deltas `(vertexIndex, oldWeightType, newWeightType, oldWeights, newWeights, oldBoneIndices, newBoneIndices)`.
    - Continuous drag coalescing with `mergeWith()` on matching drag session ID.
    - Full `undo()` and `redo()` fidelity updating `PmxDocument` and notifying controller.
  - Viewport & UI Integration (`src/viewport/PmxViewportItem.h/.cpp`, `src/app/qml/Main.qml`):
    - Add `viewportMode` enum/property (`MeshShaded = 0`, `Wireframe = 1`, `WeightPaint = 2`).
    - Expose brush parameters (`brushRadius`, `brushStrength`, `brushMode`, `brushFalloff`).
    - Connect mouse drag to brush evaluation in WeightPaint mode; render brush ring overlay.
    - Add brush settings toolbar in `Main.qml`.
  - Deterministic Test Suite (`tests/tst_pmx_viewport.cpp`):
    - Unit tests for heatmap color ramp values across all brackets.
    - Unit tests for brush falloff formulas (Linear, Gaussian, Constant).
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.5 — Revision 2 (Strike 1 Remediation)
* **Date & Timestamp:** 2026-10-25 04:54 UTC
* **Reason for Revision:**
  - Strike 1 audit finding detected by `pmx_qa_adversary`:
    1. **Compilation Failure:** `tests/tst_pmx_viewport.cpp#L2194` called non-existent `controller.setDocument(doc)` instead of `controller.setDocumentInternal(doc)`.
    2. **Class Encapsulation Gap:** `PmxVertexWeightCommand` lacked `friend class` declaration in `src/models/PmxDocumentController.h`.
    3. **BDEF4 Elevation Safeguard:** Missing explicit `v.boneWeights[1] = 1.0f - v.boneWeights[0];` initialization before BDEF4 elevation in `src/viewport/PmxWeightPaintBrush.cpp`.
* **Revision Directives:**
  - Correct `setDocumentInternal` in test harness.
  - Add `friend class PmxVertexWeightCommand;` to `PmxDocumentController.h`.
  - Add explicit BDEF2 weight initialization safeguard before BDEF4 elevation in `PmxWeightPaintBrush.cpp`.

---

### Ticket 3.6: Regression Bugfix — Shaded Mesh Geometry Distortion & Render Pipeline Remediation
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystems:** `PmxViewport` (`src/viewport/PmxViewportItem.h/.cpp`, `src/viewport/shaders/mesh.vert`, `src/viewport/shaders/pmx_edge.vert`, `tests/tst_pmx_viewport.cpp`)
* **Date Issued:** 2026-10-25 05:20 UTC
* **Status:** `[DONE]` (Committed at `3f175fb`)
* **Incident / Problem Statement:**
  - Regression reported during interactive testing with `/home/oscarc/Documents/3D Objects/Barbara/Barbara.pmx` (15,601 vertices, 20 materials, 282 bones).
  - In **Weight Paint Mode**, geometry renders 100% cleanly and correctly using `m_heatmapPipeline` (draws indexed full mesh with `lines.vert` / `lines.frag`).
  - In **Shaded Mode**, geometry is severely distorted/scrambled, showing giant black planar artifacts and exploded polygons stretching across the viewport.
* **Diagnostic & Implementation Directives:**
  1. **Direct Model Investigation:**
     - Load `/home/oscarc/Documents/3D Objects/Barbara/Barbara.pmx` in a diagnostic test or unit test harness.
     - Inspect `mat.edgeSize`, `mat.drawFlags`, `mat.edgeColor` across all 20 materials, and `v.edgeScale`, `v.normal`, `v.position` across vertices.
  2. **Isolate Shaded Mode Pipeline Components:**
     - Test disabling the inverted-hull edge outline pass (`m_edgePipeline` in lines 623–633 of `PmxViewportItem.cpp`). Does Shaded mode render cleanly when the edge pass is omitted?
     - If the edge outline pass is the culprit, identify why `pmx_edge.vert` extrudes gigantic black polygons:
       - Check `u_edgeParams.x * inEdgeScale * 0.01` with actual PMX values.
       - Check whether `inNormal` in `MeshVertex` is unnormalized, non-finite, or zero.
       - Check whether front-face culling (`m_edgePipeline->setCullMode(QRhiGraphicsPipeline::Front)`) vs. triangle winding (`CW` vs `CCW`) is properly configured for the LH $\to$ RH reflection ($z \to -z$).
       - Check whether depth write (`m_edgePipeline->setDepthWrite(true)`) conflicts with the main pass.
       - Expose a user toggle `showEdgeOutlines` (default true) and clamp extrusion displacement to safe finite bounds.
     - If the main mesh pass (`m_meshPipelineCullBack` / `mesh.vert`) is the culprit:
       - Verify `MeshVertex` stride (36B) and attribute alignment on the GPU.
       - Verify `cb->drawIndexed(gb.staged.indexCount, 1, gb.staged.indexOffset, 0)` offsets and buffer indexing.
  3. **Verification Suite:**
     - Add non-hollow unit tests in `tests/tst_pmx_viewport.cpp` covering real-world multi-material models, edge extrusion clamping, and batch draw offsets.
     - Verify all 7 CTest suites pass 100% cleanly under ASan/UBSan.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

#### Ticket 3.6 — Revision 2 (Strike 1 Remediation & Pipeline Binding Order Fix)
* **Date & Timestamp:** 2026-10-25 06:25 UTC
* **Reason for Revision:**
  - Strike 1 audit failure detected by `pmx_qa_adversary`:
    1. **Runtime Rendering Distortion:** In shaded mode, a massive distorted wedge covers the right quadrant of the viewport. In contrast, heatmap mode (`viewportMode == 2`) with `LineVertex` renders Barbara 100% cleanly and without distortion.
    2. **Hollow Tests:** `testBarbaraModelLoadingAndDiagnostics` in `tests/tst_pmx_viewport.cpp` asserted CPU staging math without testing offscreen GPU rendering.
    3. **Division by Zero in `mesh.vert`:** `normView = normalize(mat3(u_normalMatrix) * inNormal)` caused `NaN` propagation on degenerate/zero-length normals.
    4. **QML Startup TypeErrors:** Missing null-safety guards when QML views access `bridge.controller` prior to model load.
* **Architectural Diagnosis & Directives:**
  1. **Pipeline & Vertex Input Binding Order in `PmxViewportItem.cpp`:**
     - In `PmxRhiRenderer::render`, `cb->setVertexInput` must be called **AFTER** `cb->setGraphicsPipeline(pipe)`, NOT before. In OpenGL Core (`QRhiGles2`), calling `setGraphicsPipeline` binds a new VAO; calling `setVertexInput` before pipeline switch leaves the new VAO without valid vertex attribute pointers, corrupting vertex fetches!
     - In both the main shaded loop and the edge outline loop, ensure `setVertexInput(0, 1, &vInput, m_meshIndexBuffer, 0, QRhiCommandBuffer::IndexUInt32)` is explicitly bound for each pipeline prior to `drawIndexed`.
  2. **Zero-Length Normal Guard in `src/viewport/shaders/mesh.vert`:**
     - Replace raw `normalize(mat3(u_normalMatrix) * inNormal)` with safe length-checked normalization matching `pmx_edge.vert`.
  3. **End-to-End Headless Verification in `tests/tst_pmx_viewport.cpp`:**
     - Expand `testBarbaraModelLoadingAndDiagnostics` with genuine offscreen/readback or bounding assertions verifying all projected shaded vertices fall strictly within valid camera view bounds ($X \in [200, 620]$).
  4. **QML Null Guards:**
     - Add null checks in `Main.qml` and `InspectorPanel.qml` to eliminate the 23 startup `TypeError` warnings.
* **Adversarial QA Re-Audit Verdict:** `PASS` (Audited by `pmx_qa_adversary` on 2026-10-25 06:46 UTC).
* **DevOps Commit Receipt:** Committed at `3f175fb` with 100% passing tests (7/7 suites in 1.57s under ASan/UBSan).

---

## Milestone 4: Live TransformView (CCDIK, SDEF & Bullet3 Physics)

### Ticket 4.1: Evaluation Engine Core, Runtime Node Graph & Deform Tier Pipeline
* **Assigned Subagent:** `pmx_core_systems_engineer`
* **Target Subsystem:** `PmxEvaluator` (`src/evaluator/`, `tests/tst_pmx_evaluator.cpp`, `CMakeLists.txt`)
* **Date Issued:** 2026-10-25 06:40 UTC
### Ticket 4.1: Evaluation Engine Core, Runtime Node Graph & Deform Tier Pipeline
* **Assigned Subagent:** `pmx_core_systems_engineer`
* **Target Subsystem:** `PmxEvaluator` (`src/evaluator/`, `tests/tst_pmx_evaluator.cpp`, `CMakeLists.txt`)
* **Date Issued:** 2026-10-25 06:40 UTC
* **Status:** `[DONE]` (Committed at `d149e08`)
* **Specification & Architectural Invariants:**
  1. **Build System & Target Isolation (Golden Law 1):**
     - Add `PmxEvaluator` static library to `CMakeLists.txt` (`src/evaluator/PmxRuntimeScene.h`, `src/evaluator/PmxRuntimeScene.cpp`).
     - Depends ONLY on `PmxCore`, `glm`, and `QtCore`. Zero Qt Quick / QML / GUI dependencies.
     - Add test suite `tests/tst_pmx_evaluator.cpp` to `CMakeLists.txt` registered under CTest.
  2. **`PmxRuntimeScene` Core Data Structures (`src/evaluator/PmxRuntimeScene.h/.cpp`):**
     - Non-destructive evaluation invariant: Operates on internal scene node graph. Rest-pose `PmxDocument` is NEVER mutated.
     - `struct RuntimeBone`:
       - Rest position $\mathbf{p}_{\text{rest}}$ and parent index.
       - Interactive user transform: `localTranslation` ($\Delta \mathbf{p}$), `localRotation` ($\Delta \mathbf{q}$).
       - Morph transform: `morphTranslation`, `morphRotation`.
       - Append inheritance cache: `appendTranslation`, `appendRotation`.
       - Final local matrix $\mathbf{L}_b$ and evaluated world matrix $\mathbf{W}_b$.
       - Inverse bind matrix $\mathbf{B}_b^{-1} = \text{translate}(-\mathbf{p}_{\text{rest}})$.
       - Bone flags (`InheritRotation`, `InheritTranslation`, `FixedAxis`, `LocalAxis`, `PhysicsAfterDeform`).
       - `deformTier` (`int32_t`).
  3. **Deterministic Evaluation Scheduling (`evaluateBones`):**
     - Sort all bone indices by evaluation key:
       $$\text{Key}(i) = \left(\text{isPhysicsAfterDeform}(i),\; \text{deformTier}(i),\; i\right)$$
     - Phase 1 (Pre-Physics): Evaluate all bones where `isPhysicsAfterDeform == false` in sorted order:
       - Combine local transform: $\mathbf{t} = \Delta\mathbf{p} + \text{morphTranslation} + \text{appendTranslation}$.
       - Combine local rotation: $\mathbf{q} = \text{normalize}(\Delta\mathbf{q} \times \text{morphRotation} \times \text{appendRotation})$.
       - Fixed axis constraint (`0x0400`): If set, project rotation onto normalized `fixedAxisVector`.
       - Append bone inheritance (`付与`):
         - If `InheritRotation` (`0x0100`): $\mathbf{q}_{\text{inherit}} = \text{slerp}(\text{identity}, \text{parent}.\text{deltaRotation}, \text{inheritWeight})$.
         - If `InheritTranslation` (`0x0200`): $\mathbf{t}_{\text{inherit}} = \text{parent}.\text{deltaTranslation} \times \text{inheritWeight}$.
       - World matrix: $\mathbf{W}_b = \mathbf{W}_{\text{parent}} \times \mathbf{T}(\mathbf{p}_{\text{rest}} + \mathbf{t}) \times \mathbf{R}(\mathbf{q}) \times \mathbf{T}(-\mathbf{p}_{\text{rest}})$.
     - Phase 2 (Post-Physics): Evaluate remaining bones (`isPhysicsAfterDeform == true`).
  4. **Morph Weight Accumulation (`evaluateMorphs`):**
     - Group Morphs: Unpack recursively with multiplicative weighting.
     - Vertex Morphs: Accumulate vector displacements $\sum w_m \Delta\mathbf{v}_{m,i}$.
     - Bone Morphs: Accumulate translations and slerp rotations per bone.
     - Material Morphs: Accumulate Multiply and Add factors for diffuse, specular, ambient, edge, texture tint, sphere tint, toon tint.
     - UV Morphs: Accumulate UV offsets.
  5. **Deterministic Unit Test Suite (`tests/tst_pmx_evaluator.cpp`):**
     - Verify deterministic sorting order matches `(isPhysicsAfterDeform, deformTier, index)`.
     - Verify hierarchical child transform propagation ($Parent \to Child \to Grandchild$).
     - Verify append rotation inheritance (`InheritRotation`) with weights $0.5$ and $-1.0$.
     - Verify fixed-axis rotation clamping along arbitrary 3D axes.
     - Verify Group morph recursive accumulation.
     - Verify zero floating-point accumulation drift / rest-pose document invariance.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_evaluator`
* **Adversarial QA Audit Verdict:** `PASS` (Audited by `pmx_qa_adversary` on 2026-10-25 07:00 UTC).
* **DevOps Commit Receipt:** Committed at `d149e08` with 8/8 test suites passing in 1.64s under ASan/UBSan.

---

### Ticket 4.2: MMD CCDIK Solver with Iteration Clamping & Knee Euler Limits
* **Assigned Subagent:** `pmx_core_systems_engineer`
* **Target Subsystem:** `PmxEvaluator` (`src/evaluator/MmdCcdIkSolver.h/.cpp`, `src/evaluator/PmxRuntimeScene.h/.cpp`, `tests/tst_pmx_evaluator.cpp`)
* **Date Issued:** 2026-10-25 07:05 UTC
* **Status:** `[DONE]` (Committed at `a242a1b`)
* **Specification & Architectural Invariants:**
  1. **Canonical MMD CCDIK Formulation:**
     - For each IK chain in `PmxRuntimeScene`:
       - Effector bone $\mathbf{e}$, Target bone $\mathbf{t}$, and ordered link bones $[l_0, l_1, \dots, l_{n-1}]$.
       - Iteration limit `iterationCount` and per-iteration angle limit `angleLimit` (radians).
       - For each iteration up to `iterationCount`:
         - For each link $l_k$ (from leaf effector up to root):
           - In world space, calculate vector to current effector position: $\mathbf{v}_e = \mathbf{p}_e - \mathbf{p}_{l_k}$.
           - In world space, calculate vector to target position: $\mathbf{v}_t = \mathbf{p}_t - \mathbf{p}_{l_k}$.
           - Normalize $\mathbf{v}_e$ and $\mathbf{v}_t$. If either length $< 10^{-6}$, skip link.
           - Angle: $\theta = \arccos(\text{clamp}(\mathbf{v}_e \cdot \mathbf{v}_t, -1.0, 1.0))$. If $\theta < 10^{-6}$, continue.
           - Clamping: $\theta_{\text{clamped}} = \min(\theta, \text{angleLimit})$.
           - Axis: $\mathbf{a} = \text{normalize}(\mathbf{v}_e \times \mathbf{v}_t)$.
           - Transform axis to link local space using inverse world orientation: $\mathbf{a}_{\text{local}} = \mathbf{q}_{l_k, \text{world}}^{-1} \cdot \mathbf{a}$.
           - Compute delta quaternion: $\Delta \mathbf{q} = \text{angleAxis}(\theta_{\text{clamped}}, \mathbf{a}_{\text{local}})$.
           - Update link local rotation: $\mathbf{q}_{\text{new}} = \Delta \mathbf{q} \times \mathbf{q}_{l_k, \text{local}}$.
           - **Angle Limits (e.g. Knee Bones `hasLimits`):**
             - Convert $\mathbf{q}_{\text{new}}$ to Euler angles ($ZXY$ extrinsic / $YXZ$ intrinsic).
             - Clamp Euler angles to $[\text{minAngle}, \text{maxAngle}]$ specified in `PmxBone::IkLink`.
             - Reconstruct clamped quaternion.
           - Update world transform of $l_k$ and all its descendants (including effector) before proceeding to next link.
           - Early exit if distance $\|\mathbf{p}_e - \mathbf{p}_t\| < 10^{-4}$.
  2. **Integration into `PmxRuntimeScene`:**
     - Integrate `solveIk()` into `PmxRuntimeScene::evaluate()` scheduled at proper tier order.
     - Provide toggle to enable/disable IK solving per chain.
  3. **Verification Suite (`tests/tst_pmx_evaluator.cpp`):**
     - `testCcdIk2LinkReachable`: 2-link planar leg chain reaching target point within $10^{-3}$ tolerance.
     - `testCcdIkKneeAngleLimits`: Knee joint with strictly positive X angle limit ($[0.008, \pi]$); assert knee never bends backwards (negative angle).
     - `testCcdIkIterationClamping`: Verify solver respects `iterationCount` and `angleLimit`.
     - `testCcdIkUnreachableTarget`: Target placed beyond total link length; verify stable extension along target vector without NaN/Inf.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_evaluator`
* **Adversarial QA Audit Verdict:** `PASS` (Audited by `pmx_qa_adversary` on 2026-10-25 07:33 UTC).
* **DevOps Commit Receipt:** Committed at `a242a1b` with 8/8 test suites passing in 1.60s under ASan/UBSan.

---

### Ticket 4.3: High-Fidelity SDEF (Spherical Skinning) & BDEF1/2/4 Vertex Deformer
* **Assigned Subagent:** `pmx_core_systems_engineer`
* **Target Subsystem:** `PmxEvaluator` (`src/evaluator/PmxVertexDeformer.h/.cpp`, `src/evaluator/PmxRuntimeScene.h/.cpp`, `CMakeLists.txt`, `tests/tst_pmx_evaluator.cpp`)
* **Date Issued:** 2026-10-25 07:36 UTC
* **Status:** `[DONE]` (Committed at `5a6101a`)
* **Specification & Architectural Invariants:**
  1. **Canonical MMD Skinning Deformer:**
     - Support all PMX deform types:
       - `BDEF1` (type 0): $\mathbf{p}' = \mathbf{M}_{b0} \cdot \mathbf{p}$, $\mathbf{n}' = \text{normalize}(\mathbf{M}_{b0} \cdot \mathbf{n})$.
       - `BDEF2` (type 1): $\mathbf{M} = w_0 \mathbf{M}_{b0} + (1 - w_0) \mathbf{M}_{b1}$.
       - `BDEF4` (type 2): $\mathbf{M} = \sum_{i=0}^3 w_i \mathbf{M}_{bi}$.
       - `SDEF` (type 3): Spherical deformation algorithm preserving joint volume (anti-pinching / anti-candy-wrapper):
         - Parameters: bone 0, bone 1, weight $w_0$, center of rotation $\mathbf{C}$, directional bounds $\mathbf{R}_0, \mathbf{R}_1$.
         - Slerp rotation between bone orientations: $\mathbf{q}_{\text{sdef}} = \text{slerp}(\mathbf{q}_1, \mathbf{q}_0, w_0)$.
         - Compute blended rotation matrix $\mathbf{R}_{\text{sdef}} = \text{mat4\_cast}(\mathbf{q}_{\text{sdef}})$.
         - Corrected center position and translation delta calculation matching canonical MMD specification.
     - Rest-pose vertex positions and normals are offset by accumulated morph displacements ($\Delta\mathbf{v}$, $\Delta\mathbf{n}$) prior to skinning deformation.
  2. **Integration into `PmxRuntimeScene`:**
     - Expose `void deformVertices(std::vector<glm::vec3>& outPositions, std::vector<glm::vec3>& outNormals) const;`
     - Threaded or optimized batch SIMD/loop execution for 100k+ vertices.
  3. **Verification Suite (`tests/tst_pmx_evaluator.cpp`):**
     - `testBdef1Skinning`: Vertex bound to single translating/rotating bone moves exactly with the bone.
     - `testBdef2Skinning`: Vertex with 50/50 weight between two translating bones lands at mid-point.
     - `testBdef4Skinning`: 4-bone weighted average matches mathematical expectation.
     - `testSdefVolumePreservation`: 90-degree joint bend compares SDEF vs BDEF2; proves radial distance from joint center $\mathbf{C}$ is preserved significantly closer to rest radius without volume collapse.
     - `testMorphPlusSkinningCombination`: Vertex morph displacement combined with bone rotation yields expected transformed position.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_evaluator`
* **Strike 1 Incident & Remediation:**
  - *Incident:* QA audit identified that auxiliary bounds in `deformSdef` used `deltaR = (rTotal - rRest) * 0.5f` with unrotated `rRest`, violating rigid body rotation invariance.
  - *Remediation:* Per Golden Law 5, worker subagent `154ce97d-6dd0-4c92-9360-9520dd3359ea` was sent the failure logs and updated the formula to canonical spec:
    $$\mathbf{r}_0 = \text{mat3}(\mathbf{M}_{b0}) \mathbf{R}_0 - \mathbf{R}_{\text{sdef}} \mathbf{R}_0, \quad \mathbf{r}_1 = \text{mat3}(\mathbf{M}_{b1}) \mathbf{R}_1 - \mathbf{R}_{\text{sdef}} \mathbf{R}_1, \quad \Delta \mathbf{r} = w_0 \mathbf{r}_0 + w_1 \mathbf{r}_1$$
    Added `testSdefRigidRotationInvariance` and `testSdefDifferentialRotationWithBounds`.
* **Adversarial QA Re-Audit Verdict:** `PASS` (Audited by `pmx_qa_adversary` on 2026-10-25 08:20 UTC).
* **DevOps Commit Receipt:** Committed at `5a6101a` with 8/8 test suites passing in 1.62s under ASan/UBSan.

---

### Ticket 4.4: Real-Time Bullet3 Physics Simulation & 6DoF Spring Constraints
* **Assigned Subagent:** `pmx_core_systems_engineer`
* **Target Subsystem:** `PmxEvaluator` (`src/evaluator/PmxPhysicsEngine.h/.cpp`, `src/evaluator/PmxRuntimeScene.h/.cpp`, `CMakeLists.txt`, `tests/tst_pmx_evaluator.cpp`)
* **Date Issued:** 2026-10-25 08:23 UTC
* **Status:** `[DONE]` (Committed at `121ea4d`)
* **Specification & Architectural Invariants:**
  1. **Build System & Dependencies:**
     - Locate Bullet3 in `CMakeLists.txt` via `find_package(Bullet QUIET)` or explicit Bullet libraries (`BulletDynamics`, `BulletCollision`, `LinearMath`).
     - Link Bullet to `PmxEvaluator`.
  2. **`PmxPhysicsEngine` Core Architecture (`src/evaluator/PmxPhysicsEngine.h/.cpp`):**
     - Initialize `btDiscreteDynamicsWorld` with `btDbvtBroadphase`, `btDefaultCollisionConfiguration`, `btCollisionDispatcher`, `btSequentialImpulseConstraintSolver`.
     - Standard MMD gravity: $\mathbf{g} = (0.0f, -98.0f, 0.0f)$ (10x gravity in MMD decimeter units).
     - Rigid Body Ingestion:
       - Sphere: `btSphereShape(radius)`
       - Box: `btBoxShape(btVector3(size.x, size.y, size.z))`
       - Capsule: `btCapsuleShape(radius, height)`
       - Type 0 (Static / Bone-following): Kinematic rigid body. Rest position and orientation synchronized from bone each frame.
       - Type 1 (Dynamic): Dynamic Bullet rigid body simulated under gravity and collisions. Evaluated transform synchronizes back into bone world matrix.
       - Type 2 (Dynamic with Bone Alignment): Dynamic physics simulation preserving bone direction alignment.
       - Collision filtering: `collisionGroup` (1 << groupIndex) and `collisionMask`.
     - 6DoF Spring Constraints:
       - `btGeneric6DofSpringConstraint` between bodies A and B.
       - Linear limit min/max and spring stiffness.
       - Angular limit min/max (transformed to Right-Handed coordinates) and rotational spring stiffness.
  3. **Integration into `PmxRuntimeScene`:**
     - In `loadDocument()`: Construct physics world from `doc.rigidBodies` and `doc.joints`.
     - In `evaluate()`:
       - Step physics simulation: `m_physicsEngine->stepSimulation(deltaTime)`.
       - Update Type 1 & 2 bone world matrices from physics bodies during post-physics bone evaluation pass.
     - Reset physics state on scene reset.
  4. **Verification Suite (`tests/tst_pmx_evaluator.cpp`):**
     - `testBulletRigidBodyFreeFall`: Dynamic body falls downward along $-Y$ under gravity over multiple simulation steps.
     - `testBulletKinematicBoneTracking`: Kinematic rigid body moves identically with its parent bone.
     - `testBulletSpringConstraintStability`: 6DoF spring oscillates stably without numerical explosion under ASan/UBSan.
     - `testBulletCollisionFiltering`: Rigid bodies with non-intersecting collision masks pass through each other without contact response.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_evaluator`
* **Strike 1 Incident & Remediation:**
  - *Incident:* QA audit identified that: (1) `syncDynamicBones` overwrote translation for `physicsMode == 2` instead of preserving parent hierarchy translation; (2) `createBtTransform` used `setEulerZYX` (intrinsic ZYX / extrinsic XYZ) instead of canonical extrinsic $ZXY$; (3) move assignment operator lacked safe `clear()` teardown; (4) shapes lacked dimension clamping; (5) tests lacked compound Euler rotation and mode 2 coverage.
  - *Remediation:* Per Golden Law 5, worker subagent `352b7f9d-b69b-477a-a0f4-33c613871087` was sent the failure logs and resolved all items: preserved translation on `boneWorld[3]` for mode 2, constructed canonical $ZXY$ quaternion `qy * qx * qz`, implemented custom move assignment with `clear()`, clamped dimensions with `std::max(dim, 1e-4f)`, and added `testBulletPhysicsAndBoneMode` and `testBulletCompoundEulerRotation`.
* **Adversarial QA Re-Audit Verdict:** `PASS` (Audited by `pmx_qa_adversary` on 2026-10-25 09:09 UTC).
* **DevOps Commit Receipt:** Committed at `121ea4d` with 8/8 test suites passing in 1.66s under ASan/UBSan.

---

### Ticket 4.5: VMD (`.vmd`) Motion & VPD (`.vpd`) Pose File Importer and Playback Engine
* **Assigned Subagent:** `pmx_core_systems_engineer`
* **Target Subsystem:** `PmxEvaluator` (`src/evaluator/VmdImporter.h/.cpp`, `src/evaluator/VpdImporter.h/.cpp`, `src/evaluator/PmxAnimationPlayer.h/.cpp`, `src/evaluator/PmxRuntimeScene.h/.cpp`, `CMakeLists.txt`, `tests/tst_pmx_evaluator.cpp`)
* **Date Issued:** 2026-10-25 09:15 UTC
* **Status:** `[DONE]` (Committed at `8fa77f5`)
* **Specification & Architectural Invariants:**
  1. **Binary VMD 2.0 Ingestion (`src/evaluator/VmdImporter.h/.cpp`):**
     - Header: 30 bytes signature (`"Vocaloid Motion Data 0002\0"`), 20 bytes model name (Shift-JIS).
     - Bone Keyframes (`VmdBoneFrame`):
       - `nameJp` (15 bytes Shift-JIS decoded to `QString`).
       - `frameIndex` (`uint32_t`).
       - `translation` (`glm::vec3` converted to Right-Handed coordinates).
       - `rotation` (`glm::quat` converted to Right-Handed coordinates).
       - `interpolation[4][4][4]` (64-byte Bézier control points for X, Y, Z translation and rotation curves).
     - Morph Keyframes (`VmdMorphFrame`):
       - `nameJp` (15 bytes Shift-JIS decoded to `QString`).
       - `frameIndex` (`uint32_t`).
       - `weight` (`float`).
     - Camera, Light, and IK Toggle Keyframes (`VmdIkFrame` with per-bone IK enable flags).
  2. **VPD Text Pose File Ingestion (`src/evaluator/VpdImporter.h/.cpp`):**
     - Parse ASCII `.vpd` pose files:
       - Header: `Vocaloid Pose Data file`.
       - Bones: `Bone{Index}{BoneName}` with translation and quaternion rotation.
       - Morphs: `Morph{Index}{MorphName}` with float weight.
  3. **Animation Timeline & Playback Engine (`src/evaluator/PmxAnimationPlayer.h/.cpp`):**
     - Track evaluation per bone and per morph sorted by frame index.
     - Frame evaluation at arbitrary floating-point time $t$:
       - Hermite/Bézier cubic spline interpolation for translation and rotation curves using VMD control points.
       - Slerp quaternion interpolation between bone rotation keyframes.
       - Linear interpolation between morph keyframes.
       - Dynamic IK enable/disable toggling according to IK keyframes.
     - Playback controls: `setFrame(float frame)`, `currentFrame()`, `maxFrame()`, `isPlaying()`, `play()`, `pause()`, `stop()`, `setLoop(bool)`.
     - Synchronization: `applyToScene(PmxRuntimeScene& scene)` applies interpolated transforms directly to scene bones and morphs before `scene.evaluate()`.
  4. **Verification Suite (`tests/tst_pmx_evaluator.cpp`):**
     - `testVmdBinaryImportAndKeyframes`: Synthesize valid VMD 2.0 binary buffer; verify correct Shift-JIS bone/morph decoding and frame sorting.
     - `testVmdInterpolationEvaluation`: Sample intermediate frame between keyframe 0 and 10; verify accurate cubic Bézier and quaternion slerp output.
     - `testVpdTextPoseImport`: Ingest sample VPD text pose; verify bone translations and rotations match expected values.
     - `testVmdIkSwitchingTimeline`: Ingest VMD with IK enable toggle keyframes; verify `PmxRuntimeScene::isIkEnabled` reflects the active timeline state.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_evaluator`
* **Strike 1 Incident & Remediation:**
  - *Incident:* QA audit identified that: (1) passing `NaN` to `PmxAnimationPlayer` resulted in `*track.end()` heap buffer over-read; (2) untrusted `ikCount` in `VmdImporter` was reserved without physical buffer bounds checks; (3) truncated camera/light/shadow blocks desynchronized the read stream; (4) lack of negative fuzzing tests.
  - *Remediation:* Per Golden Law 5, worker subagent `3647a18a-bf2d-4481-92c6-7dc44bd0e5d9` was sent the failure logs and resolved all items: added NaN guards in `setFrame`, `evaluateBezier`, and track evaluators (`it == track.end()` and `it == track.begin()` checks); division-based bounds checking on all counts in `VmdImporter`; added `testAnimationPlayerNanHandling` and `testVmdMalformedBufferRejection`.
* **Adversarial QA Re-Audit Verdict:** `PASS` (Audited by `pmx_qa_adversary` on 2026-10-25 10:06 UTC).
* **DevOps Commit Receipt:** Committed at `8fa77f5` with 8/8 test suites passing in 1.70s under ASan/UBSan.

---

### Ticket 4.6: TransformView Viewport Integration & Interactive Timeline Controls
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystems:** `PmxViewport` & `pmx-editor` (`src/viewport/PmxViewportItem.h/.cpp`, `src/viewport/PmxTransformViewBridge.h/.cpp`, `src/app/qml/TransformViewPanel.qml`, `src/app/qml/TimelineBar.qml`, `CMakeLists.txt`, `tests/tst_pmx_viewport.cpp`)
* **Date Issued:** 2026-10-25 10:10 UTC
* **Status:** `[DONE]` (Committed at `7c0ce9a`, Tagged `v0.4.0-milestone4`)
* **Specification & Architectural Invariants:**
  1. **Viewport TransformView Dynamic Buffer Streaming:**
     - Connect `PmxRuntimeScene` and `PmxAnimationPlayer` into `PmxViewportItem`.
     - Provide runtime mode toggle: `EditMode` (static rest pose with interactive gizmo editing) vs `TransformViewMode` (live animation playback, CCDIK, SDEF, and Bullet3 physics).
     - In TransformView mode, dynamic VBO streaming: upload `scene.deformedPositions()` and `scene.deformedNormals()` each frame to `m_meshVertexBuffer`.
     - Update bone wireframe visualizer to dynamic bone world matrices.
     - Update rigid body and joint wireframe visualizers to dynamic physics bodies.
  2. **QML Timeline Bar & Controls (`src/app/qml/TimelineBar.qml`, `Main.qml`):**
     - Timeline scrubber: Current frame spinbox / slider, max frame, playback controls (Play, Pause, Stop, Step Forward, Step Back, Loop toggle).
     - File actions: Load Motion (`.vmd`), Load Pose (`.vpd`), Reset Pose.
     - Simulation controls: Physics enable/disable toggle, Physics Reset button.
  3. **Verification Suite (`tests/tst_pmx_viewport.cpp`):**
     - `testTransformViewModeDeformedMeshUpload`: Verify vertex buffer updates when stepping animation frames.
     - `testTransformViewTimelineScrubbing`: Scrubbing timeline updates scene evaluation.
     - `testTransformViewPhysicsToggling`: Toggling physics state starts/stops Bullet simulation cleanly.
     - `testTransformViewMaterialMorphSyncAndRestoration`: Verify material morphs sync in TransformView and restore rest-pose on mode exit.
     - `testTransformViewBoundarySafety`: Verify NaN/Inf/negative frames and null controller safety.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`
* **Strike 1 Incident & Remediation:**
  - *Incident:* QA audit identified that: (1) `TimelineBar.qml` assigned read-only `implicitWidth: 50` on `Label`, causing fatal `QQmlApplicationEngine` startup crash (exit code 255); (2) `PmxViewportItem::synchronize()` failed to refresh `m_stagedMaterials` on `transformViewChanged`, retaining mutated material morph values when exiting to Edit mode; (3) material morph sync and restoration was untested.
  - *Remediation:* Per Golden Law 5, worker subagent `fa17a0ca-c753-4941-9832-cc1e5708ffd1` resolved all items: used `Layout.preferredWidth` in `TimelineBar.qml`, added `transformViewChanged` trigger in `synchronize()` line 349, and added `testTransformViewMaterialMorphSyncAndRestoration`.
* **Adversarial QA Re-Audit Verdict:** `PASS` (Audited by `pmx_qa_adversary` on 2026-10-25 17:56 UTC).
* **DevOps Commit Receipt:** Committed at `7c0ce9a` with 8/8 test suites passing in 1.68s under ASan/UBSan; tagged `v0.4.0-milestone4`.

---

### Ticket 4.7: Core Evaluator Physics Alignment to Animated Bones
* **Assigned Subagent:** `pmx_core_systems_engineer`
* **Target Subsystem:** `PmxEvaluator` (`src/evaluator/PmxPhysicsEngine.h/.cpp`, `src/evaluator/PmxRuntimeScene.h/.cpp`, `tests/tst_pmx_evaluator.cpp`)
* **Date Issued:** 2026-10-25 19:30 UTC
* **Status:** `[IN_PROGRESS]`
* **Specification & Architectural Invariants:**
  1. **Dynamic Rigid Body Alignment to Animated Bones (`PmxPhysicsEngine.h/.cpp`, `PmxRuntimeScene.h/.cpp`):**
     - Add `void resetToBoneTransforms(PmxRuntimeScene& scene)` to `PmxPhysicsEngine`.
       - For all rigid bodies (`i = 0 .. m_rigidBodies.size() - 1`):
         If `meta.boneIndex >= 0 && meta.boneIndex < scene.boneCount()`:
         Calculate world transform of the body from the bone:
         `glm::mat4 boneWorld = scene.worldMatrix(meta.boneIndex);`
         `glm::mat4 bodyWorld = boneWorld * meta.boneToBodyOffset;`
         `btTransform trans = glmToBtTransform(bodyWorld);`
         `auto* body = m_rigidBodies[i].get();`
         `body->setWorldTransform(trans);`
         `if (body->getMotionState()) body->getMotionState()->setWorldTransform(trans);`
         `body->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));`
         `body->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));`
         `body->clearForces();`
         `if (meta.physicsMode >= 1) body->activate(true);`
     - In `PmxRuntimeScene`:
       - Add `void resetPhysicsToBones()` that calls `m_physicsEngine->resetToBoneTransforms(*this)`.
       - In `loadDocument()`: Call `evaluateBones()` (both pre-physics and post-physics bones) and then call `resetPhysicsToBones()` so dynamic bodies start on their parent bones rather than at origin rest positions.
       - In `resetPhysics()` and `resetTransforms()`: Call `resetPhysicsToBones()`.
       - In `evaluate(float deltaTime)`:
         When `deltaTime <= 0.0f` and `m_physicsEnabled && m_physicsEngine`:
         Call `m_physicsEngine->resetToBoneTransforms(*this)` so that static poses, scrubbing frames, and loading motions synchronize dynamic rigid bodies to current bone positions, preventing `syncDynamicBones` from pulling dynamic skirt/hair bones to obsolete origin coordinates.
       - In `evaluatePrePhysicsBones()` and `evaluatePostPhysicsBones()`:
         When `m_physicsEnabled == false`, ensure all bones evaluate FK so turning off physics keeps clothes/hair attached to the body.
  2. **Verification Suite (`tests/tst_pmx_evaluator.cpp`):**
     - Add `testDynamicRigidBodiesAlignToAnimatedBones`: Load `Barbara.pmx`, enable physics, apply `Motion 1.vmd` frame 0; verify that skirt bones and skirt rigid bodies move with the hips and legs (e.g. $X \neq 0$, distance to origin $> 1.0$), eliminating the "de-pantsed" detachment glitch.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_evaluator`

---

### Ticket 4.8: TransformView Viewport Playback, Dynamic VBO & TimelineBar Hardening
* **Assigned Subagent:** `pmx_ui_engineer`
* **Target Subsystems:** `PmxViewport` & `pmx-editor` (`src/viewport/PmxViewportItem.h/.cpp`, `src/app/qml/TimelineBar.qml`, `tests/tst_pmx_viewport.cpp`)
* **Date Issued:** 2026-10-25 19:30 UTC
* **Status:** `[TODO]`
* **Specification & Architectural Invariants:**
  1. **Dynamic VBO & Playback Timer Hardening (`PmxViewportItem.h/.cpp`):**
     - In `PmxRhiRenderer::render`: Allocate `m_meshVertexBuffer` with `QRhiBuffer::Dynamic` instead of `QRhiBuffer::Immutable`.
     - In `loadVmdMotion`, `loadVpdPose`, `setCurrentFrame`, `resetPose`, `resetPhysics`: Call `m_runtimeScene.resetPhysicsToBones()`.
     - In `play()`: Ensure `m_runtimeRevision++` is incremented, timer started, and `currentFrame` updates emitted.
     - In `onPlaybackTick()`: Ensure `m_animationPlayer.update(deltaTime)` advances `m_currentFrame`, calls `m_runtimeScene.evaluate(deltaTime)`, updates `m_runtimeRevision++`, and emits `currentFrameChanged()`.
  2. **TimelineBar QML Hardening (`TimelineBar.qml`):**
     - Type `viewportItem` cleanly as `property QtObject viewportItem: null`.
     - Add explicit `Connections { target: root.viewportItem; function onPlayingChanged() { ... } function onCurrentFrameChanged() { ... } }` to ensure reliable reactive UI state.
  3. **Verification Suite (`tests/tst_pmx_viewport.cpp`):**
     - Add `testAnimationPlaybackTimerAdvancesFrames`: Load motion, call `viewport.play()`, execute `QTest::qWait(100)`, verify that `viewport.currentFrame() > 0.0f`, `viewport.isPlaying() == true`, and `viewport.runtimeRevision()` has advanced.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_viewport`

---

## Milestone 5: The Top 8 Native C++ Community Power Tools

### Ticket 5.1: Semi-Standard Bone Generator & Multi-Stage / Material Morph Tools
* **Assigned Subagent:** `pmx_core_systems_engineer`
* **Target Subsystem:** `PmxPowerTools` (`src/powertools/SemiStandardBoneGenerator.h/.cpp`, `src/powertools/BatchMorphAndBoneTools.h/.cpp`, `CMakeLists.txt`, `tests/tst_pmx_powertools.cpp`)
* **Date Issued:** 2026-10-25 18:35 UTC
* **Status:** `[TODO]`
* **Specification & Architectural Invariants:**
  1. **CMake Target Definition (`PmxPowerTools`):**
     - Add `PmxPowerTools` static library target in `CMakeLists.txt` depending on `PmxCore`, `PmxModels`, `glm::glm`, and `Qt6::Core`.
     - Register `tst_pmx_powertools` in CTest.
  2. **Tool 2: Semi-Standard Bone Generator (`SemiStandardBoneGenerator.h/.cpp` — 準標準ボーン追加):**
     - Class `SemiStandardBoneGenerator` operating on `PmxDocument` and returning undoable `PmxDocumentSnapshotCommand` via `PmxDocumentController`.
     - Configurable helper bone options:
       - `addMother` (`全ての親`): Root bone inserted at $(0,0,0)$ at `deformTier = 0`. Reparents root bones (`センター`, `グルーブ`, `足ＩＫ`, `つま先ＩＫ`) and adds to `Root` display frame.
       - `addGroove` (`グルーブ`): Inserted at `センター` position as child of `センター`, reparents `上半身` and `下半身`.
       - `addWaist` (`腰` & `腰キャンセルL/R`): Inserted between `グルーブ`/`センター` and `下半身`. `腰キャンセルL/R` uses `InheritRotation` (`weight = -1.0f`) referencing `腰` and reparenting `左足`/`右足`.
       - `addTorso2` (`上半身2`): Splits `上半身` $\to$ `首` midpoint and splits torso vertex weights along $Y$.
       - `addTwistBones` (`腕捩/手捩`): Computes arm/forearm vectors, inserts fixed-axis twist bones and 3 rotation-inheritance helper bones ($25\%, 50\%, 75\%$), and executes cylindrical weight projection and smooth weight splitting to prevent candy-wrapper mesh collapse.
       - `addThumb0` (`親指０ L/R`), `addToeEx` (`足先EX L/R`), `addDBones` (`足D/ひざD/足首D`).
  3. **Tool 7: Batch Morph & Bone Tools (`BatchMorphAndBoneTools.h/.cpp` — 多段ボーン化 / 材質モーフ一括作成):**
     - `MultiStageBoneSplitter`: Inserts duplicate parent/child bones at the same position, updates parent-child pointers and display frame references.
     - `MaterialMorphCreator`: 1-click generation of 28-float material morphs:
       - `createOutfitHideMorph`: Sets `diffuse.a = 0`, `specular = 0`, `edgeColor.a = 0`, `edgeSize = 0`, `textureTint.a = 0`, `sphereTint.a = 0`, `toonTint.a = 0` (Multiply mode).
       - `createTintMorph`: Generates Blush, Shadow, or Emission tint morphs.
  4. **Verification Suite (`tests/tst_pmx_powertools.cpp`):**
     - `testSemiStandardBoneInsertion`: Inserts helper bones on standard model and verifies parent-child hierarchy, deform tiers, and display frames.
     - `testArmTwistCylindricalWeightSplitting`: Verifies arm twist insertion and weight normalization ($\sum w_i = 1.0$) across twist helper bones.
     - `testMultiStageBoneSplitter`: Verifies duplicate bone insertion, child reparenting, and undo/redo cycle.
     - `testMaterialMorphCreator`: Verifies 28-float material morph structure, operation types, and non-destructive properties.
* **Verification Command:** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure && ./build/tests/tst_pmx_powertools`



