# CM-MATH-001

## certifiable-monitor: Mathematical Specification (Phase 1)

**Status:** ✅ Draft (Architecture-approved, ready for implementation)

**Scope:**
- Deterministic drift metrics TV, JSD, PSI over fixed bins
- Canonical, bit-level ledger entry layout and hashing
- Deterministic reaction mapping semantics (math-level)

**Assumptions:**
- SHA-256 collision resistance
- Windowing uses count windows by default (clock-free determinism)
- Histogram bin edges are fixed and stored in policy

---

## 1. Canonical Histogram Model

### 1.1 Binning

For each monitored signal s (input feature, class id, activation sentinel), policy defines:

- Bin edges: e₀ < e₁ < ⋯ < e_B
- Bins b = 1..B where value x maps to exactly one bin

**Deterministic bin assignment:**

| Condition | Bin |
|-----------|-----|
| x ∈ [e_{b-1}, e_b) | b |
| x ∈ [e_{B-1}, e_B] | B (last bin includes right edge) |

### 1.2 Counts

Calibration reference counts Q_b ∈ ℕ are stored in policy. Runtime window counts P_b ∈ ℕ computed by integer increments.

Let:
```
N_P = Σ_{b=1}^{B} P_b
N_Q = Σ_{b=1}^{B} Q_b
```

**Invariant:** N_P > 0 for any completed window; if a window is empty, it is rejected and not evaluated.

### 1.3 Normalized Probabilities (Fixed-Point)

We represent probabilities in Q0.32:

```
p_b = ⌊(P_b · 2³²) / N_P⌉
q_b = ⌊(Q_b · 2³²) / N_Q⌉
```

All subsequent drift metrics operate on p_b, q_b in Q0.32.

---

## 2. Total Variation Distance (TV)

### 2.1 Definition

```
TV(p, q) = (1/2) Σ_{b=1}^{B} |p_b - q_b|
```

### 2.2 Deterministic Computation in Q0.32

Compute:
```
S = Σ_{b=1}^{B} |p_b - q_b|  (Q0.32)
```

Then:
```
TV = S / 2
```

Implemented as a right shift by 1 with explicit rounding:
- Since Q0.32 stored in uint64 accumulator, compute `(S + 1) >> 1` if you want round-up
- Default is truncation (must be fixed in policy or spec)

### 2.3 Bounds

```
0 ≤ TV(p, q) ≤ 1
```

In Q0.32, max is 2³².

**No logarithms required. TV is the "safest" detector.**

---

## 3. Jensen–Shannon Divergence (JSD)

### 3.1 Definition

```
JSD(p, q) = (1/2) KL(p ∥ m) + (1/2) KL(q ∥ m)

m = (p + q) / 2
```

with:
```
KL(p ∥ m) = Σ_b p_b log(p_b / m_b)
```

### 3.2 Practical Certified Variant (Q0.32 + LUT log2)

To avoid floating logs, define:

1. Use log base 2: log₂(·)
2. Represent log outputs in Q16.16
3. Compute: `p_b log₂(p_b / m_b)` using fixed-point ratio + LUT log2

### 3.3 Zero Handling (Mandatory)

Direct KL is undefined for p_b > 0, m_b = 0. Since m_b = (p_b + q_b) / 2, m_b = 0 iff p_b = q_b = 0, so terms are safely defined as 0.

**Policy mandates:**
- If p_b = 0 then contribution is 0 (by continuity)
- If q_b = 0 then similar

### 3.4 Deterministic Algorithm

For each bin b:

**Step 1:** Compute m_b = (p_b + q_b) / 2 with deterministic rounding.

**Step 2:** Compute ratio r_p = p_b / m_b in Q16.16 using integer division:
```
r_p = ⌊(p_b · 2¹⁶) / m_b⌉
```

If p_b = 0, skip term. If m_b = 0, term is 0.

**Step 3:** Compute log₂(r_p) using LUT:
- Normalize r_p to range [1, 2) by shifting; track exponent k
- log₂(r_p) = k + log₂(r_norm)
- log₂(r_norm) from LUT indexed by top bits of fraction

**Step 4:** Multiply p_b (Q0.32) by log (Q16.16) → accumulate in Q16.48 in int64.

**Step 5:** Repeat for q ∥ m. Then scale by 1/2.

### 3.5 Output Representation

Emit JSD in Q16.16, representing value in [0, 1] approximately (bounded by log2 in natural base; in log2, bounded by 1).

**Certified claim:** Deterministic approximation with bounded LUT error. LUT error bounds belong in CM-MATH-001 Appendix A once LUT resolution is chosen.

---

## 4. Population Stability Index (PSI)

### 4.1 Definition

```
PSI(p, q) = Σ_b (p_b - q_b) ln(p_b / q_b)
```

### 4.2 Certified Variant (log2, smoothing)

PSI is sensitive to zeros. In certifiable monitoring we mandate epsilon smoothing:

Let ε be a fixed Q0.32 constant stored in policy (e.g., ε = 1/2³² or a small count-based epsilon).

Define:
```
p̃_b = max(p_b, ε)
q̃_b = max(q_b, ε)
```

Then compute log ratio via log2 LUT:
```
ln(p̃_b / q̃_b) = log₂(p̃_b) - log₂(q̃_b)
```

Convert to natural log if desired via multiply by ln(2) (Q16.16 constant).

### 4.3 Deterministic Algorithm (Q0.32 + log LUT)

For each bin:

1. Compute d_b = p_b - q_b in signed Q0.32
2. Compute ℓ_b = ln(p̃_b / q̃_b) in Q16.16
3. Accumulate d_b · ℓ_b in Q16.48

Emit PSI in Q16.16.

**Note:** PSI is unbounded in principle; policy must define expected operational thresholds for alarms.

---

## 5. Decision Rules (Deterministic)

For each detector output S_k (TV/JSD/PSI) computed per window:

```
S_k > τ ⇒ Violation_Drift
```

Threshold τ stored in policy in same fixed-point format as S_k.

**Hysteresis (optional)** must be explicitly specified:
- Trigger at τ_high
- Clear at τ_low

---

## 6. Audit Ledger: Canonical Entry Encoding

### 6.1 Canonical Ledger Entry E_t

Ledger entry is a fixed binary struct serialized in little-endian with no padding, plus variable payload bytes with explicit length.

Define:
```
E_t = Header ∥ Payload
```

**Header fields (fixed):**

| Field | Type | Description |
|-------|------|-------------|
| seq | u64 | Monotonic sequence number |
| window_id | u64 | Window identifier |
| event_type | u32 | Event type enum |
| payload_len | u32 | Length of payload bytes |
| time_tick | u64 | Monotonic tick (0 in count-window mode) |
| bundle_root | u8[32] | R (attestation root) |
| policy_hash | u8[32] | H_P (policy hash) |

**Payload** is event-specific, but must be:
- Deterministic encoding
- Length-delimited
- Little-endian

### 6.2 Ledger Hash Chain

**Genesis:**
```
L_0 = H("CM:LEDGER:GENESIS:v1" ∥ R ∥ H_P)
```

**Entry hash:**
```
e_t = H("CM:LEDGER:ENTRY:v1" ∥ E_t)
```

**Chain:**
```
L_t = H("CM:LEDGER:v1" ∥ L_{t-1} ∥ e_t)
```

---

## 7. Reaction Semantics (Math-Level)

Let violation types be an enum set V. Policy defines mapping:

```
R: V → A
```

where actions A are deterministic.

**Soundness invariant:**

If an action disables inference:
```
R(v) = EmergencyStop ⇒ ¬InferenceEnabled
```

and the action must be logged as an event (ledger entry).

---

## Appendix A: Deterministic Logarithm (log₂) Specification

This specification governs the implementation of `cm_log2_q16` and the fixed-point logarithm used in JSD and PSI computations.

### A.1 Strategy

- **Normalization:** Input x is normalized to range [1, 2) via bitwise shift (x = m · 2^k)
- **Decomposition:** log₂(x) = k + log₂(m)
- **Approximation:** log₂(m) is computed via a Look-Up Table (LUT) with Linear Interpolation

### A.2 Look-Up Table (LUT) Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| Input Domain | m ∈ [1.0, 2.0) | Normalized mantissa range |
| Table Size | N = 512 entries (2⁹) | Trade-off: accuracy vs memory |
| Index Mapping | i = ⌊(m - 1.0) · 512⌋ | 9-bit index extraction |
| Stored Values | L[i] = ⌊log₂(1.0 + i/512) · 2¹⁶⌋ | Q16.16 fractional part |
| Storage Type | uint16_t | Integer part always 0 for [1,2) |

### A.3 Interpolation Formula

For input value v in Q16.16:

```c
/* Extract 9-bit index from fractional part */
uint32_t idx = (v >> 7) & 0x1FF;

/* Get table values */
uint16_t y0 = log2_lut[idx];
uint16_t y1 = log2_lut[idx + 1];  /* idx < 511 */

/* 7-bit fractional interpolation weight */
uint32_t frac = v & 0x7F;

/* Linear interpolation with truncation */
int32_t result = y0 + (((int32_t)(y1 - y0) * frac) >> 7);
```

### A.4 Error Budget (Q16.16)

| Error Source | Magnitude | Notes |
|--------------|-----------|-------|
| Interpolation Error | ≤ 0.7 × 10⁻⁶ | Max error for log₂ with h=1/512 |
| Quantization Error | ±0.5 LSB | LUT values rounded to nearest |
| Arithmetic Error | ±1 LSB | Intermediate truncation |
| **Certified Bound** | **±2 LSBs** | Total worst-case deviation |

**Certified Claim:** The output of `cm_log2_q16(x)` SHALL NOT deviate from the true mathematical ⌊log₂(x) · 2¹⁶⌋ by more than ±2 LSBs.

### A.5 Reference Test Vectors

These values MUST pass for implementation conformance:

| Input (Q16.16) | True log₂ | Expected Output | Tolerance |
|----------------|-----------|-----------------|-----------|
| 0x10000 (1.0) | 0.0 | 0x00000 | Exact |
| 0x20000 (2.0) | 1.0 | 0x10000 | Exact |
| 0x18000 (1.5) | 0.58496 | 0x95C0 (38336) | ±2 |
| 0x14000 (1.25) | 0.32193 | 0x5269 (21097) | ±2 |
| 0x1C000 (1.75) | 0.80735 | 0xCE99 (52889) | ±2 |

### A.6 Determinism Guarantee

The implementation is deterministic because:

1. **Fixed Domain:** Input normalized to [1, 2) using `clz` (count leading zeros), a pure bitwise operation
2. **Fixed Grid:** Step size between LUT entries is exactly 1/512, computed via integer shift
3. **Fixed Rounding:** Linear interpolation uses integer truncation (>> operator)
4. **No Floating-Point:** Entire computation uses 32-bit integers only

**Platform Independence:** This implementation produces bit-identical results on:
- x86-64 (little-endian, 64-bit)
- ARM Cortex-M (little-endian, 32-bit)
- RISC-V (little-endian, 32/64-bit)

### A.7 LUT Generation

The LUT SHALL be generated offline using the following algorithm:

```python
import math

LUT_SIZE = 512

def generate_log2_lut():
    lut = []
    for i in range(LUT_SIZE + 1):  # +1 for interpolation
        x = 1.0 + i / LUT_SIZE      # x in [1.0, 2.0]
        log_val = math.log2(x)       # log₂(x) in [0.0, 1.0)
        q16 = int(round(log_val * 65536))  # Convert to Q16.16
        lut.append(min(q16, 65535))  # Clamp to uint16_t
    return lut
```

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
