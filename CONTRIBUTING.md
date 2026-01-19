# Contributing to Certifiable Monitor

Thank you for your interest! We are building the world's first deterministic runtime monitoring system for safety-critical ML inference.

## 1. The Legal Bit (CLA)

All contributors must sign our **Contributor License Agreement (CLA)**.

**Why?** It allows SpeyTech to provide commercial licenses to companies that cannot use GPL code while keeping the project open source.

**How?** Our [CLA Assistant](https://cla-assistant.io/) will prompt you when you open your first Pull Request.

## 2. Coding Standards

All code must adhere to our **DVM Compliance Guidelines**:

- **No Dynamic Allocation:** Do not use `malloc`, `free`, or `realloc`
- **MISRA-C Compliance:** Follow MISRA-C:2012 guidelines
- **Explicit Types:** Use `int32_t`, `uint32_t`, not `int` or `long`
- **No Floating-Point:** All arithmetic must be pure fixed-point (Q16.16, Q0.32)
- **Bounded Loops:** All loops must have provable upper bounds
- **DVM Primitives Only:** Use `dvm_add64()`, `dvm_clamp32()`, `cm_log2_q16()`, etc.

## 3. The Definition of Done

A PR is only merged when:

1. ✅ It is linked to a **Requirement ID** in the SRS documents
2. ✅ It has **100% Branch Coverage** in unit tests
3. ✅ It passes our **Bit-Identity Test** (identical output on x86, ARM, RISC-V)
4. ✅ It is **MISRA-C compliant**
5. ✅ It traces to **CM-MATH-001** or **CM-STRUCT-001**
6. ✅ It has been reviewed by the Project Lead

## 4. Documentation

Every function must document:
- Purpose
- Preconditions
- Postconditions
- Complexity (O(1), O(n), etc.)
- Determinism guarantee
- Traceability reference

Example:
```c
/**
 * @brief Compute Total Variation distance between distributions
 *
 * @traceability CM-MATH-001 §2, SRS-002-DETECTORS
 *
 * Precondition: p and q are valid Q0.32 probability arrays, bin_count > 0
 * Postcondition: Returns TV in Q0.32, 0 = identical, UINT32_MAX = disjoint
 * Complexity: O(bin_count) time, O(1) space
 * Determinism: Bit-perfect across all platforms
 */
uint32_t cm_detect_tv(const uint32_t *p, const uint32_t *q,
                      uint32_t bin_count, ct_fault_flags_t *faults);
```

## 5. DVM Compliance

All monitoring code must use DVM primitives (see `include/dvm.h`):

- `dvm_add64()`, `dvm_sub64()` — Saturating 64-bit arithmetic
- `dvm_clamp32()`, `dvm_clamp32u()` — Explicit saturation
- `dvm_round_shift_rne()` — Round-to-nearest-even
- `dvm_div_q16()` — Q16.16 fixed-point division
- `cm_log2_q16()` — LUT-based logarithm for JSD/PSI
- `cm_sha256()` — Deterministic hashing for ledger

**Never** use raw `+`, `-`, `*` on `int32_t` without explicit overflow handling.

## 6. Fault Handling

All arithmetic operations must:
1. Accept a `ct_fault_flags_t *faults` parameter
2. Set appropriate flags on overflow/underflow/domain error
3. Return a deterministic value (even on fault)

Monitor-specific faults:
```c
typedef struct {
    uint32_t overflow    : 1;  // Saturated high
    uint32_t underflow   : 1;  // Saturated low
    uint32_t div_zero    : 1;  // Division by zero
    uint32_t domain      : 1;  // Invalid input (e.g., log of 0)
    uint32_t input_range : 1;  // Input outside COE envelope
    uint32_t output_range: 1;  // Output outside COE envelope
    uint32_t policy_fail : 1;  // Policy validation failed
    uint32_t hash_fail   : 1;  // Hash verification failed
    uint32_t ledger_fail : 1;  // Ledger integrity failed
} ct_fault_flags_t;
```

## 7. Test Requirements

Every module needs:
- **Unit tests**: Production-grade test suite (see `tests/unit/`)
- **Test vectors**: Exact values from CM-MATH-001 specification
- **Cross-platform**: Verify bit-identity on x86, ARM, RISC-V
- **Coverage**: RUN_TEST() macro with clear pass/fail output

Current test suites (253 tests total):
- `test_primitives` — DVM operations (33 tests)
- `test_ledger` — Hash chain (18 tests)
- `test_detectors` — TV, JSD, PSI (20 tests)
- `test_policy` — COE parsing (25 tests)
- `test_input` — Input monitor (22 tests)
- `test_activation` — Activation monitor (24 tests)
- `test_output` — Output monitor (19 tests)
- `test_health` — Health FSM (19 tests)
- `test_react` — Reaction handler (14 tests)
- `test_verify` — Chain verification (32 tests)
- `test_bit_identity` — Cross-platform (27 tests)

## 8. Getting Started

Look for issues labeled `good-first-issue` or `dvm-layer`.

We recommend starting with:
- DVM primitive tests (test vectors from CM-MATH-001)
- Drift detector edge cases
- Health FSM state transition tests

## Questions?

- **Technical questions:** Open an issue
- **General inquiries:** william@fstopify.com
- **Security issues:** Email william@fstopify.com (do not open public issues)

Thank you for helping make deterministic runtime monitoring a reality! 🎯
