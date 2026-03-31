/**
 * @file test_detectors.c
 * @brief Drift Detection Test Suite (TV, JSD, PSI)
 * @traceability CM-MATH-001 §2-4
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "cm_detect.h"
#include "cm_types.h"
#include "ct_types.h"
#include "dvm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*============================================================================
 * Test Framework
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(name, func) do { \
    tests_run++; \
    printf("  [%d] %-50s ", tests_run, name); \
    if (func()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("ASSERT FAILED: %s ", #cond); \
        return 0; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("ASSERT_EQ FAILED: %s != %s (%ld vs %ld) ", \
               #a, #b, (long)(a), (long)(b)); \
        return 0; \
    } \
} while(0)

/*============================================================================
 * Histogram Operation Tests
 *============================================================================*/

static int test_hist_assign_bin_basic(void)
{
    cm_hist_spec_t spec;
    spec.bin_count = 4;
    spec.edges_q16[0] = 0;           /* 0.0 */
    spec.edges_q16[1] = 65536;       /* 1.0 */
    spec.edges_q16[2] = 131072;      /* 2.0 */
    spec.edges_q16[3] = 196608;      /* 3.0 */
    spec.edges_q16[4] = 262144;      /* 4.0 */
    
    /* Value in middle of bin 0 */
    ASSERT_EQ(cm_hist_assign_bin(32768, &spec), 0);
    
    /* Value at start of bin 1 */
    ASSERT_EQ(cm_hist_assign_bin(65536, &spec), 1);
    
    /* Value in bin 2 */
    ASSERT_EQ(cm_hist_assign_bin(150000, &spec), 2);
    
    /* Value at right edge of last bin (inclusive) */
    ASSERT_EQ(cm_hist_assign_bin(262144, &spec), 3);
    
    return 1;
}

static int test_hist_assign_bin_edge_cases(void)
{
    cm_hist_spec_t spec;
    spec.bin_count = 2;
    spec.edges_q16[0] = -65536;      /* -1.0 */
    spec.edges_q16[1] = 0;           /* 0.0 */
    spec.edges_q16[2] = 65536;       /* 1.0 */
    
    /* Below range */
    ASSERT_EQ(cm_hist_assign_bin(-100000, &spec), -1);
    
    /* Above range */
    ASSERT_EQ(cm_hist_assign_bin(100000, &spec), -1);
    
    /* Exactly at left edge */
    ASSERT_EQ(cm_hist_assign_bin(-65536, &spec), 0);
    
    /* Exactly at right edge */
    ASSERT_EQ(cm_hist_assign_bin(65536, &spec), 1);
    
    return 1;
}

static int test_hist_accumulate_basic(void)
{
    cm_hist_spec_t spec;
    spec.bin_count = 3;
    spec.edges_q16[0] = 0;
    spec.edges_q16[1] = 65536;
    spec.edges_q16[2] = 131072;
    spec.edges_q16[3] = 196608;
    
    uint32_t counts[3] = {0, 0, 0};
    
    ASSERT_EQ(cm_hist_accumulate(30000, &spec, counts), CT_OK);
    ASSERT_EQ(counts[0], 1);
    
    ASSERT_EQ(cm_hist_accumulate(70000, &spec, counts), CT_OK);
    ASSERT_EQ(counts[1], 1);
    
    ASSERT_EQ(cm_hist_accumulate(70000, &spec, counts), CT_OK);
    ASSERT_EQ(counts[1], 2);
    
    /* Out of range should fail */
    ASSERT_EQ(cm_hist_accumulate(300000, &spec, counts), CT_ERR_RANGE);
    
    return 1;
}

static int test_hist_total(void)
{
    uint32_t counts[4] = {100, 200, 50, 150};
    
    ASSERT_EQ(cm_hist_total(counts, 4), 500);
    ASSERT_EQ(cm_hist_total(counts, 2), 300);
    ASSERT_EQ(cm_hist_total(NULL, 4), 0);
    
    return 1;
}

/*============================================================================
 * Normalization Tests
 *============================================================================*/

static int test_hist_normalize_uniform(void)
{
    uint32_t counts[4] = {100, 100, 100, 100};
    uint32_t probs[4];
    ct_fault_flags_t faults = {0};
    
    ct_result_t rc = cm_hist_normalize(counts, 4, 400, probs, &faults);
    ASSERT_EQ(rc, CT_OK);
    ASSERT(!ct_has_fault(&faults));
    
    /* Each bin should be ~0.25 in Q0.32 = 2^30 = 1073741824 */
    /* Actually (100 * 2^32) / 400 = 1073741824 */
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(probs[i], 1073741824);
    }
    
    return 1;
}

static int test_hist_normalize_div_zero(void)
{
    uint32_t counts[2] = {0, 0};
    uint32_t probs[2];
    ct_fault_flags_t faults = {0};
    
    ct_result_t rc = cm_hist_normalize(counts, 2, 0, probs, &faults);
    ASSERT_EQ(rc, CT_ERR_DIV_ZERO);
    ASSERT(faults.div_zero);
    
    return 1;
}

/*============================================================================
 * Total Variation Tests
 *============================================================================*/

static int test_tv_identical_distributions(void)
{
    /* Identical distributions should have TV = 0 */
    uint32_t p[4] = {1073741824, 1073741824, 1073741824, 1073741824};  /* Uniform 0.25 each */
    uint32_t q[4] = {1073741824, 1073741824, 1073741824, 1073741824};
    ct_fault_flags_t faults = {0};
    
    uint32_t tv = cm_detect_tv(p, q, 4, &faults);
    ASSERT_EQ(tv, 0);
    
    return 1;
}

static int test_tv_disjoint_distributions(void)
{
    /* Completely disjoint distributions should have TV = 1.0 */
    /* p = [1, 0, 0, 0], q = [0, 0, 0, 1] */
    uint32_t p[4] = {0xFFFFFFFF, 0, 0, 0};
    uint32_t q[4] = {0, 0, 0, 0xFFFFFFFF};
    ct_fault_flags_t faults = {0};
    
    uint32_t tv = cm_detect_tv(p, q, 4, &faults);
    
    /* TV = 0.5 * (|1-0| + |0-0| + |0-0| + |0-1|) = 1.0 */
    /* In Q0.32, 1.0 = 0xFFFFFFFF (max) */
    ASSERT(tv > 0x80000000);  /* Should be close to 1.0 */
    
    return 1;
}

static int test_tv_partial_overlap(void)
{
    /* Half overlap: p = [0.5, 0.5, 0, 0], q = [0, 0, 0.5, 0.5] */
    uint32_t half = 0x80000000;  /* 0.5 in Q0.32 */
    uint32_t p[4] = {half, half, 0, 0};
    uint32_t q[4] = {0, 0, half, half};
    ct_fault_flags_t faults = {0};
    
    uint32_t tv = cm_detect_tv(p, q, 4, &faults);
    
    /* TV = 0.5 * 4 * 0.5 = 1.0 (disjoint) */
    ASSERT(tv > 0x80000000);
    
    return 1;
}

/*============================================================================
 * Jensen-Shannon Divergence Tests
 *============================================================================*/

static int test_jsd_identical_distributions(void)
{
    /* Identical distributions should have JSD = 0 */
    uint32_t p[4] = {1073741824, 1073741824, 1073741824, 1073741824};
    uint32_t q[4] = {1073741824, 1073741824, 1073741824, 1073741824};
    ct_fault_flags_t faults = {0};
    
    int32_t jsd = cm_detect_jsd(p, q, 4, &faults);
    
    /* JSD should be 0 for identical distributions */
    ASSERT(jsd >= 0);
    ASSERT(jsd < 1000);  /* Should be essentially 0 */
    
    return 1;
}

static int test_jsd_disjoint_bounded(void)
{
    /* JSD is bounded by log2(2) = 1.0 */
    uint32_t p[4] = {0xFFFFFFFF, 0, 0, 0};
    uint32_t q[4] = {0, 0, 0, 0xFFFFFFFF};
    ct_fault_flags_t faults = {0};
    
    int32_t jsd = cm_detect_jsd(p, q, 4, &faults);
    
    /* JSD max is 1.0 = 65536 in Q16.16 */
    ASSERT(jsd > 0);
    ASSERT(jsd <= CT_Q16_ONE);
    
    return 1;
}

static int test_jsd_symmetric(void)
{
    /* JSD should be symmetric: JSD(p,q) = JSD(q,p) */
    uint32_t p[4] = {0x80000000, 0x40000000, 0x30000000, 0x10000000};
    uint32_t q[4] = {0x40000000, 0x80000000, 0x10000000, 0x30000000};
    ct_fault_flags_t faults = {0};
    
    int32_t jsd_pq = cm_detect_jsd(p, q, 4, &faults);
    int32_t jsd_qp = cm_detect_jsd(q, p, 4, &faults);
    
    ASSERT_EQ(jsd_pq, jsd_qp);
    
    return 1;
}

/*============================================================================
 * Population Stability Index Tests
 *============================================================================*/

static int test_psi_identical_distributions(void)
{
    /* Identical distributions should have PSI = 0 */
    uint32_t p[4] = {1073741824, 1073741824, 1073741824, 1073741824};
    uint32_t q[4] = {1073741824, 1073741824, 1073741824, 1073741824};
    ct_fault_flags_t faults = {0};
    
    int32_t psi = cm_detect_psi(p, q, 4, 1, &faults);  /* epsilon = 1 */
    
    /* PSI should be 0 for identical distributions */
    ASSERT(psi >= -100);
    ASSERT(psi < 100);
    
    return 1;
}

static int test_psi_positive_for_drift(void)
{
    /* Different distributions should have positive PSI */
    uint32_t p[4] = {0x80000000, 0x40000000, 0x20000000, 0x20000000};
    uint32_t q[4] = {0x20000000, 0x20000000, 0x40000000, 0x80000000};
    ct_fault_flags_t faults = {0};
    
    int32_t psi = cm_detect_psi(p, q, 4, 1, &faults);
    
    /* PSI should be positive for different distributions */
    ASSERT(psi > 0);
    
    return 1;
}

/*============================================================================
 * Combined Drift Detection Tests
 *============================================================================*/

static int test_detect_drift_no_drift(void)
{
    /* Same distribution, no detectors should trigger */
    uint32_t runtime[4] = {100, 100, 100, 100};
    uint32_t ref[4] = {100, 100, 100, 100};
    
    cm_drift_policy_t policy = {0};
    policy.enabled_detectors = CM_DRIFT_TV_ENABLED | CM_DRIFT_JSD_ENABLED;
    policy.tv_threshold_q0_32 = 0x40000000;  /* 0.25 */
    policy.jsd_threshold_q16_16 = 6554;      /* 0.1 */
    policy.epsilon_q0_32 = 1;
    
    cm_drift_result_t result;
    ct_fault_flags_t faults = {0};
    
    ct_result_t rc = cm_detect_drift(runtime, ref, 4, &policy, &result, &faults);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.flags, 0);  /* No triggers */
    ASSERT_EQ(result.tv_q0_32, 0);
    
    return 1;
}

static int test_detect_drift_triggers_tv(void)
{
    /* Very different distribution, TV should trigger */
    uint32_t runtime[4] = {400, 0, 0, 0};
    uint32_t ref[4] = {0, 0, 0, 400};
    
    cm_drift_policy_t policy = {0};
    policy.enabled_detectors = CM_DRIFT_TV_ENABLED;
    policy.tv_threshold_q0_32 = 0x40000000;  /* 0.25 threshold */
    policy.epsilon_q0_32 = 1;
    
    cm_drift_result_t result;
    ct_fault_flags_t faults = {0};
    
    ct_result_t rc = cm_detect_drift(runtime, ref, 4, &policy, &result, &faults);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(result.tv_q0_32 > policy.tv_threshold_q0_32);
    ASSERT(result.flags & CM_DRIFT_TV_TRIGGERED);
    
    return 1;
}

/*============================================================================
 * Range Checking Tests
 *============================================================================*/

static int test_in_range_basic(void)
{
    ASSERT(cm_in_range(50000, 0, 100000));
    ASSERT(cm_in_range(0, 0, 100000));
    ASSERT(cm_in_range(100000, 0, 100000));
    ASSERT(!cm_in_range(-1, 0, 100000));
    ASSERT(!cm_in_range(100001, 0, 100000));
    
    return 1;
}

static int test_count_violations_basic(void)
{
    int32_t values[5] = {-10, 50, 150, 200, 250};
    
    uint32_t count = cm_count_violations(values, 5, 0, 100);
    
    /* -10, 150, 200, 250 are violations */
    ASSERT_EQ(count, 4);
    
    return 1;
}

static int test_max_overrange_basic(void)
{
    int32_t values[4] = {-20, 50, 120, 150};
    ct_fault_flags_t faults = {0};
    
    int32_t max_over = cm_max_overrange(values, 4, 0, 100, &faults);
    
    /* -20 is 20 below min, 150 is 50 above max. Max is 50. */
    ASSERT_EQ(max_over, 50);
    
    return 1;
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("certifiable-monitor: Drift Detectors Test Suite\n");
    printf("================================================\n\n");
    
    printf("Histogram Operations:\n");
    RUN_TEST("hist_assign_bin_basic", test_hist_assign_bin_basic);
    RUN_TEST("hist_assign_bin_edge_cases", test_hist_assign_bin_edge_cases);
    RUN_TEST("hist_accumulate_basic", test_hist_accumulate_basic);
    RUN_TEST("hist_total", test_hist_total);
    
    printf("\nNormalization:\n");
    RUN_TEST("hist_normalize_uniform", test_hist_normalize_uniform);
    RUN_TEST("hist_normalize_div_zero", test_hist_normalize_div_zero);
    
    printf("\nTotal Variation:\n");
    RUN_TEST("tv_identical_distributions", test_tv_identical_distributions);
    RUN_TEST("tv_disjoint_distributions", test_tv_disjoint_distributions);
    RUN_TEST("tv_partial_overlap", test_tv_partial_overlap);
    
    printf("\nJensen-Shannon Divergence:\n");
    RUN_TEST("jsd_identical_distributions", test_jsd_identical_distributions);
    RUN_TEST("jsd_disjoint_bounded", test_jsd_disjoint_bounded);
    RUN_TEST("jsd_symmetric", test_jsd_symmetric);
    
    printf("\nPopulation Stability Index:\n");
    RUN_TEST("psi_identical_distributions", test_psi_identical_distributions);
    RUN_TEST("psi_positive_for_drift", test_psi_positive_for_drift);
    
    printf("\nCombined Drift Detection:\n");
    RUN_TEST("detect_drift_no_drift", test_detect_drift_no_drift);
    RUN_TEST("detect_drift_triggers_tv", test_detect_drift_triggers_tv);
    
    printf("\nRange Checking:\n");
    RUN_TEST("in_range_basic", test_in_range_basic);
    RUN_TEST("count_violations_basic", test_count_violations_basic);
    RUN_TEST("max_overrange_basic", test_max_overrange_basic);
    
    printf("\n================================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
