# CM-ARCH-MATH-001

## certifiable-monitor: Closed Mathematical Architecture Design

**Status:** 📄 Draft for Review (incorporates Audit Ledger + Reaction Interface)

**Purpose:** Runtime drift detection and envelope enforcement for deterministic inference systems, with tamper-evident audit logging and fail-safe reaction policies.

---

### Dependencies

| Project | Provides |
|---------|----------|
| certifiable-deploy | CBF bundle, attestation root R, secure loader (CD-LOAD) |
| certifiable-quant | Calibration ranges, activation ranges, coverage metrics, quant certificate |
| certifiable-inference | Deterministic execution, fault flags, saturation/clamp signals |

### Outputs

- Deterministic monitoring decisions (ALARM/WARN/OK)
- Hash-chained audit ledger (flight recorder)
- Reaction actions (clamp / degrade / stop / operator alert) per COE policy
- Machine-readable monitoring certificate report (optional periodic snapshot)

---

## 0. Certification Claims

### C0. Policy Integrity (Chain of Custody)

The monitor policy (COE) is packaged and hashed into the deployment bundle, verified by CD-LOAD. Monitor decisions are therefore bound to the certified configuration.

### C1. Deterministic Monitoring

Given identical inputs, model, policy, and windowing schedule, monitor metrics and alarms are bit-identical across compliant targets.

### C2. Behavioral Closure (Dynamic Safety)

If monitor reports "IN-ENVELOPE" continuously, then observed runtime behavior remains within the Certified Operating Envelope (COE) assumptions that underpin the quantization/inference certificates.

### C3. Tamper-Evident Evidence (Black Box)

All monitoring events are recorded into a hash-chained audit ledger such that tampering, truncation, or deletion is detectable.

### C4. Reaction Soundness

When a violation is detected, the system transitions to a COE-defined Fail-Safe State deterministically, and records that transition in the audit ledger.

---

## 1. Core Object: Certified Operating Envelope (COE)

### 1.1 COE Definition

A COE is a signed/hashed policy object P containing:

```
P = (X_cert, {A_ℓ}, Y_cert, D, R, W, E)
```

Where:

| Symbol | Description |
|--------|-------------|
| X_cert | Input envelope (range + distribution reference) |
| A_ℓ | Activation envelope per monitored layer ℓ |
| Y_cert | Output envelope (range + distribution reference) |
| D | Drift detectors (metrics + thresholds) |
| R | Reaction policy mapping (violation → action) |
| W | Windowing scheme (N-sample or time-based + update rules) |
| E | Event budget thresholds (fault flags, saturations, clamps) |

### 1.2 Policy Hash Binding

COE policy bytes are hashed and included in deploy bundle as component hash H_P (recommended) or under H_I (acceptable, less clean). In certified mode:

```
H_P is verified by CD-LOAD before monitor enables
```

---

## 2. Deterministic Streaming Windows

### 2.1 Window Schedule

Let inference events occur at discrete indices t = 1, 2, …. Define window W_k as:

**Count window:**
```
W_k = {t : (k-1)N + 1 ≤ t ≤ kN}
```

**Time window:** Fixed T seconds, using monotonic clock quantized to integer ticks (must be specified).

**Certified default:** Count windows (removes clock nondeterminism).

### 2.2 Streaming Update Rule

All monitor statistics must admit an update function:

```
S_k = Update(S_{k-1}, sample_t)
```

using integer arithmetic (or strictly defined fixed-point).

---

## 3. Input Monitoring

### 3.1 Hard Range Contract (Membership)

For input vector x ∈ ℝ^d, COE provides per-feature bounds L_i, U_i.

Define violation indicator:

```
v_i(x) = 𝟙[x_i < L_i ∨ x_i > U_i]
```

Total violation count:

```
V(x) = Σ_{i=1}^{d} v_i(x)
```

Deterministic decision rule (example):

```
If V(x) > 0 ⇒ Violation_Input_Range
```

(Policy may allow "tolerated" violations via V(x) ≤ k, but default is fail-closed.)

### 3.2 Distribution Drift on Inputs (Histogram-Based)

COE defines fixed bin edges per feature (or per embedding). For feature i, bins b = 1..B. Calibration reference distribution q^(i) stored as counts Q_b.

Runtime window counts P_b computed deterministically.

Define normalized distributions:

```
p_b = P_b / Σ_j P_j
q_b = Q_b / Σ_j Q_j
```

**Detectors (choose one per policy):**

**Total Variation:**
```
TV(p, q) = (1/2) Σ_b |p_b - q_b|
```

**Jensen–Shannon (bounded, symmetric):**
```
JSD(p, q) = (1/2) KL(p ∥ m) + (1/2) KL(q ∥ m),  m = (p + q) / 2
```

**PSI (industry drift score):**
```
PSI(p, q) = Σ_b (p_b - q_b) ln(p_b / q_b)
```

**Determinism rule:** All logs/ratios computed via fixed-point approximations with bounded error, or via precomputed LUTs; the chosen method must be recorded in policy.

**Alarm:**
```
Drift(W_k) ≜ D(p, q) > τ
```

---

## 4. Activation Monitoring (Layer Contracts)

For each monitored layer ℓ, COE includes bounds [L_ℓ, U_ℓ] expressed in the layer's fixed-point format.

For activation tensor a_ℓ with elements a_{ℓj}:

**Violation rate:**
```
S_ℓ = (1/N_ℓ) Σ_{j=1}^{N_ℓ} 𝟙[a_{ℓj} < L_ℓ ∨ a_{ℓj} > U_ℓ]
```

**Maximum over-range magnitude:**
```
M_ℓ = max_j max(0, a_{ℓj} - U_ℓ, L_ℓ - a_{ℓj})
```

**Critical invariant (Envelope falsification):**
If runtime observes values outside certified envelope beyond tolerance, then calibration assumptions are violated.

**Events:**

| Event | Condition |
|-------|-----------|
| Violation_Activation_Range(ℓ) | S_ℓ > τ_ℓ |
| Violation_Activation_Max(ℓ) | M_ℓ > m_ℓ |
| Event_Saturation(ℓ) | Requantization clamp occurred (surfaced by inference engine) |

---

## 5. Output Monitoring

### 5.1 Output Range Contract

For outputs y, COE provides bounds [L_y, U_y] or per-dimension bounds.

**Violation:**
```
V_y(y) = Σ_i 𝟙[y_i < L_{y,i} ∨ y_i > U_{y,i}]
```

### 5.2 Output Distribution Drift

**For classification:** Track predicted class counts; compare to calibration reference distribution using TV/JSD/PSI.

**For regression:** Track:
- Mean, variance bounds
- Rate-of-change proxy:

```
R_t = ‖y_t - y_{t-1}‖ / (‖x_t - x_{t-1}‖ + ε)
```

and alarm on deviation beyond policy thresholds.

---

## 6. Health Signals (Fault Flags and Event Budgets)

Let fault flags from inference be f_t ∈ {0,1}^k (saturation, overflow guard, range clamp, etc.). Define window fault count:

```
F(W_k) = Σ_{t ∈ W_k} ‖f_t‖_1
```

And per-flag counts F_i(W_k).

COE includes budgets B_i per window or per hour/day.

**Violation:**
```
F_i(W_k) > B_i ⇒ Violation_FaultBudget(i)
```

---

## 7. Black Box (Cryptographic Audit Ledger)

*This is the flight recorder.*

### 7.1 Ledger Entry Structure

Each runtime event produces an entry E_t with canonical encoding:

```
E_t = (seq = t, event_type, payload, time_tick)
```

| Field | Description |
|-------|-------------|
| seq | Monotonic integer |
| time_tick | Deterministic monotonic tick count (or 0 in purely count-window mode) |
| payload | Measured stats (e.g., TV score), layer id, counts, etc. |

### 7.2 Hash Chain Definition

Let L_t be the ledger digest at step t, with fixed domain separation:

```
L_t = H("CM:LEDGER:v1" ‖ L_{t-1} ‖ H(E_t))
```

**Base case:**
```
L_0 = H("CM:LEDGER:GENESIS:v1" ‖ R ‖ H_P)
```

Where:
- R is the deploy attestation root (Merkle root) of the bundle actually loaded
- H_P is the COE policy hash

This binds logs to the exact deployed artifact and policy.

### 7.3 Tamper/Truncation Detection

Given L_t, any deletion/reordering/modification of events changes L_t. If a verifier later checks the chain, truncation is detectable unless attacker can break SHA-256 collision resistance.

### 7.4 Checkpointing (Optional)

To support bounded storage, the ledger may periodically emit signed checkpoints:

```
C_k = (k, L_{t_k})
```

Optionally signed by a device key (future extension).

---

## 8. Reaction Interface (Fail-Safe State)

*Monitoring is not complete without deterministic reaction semantics.*

### 8.1 Reaction Policy Mapping

COE contains a mapping:

```
R: ViolationType → Action
```

**Example (policy-defined):**

| Violation | Action |
|-----------|--------|
| Violation_Input_Range | Action_Clamp_And_Log |
| Violation_Inference_Drift | Action_Warn_Operator |
| Violation_Safety_Limit | Action_Emergency_Stop |

### 8.2 Action Semantics (Deterministic)

Actions are executed by the runtime integration boundary (monitor ↔ system controller). Actions must be:

- Deterministic
- Logged in the ledger as events
- Fail-closed on inability to perform

**Minimum action set:**

| Action | Description |
|--------|-------------|
| Action_LogOnly | Record event, no change |
| Action_WarnOperator | Raise operator alert + log |
| Action_ClampOutput | Apply deterministic clamp y ← Π_{Y_safe}(y) |
| Action_DegradeMode | Switch to reduced capability mode (policy-defined) |
| Action_EmergencyStop | Disable inference API; require reload/recovery |

### 8.3 Safe State Invariant

If a violation mapped to emergency stop occurs:

```
Violation ∧ R(Violation) = Stop ⇒ ¬InferenceEnabled
```

The transition must be recorded in the ledger.

---

## 9. Secure Enablement Contract (Monitor + Loader)

*Monitor must not operate on unverified artifacts.*

### 9.1 Enablement Preconditions

Monitor becomes active only if:

1. CD-LOAD verification succeeded (weights/kernels verified)
2. Policy hash H_P verified from bundle
3. Genesis ledger L_0 computed from R and H_P

---

## 10. Verification of Monitoring (Post-Incident Audit)

Given:
- Bundle attestation root R
- Policy hash H_P
- Ledger entries E_1..E_t
- Final digest L_t

An auditor recomputes:
1. L_0 and L_1..L_t
2. Checks L_t matches reported final digest/checkpoints
3. Validates policy and artifact binding

**This enables:**
- Proof logs were not tampered with
- Proof that reactions occurred as specified
- Evidence of when envelope violations began

---

## 11. Module Structure (Proposed)

| Module | Purpose |
|--------|---------|
| policy/ | COE schema, parsing, canonical encoding, policy hash |
| input/ | Range + drift detectors |
| activation/ | Layer probes, counters, saturation hooks |
| output/ | Output envelope + drift detectors |
| health/ | Fault flag aggregation + budgets |
| ledger/ | Hash-chained flight recorder, checkpoints |
| react/ | Reaction interface and deterministic actions |
| verify/ | Offline verification of ledgers + policy binding (auditor tooling) |

---

## 12. Open Questions (Explicitly Deferred, But Framed)

| Question | Notes |
|----------|-------|
| Numeric method for divergence | Fixed-point log/ratio LUT vs polynomial; must be fixed in CM-MATH-001 |
| Layer selection | Monitor all layers vs sentinel layers; trade-off cost vs coverage |
| Storage model | Ring buffer + signed checkpoints vs continuous storage |
| Operator channel | How warnings are routed (out of scope but interface must exist) |

---

## 13. Minimal Test Suites (Phase 1)

| Test | Description |
|------|-------------|
| test_policy_canonical_hash | Same COE → same H_P |
| test_range_membership | Hard bounds violations |
| test_hist_drift_tv | Known histograms → known TV score |
| test_activation_contract | Forced clamp → violation + ledger entry |
| test_fault_budget | Budget exceed triggers reaction |
| test_ledger_chain | Tamper/truncate detection |
| test_reaction_mapping | Violation → correct action + logged |
| test_end_to_end_binding | L_0 binds to R and H_P |

---

## Summary: What the Audit Ledger and Reaction Interface Add

**Black Box Ledger** ensures the monitor itself is auditable: "who watches the watcher" is solved cryptographically.

**Reaction Interface** ensures monitoring has an operational meaning, not just telemetry, and it becomes part of the certified operating envelope.

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
