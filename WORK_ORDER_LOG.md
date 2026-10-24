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
