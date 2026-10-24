# AGENTS.md — Native Linux PMX Editor

> **Critical Context Anchor for Autonomous Coding Agents**  
> All agents working on this repository MUST read and adhere strictly to this document, [MASTER_SPECIFICATION.md](file:///home/oscarc/Documents/MMDs/MASTER_SPECIFICATION.md), and [PRODUCT_REQUIREMENTS_AND_TECHNICAL_BLUEPRINT.md](file:///home/oscarc/Documents/MMDs/PRODUCT_REQUIREMENTS_AND_TECHNICAL_BLUEPRINT.md).

---

## 1. Operating Roles & Responsibilities

### A. For the Lead Architect / Orchestrator:
* You oversee the overarching architecture, specifications, work order dispatching, and git commits.
* You do NOT write implementation code directly in this main chat context.
* You keep coding subagents in the "Smart Zone" by assigning atomic work orders with clean contexts.

### B. For Implementation Subagents (Workers):
* You are assigned to execute a specific, atomic work order ticket.
* **Stay Focused:** Create or edit ONLY the files specified in your ticket. Do not refactor unrelated code.
* **Preserve Invariants:** Adhere strictly to the golden architectural rules and binary schemas below.
* **Verify Before Answering:** Run the deterministic test commands below before reporting completion.

---

## 2. Golden Architectural Rules

1. **Target Boundary Isolation:**
   * **`PmxCore` (Target 1):** Zero Qt Quick/GUI dependencies. Depends ONLY on standard C++20, `glm`, and `QtCore` (`QString` UTF-8/UTF-16 codecs).
   * **`PmxEvaluator` (Target 2):** Never mutates `PmxDocument`. Operates strictly on `PmxRuntimeScene` during 60 Hz evaluation.
   * **`PmxModels` (Target 3):** No direct mutations to `PmxDocument`. All modifications MUST be encapsulated in a `QUndoCommand` and pushed to `PmxDocumentController::undoStack()`.
   * **`PmxPowerTools` (Target 5):** Pure C++20 algorithmic libraries operating on `PmxDocument` and returning undoable commands.

2. **Binary Layout & Signedness Law:**
   * **Vertex Index:** Size 1 = `uint8_t` (0..255), Size 2 = `uint16_t` (0..65535), Size 4 = `int32_t` (0..2147483647).
   * **Non-Vertex Indices (Texture, Material, Bone, Morph, RigidBody):** Size 1 = `int8_t` (-128..127), Size 2 = `int16_t` (-32768..32767), Size 4 = `int32_t`.
   * **Null Reference:** A value of `-1` (0xFF in 1-byte, 0xFFFF in 2-byte, 0xFFFFFFFF in 4-byte) MUST be decoded as `-1` and re-encoded back to exact matching bit patterns according to the header's index size.

3. **Coordinate System Handedness Conversions:**
   * **MMD / PMX Space:** Left-Handed, Y-Up ($+X$ Right, $+Y$ Up, $+Z$ Forward).
   * **Vulkan / OpenGL (`QRhi`) & Bullet3 Space:** Right-Handed, Y-Up ($+X$ Right, $+Y$ Up, $-Z$ Forward).
   * **Conversion Formulas:**
     * Position: $p_{RH} = (p_x, p_y, -p_z)$
     * Euler rotation ($YXZ$ intrinsic / $ZXY$ extrinsic): $r_{RH} = (-r_x, -r_y, r_z)$
     * Quaternion: $q_{RH} = (-q_x, -q_y, q_z, q_w)$

4. **Deterministic Test Verification Gate:**
   * Every coding agent MUST run `ctest --test-dir build --output-on-failure` and ensure 100% test passage before reporting a task complete.

---

## 2. Standard Build & Test Commands

* **Configure:** `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
* **Build All:** `cmake --build build -j$(nproc)`
* **Build Target:** `cmake --build build --target <target-name> -j$(nproc)`
* **Run Tests:** `ctest --test-dir build --output-on-failure`
* **Run Specific Test:** `./build/tests/tst_pmx_roundtrip`
