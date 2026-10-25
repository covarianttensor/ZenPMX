# WORK_ORDER_LOG.md — Master Work Order & Ticket Revision Ledger

> **Notice:** This document is maintained strictly by the Lead Architect / Orchestrator.  
> Worker subagents (`pmx_core_engineer`, `pmx_ui_engineer`, `pmx_qa_adversary`, `pmx_devops_engineer`) MUST NOT read or modify this file.

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
