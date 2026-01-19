# SRS-006-LEDGER

## Software Requirements Specification: Audit Ledger

**Document ID:** SRS-006-LEDGER  
**Version:** 1.0  
**Status:** ✅ Approved  
**Traceability:** CM-MATH-001 §6, CM-ARCH-MATH-001 §7

---

## 1. Purpose

This document specifies the software requirements for the Audit Ledger module (`src/audit/ledger.c`), which provides a tamper-evident hash-chained flight recorder for monitoring events.

---

## 2. Scope

The Audit Ledger SHALL:
- Compute cryptographic genesis binding to deployment artifacts
- Maintain hash chain integrity for all entries
- Provide deterministic serialization of ledger entries
- Support various event types with structured payloads

---

## 3. Requirements

### 3.1 Initialization

**SRS-006-LEDGER-01:** The function `cm_ledger_init()` SHALL initialize a ledger context with:
- Chain digest L_prev zeroed
- Sequence counter set to 0
- Initialized flag set to false

**SRS-006-LEDGER-02:** The function `cm_ledger_genesis()` SHALL compute the genesis digest:
```
L_0 = SHA256("CM:LEDGER:GENESIS:v1" ∥ R ∥ H_P)
```
Where R is the bundle attestation root and H_P is the policy hash.

**SRS-006-LEDGER-03:** After genesis, the context SHALL:
- Store R and H_P for future entries
- Set L_prev to L_0
- Set seq_next to 1
- Set initialized to true

### 3.2 Entry Structure

**SRS-006-LEDGER-04:** The ledger header (`cm_ledger_header_t`) SHALL contain exactly 96 bytes:

| Field | Type | Size | Description |
|-------|------|------|-------------|
| seq | uint64_t | 8 | Monotonic sequence number |
| window_id | uint64_t | 8 | Window identifier |
| event_type | uint32_t | 4 | Event type enum |
| payload_len | uint32_t | 4 | Payload length in bytes |
| time_tick | uint64_t | 8 | Monotonic tick (0 for count-window) |
| bundle_root | uint8_t[32] | 32 | R |
| policy_hash | uint8_t[32] | 32 | H_P |

**SRS-006-LEDGER-05:** Header serialization SHALL use little-endian byte order with no padding.

### 3.3 Hash Chain

**SRS-006-LEDGER-06:** Entry hash computation SHALL follow:
```
e_t = SHA256("CM:LEDGER:ENTRY:v1" ∥ Header ∥ Payload)
```

**SRS-006-LEDGER-07:** Chain advancement SHALL follow:
```
L_t = SHA256("CM:LEDGER:v1" ∥ L_{t-1} ∥ e_t)
```

**SRS-006-LEDGER-08:** The function `cm_ledger_append()` SHALL:
- Verify context is initialized
- Build entry with auto-assigned sequence number
- Compute entry hash e_t
- Compute new chain digest L_t
- Update context (L_prev, seq_next)
- Return L_t to caller

### 3.4 Event Types

**SRS-006-LEDGER-09:** The ledger SHALL support the following event types:

| Event | Value | Description |
|-------|-------|-------------|
| CM_EVENT_GENESIS | 0x00 | Ledger initialization |
| CM_EVENT_WINDOW_OK | 0x01 | Window completed normally |
| CM_EVENT_VIOL_INPUT | 0x10 | Input range violation |
| CM_EVENT_VIOL_ACTIV | 0x11 | Activation range violation |
| CM_EVENT_VIOL_OUTPUT | 0x12 | Output range violation |
| CM_EVENT_VIOL_DRIFT | 0x13 | Drift threshold exceeded |
| CM_EVENT_VIOL_FAULT | 0x14 | Fault budget exceeded |
| CM_EVENT_REACT_WARN | 0x20 | Warning issued |
| CM_EVENT_REACT_CLAMP | 0x21 | Output clamped |
| CM_EVENT_REACT_DEGRADE | 0x22 | Degraded mode entered |
| CM_EVENT_REACT_STOP | 0x23 | Emergency stop |
| CM_EVENT_CHECKPOINT | 0x30 | Periodic checkpoint |

### 3.5 Convenience Functions

**SRS-006-LEDGER-10:** `cm_ledger_append_window_ok()` SHALL create a window-OK entry with payload:
- window_id (8 bytes)
- sample_count (4 bytes)
- max_tv (4 bytes)

**SRS-006-LEDGER-11:** `cm_ledger_append_violation()` SHALL create a violation entry with payload:
- violation type (4 bytes)
- feature_or_layer (4 bytes)
- observed value (4 bytes)
- violated bound (4 bytes)

**SRS-006-LEDGER-12:** `cm_ledger_append_drift()` SHALL create a drift entry with payload:
- feature_id (4 bytes)
- tv_q0_32 (4 bytes)
- jsd_q16_16 (4 bytes)
- psi_q16_16 (4 bytes)
- triggered_flags (4 bytes)

**SRS-006-LEDGER-13:** `cm_ledger_append_reaction()` SHALL create a reaction entry with payload:
- violation type (4 bytes)
- action taken (4 bytes)

### 3.6 Query Functions

**SRS-006-LEDGER-14:** `cm_ledger_get_digest()` SHALL return the current chain digest L_prev.

**SRS-006-LEDGER-15:** `cm_ledger_get_seq()` SHALL return the next sequence number.

---

## 4. Error Handling

**SRS-006-LEDGER-16:** All functions SHALL return `CT_ERR_NULL` for null pointer arguments.

**SRS-006-LEDGER-17:** `cm_ledger_append()` SHALL return `CT_ERR_STATE` if context is not initialized.

**SRS-006-LEDGER-18:** Hash computation failures SHALL set `faults->ledger_fail`.

**SRS-006-LEDGER-19:** The ledger SHALL NOT allocate memory; payloads are caller-provided.

---

## 5. Determinism Requirements

**SRS-006-LEDGER-20:** All hash computations SHALL produce bit-identical results across platforms.

**SRS-006-LEDGER-21:** Serialization SHALL be deterministic (fixed byte order, no padding).

**SRS-006-LEDGER-22:** Domain separation tags SHALL be constant strings with no trailing null in hash.

---

## 6. Security Properties

**SRS-006-LEDGER-23:** The hash chain SHALL detect any modification to past entries.

**SRS-006-LEDGER-24:** The hash chain SHALL detect any truncation of entries.

**SRS-006-LEDGER-25:** The genesis SHALL cryptographically bind the ledger to the deployment bundle.

---

## 7. Traceability Matrix

| Requirement | Implementation | Test |
|-------------|----------------|------|
| SRS-006-LEDGER-01 | `cm_ledger_init()` | `test_ledger_init_*` |
| SRS-006-LEDGER-02 | `cm_ledger_genesis()` | `test_ledger_genesis_*` |
| SRS-006-LEDGER-06 | `cm_ledger_hash_entry()` | `test_ledger_hash_*` |
| SRS-006-LEDGER-07 | `cm_ledger_append()` | `test_ledger_chain_*` |
| SRS-006-LEDGER-23 | `cm_ledger_verify_chain()` | `test_ledger_tamper_*` |

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
