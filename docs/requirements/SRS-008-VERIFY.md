# SRS-008-VERIFY

## Software Requirements Specification: Ledger Verification

**Document ID:** SRS-008-VERIFY  
**Version:** 1.0  
**Status:** ✅ Approved  
**Traceability:** CM-ARCH-MATH-001 §10

---

## 1. Purpose

This document specifies the software requirements for the Verification module (`src/audit/verify.c`), which provides offline verification of audit ledgers for post-incident analysis and regulatory compliance.

---

## 2. Scope

The Verification Module SHALL:
- Verify genesis binding to deployment artifacts
- Verify chain integrity by replaying entries
- Detect tampering, truncation, and binding violations
- Support both batch and incremental verification modes
- Provide detailed verification reports

---

## 3. Requirements

### 3.1 Verification Result Codes

**SRS-008-VERIFY-01:** The module SHALL define the following result codes:

| Code | Value | Description |
|------|-------|-------------|
| CM_VERIFY_OK | 0 | Verification successful |
| CM_VERIFY_ERR_NULL | 1 | Null pointer argument |
| CM_VERIFY_ERR_GENESIS | 2 | Genesis hash mismatch |
| CM_VERIFY_ERR_CHAIN | 3 | Chain hash mismatch |
| CM_VERIFY_ERR_ENTRY | 4 | Entry hash mismatch |
| CM_VERIFY_ERR_BINDING | 5 | Bundle/policy binding mismatch |
| CM_VERIFY_ERR_SEQUENCE | 6 | Sequence discontinuity |
| CM_VERIFY_ERR_TRUNCATED | 7 | Chain appears truncated |
| CM_VERIFY_ERR_FINAL | 8 | Final digest mismatch |
| CM_VERIFY_ERR_EMPTY | 9 | Empty entry array |

### 3.2 Verification Report

**SRS-008-VERIFY-02:** The verification report (`cm_verify_report_t`) SHALL contain:
- Overall result code
- Sequence number where error occurred (if any)
- Count of entries successfully verified
- Computed final chain digest
- Boolean flags: genesis_valid, bindings_valid, sequence_valid

### 3.3 Genesis Verification

**SRS-008-VERIFY-03:** The function `cm_verify_compute_genesis()` SHALL compute:
```
L_0 = SHA256("CM:LEDGER:GENESIS:v1" ∥ R ∥ H_P)
```

**SRS-008-VERIFY-04:** The function `cm_verify_genesis()` SHALL compare computed genesis against expected value.

**SRS-008-VERIFY-05:** Genesis mismatch SHALL return CM_VERIFY_ERR_GENESIS.

### 3.4 Entry Verification

**SRS-008-VERIFY-06:** The function `cm_verify_entry_binding()` SHALL verify that entry headers contain expected R and H_P values.

**SRS-008-VERIFY-07:** Binding mismatch SHALL return CM_VERIFY_ERR_BINDING.

**SRS-008-VERIFY-08:** The function `cm_verify_entry_hash()` SHALL recompute entry hash and compare to expected.

**SRS-008-VERIFY-09:** Entry hash mismatch SHALL return CM_VERIFY_ERR_ENTRY.

### 3.5 Sequence Verification

**SRS-008-VERIFY-10:** The function `cm_verify_sequence_monotonic()` SHALL verify:
```
entries[i].seq == start_seq + i for all i
```

**SRS-008-VERIFY-11:** Sequence discontinuity SHALL return CM_VERIFY_ERR_SEQUENCE.

### 3.6 Incremental Verification

**SRS-008-VERIFY-12:** The function `cm_verify_init()` SHALL:
- Compute and store genesis digest
- Store expected R and H_P
- Initialize sequence counter to 1
- Set initialized flag

**SRS-008-VERIFY-13:** The function `cm_verify_entry()` SHALL:
- Verify entry binding matches stored R and H_P
- Verify sequence number matches expected
- Compute entry hash
- Advance chain digest: L_t = SHA256("CM:LEDGER:v1" ∥ L_{t-1} ∥ e_t)
- Increment sequence counter and entry count

**SRS-008-VERIFY-14:** The function `cm_verify_finalize()` SHALL:
- Optionally compare final digest to expected
- Populate verification report
- Return appropriate result code

**SRS-008-VERIFY-15:** `cm_verify_get_digest()` SHALL return current chain digest from context.

### 3.7 Batch Verification

**SRS-008-VERIFY-16:** The function `cm_verify_chain()` SHALL:
- Initialize verification context
- Process all entries in sequence
- Compare final digest to expected (if provided)
- Return detailed report

**SRS-008-VERIFY-17:** Batch verification SHALL stop at first error and report the failing entry.

**SRS-008-VERIFY-18:** Empty entry arrays with entry_count=0 SHALL verify successfully (genesis-only chain).

### 3.8 Utility Functions

**SRS-008-VERIFY-19:** `cm_verify_result_name()` SHALL return human-readable strings for all result codes.

**SRS-008-VERIFY-20:** `cm_verify_digests_equal()` SHALL perform constant-time comparison of two 32-byte digests.

---

## 4. Error Handling

**SRS-008-VERIFY-21:** All functions SHALL return CM_VERIFY_ERR_NULL for null required arguments.

**SRS-008-VERIFY-22:** Functions requiring initialized context SHALL check the initialized flag.

**SRS-008-VERIFY-23:** The module SHALL NOT allocate memory.

---

## 5. Determinism Requirements

**SRS-008-VERIFY-24:** All hash computations SHALL produce bit-identical results across platforms.

**SRS-008-VERIFY-25:** Verification results SHALL be deterministic: same inputs always yield same result.

**SRS-008-VERIFY-26:** String functions SHALL return pointers to static storage.

---

## 6. Security Properties

**SRS-008-VERIFY-27:** The verifier SHALL detect any modification to any entry in the chain.

**SRS-008-VERIFY-28:** The verifier SHALL detect any truncation of the chain.

**SRS-008-VERIFY-29:** The verifier SHALL detect any binding mismatch (wrong R or H_P).

**SRS-008-VERIFY-30:** The verifier SHALL detect any reordering of entries.

---

## 7. Traceability Matrix

| Requirement | Implementation | Test |
|-------------|----------------|------|
| SRS-008-VERIFY-01 | `cm_verify_result_t` | `test_verify_result_*` |
| SRS-008-VERIFY-03 | `cm_verify_compute_genesis()` | `test_verify_genesis_*` |
| SRS-008-VERIFY-06 | `cm_verify_entry_binding()` | `test_verify_binding_*` |
| SRS-008-VERIFY-10 | `cm_verify_sequence_monotonic()` | `test_verify_sequence_*` |
| SRS-008-VERIFY-12 | `cm_verify_init()` | `test_verify_init_*` |
| SRS-008-VERIFY-13 | `cm_verify_entry()` | `test_verify_entry_*` |
| SRS-008-VERIFY-16 | `cm_verify_chain()` | `test_verify_chain_*` |
| SRS-008-VERIFY-27 | `cm_verify_chain()` | `test_verify_tamper_*` |

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
