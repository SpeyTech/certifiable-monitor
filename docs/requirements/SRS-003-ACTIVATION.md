# SRS-003-ACTIVATION

## Software Requirements Specification: Activation Monitor

**Document ID:** SRS-003-ACTIVATION  
**Version:** 1.0  
**Status:** ✅ Approved  
**Traceability:** CM-ARCH-MATH-001 §4

---

## 1. Purpose

This document specifies the software requirements for the Activation Monitor module (`src/monitor/activation.c`), which provides deterministic monitoring of neural network layer activations against layer contracts defined in the COE.

---

## 2. Scope

The Activation Monitor SHALL:
- Check activation values against per-layer bounds
- Detect saturation events from inference engine fault flags
- Calculate violation rates per monitoring window
- Track maximum over-range magnitudes

---

## 3. Requirements

### 3.1 Initialization

**SRS-003-ACTIVATION-01:** The function `cm_activ_init()` SHALL initialize an activation monitor context with:
- A reference to layer contracts from policy (not owned, may be NULL)
- Caller-provided per-layer state storage
- Layer count validation against CM_MAX_LAYERS

**SRS-003-ACTIVATION-02:** The function `cm_activ_reset_window()` SHALL reset all per-layer counters for a new monitoring window, identified by `window_id`.

### 3.2 Bounds Checking

**SRS-003-ACTIVATION-03:** For each activation tensor, `cm_activ_check_layer()` SHALL check every element against the layer's bounds:
```
violation_j = (a_j < min_q16) ∨ (a_j > max_q16)
```

**SRS-003-ACTIVATION-04:** The function SHALL count total violations and compute violation rate:
```
violation_rate = violations / total_elements (in Q0.32)
```

**SRS-003-ACTIVATION-05:** The function SHALL track maximum over-range magnitude:
```
M = max(0, a_j - max_q16, min_q16 - a_j) for all j
```

**SRS-003-ACTIVATION-06:** The layer result SHALL indicate:
- Number of bound violations
- Maximum over-range magnitude (Q16.16)
- Whether violation rate exceeds tolerance
- Whether max overrange exceeds limit

### 3.3 Saturation Detection

**SRS-003-ACTIVATION-07:** The function `cm_activ_record_saturation()` SHALL increment the saturation counter for a specified layer.

**SRS-003-ACTIVATION-08:** Saturation events MAY also be detected from fault flags passed to `cm_activ_check_layer()`.

### 3.4 Statistics Tracking

**SRS-003-ACTIVATION-09:** The monitor SHALL track per-layer:
- Layer identifier
- Total elements checked in window
- Violation count for window
- Saturation count for window
- Maximum over-range magnitude
- Minimum/maximum observed values

**SRS-003-ACTIVATION-10:** The monitor SHALL track window-level:
- Number of samples (inferences) processed
- Current window ID

### 3.5 Tolerance Checking

**SRS-003-ACTIVATION-11:** The function `cm_activ_exceeds_tolerance()` SHALL return true if:
```
violation_rate > tol_violations_q0_32
```

**SRS-003-ACTIVATION-12:** The function `cm_activ_get_violation_rate()` SHALL return the violation rate in Q0.32 format.

### 3.6 Query Functions

**SRS-003-ACTIVATION-13:** `cm_activ_get_window_summary()` SHALL return aggregate statistics across all layers.

**SRS-003-ACTIVATION-14:** `cm_activ_get_layer_state()` SHALL return per-layer statistics for a given layer index.

**SRS-003-ACTIVATION-15:** `cm_activ_find_layer()` SHALL locate a layer by its `layer_id` and return the index.

---

## 4. Error Handling

**SRS-003-ACTIVATION-16:** All functions SHALL return `CT_ERR_NULL` if passed null pointers for required arguments.

**SRS-003-ACTIVATION-17:** Functions accepting layer indices SHALL return `CT_ERR_RANGE` for out-of-bounds indices.

**SRS-003-ACTIVATION-18:** The monitor SHALL NOT allocate memory; all storage is caller-provided.

---

## 5. Determinism Requirements

**SRS-003-ACTIVATION-19:** All operations SHALL produce bit-identical results across x86, ARM, and RISC-V platforms given identical inputs.

**SRS-003-ACTIVATION-20:** Violation rate calculation SHALL use deterministic fixed-point division.

**SRS-003-ACTIVATION-21:** All loops SHALL have statically bounded iteration counts.

---

## 6. Traceability Matrix

| Requirement | Implementation | Test |
|-------------|----------------|------|
| SRS-003-ACTIVATION-01 | `cm_activ_init()` | `test_activ_init_*` |
| SRS-003-ACTIVATION-02 | `cm_activ_reset_window()` | `test_activ_reset_*` |
| SRS-003-ACTIVATION-03 | `cm_activ_check_layer()` | `test_activ_check_*` |
| SRS-003-ACTIVATION-04 | `cm_activ_get_violation_rate()` | `test_activ_rate_*` |
| SRS-003-ACTIVATION-05 | `cm_activ_check_layer()` | `test_activ_overrange_*` |
| SRS-003-ACTIVATION-07 | `cm_activ_record_saturation()` | `test_activ_saturation_*` |
| SRS-003-ACTIVATION-11 | `cm_activ_exceeds_tolerance()` | `test_activ_tolerance_*` |

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
