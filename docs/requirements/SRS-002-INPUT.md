# SRS-002-INPUT

## Software Requirements Specification: Input Monitor

**Document ID:** SRS-002-INPUT  
**Version:** 1.0  
**Status:** ✅ Approved  
**Traceability:** CM-ARCH-MATH-001 §3

---

## 1. Purpose

This document specifies the software requirements for the Input Monitor module (`src/monitor/input.c`), which provides deterministic monitoring of inference input vectors against the Certified Operating Envelope (COE).

---

## 2. Scope

The Input Monitor SHALL:
- Check input feature values against per-feature range bounds
- Accumulate histograms for drift detection
- Track violations and statistics per monitoring window
- Support drift computation at window boundaries

---

## 3. Requirements

### 3.1 Initialization

**SRS-002-INPUT-01:** The function `cm_input_init()` SHALL initialize an input monitor context with:
- A reference to the input envelope from policy (not owned)
- Caller-provided per-feature state storage
- Feature count validation against CM_MAX_FEATURES

**SRS-002-INPUT-02:** The function `cm_input_reset_window()` SHALL reset all per-feature counters for a new monitoring window, identified by `window_id`.

### 3.2 Range Checking

**SRS-002-INPUT-03:** For each input vector, `cm_input_process()` SHALL check every feature value against its envelope bounds:
```
violation_i = (value_i < min_q16[i]) ∨ (value_i > max_q16[i])
```

**SRS-002-INPUT-04:** The function SHALL return `CT_ERR_RANGE` if any feature violates its bounds, and `CT_OK` otherwise.

**SRS-002-INPUT-05:** The result structure SHALL report:
- Total violation count for the sample
- Index of first violating feature
- Value and bound of first violation
- Whether upper or lower bound was violated

### 3.3 Histogram Accumulation

**SRS-002-INPUT-06:** If the envelope has histograms enabled (`has_hists == true`), each feature value SHALL be accumulated into the appropriate histogram bin.

**SRS-002-INPUT-07:** Bin assignment SHALL follow CM-MATH-001 §1.1:
- Value in [e_{b-1}, e_b) maps to bin b
- Value in [e_{B-1}, e_B] maps to bin B (last bin includes right edge)

**SRS-002-INPUT-08:** Values outside all bin edges SHALL be clamped to the first or last bin respectively.

### 3.4 Statistics Tracking

**SRS-002-INPUT-09:** The monitor SHALL track per-feature:
- Violation count for current window
- Sample count for current window
- Minimum observed value (Q16.16)
- Maximum observed value (Q16.16)

**SRS-002-INPUT-10:** The monitor SHALL track window-level:
- Total violations across all features
- Total samples processed
- Current window ID

### 3.5 Drift Detection

**SRS-002-INPUT-11:** The function `cm_input_compute_drift()` SHALL compute drift metrics for all features with histograms at window end.

**SRS-002-INPUT-12:** Drift computation SHALL compare runtime histogram counts to reference counts in the envelope using the detectors specified in the drift policy (TV, JSD, PSI).

**SRS-002-INPUT-13:** The drift result SHALL report:
- Count of features with drift exceeding thresholds
- Index of first drifting feature
- Full drift metrics for first drifting feature

### 3.6 Query Functions

**SRS-002-INPUT-14:** `cm_input_get_violations()` SHALL return the total violation count for the current window.

**SRS-002-INPUT-15:** `cm_input_get_sample_count()` SHALL return the number of samples processed in the current window.

**SRS-002-INPUT-16:** `cm_input_get_feature_state()` SHALL return per-feature statistics for a given feature index.

**SRS-002-INPUT-17:** `cm_input_check_feature()` SHALL return true if a single value is within bounds for a given feature.

---

## 4. Error Handling

**SRS-002-INPUT-18:** All functions SHALL return `CT_ERR_NULL` if passed null pointers for required arguments.

**SRS-002-INPUT-19:** Functions accepting feature indices SHALL return `CT_ERR_RANGE` for out-of-bounds indices.

**SRS-002-INPUT-20:** The monitor SHALL NOT allocate memory; all storage is caller-provided.

---

## 5. Determinism Requirements

**SRS-002-INPUT-21:** All operations SHALL produce bit-identical results across x86, ARM, and RISC-V platforms given identical inputs.

**SRS-002-INPUT-22:** All arithmetic SHALL use integer or fixed-point operations only.

**SRS-002-INPUT-23:** All loops SHALL have statically bounded iteration counts.

---

## 6. Traceability Matrix

| Requirement | Implementation | Test |
|-------------|----------------|------|
| SRS-002-INPUT-01 | `cm_input_init()` | `test_input_init_*` |
| SRS-002-INPUT-02 | `cm_input_reset_window()` | `test_input_reset_*` |
| SRS-002-INPUT-03 | `cm_input_process()` | `test_input_process_*` |
| SRS-002-INPUT-04 | `cm_input_process()` | `test_input_violation_*` |
| SRS-002-INPUT-05 | `cm_input_result_t` | `test_input_result_*` |
| SRS-002-INPUT-06 | `cm_input_process()` | `test_input_histogram_*` |
| SRS-002-INPUT-11 | `cm_input_compute_drift()` | `test_input_drift_*` |

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
