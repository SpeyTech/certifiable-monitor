/**
 * @file dvm.h
 * @brief Deterministic Virtual Machine (DVM) Primitives
 * @traceability CM-MATH-001 (All arithmetic operations)
 *
 * @details
 * Defines the only legal arithmetic operations for certifiable-monitor.
 * All operations are integer-only and produce bit-identical results
 * across x86, ARM, and RISC-V platforms.
 *
 * Forbidden operations:
 * - IEEE-754 float/double
 * - FMA instructions
 * - Library math functions (exp, log, sqrt)
 * - Raw signed +/- on int32_t (undefined behavior on overflow)
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef DVM_H
#define DVM_H

#include "ct_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Basic Arithmetic with Saturation
 *============================================================================*/

/**
 * @brief 64-bit addition with saturation
 * @param a First operand
 * @param b Second operand
 * @param faults Fault flags (updated on saturation)
 * @return a + b, saturated to [INT64_MIN, INT64_MAX]
 * @traceability CM-MATH-001 §DVM-ADD64
 */
int64_t dvm_add64(int64_t a, int64_t b, ct_fault_flags_t *faults);

/**
 * @brief 64-bit subtraction with saturation
 * @param a First operand
 * @param b Second operand
 * @param faults Fault flags (updated on saturation)
 * @return a - b, saturated to [INT64_MIN, INT64_MAX]
 * @traceability CM-MATH-001 §DVM-SUB64
 */
int64_t dvm_sub64(int64_t a, int64_t b, ct_fault_flags_t *faults);

/**
 * @brief 32×32→64 multiply (no overflow possible)
 * @param a First operand (32-bit)
 * @param b Second operand (32-bit)
 * @return a * b as 64-bit result
 * @traceability CM-MATH-001 §DVM-MUL64
 */
int64_t dvm_mul64(int32_t a, int32_t b);

/**
 * @brief Clamp 64-bit value to 32-bit range with fault flag
 * @param x 64-bit input
 * @param faults Fault flags (updated if clamped)
 * @return x clamped to [INT32_MIN, INT32_MAX]
 * @traceability CM-MATH-001 §DVM-CLAMP32
 */
int32_t dvm_clamp32(int64_t x, ct_fault_flags_t *faults);

/**
 * @brief Clamp 64-bit value to unsigned 32-bit range
 * @param x 64-bit input
 * @param faults Fault flags (updated if clamped)
 * @return x clamped to [0, UINT32_MAX]
 * @traceability CM-MATH-001 §DVM-CLAMP32U
 */
uint32_t dvm_clamp32u(int64_t x, ct_fault_flags_t *faults);

/*============================================================================
 * Round-to-Nearest-Even Shift
 *============================================================================*/

/**
 * @brief Arithmetic right shift with round-to-nearest-even
 * @param x Value to shift
 * @param shift Number of bits to shift (0-62)
 * @param faults Fault flags (updated on domain error)
 * @return x >> shift with RNE rounding, then clamped to int32
 * @traceability CM-MATH-001 §DVM-ROUND-SHIFT-RNE
 *
 * This is the ONLY rounding operation in the DVM.
 * RNE is chosen for statistical unbiasedness.
 */
int32_t dvm_round_shift_rne(int64_t x, uint32_t shift, ct_fault_flags_t *faults);

/*============================================================================
 * Fixed-Point Division
 *============================================================================*/

/**
 * @brief Q16.16 fixed-point division
 * @param num Numerator in Q16.16
 * @param den Denominator in Q16.16 (must be non-zero)
 * @param faults Fault flags (updated on div-by-zero or overflow)
 * @return (num / den) in Q16.16
 * @traceability CM-MATH-001 §DVM-DIV-Q
 */
int32_t dvm_div_q16(int32_t num, int32_t den, ct_fault_flags_t *faults);

/*============================================================================
 * Fixed-Point Math (LUT-based)
 *============================================================================*/

/**
 * @brief Compute ratio a/b in Q16.16 format
 * @param a Numerator (unsigned)
 * @param b Denominator (must be > 0)
 * @param faults Fault flags (updated on div-by-zero)
 * @return (a << 16) / b with rounding
 * @traceability CM-MATH-001 §2.2
 */
int32_t cm_ratio_q16(uint32_t a, uint32_t b, ct_fault_flags_t *faults);

/**
 * @brief Compute log2(x) in Q16.16 format using LUT
 * @param x Input value in Q16.16 (must be > 0)
 * @param faults Fault flags (updated on domain error)
 * @return log2(x) in Q16.16
 * @traceability CM-MATH-001 §3 (LUT-based log2)
 *
 * Algorithm:
 * 1. Normalize x to [1.0, 2.0) by finding MSB position
 * 2. Use top 9 bits to index 512-entry LUT
 * 3. Linear interpolation with remaining bits
 */
int32_t cm_log2_q16(uint32_t x, ct_fault_flags_t *faults);

/**
 * @brief Compute ln(x) in Q16.16 using log2
 * @param x Input value in Q16.16 (must be > 0)
 * @param faults Fault flags (updated on domain error)
 * @return ln(x) in Q16.16
 * @traceability CM-MATH-001 §3
 *
 * Computed as: ln(x) = log2(x) * ln(2)
 * where ln(2) = 45426 in Q16.16
 */
int32_t cm_ln_q16(uint32_t x, ct_fault_flags_t *faults);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Count leading zeros (portable)
 * @param x 32-bit input
 * @return Number of leading zero bits (0-32)
 * @traceability CM-MATH-001 §3 (normalization)
 */
uint32_t cm_clz32(uint32_t x);

/**
 * @brief Absolute value with saturation
 * @param x Input value
 * @param faults Fault flags (updated if INT32_MIN)
 * @return |x|, saturated to INT32_MAX if x == INT32_MIN
 * @traceability CM-MATH-001 §DVM-ABS
 */
int32_t dvm_abs(int32_t x, ct_fault_flags_t *faults);

#ifdef __cplusplus
}
#endif

#endif /* DVM_H */
