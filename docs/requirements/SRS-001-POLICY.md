# SRS-001-POLICY

## certifiable-monitor: COE Policy Schema & Emission Requirements (Phase 1)

**Status:** ✅ Draft

**Purpose:** Define a canonical policy JSON that certifiable-quant can emit and certifiable-monitor can consume deterministically.

**Traceability:** CM-ARCH-MATH-001 §1, CM-MATH-001 §5, CM-STRUCT-001 §1

---

## 1. Canonicalization

### SRS-001-POLICY-01: JCS Serialization

Policy SHALL be serialized using JCS (RFC 8785) before hashing to produce H_P.

**Rationale:** Canonical JSON ensures identical policy content produces identical bytes produces identical hash.

**Verification:** V-POL-01 — Round-trip test: parse → serialize → hash must be deterministic.

### SRS-001-POLICY-02: Bundle Inclusion

The bundled policy bytes MUST be canonical JCS.

**Rationale:** Non-canonical bytes would produce different H_P from semantically identical policy.

**Verification:** V-POL-02 — Canonical check on policy bytes before acceptance.

---

## 2. Required Fields (v1)

### SRS-001-POLICY-03: Version Field

Policy SHALL include `policy_version` field with value `1`.

**Verification:** V-POL-03 — Reject policies with missing or unsupported version.

### SRS-001-POLICY-04: Window Size

Policy SHALL include `window_size` field specifying N samples per window.

**Constraints:**
- Type: positive integer
- Range: 1 ≤ window_size ≤ 2³¹ - 1

**Verification:** V-POL-04 — Bounds check on parse.

### SRS-001-POLICY-05: Input Envelope

Policy SHALL include `input` object containing:
- `feature_count`: number of input features
- `ranges_q16`: per-feature min/max bounds in Q16.16
- `hists` (optional): per-feature histogram specifications

**Verification:** V-POL-05 — Schema validation; array lengths match feature_count.

### SRS-001-POLICY-06: Layer Contracts

Policy SHALL include `layers` array containing monitored layer contracts.

Each layer contract SHALL include:
- `layer_id`: unique identifier
- `min_q16`: minimum activation bound
- `max_q16`: maximum activation bound
- `tol_rate_q0_32`: tolerated violation rate
- `max_over_q16`: maximum overrange magnitude

**Verification:** V-POL-06 — Schema validation; layer_id uniqueness check.

### SRS-001-POLICY-07: Drift Configuration

Policy SHALL include `drift` object containing:
- `enabled`: list of enabled detectors ("tv", "jsd", "psi")
- `tv_threshold_q0_32`: Total Variation threshold
- `jsd_threshold_q16_16`: Jensen-Shannon threshold
- `psi_threshold_q16_16`: PSI threshold
- `epsilon_q0_32`: smoothing constant for PSI

**Verification:** V-POL-07 — At least one detector enabled; thresholds non-negative.

### SRS-001-POLICY-08: Reaction Map

Policy SHALL include `reaction_map` array mapping violations to actions.

Each entry SHALL include:
- `violation`: violation type string
- `action`: action type string

**Valid violation types:**
- `input_range`
- `input_drift`
- `activation_range`
- `activation_saturation`
- `output_range`
- `output_drift`
- `fault_budget`

**Valid action types:**
- `log_only`
- `warn_operator`
- `clamp_and_log`
- `degrade_mode`
- `emergency_stop`

**Verification:** V-POL-08 — All violations covered; no unknown types.

---

## 3. JSON Schema (Normative Shape)

```json
{
  "policy_version": 1,
  "window_size": 256,
  "input": {
    "feature_count": 128,
    "ranges_q16": {
      "min": [-65536, -65536, "..."],
      "max": [65536, 65536, "..."]
    },
    "hists": [
      {
        "bin_edges_q16": [-65536, 0, 65536],
        "ref_counts": [100, 100]
      }
    ]
  },
  "layers": [
    {
      "layer_id": 3,
      "min_q16": -65536,
      "max_q16": 65536,
      "tol_rate_q0_32": 0,
      "max_over_q16": 0
    }
  ],
  "drift": {
    "enabled": ["tv", "jsd"],
    "tv_threshold_q0_32": 42949673,
    "jsd_threshold_q16_16": 1311,
    "psi_threshold_q16_16": 0,
    "epsilon_q0_32": 1
  },
  "reaction_map": [
    { "violation": "input_range", "action": "clamp_and_log" },
    { "violation": "input_drift", "action": "warn_operator" },
    { "violation": "activation_range", "action": "warn_operator" },
    { "violation": "output_range", "action": "clamp_and_log" },
    { "violation": "fault_budget", "action": "emergency_stop" }
  ]
}
```

---

## 4. Policy Hash Computation

### SRS-001-POLICY-09: Hash Algorithm

H_P SHALL be computed as:

```
H_P = SHA256("CM:POLICY:v1" ∥ canonical_policy_bytes)
```

**Verification:** V-POL-09 — Known test vector produces expected hash.

### SRS-001-POLICY-10: Hash Binding

H_P SHALL be included in the deployment bundle and verified by CD-LOAD before monitor enables.

**Verification:** V-POL-10 — Monitor rejects if H_P mismatch detected.

---

## 5. Policy Parsing Requirements

### SRS-001-POLICY-11: Strict Parsing

Parser SHALL reject policies with:
- Unknown fields (strict mode)
- Missing required fields
- Type mismatches
- Out-of-range values

**Verification:** V-POL-11 — Malformed policy test suite.

### SRS-001-POLICY-12: Deterministic Parsing

Given identical policy bytes, parsing SHALL produce identical cm_policy_t structure.

**Verification:** V-POL-12 — Parse twice, memcmp structures.

---

## 6. Emission from certifiable-quant

### SRS-001-POLICY-13: Quant Certificate Binding

certifiable-quant MAY emit a COE policy template based on:
- Calibration ranges (input envelope)
- Activation ranges per layer
- Coverage metrics

**Verification:** V-POL-13 — Quant-emitted policy passes validation.

### SRS-001-POLICY-14: Human Review

Emitted policy SHOULD be reviewed and thresholds tuned before deployment.

**Rationale:** Automatic thresholds may be too strict or too loose for operational context.

---

## 7. Verification Matrix

| Requirement | Test | Method |
|-------------|------|--------|
| SRS-001-POLICY-01 | test_policy_jcs_canonical | Round-trip hash comparison |
| SRS-001-POLICY-02 | test_policy_bundle_canonical | Canonical byte check |
| SRS-001-POLICY-03 | test_policy_version_required | Missing field rejection |
| SRS-001-POLICY-04 | test_policy_window_bounds | Bounds validation |
| SRS-001-POLICY-05 | test_policy_input_schema | Schema validation |
| SRS-001-POLICY-06 | test_policy_layers_unique | Uniqueness check |
| SRS-001-POLICY-07 | test_policy_drift_enabled | At least one detector |
| SRS-001-POLICY-08 | test_policy_reaction_complete | Coverage check |
| SRS-001-POLICY-09 | test_policy_hash_vector | Known vector |
| SRS-001-POLICY-10 | test_policy_hash_binding | Mismatch rejection |
| SRS-001-POLICY-11 | test_policy_strict_parse | Malformed rejection |
| SRS-001-POLICY-12 | test_policy_parse_determinism | Memcmp equality |

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
