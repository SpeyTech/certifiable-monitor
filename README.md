# certifiable-monitor

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/SpeyTech/certifiable-monitor)
[![Tests](https://img.shields.io/badge/tests-253%20passing-brightgreen)](https://github.com/SpeyTech/certifiable-monitor)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-x86%20%7C%20ARM%20%7C%20RISC--V-lightgrey)](https://github.com/SpeyTech/certifiable-monitor)

**Deterministic runtime monitoring for safety-critical ML inference.**

Pure C99. Zero dynamic allocation. Certifiable for DO-178C, IEC 62304, and ISO 26262.

---

## The Problem

Deploying ML models in safety-critical systems isn't just about correct inference—it's about *knowing* when the model is operating outside its certified envelope:

- **Input drift** — Real-world data shifts from training distribution
- **Activation anomalies** — Internal values exceed certified bounds  
- **Output drift** — Prediction patterns change unexpectedly
- **Silent failures** — No audit trail when things go wrong

Current monitoring solutions are non-deterministic, rely on floating-point statistics, and provide no cryptographic proof of execution.

**Read more:**
- [Why Floating Point Is Dangerous](https://speytech.com/ai-architecture/floating-point-danger/)
- [Bit-Perfect Reproducibility: Why It Matters and How to Prove It](https://speytech.com/insights/bit-perfect-reproducibility/)

## The Solution

`certifiable-monitor` provides **deterministic runtime monitoring** with:

### 1. Fixed-Point Drift Detection
Total Variation (TV), Jensen-Shannon Divergence (JSD), and Population Stability Index (PSI) computed in Q0.32 and Q16.16 fixed-point. Same inputs = same drift score, every platform.

**Read more:** [Fixed-Point Neural Networks: The Math Behind Q16.16](https://speytech.com/insights/fixed-point-neural-networks/)

### 2. Cryptographic Audit Ledger
Every monitoring event logged to a SHA-256 hash chain. Tamper-evident. Any event verifiable in O(1) time.

**Read more:** [Cryptographic Execution Tracing and Evidentiary Integrity](https://speytech.com/insights/cryptographic-proof-execution/)

### 3. Deterministic Health FSM
State machine with defined transitions: UNINIT → INIT → ENABLED → ALARM → DEGRADED → STOPPED. No ambiguous states.

### 4. Policy-Driven Reactions
JSON-defined Certified Operational Envelope (COE) maps violations to actions: LOG_ONLY, WARN_OPERATOR, CLAMP_OUTPUT, DEGRADE_MODE, EMERGENCY_STOP.

**Result:** `Monitor(input, policy) → (status, action, audit_entry)` — Monitoring is a pure function.

## Status

**All modules complete — 11/11 test suites passing (253 tests).**

| Module | Description | Tests | Status |
|--------|-------------|-------|--------|
| DVM Primitives | Fixed-point arithmetic, LUT log2 | 33 | ✅ |
| Audit Ledger | SHA-256 hash chain | 18 | ✅ |
| Drift Detectors | TV, JSD, PSI algorithms | 20 | ✅ |
| Policy Parser | COE JSON parsing, JCS hash | 25 | ✅ |
| Input Monitor | Feature range checking | 22 | ✅ |
| Activation Monitor | Layer bounds checking | 24 | ✅ |
| Output Monitor | Output envelope checking | 19 | ✅ |
| Health FSM | Monitor state machine | 19 | ✅ |
| Reaction Handler | Violation → action mapping | 14 | ✅ |
| Ledger Verification | Offline chain verification | 32 | ✅ |
| Bit Identity | Cross-platform verification | 27 | ✅ |

## Quick Start

### Build

All project tasks are available as Makefile targets, and GitHub Actions CI uses
these to ensure that they are not stale. Documentation of the commands are
available via `make help`.

When building the project for the first time, run `make deps`.

To building everything (i.e. config, build, test), run `make`. Otherwise, use
individual Makefile targets as desired.

```bash
$ make help
Makefile Usage:
  make <target>

Dependencies
  deps             Install project dependencies

Development
  config           Configure the build
  build            Build the project

Testing
  test             Run tests

Project Management
  install          Install the project
  release          Build release artifacts

Maintenance
  clean            Remove all build artifacts

Documentation
  help             Display this help
```

### Expected Output
```
100% tests passed, 0 tests failed out of 11
Total Test time (real) = 0.04 sec
```

### Basic Monitoring
```c
#include "ct_types.h"
#include "cm_types.h"
#include "policy.h"
#include "input.h"
#include "activation.h"
#include "output.h"
#include "health.h"
#include "react.h"
#include "ledger.h"

// All buffers pre-allocated (no malloc)
ct_fault_flags_t faults = {0};

// Load policy from JSON
cm_policy_t policy;
cm_policy_parse(policy_json, policy_len, &policy, &faults);

// Initialize ledger with attestation binding
cm_ledger_ctx_t ledger;
cm_ledger_init(&ledger);
cm_ledger_genesis(&ledger, policy.bundle_root, policy.policy_hash, &faults);

// Initialize monitors
cm_input_ctx_t input_mon;
cm_input_init(&input_mon, &policy.input);

cm_activation_ctx_t activ_mon;
cm_activation_init(&activ_mon, policy.layers, policy.layer_count);

cm_output_ctx_t output_mon;
cm_output_init(&output_mon, &policy.output);

// Initialize health FSM
cm_health_ctx_t health;
cm_health_init(&health, &policy.fault_budget);
cm_health_enable(&health);

// --- Per-inference monitoring ---

// Check input envelope
cm_input_result_t input_result;
cm_input_check(&input_mon, input_vector, num_features, &input_result, &faults);

if (input_result.violations > 0) {
    // Log violation to ledger
    uint8_t L_out[32];
    cm_ledger_append_violation(&ledger, window_id, CM_VIOL_INPUT_RANGE,
                               input_result.first_violation_idx,
                               input_result.first_violation_value,
                               input_result.first_violation_bound,
                               L_out, &faults);
    
    // Get reaction from policy
    cm_reaction_t action = cm_policy_get_reaction(&policy, CM_VIOL_INPUT_RANGE);
    
    // Update health FSM
    cm_health_report_violation(&health, CM_VIOL_INPUT_RANGE);
}

// Check activations after inference
cm_activation_result_t activ_result;
cm_activation_check(&activ_mon, layer_outputs, layer_faults, &activ_result, &faults);

// Check output envelope
cm_output_result_t output_result;
cm_output_check(&output_mon, output_vector, num_outputs, &output_result, &faults);

// Compute drift at window boundary
if (window_complete) {
    cm_drift_result_t drift;
    cm_detect_drift(runtime_counts, ref_counts, bin_count, 
                    &policy.drift, &drift, &faults);
    
    if (drift.flags & CM_DRIFT_TV_TRIGGERED) {
        cm_health_report_violation(&health, CM_VIOL_DRIFT);
    }
}

// Check health state
cm_health_state_t state = cm_health_get_state(&health);
if (state == CM_HEALTH_STOPPED) {
    // Emergency stop triggered - halt inference
}
```

## Architecture

### Pipeline Position

```
certifiable-data → certifiable-training → certifiable-quant → certifiable-deploy → certifiable-inference
                                                                                          ↓
                                                                              certifiable-monitor
                                                                                          ↓
                                                                                   Audit Ledger
```

The monitor receives:
1. **From certifiable-deploy:** Bundle root `R` and policy hash `H_P`
2. **From certifiable-inference:** Input vectors, activation values, output vectors, fault flags
3. **From policy:** Thresholds, envelopes, reaction mappings

### Drift Detection (CM-MATH-001 §2-4)

**Total Variation (TV):**
```
TV(p, q) = (1/2) Σ_b |p_b - q_b|
```
- No logarithms required — safest detector
- Output in Q0.32: 0 = identical, UINT32_MAX = disjoint
- O(B) time, O(1) space where B = number of bins

**Jensen-Shannon Divergence (JSD):**
```
JSD(p, q) = (1/2) KL(p ∥ m) + (1/2) KL(q ∥ m)
where m = (p + q) / 2
```
- Uses 512-entry LUT-based log2 for determinism
- Output in Q16.16: [0, 65536] representing [0, 1]
- Symmetric: JSD(p,q) = JSD(q,p)

**Population Stability Index (PSI):**
```
PSI(p, q) = Σ_b (p_b - q_b) ln(p_b / q_b)
```
- Epsilon smoothing prevents log(0)
- Unbounded; policy defines operational thresholds

**Read more:** [From Proofs to Code: Mathematical Transcription in C](https://speytech.com/insights/mathematical-proofs-to-code/)

### Ledger Hash Chain (CM-MATH-001 §6)

**Genesis (binds to deployment):**
```
L_0 = SHA256("CM:LEDGER:GENESIS:v1" ∥ R ∥ H_P)
```

**Entry hash:**
```
e_t = SHA256("CM:LEDGER:ENTRY:v1" ∥ E_t)
```

**Chain extension:**
```
L_t = SHA256("CM:LEDGER:v1" ∥ L_{t-1} ∥ e_t)
```

Any tampering, truncation, or reordering is detectable. Post-incident analysis can replay the entire monitoring history.

### Health FSM (CM-ARCH-MATH-001 §7)

```
UNINIT ──init──► INIT ──enable──► ENABLED
                                     │
                    ┌────alarm───────┤
                    ▼                │
                  ALARM ─────────────┤
                    │                │
                    └──degrade──► DEGRADED
                                     │
                    ┌────stop────────┘
                    ▼
                  STOPPED
```

Fault budgets define thresholds for state transitions. Once STOPPED, only manual intervention can restart.

### Fault Model

Every operation signals faults without silent failure:
```c
typedef struct {
    uint32_t overflow    : 1;  // Saturated high
    uint32_t underflow   : 1;  // Saturated low
    uint32_t div_zero    : 1;  // Division by zero
    uint32_t domain      : 1;  // Invalid input (log of 0)
    uint32_t input_range : 1;  // Input outside envelope
    uint32_t output_range: 1;  // Output outside envelope
    uint32_t policy_fail : 1;  // Policy validation failed
    uint32_t hash_fail   : 1;  // Hash verification failed
    uint32_t ledger_fail : 1;  // Ledger integrity failed
} ct_fault_flags_t;
```

**Read more:** [Closure, Totality, and the Algebra of Safe Systems](https://speytech.com/insights/closure-totality-algebra/)

## Test Coverage

| Test Suite | Tests | Coverage |
|------------|-------|----------|
| test_primitives | 33 | DVM operations, LUT log2 |
| test_ledger | 18 | Genesis, append, chain integrity |
| test_detectors | 20 | TV, JSD, PSI, histogram normalization |
| test_policy | 25 | JSON parsing, JCS hash, validation |
| test_input | 22 | Range checking, violation tracking |
| test_activation | 24 | Bounds checking, saturation detection |
| test_output | 19 | Envelope checking, drift accumulation |
| test_health | 19 | FSM transitions, fault budgets |
| test_react | 14 | Reaction mapping, action classification |
| test_verify | 32 | Chain verification, tamper detection |
| test_bit_identity | 27 | Cross-platform determinism |

**Total: 253 tests**

## Codebase Metrics

| Category | Files | Lines |
|----------|-------|-------|
| Source (.c) | 11 | 4,460 |
| Headers (.h) | 13 | 2,699 |
| Tests (.c) | 11 | 5,155 |
| Documentation (.md) | 11 | 1,397 |
| **Total** | **46** | **~13,700** |

## Documentation

- **CM-MATH-001.md** — Mathematical foundations (drift metrics, ledger hashing, log2 LUT)
- **CM-STRUCT-001.md** — Data structure specifications
- **CM-ARCH-MATH-001.md** — Architecture-level math (health FSM, window semantics)
- **docs/requirements/** — SRS documents (8 modules with full traceability)

## Related Projects

| Project | Description |
|---------|-------------|
| [certifiable-data](https://github.com/SpeyTech/certifiable-data) | Deterministic data pipeline |
| [certifiable-training](https://github.com/SpeyTech/certifiable-training) | Deterministic training engine |
| [certifiable-quant](https://github.com/SpeyTech/certifiable-quant) | Deterministic quantization |
| [certifiable-deploy](https://github.com/SpeyTech/certifiable-deploy) | Deterministic model packaging |
| [certifiable-inference](https://github.com/SpeyTech/certifiable-inference) | Deterministic inference engine |

Together, these projects provide a complete deterministic ML pipeline for safety-critical systems:
```
certifiable-data → certifiable-training → certifiable-quant → certifiable-deploy → certifiable-inference
                                                                                          ↓
                                                                              certifiable-monitor
```

## Why This Matters

### Medical Devices
IEC 62304 Class C requires traceable, reproducible software. Runtime monitoring must be deterministic and auditable.

**Read more:** [IEC 62304 Class C: What Medical Device Software Actually Requires](https://speytech.com/insights/iec-62304-class-c/)

### Autonomous Vehicles
ISO 26262 ASIL-D demands provable behavior. When drift is detected, the reaction must be deterministic.

**Read more:**
- [ISO 26262 and ASIL-D: The Role of Determinism](https://speytech.com/insights/iso-26262-asil-d-determinism/)
- [NVIDIA's ASIL-B Linux Kernel: The Path to ASIL-D](https://speytech.com/insights/nvidia-asil-determinism/)

### Aerospace
DO-178C Level A requires complete requirements traceability. "The model drifted so we logged a warning" is not certifiable—you need cryptographic proof.

**Read more:** [DO-178C Level A Certification: How Deterministic Execution Can Streamline Certification Effort](https://speytech.com/insights/do178c-certification/)

This is the first ML runtime monitor designed from the ground up for safety-critical certification.

## Deep Dives

Want to understand the engineering principles behind certifiable-monitor?

**Determinism & Reproducibility:**
- [Bit-Perfect Reproducibility: Why It Matters and How to Prove It](https://speytech.com/insights/bit-perfect-reproducibility/)
- [The ML Non-Determinism Problem](https://speytech.com/insights/ml-nondeterminism-problem/)
- [From Proofs to Code: Mathematical Transcription in C](https://speytech.com/insights/mathematical-proofs-to-code/)

**Safety-Critical Foundations:**
- [The Real Cost of Dynamic Memory in Safety-Critical Systems](https://speytech.com/insights/dynamic-memory-safety-critical/)
- [Closure, Totality, and the Algebra of Safe Systems](https://speytech.com/insights/closure-totality-algebra/)

**Production ML Architecture:**
- [A Complete Deterministic ML Pipeline for Safety-Critical Systems](https://speytech.com/ai-architecture/deterministic-ml-pipeline/)

## Compliance Support

This implementation is designed to support certification under:
- **DO-178C** (Aerospace software)
- **IEC 62304** (Medical device software)
- **ISO 26262** (Automotive functional safety)
- **IEC 61508** (Industrial safety systems)

For compliance packages and certification assistance, contact below.

## Contributing

We welcome contributions from systems engineers working in safety-critical domains. See [CONTRIBUTING.md](CONTRIBUTING.md).

**Important:** All contributors must sign a [Contributor License Agreement](CONTRIBUTOR-LICENSE-AGREEMENT.md).

## License

**Dual Licensed:**
- **Open Source:** GNU General Public License v3.0 (GPLv3)
- **Commercial:** Available for proprietary use in safety-critical systems

For commercial licensing and compliance documentation packages, contact below.

## Patent Protection

This implementation is built on the **Murray Deterministic Computing Platform (MDCP)**,
protected by UK Patent **GB2521625.0**.

MDCP defines a deterministic computing architecture for safety-critical systems,
providing:
- Provable execution bounds
- Resource-deterministic operation
- Certification-ready patterns
- Platform-independent behavior

**Read more:** [MDCP vs. Conventional RTOS](https://speytech.com/insights/mdcp-vs-conventional-rtos/)

For commercial licensing inquiries: william@fstopify.com

## About

Built by **SpeyTech** in the Scottish Highlands.

30 years of UNIX infrastructure experience applied to deterministic computing for safety-critical systems.

Patent: UK GB2521625.0 - Murray Deterministic Computing Platform (MDCP)

**Contact:**
William Murray  
william@fstopify.com  
[speytech.com](https://speytech.com)

**More from SpeyTech:**
- [Technical Articles](https://speytech.com/ai-architecture/)
- [Open Source Projects](https://speytech.com/open-source/)

---

*Building deterministic AI systems for when lives depend on the answer.*

Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.
