# SRS-004-OUTPUT

## Software Requirements Specification: Output Monitor

**Document ID:** SRS-004-OUTPUT  
**Version:** 1.0  
**Status:** ✅ Approved  
**Traceability:** CM-ARCH-MATH-001 §5

---

## 1. Purpose

This document specifies the software requirements for the Output Monitor module (`src/monitor/output.c`), which provides deterministic monitoring of inference output vectors against the Certified Operating Envelope (COE).

---

## 2. Scope

The Output Monitor SHALL:
- Check output values against per-output range bounds
- Accumulate histograms for output drift detection
- Track violations and statistics per monitoring window
- Support drift computation at window boundaries

---

## 3. Requirements

### 3.1 Initialization

**SRS-004-OUTPUT-01:** The function `cm_output_init()` SHALL initialize an output monitor context with:
- A reference to the output envelope from policy (not owned)
- Caller-provided per-output state storage
- Output count validation against CM_MAX_FEATURES

**SRS-004-OUTPUT-02:** The function `cm_output_reset_window()` SHALL reset all per-output counters for a new monitoring window, identified by `window_id`.

### 3.2 Range Checking

**SRS-004-OUTPUT-03:** For each output vector, `cm_output_process()` SHALL check every output value against its envelope bounds:
```
violation_i = (value_i < min_q16[i]) ∨ (value_i > max_q16[i])
```

**SRS-004-OUTPUT-04:** The function SHALL return `CT_ERR_RANGE` if any output violates its bounds, and `CT_OK` otherwise.

**SRS-004-OUTPUT-05:** The result structure SHALL report:
- Total violation count for the sample
- Index of first violating output
- Value and bound of first violation
- Whether upper or lower bound was violated

### 3.3 Histogram Accumulation

**SRS-004-OUTPUT-06:** If the envelope has histograms enabled (`has_hists == true`), each output value SHALL be accumulated into the appropriate histogram bin.

**SRS-004-OUTPUT-07:** Bin assignment SHALL follow CM-MATH-001 §1.1:
- Value in [e_{b-1}, e_b) maps to bin b
- Value in [e_{B-1}, e_B] maps to bin B (last bin includes right edge)

**SRS-004-OUTPUT-08:** Values outside all bin edges SHALL be clamped to the first or last bin respectively.

### 3.4 Statistics Tracking

**SRS-004-OUTPUT-09:** The monitor SHALL track per-output:
- Violation count for current window
- Sample count for current window
- Minimum observed value (Q16.16)
- Maximum observed value (Q16.16)

**SRS-004-OUTPUT-10:** The monitor SHALL track window-level:
- Total violations across all outputs
- Total samples processed
- Current window ID

### 3.5 Drift Detection

**SRS-004-OUTPUT-11:** The function `cm_output_compute_drift()` SHALL compute drift metrics for all outputs with histograms at window end.

**SRS-004-OUTPUT-12:** Drift computation SHALL compare runtime histogram counts to reference counts in the envelope using the detectors specified in the drift policy (TV, JSD, PSI).

**SRS-004-OUTPUT-13:** The drift result SHALL report:
- Count of outputs with drift exceeding thresholds
- Index of first drifting output
- Full drift metrics for first drifting output

### 3.6 Query Functions

**SRS-004-OUTPUT-14:** `cm_output_get_violations()` SHALL return the total violation count for the current window.

**SRS-004-OUTPUT-15:** `cm_output_get_sample_count()` SHALL return the number of samples processed in the current window.

**SRS-004-OUTPUT-16:** `cm_output_get_state()` SHALL return per-output statistics for a given output index.

**SRS-004-OUTPUT-17:** `cm_output_check_value()` SHALL return true if a single value is within bounds for a given output.

---

## 4. Error Handling

**SRS-004-OUTPUT-18:** All functions SHALL return `CT_ERR_NULL` if passed null pointers for required arguments.

**SRS-004-OUTPUT-19:** Functions accepting output indices SHALL return `CT_ERR_RANGE` for out-of-bounds indices.

**SRS-004-OUTPUT-20:** The monitor SHALL NOT allocate memory; all storage is caller-provided.

---

## 5. Determinism Requirements

**SRS-004-OUTPUT-21:** All operations SHALL produce bit-identical results across x86, ARM, and RISC-V platforms given identical inputs.

**SRS-004-OUTPUT-22:** All arithmetic SHALL use integer or fixed-point operations only.

**SRS-004-OUTPUT-23:** All loops SHALL have statically bounded iteration counts.

---

## 6. Traceability Matrix

| Requirement | Implementation | Test |
|-------------|----------------|------|
| SRS-004-OUTPUT-01 | `cm_output_init()` | `test_output_init_*` |
| SRS-004-OUTPUT-02 | `cm_output_reset_window()` | `test_output_reset_*` |
| SRS-004-OUTPUT-03 | `cm_output_process()` | `test_output_process_*` |
| SRS-004-OUTPUT-04 | `cm_output_process()` | `test_output_violation_*` |
| SRS-004-OUTPUT-05 | `cm_output_result_t` | `test_output_result_*` |
| SRS-004-OUTPUT-06 | `cm_output_process()` | `test_output_histogram_*` |
| SRS-004-OUTPUT-11 | `cm_output_compute_drift()` | `test_output_drift_*` |

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
