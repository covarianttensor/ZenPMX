# AGENTS.md — Native Linux PMX Editor

> **Machine Context Anchor for All Agents**  
> All agents (Architect, Feature Workers, DevOps, and QA) working in this workspace must adhere to this document, `MASTER_SPECIFICATION.md`, and `PRODUCT_REQUIREMENTS_AND_TECHNICAL_BLUEPRINT.md`.

---

## 1. Operating Roles & Responsibilities

### A. For the Lead Architect / Orchestrator:
* You oversee overarching system architecture, project specifications, work-order dispatching, and git commits.
* You do NOT write implementation code or run noisy terminal setup commands directly in this main chat context.
* You maintain context hygiene, verify deterministic test receipts, maintain the Living State Triad, and perform git checkpoint commits.

### B. For Feature Implementation Subagents (`pmx_core_engineer`, `pmx_ui_engineer`):
* You are assigned to execute a specific, atomic work order ticket.
* **Stay Focused:** Create or edit ONLY the files specified in your ticket. Do not refactor unrelated code.
* **Preserve Invariants:** Respect the golden architectural rules, binary layouts, signedness laws, and coordinate transformations below.
* **Verify Before Answering:** Run the deterministic verification commands before reporting completion.
* **Strict State Triad Ignore Rule:** You **MUST IGNORE** `IMPLEMENTATION_PLAN.md`, `WORK_ORDER_LOG.md`, and `PROGRESS.md`. These files are strictly reserved for the Lead Architect / Orchestrator. Do not read or modify them.

### C. For DevOps, Tooling & Environment Specialists (`pmx_devops_engineer`):
* **Context Shield:** Execute verbose terminal tools, dependency diagnostics, package queries, shell pipelines, and git operations in your isolated context to protect the Lead Architect's context hygiene.
* **Zero Domain Mutation:** Strictly forbidden from modifying domain data structures, core algorithms, state stores, or UI components. Confined strictly to build scripts, CI configs, git operations, environment setups, and process launching.
* **GitHub & Infrastructure:** Manage GitHub remotes, upstream repo creation (`gh repo create`), branches, tags, and CI workflows safely. Never run destructive git commands without human confirmation.
* **Runtime Testing:** Launch and monitor application processes (`pmx-editor`, `pmx-mcp`) for human interactive testing and report clear operational status.
* **Strict State Triad Ignore Rule:** You **MUST IGNORE** `IMPLEMENTATION_PLAN.md`, `WORK_ORDER_LOG.md`, and `PROGRESS.md`.

### D. For Adversarial QA Inspectors (`pmx_qa_adversary`):
* Audit code diffs against `MASTER_SPECIFICATION.md` for the Five Red Flags (Hollow tests, spec drift, memory leaks, boundary failures, target leakage).
* Never comment on code style or whitespace. Focus 100% on correctness, edge cases, memory safety under ASan/UBSan, and test validity.
* **Strict State Triad Ignore Rule:** You **MUST IGNORE** `IMPLEMENTATION_PLAN.md`, `WORK_ORDER_LOG.md`, and `PROGRESS.md`.

---

## 2. Registered Subagent Roster

1. **`pmx_core_engineer` (Headless C++20 Systems Specialist):**
   * Scope: `PmxCore` (canonical types, Little-Endian binary IO, endianness/signedness), `PmxEvaluator` (runtime scene, CCDIK, SDEF, Bullet3 physics), `PmxPowerTools` (spatial indexing, rigging/weight engines), and headless `PmxModels`/`PmxMcpServer` backends.
2. **`pmx_ui_engineer` (Qt6 / QML & 3D Viewport Specialist):**
   * Scope: `PmxViewport` (`QQuickRhiItem` / OpenGL 3.3+ Core & Vulkan pipeline, camera orbital math, GLSL 450 shaders, 3D gizmos, picking) and `pmx-editor` QML shell layout and UI state bindings.
3. **`pmx_qa_adversary` (Adversarial QA Inspector):**
   * Scope: CMake `ctest` harnesses, AddressSanitizer (ASan) & UndefinedBehaviorSanitizer (UBSan) verification, corrupted PMX binary fuzzing, headless regression checks, and anti-sycophantic diff auditing.
4. **`pmx_devops_engineer` (DevOps, Tooling & Environment Specialist):**
   * Scope: Toolchain setup, GitHub upstream remotes (`gh repo create`), CI/CD workflows, package diagnostics, git push/pull operations, and launching interactive GUI/CLI processes for user testing.

---

## 3. Golden Architectural Rules (The Non-Negotiables)

1. **Target Boundary Isolation:**
   * **`PmxCore` (Target 1):** Zero Qt Quick/GUI dependencies. Depends ONLY on standard C++20, `glm`, and `QtCore` (`QString` UTF-8/UTF-16 codecs).
   * **`PmxEvaluator` (Target 2):** Never mutates `PmxDocument`. Operates strictly on `PmxRuntimeScene` during 60 Hz evaluation.
   * **`PmxModels` (Target 3):** No direct mutations to `PmxDocument`. All modifications MUST be encapsulated in a `QUndoCommand` and pushed to `PmxDocumentController::undoStack()`. Zero Qt Quick/QML dependencies.
   * **`PmxPowerTools` (Target 5):** Pure C++20 algorithmic libraries operating on `PmxDocument` and returning undoable commands.
   * **`PmxViewport` (Target 4):** View-only presentation layer on `QQuickRhiItem`. Never writes raw file parsers or mutates `PmxDocument` directly.

2. **Binary Layout & Signedness Law:**
   * All multi-byte primitives are Little-Endian.
   * **Vertex Index:** Size 1 = `uint8_t` (0..255), Size 2 = `uint16_t` (0..65535), Size 4 = `int32_t` (0..2147483647).
   * **Non-Vertex Indices (Texture, Material, Bone, Morph, RigidBody):** Size 1 = `int8_t` (-128..127), Size 2 = `int16_t` (-32768..32767), Size 4 = `int32_t`.
   * **Null Reference:** A value of `-1` (`0xFF` in 1-byte, `0xFFFF` in 2-byte, `0xFFFFFFFF` in 4-byte) MUST decode as `-1` and re-encode back to exact matching bit patterns according to the header's index size.

3. **Coordinate System Handedness Conversions:**
   * **MMD / PMX Space:** Left-Handed, Y-Up ($+X$ Right, $+Y$ Up, $+Z$ Forward).
   * **Vulkan / OpenGL (`QRhi`) & Bullet3 Space:** Right-Handed, Y-Up ($+X$ Right, $+Y$ Up, $-Z$ Forward).
   * **Conversion Formulas (Computed strictly at the evaluation/rendering boundary):**
     * Position: $p_{RH} = (p_x, p_y, -p_z) \iff p_{LH} = (p_x, p_y, -p_z)$
     * Euler rotation ($YXZ$ intrinsic / $ZXY$ extrinsic): $r_{RH} = (-r_x, -r_y, r_z) \iff r_{LH} = (-r_x, -r_y, r_z)$
     * Quaternion: $q_{RH} = (-q_x, -q_y, q_z, q_w) \iff (w, -x, -y, z)$
   * **Zero Floating-Point Accumulation Drift:** Rest-pose geometry in `PmxDocument` stores native IEEE-754 floats. Never mutate rest-pose floats with lossy conversions.

4. **Deterministic Test Verification Gate:**
   * Every coding agent MUST run `ctest --test-dir build --output-on-failure` and ensure 100% test passage (exit code 0) under ASan/UBSan before reporting a task complete.

---

## 4. Standard Build & Test Commands

* **Configure:** `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
* **Build All:** `cmake --build build -j$(nproc)`
* **Build Target:** `cmake --build build --target <target-name> -j$(nproc)`
* **Run All Tests:** `ctest --test-dir build --output-on-failure`
* **Run Specific Unit Tests:**
  * `./build/tests/tst_pmx_roundtrip`
  * `./build/tests/tst_pmx_undo_models`
  * `./build/tests/tst_pmx_viewport`
  * `./build/tests/tst_pmx_mcp`

---

## 5. Subsystem Layout Reference

```text
/home/oscarc/Documents/MMDs/
├── CMakeLists.txt
├── MASTER_SPECIFICATION.md
├── PRODUCT_REQUIREMENTS_AND_TECHNICAL_BLUEPRINT.md
├── AGENTS.md                                        # Read by all agents
├── IMPLEMENTATION_PLAN.md                           # Living State Triad (Architect only)
├── WORK_ORDER_LOG.md                                # Living State Triad (Architect only)
├── PROGRESS.md                                      # Living State Triad (Architect only)
├── docs/                                            # Financial ledgers & specs
├── src/
│   ├── core/           # Layer 1: Canonical types & Little-Endian binary IO (PmxCore)
│   ├── evaluator/      # Layer 2: TransformView runtime evaluation, CCDIK, SDEF, Bullet3 (PmxEvaluator)
│   ├── models/         # Layer 3: PmxDocumentController, QUndoStack & Table Models (PmxModels)
│   ├── viewport/       # Layer 4: QQuickRhiItem, camera, GLSL 450 shaders (PmxViewport)
│   ├── powertools/     # Layer 2: Top 8 Native C++ Rigging/Physics/Weight Engines (PmxPowerTools)
│   ├── plugin_bridge/  # Layer 3: Unix socket RPC bridge to Mono host (PmxPluginBridge)
│   ├── plugin_host_cs/ # Layer 4: Clean-room PEPlugin.dll & SlimDX shim (PmxPluginHost)
│   ├── mcp/            # Layer 3: JSON-RPC 2.0 AI Co-Pilot & CLI (PmxMcpServer / pmx-mcp)
│   └── app/            # Layer 4: Main QML workspace executable (pmx-editor)
└── tests/              # QtTest suites (tst_pmx_roundtrip, undo_models, viewport, mcp, etc.)
```

---

## 6. Worker Deliverable Contract

When a coding subagent completes a ticket:
1. Ensure all compiler, linker, and `ctest` commands exit with status `0`.
2. Output a concise summary of modified/created files.
3. Provide the verbatim terminal test execution receipt proving all tests passed.
4. **Never modify or read** `IMPLEMENTATION_PLAN.md`, `WORK_ORDER_LOG.md`, or `PROGRESS.md`.

---

## 7. Post-Ticket Checkpoint Inquiries (Executed by Architect & DevOps)

At every ticket breakpoint, the Architect queries the user:
1. **GitHub Remote Backup:** Query whether to push the latest commit to GitHub. If no upstream remote exists, prompt the user for setup instructions and delegate repository initialization to `pmx_devops_engineer`.
2. **Local Runtime Testing:** Query whether to launch the application locally via `pmx_devops_engineer` so the user can interactively test.
