# SRS-007-REACT

## Software Requirements Specification: Reaction Handler

**Document ID:** SRS-007-REACT  
**Version:** 1.0  
**Status:** ✅ Approved  
**Traceability:** CM-ARCH-MATH-001 §8

---

## 1. Purpose

This document specifies the software requirements for the Reaction Handler module (`src/monitor/react.c`), which provides deterministic mapping from violations to actions based on the COE reaction policy.

---

## 2. Scope

The Reaction Handler SHALL:
- Look up appropriate reactions for violation types
- Classify reactions by severity (stop/degrade/clamp)
- Provide human-readable names for logging
- Support configurable default reactions

---

## 3. Requirements

### 3.1 Reaction Types

**SRS-007-REACT-01:** The handler SHALL support the following reaction types:

| Reaction | Value | Description |
|----------|-------|-------------|
| CM_REACT_LOG_ONLY | 0 | Record event, no action |
| CM_REACT_WARN_OPERATOR | 1 | Raise operator alert |
| CM_REACT_CLAMP_OUTPUT | 2 | Apply deterministic output clamp |
| CM_REACT_DEGRADE_MODE | 3 | Switch to reduced capability |
| CM_REACT_EMERGENCY_STOP | 4 | Disable inference |

**SRS-007-REACT-02:** Reaction severity SHALL be ordered: LOG < WARN < CLAMP < DEGRADE < STOP.

### 3.2 Violation Types

**SRS-007-REACT-03:** The handler SHALL recognize the following violation types:

| Violation | Value | Description |
|-----------|-------|-------------|
| CM_VIOL_NONE | 0 | No violation |
| CM_VIOL_INPUT_RANGE | 1 | Input outside envelope |
| CM_VIOL_INPUT_DRIFT | 2 | Input distribution drift |
| CM_VIOL_ACTIV_RANGE | 3 | Activation outside bounds |
| CM_VIOL_ACTIV_SAT | 4 | Activation saturation |
| CM_VIOL_OUTPUT_RANGE | 5 | Output outside bounds |
| CM_VIOL_OUTPUT_DRIFT | 6 | Output distribution drift |
| CM_VIOL_FAULT_BUDGET | 7 | Fault budget exceeded |

### 3.3 Initialization

**SRS-007-REACT-04:** The function `cm_react_init()` SHALL initialize a reaction handler with:
- Reference to reaction mapping table from policy
- Count of mapping entries
- Default reaction set to CM_REACT_LOG_ONLY

**SRS-007-REACT-05:** The function `cm_react_set_default()` SHALL update the default reaction for unmapped violations.

### 3.4 Lookup

**SRS-007-REACT-06:** The function `cm_react_lookup()` SHALL:
- Search the reaction map for the given violation type
- Return the mapped reaction if found
- Return the default reaction if not found

**SRS-007-REACT-07:** Lookup SHALL be O(n) where n = map_count (linear search).

**SRS-007-REACT-08:** If handler is NULL or uninitialized, lookup SHALL return CM_REACT_LOG_ONLY.

### 3.5 Processing

**SRS-007-REACT-09:** The function `cm_react_process()` SHALL:
- Look up the reaction for the given violation
- Populate the result structure with violation, action, and execution status

**SRS-007-REACT-10:** The result structure SHALL contain:
- Input violation type
- Action determined
- Whether action was executed
- Whether ledger event was emitted

### 3.6 Classification Functions

**SRS-007-REACT-11:** `cm_react_is_stop()` SHALL return true if reaction == CM_REACT_EMERGENCY_STOP.

**SRS-007-REACT-12:** `cm_react_is_degrade()` SHALL return true if reaction is DEGRADE_MODE or EMERGENCY_STOP.

**SRS-007-REACT-13:** `cm_react_is_clamp()` SHALL return true if reaction == CM_REACT_CLAMP_OUTPUT.

### 3.7 String Conversion

**SRS-007-REACT-14:** `cm_react_name()` SHALL return a static string for each reaction type:
- CM_REACT_LOG_ONLY → "LOG_ONLY"
- CM_REACT_WARN_OPERATOR → "WARN_OPERATOR"
- CM_REACT_CLAMP_OUTPUT → "CLAMP_OUTPUT"
- CM_REACT_DEGRADE_MODE → "DEGRADE_MODE"
- CM_REACT_EMERGENCY_STOP → "EMERGENCY_STOP"
- Unknown → "UNKNOWN"

**SRS-007-REACT-15:** `cm_viol_name()` SHALL return a static string for each violation type:
- CM_VIOL_NONE → "NONE"
- CM_VIOL_INPUT_RANGE → "INPUT_RANGE"
- CM_VIOL_INPUT_DRIFT → "INPUT_DRIFT"
- CM_VIOL_ACTIV_RANGE → "ACTIV_RANGE"
- CM_VIOL_ACTIV_SAT → "ACTIV_SAT"
- CM_VIOL_OUTPUT_RANGE → "OUTPUT_RANGE"
- CM_VIOL_OUTPUT_DRIFT → "OUTPUT_DRIFT"
- CM_VIOL_FAULT_BUDGET → "FAULT_BUDGET"
- Unknown → "UNKNOWN"

---

## 4. Error Handling

**SRS-007-REACT-16:** All functions SHALL return `CT_ERR_NULL` for null required arguments.

**SRS-007-REACT-17:** Functions SHALL be safe to call with NULL handler (return safe defaults).

**SRS-007-REACT-18:** The handler SHALL NOT allocate memory; mapping table is policy-owned.

---

## 5. Determinism Requirements

**SRS-007-REACT-19:** Lookup SHALL be deterministic: same violation always yields same reaction.

**SRS-007-REACT-20:** String functions SHALL return pointers to static storage (no allocation).

**SRS-007-REACT-21:** All operations SHALL produce identical results across platforms.

---

## 6. Traceability Matrix

| Requirement | Implementation | Test |
|-------------|----------------|------|
| SRS-007-REACT-01 | `cm_reaction_t` | `test_react_types_*` |
| SRS-007-REACT-04 | `cm_react_init()` | `test_react_init_*` |
| SRS-007-REACT-06 | `cm_react_lookup()` | `test_react_lookup_*` |
| SRS-007-REACT-09 | `cm_react_process()` | `test_react_process_*` |
| SRS-007-REACT-11 | `cm_react_is_stop()` | `test_react_classify_*` |
| SRS-007-REACT-14 | `cm_react_name()` | `test_react_names_*` |

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
