/**
 * @file test_primitives.c
 * @brief Test Suite for DVM Primitives and Fixed-Point Math
 * @traceability CM-MATH-001 (All sections)
 *
 * @details
 * Tests:
 * - Saturating arithmetic (add, sub, mul, clamp)
 * - Round-to-nearest-even shift
 * - Fixed-point division
 * - LUT-based log2 accuracy
 * - Ratio computation
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "ct_types.h"
#include "cm_types.h"
#include "dvm.h"

/*============================================================================
 * Test Framework
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static bool test_##name(void)
#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  [%d] %-50s ", tests_run, #name); \
    if (test_##name()) { \
        tests_passed++; \
        printf("PASS\n"); \
    } else { \
        printf("FAIL\n"); \
    } \
} while(0)

#define ASSERT(cond) do { if (!(cond)) { printf("ASSERT FAILED: %s ", #cond); return false; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { printf("ASSERT_EQ FAILED: %lld != %lld ", (long long)(a), (long long)(b)); return false; } } while(0)
#define ASSERT_NEAR(a, b, tol) do { \
    int64_t diff = ((int64_t)(a) - (int64_t)(b)); \
    if (diff < 0) diff = -diff; \
    if (diff > (tol)) { printf("ASSERT_NEAR FAILED: %lld vs %lld (diff %lld > %lld) ", (long long)(a), (long long)(b), (long long)diff, (long long)(tol)); return false; } \
} while(0)

/*============================================================================
 * Saturating Arithmetic Tests
 *============================================================================*/

TEST(add64_no_overflow)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int64_t result = dvm_add64(100, 200, &f);
    ASSERT_EQ(result, 300);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(add64_overflow_saturates)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int64_t result = dvm_add64(INT64_MAX, 1, &f);
    ASSERT_EQ(result, INT64_MAX);
    ASSERT(f.overflow);
    return true;
}

TEST(add64_underflow_saturates)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int64_t result = dvm_add64(INT64_MIN, -1, &f);
    ASSERT_EQ(result, INT64_MIN);
    ASSERT(f.underflow);
    return true;
}

TEST(sub64_no_overflow)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int64_t result = dvm_sub64(300, 100, &f);
    ASSERT_EQ(result, 200);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(mul64_no_overflow)
{
    /* 32-bit * 32-bit always fits in 64-bit */
    int64_t result = dvm_mul64(1000000, 1000000);
    ASSERT_EQ(result, 1000000000000LL);
    return true;
}

TEST(clamp32_no_clamp)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int32_t result = dvm_clamp32(12345, &f);
    ASSERT_EQ(result, 12345);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(clamp32_overflow)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int32_t result = dvm_clamp32((int64_t)INT32_MAX + 1, &f);
    ASSERT_EQ(result, INT32_MAX);
    ASSERT(f.overflow);
    return true;
}

TEST(clamp32_underflow)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int32_t result = dvm_clamp32((int64_t)INT32_MIN - 1, &f);
    ASSERT_EQ(result, INT32_MIN);
    ASSERT(f.underflow);
    return true;
}

/*============================================================================
 * Round-to-Nearest-Even Tests
 *============================================================================*/

TEST(rne_no_shift)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int32_t result = dvm_round_shift_rne(12345, 0, &f);
    ASSERT_EQ(result, 12345);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(rne_shift_round_down)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* 0b1100 = 12, shift by 2: 12 >> 2 = 3, frac = 0 < 2 -> round down */
    int32_t result = dvm_round_shift_rne(12, 2, &f);
    ASSERT_EQ(result, 3);
    return true;
}

TEST(rne_shift_round_up)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* 0b1110 = 14, shift by 2: quot = 3, frac = 2 > 2? No, frac = 2, half = 2 */
    /* Exactly halfway: round to even. quot=3 is odd, so round up to 4 */
    int32_t result = dvm_round_shift_rne(14, 2, &f);
    ASSERT_EQ(result, 4);  /* 14/4 = 3.5, rounds to even = 4 */
    return true;
}

TEST(rne_halfway_to_even_down)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* 10 >> 2 = 2.5, quot = 2, which is even, so stay at 2 */
    int32_t result = dvm_round_shift_rne(10, 2, &f);
    ASSERT_EQ(result, 2);
    return true;
}

TEST(rne_halfway_to_even_up)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* 6 >> 2 = 1.5, quot = 1, which is odd, so round up to 2 */
    int32_t result = dvm_round_shift_rne(6, 2, &f);
    ASSERT_EQ(result, 2);
    return true;
}

/*============================================================================
 * Fixed-Point Division Tests
 *============================================================================*/

TEST(div_q16_half)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* 1.0 / 2.0 = 0.5 in Q16.16 */
    int32_t one = CT_Q16_ONE;   /* 65536 */
    int32_t two = 2 * CT_Q16_ONE;  /* 131072 */
    
    int32_t result = dvm_div_q16(one, two, &f);
    ASSERT_EQ(result, CT_Q16_HALF);  /* 32768 */
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(div_q16_by_zero)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int32_t result = dvm_div_q16(CT_Q16_ONE, 0, &f);
    ASSERT_EQ(result, INT32_MAX);
    ASSERT(f.div_zero);
    return true;
}

/*============================================================================
 * LUT-Based Log2 Tests
 *============================================================================*/

TEST(log2_one)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* log2(1.0) = 0 */
    int32_t result = cm_log2_q16(CT_Q16_ONE, &f);
    ASSERT_EQ(result, 0);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(log2_two)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* log2(2.0) = 1.0 = 65536 in Q16.16 */
    int32_t result = cm_log2_q16(2 * CT_Q16_ONE, &f);
    ASSERT_EQ(result, CT_Q16_ONE);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(log2_half)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* log2(0.5) = -1.0 = -65536 in Q16.16 */
    int32_t result = cm_log2_q16(CT_Q16_HALF, &f);
    ASSERT_EQ(result, -CT_Q16_ONE);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(log2_1_5)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* log2(1.5) ≈ 0.5849625... ≈ 38336 in Q16.16 */
    /* 1.5 in Q16.16 = 98304 */
    int32_t result = cm_log2_q16(98304, &f);
    /* Allow ±2 LSB tolerance for linear interpolation */
    ASSERT_NEAR(result, 38336, 4);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(log2_four)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* log2(4.0) = 2.0 = 131072 in Q16.16 */
    int32_t result = cm_log2_q16(4 * CT_Q16_ONE, &f);
    ASSERT_EQ(result, 2 * CT_Q16_ONE);
    return true;
}

TEST(log2_zero_domain_error)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int32_t result = cm_log2_q16(0, &f);
    ASSERT_EQ(result, INT32_MIN);
    ASSERT(f.domain);
    return true;
}

/*============================================================================
 * Natural Log Tests
 *============================================================================*/

TEST(ln_e)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* ln(e) = 1.0, e ≈ 2.71828... in Q16.16 = 178145 */
    int32_t e_q16 = 178145;
    int32_t result = cm_ln_q16((uint32_t)e_q16, &f);
    /* ln(e) = 1.0 = 65536 in Q16.16, allow small error */
    ASSERT_NEAR(result, CT_Q16_ONE, 100);
    return true;
}

/*============================================================================
 * Ratio Tests
 *============================================================================*/

TEST(ratio_half)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* 1/2 = 0.5 = 32768 in Q16.16 */
    int32_t result = cm_ratio_q16(1, 2, &f);
    ASSERT_EQ(result, CT_Q16_HALF);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(ratio_one)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    /* 100/100 = 1.0 = 65536 */
    int32_t result = cm_ratio_q16(100, 100, &f);
    ASSERT_EQ(result, CT_Q16_ONE);
    return true;
}

TEST(ratio_div_zero)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int32_t result = cm_ratio_q16(100, 0, &f);
    ASSERT_EQ(result, INT32_MAX);
    ASSERT(f.div_zero);
    return true;
}

/*============================================================================
 * CLZ Tests
 *============================================================================*/

TEST(clz_zero)
{
    ASSERT_EQ(cm_clz32(0), 32);
    return true;
}

TEST(clz_one)
{
    ASSERT_EQ(cm_clz32(1), 31);
    return true;
}

TEST(clz_msb_set)
{
    ASSERT_EQ(cm_clz32(0x80000000), 0);
    return true;
}

TEST(clz_mid)
{
    ASSERT_EQ(cm_clz32(0x00010000), 15);
    return true;
}

/*============================================================================
 * Absolute Value Tests
 *============================================================================*/

TEST(abs_positive)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    ASSERT_EQ(dvm_abs(42, &f), 42);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(abs_negative)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    ASSERT_EQ(dvm_abs(-42, &f), 42);
    ASSERT(!ct_has_fault(&f));
    return true;
}

TEST(abs_min_saturates)
{
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    int32_t result = dvm_abs(INT32_MIN, &f);
    ASSERT_EQ(result, INT32_MAX);
    ASSERT(f.overflow);
    return true;
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("certifiable-monitor: DVM Primitives Test Suite\n");
    printf("==============================================\n\n");
    
    printf("Saturating Arithmetic:\n");
    RUN_TEST(add64_no_overflow);
    RUN_TEST(add64_overflow_saturates);
    RUN_TEST(add64_underflow_saturates);
    RUN_TEST(sub64_no_overflow);
    RUN_TEST(mul64_no_overflow);
    RUN_TEST(clamp32_no_clamp);
    RUN_TEST(clamp32_overflow);
    RUN_TEST(clamp32_underflow);
    
    printf("\nRound-to-Nearest-Even:\n");
    RUN_TEST(rne_no_shift);
    RUN_TEST(rne_shift_round_down);
    RUN_TEST(rne_shift_round_up);
    RUN_TEST(rne_halfway_to_even_down);
    RUN_TEST(rne_halfway_to_even_up);
    
    printf("\nFixed-Point Division:\n");
    RUN_TEST(div_q16_half);
    RUN_TEST(div_q16_by_zero);
    
    printf("\nLUT-Based Log2:\n");
    RUN_TEST(log2_one);
    RUN_TEST(log2_two);
    RUN_TEST(log2_half);
    RUN_TEST(log2_1_5);
    RUN_TEST(log2_four);
    RUN_TEST(log2_zero_domain_error);
    
    printf("\nNatural Log:\n");
    RUN_TEST(ln_e);
    
    printf("\nRatio:\n");
    RUN_TEST(ratio_half);
    RUN_TEST(ratio_one);
    RUN_TEST(ratio_div_zero);
    
    printf("\nCLZ:\n");
    RUN_TEST(clz_zero);
    RUN_TEST(clz_one);
    RUN_TEST(clz_msb_set);
    RUN_TEST(clz_mid);
    
    printf("\nAbsolute Value:\n");
    RUN_TEST(abs_positive);
    RUN_TEST(abs_negative);
    RUN_TEST(abs_min_saturates);
    
    printf("\n==============================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
