/**
 * @file test_input.c
 * @brief Input Monitor Test Suite
 * @traceability CM-ARCH-MATH-001 §3, SRS-002-INPUT
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "input.h"
#include "cm_detect.h"
#include "cm_types.h"
#include "ct_types.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

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
 * Test Fixtures
 *============================================================================*/

/* Simple envelope for 3 features */
static cm_input_envelope_t create_test_envelope(void)
{
    static cm_input_envelope_t env;
    memset(&env, 0, sizeof(env));
    
    env.feature_count = 3;
    
    /* Feature 0: [-1.0, 1.0] in Q16.16 */
    env.min_q16[0] = -65536;   /* -1.0 */
    env.max_q16[0] = 65536;    /* 1.0 */
    
    /* Feature 1: [0.0, 2.0] in Q16.16 */
    env.min_q16[1] = 0;        /* 0.0 */
    env.max_q16[1] = 131072;   /* 2.0 */
    
    /* Feature 2: [-0.5, 0.5] in Q16.16 */
    env.min_q16[2] = -32768;   /* -0.5 */
    env.max_q16[2] = 32768;    /* 0.5 */
    
    env.has_hists = false;
    
    return env;
}

/* Envelope with histogram for drift detection */
static cm_input_envelope_t create_hist_envelope(void)
{
    static cm_input_envelope_t env;
    memset(&env, 0, sizeof(env));
    
    env.feature_count = 1;
    env.min_q16[0] = 0;
    env.max_q16[0] = 262144;  /* 4.0 in Q16.16 */
    env.has_hists = true;
    
    /* 4 bins: [0,1), [1,2), [2,3), [3,4] */
    env.hists[0].bin_count = 4;
    env.hists[0].edges_q16[0] = 0;
    env.hists[0].edges_q16[1] = 65536;   /* 1.0 */
    env.hists[0].edges_q16[2] = 131072;  /* 2.0 */
    env.hists[0].edges_q16[3] = 196608;  /* 3.0 */
    env.hists[0].edges_q16[4] = 262144;  /* 4.0 */
    
    /* Uniform reference distribution */
    env.hists[0].ref_counts[0] = 25;
    env.hists[0].ref_counts[1] = 25;
    env.hists[0].ref_counts[2] = 25;
    env.hists[0].ref_counts[3] = 25;
    
    return env;
}

/*============================================================================
 * Initialization Tests
 *============================================================================*/

static int test_init_basic(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    ct_result_t rc = cm_input_init(&ctx, &env, states, 3);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(ctx.initialized);
    ASSERT_EQ(ctx.feature_count, 3);
    ASSERT_EQ(ctx.window_samples, 0);
    ASSERT_EQ(ctx.total_violations, 0);
    
    return 1;
}

static int test_init_null_ctx(void)
{
    cm_input_feature_state_t states[3];
    ct_result_t rc = cm_input_init(NULL, NULL, states, 3);
    ASSERT_EQ(rc, CT_ERR_NULL);
    return 1;
}

static int test_init_null_states(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    ct_result_t rc = cm_input_init(&ctx, &env, NULL, 3);
    ASSERT_EQ(rc, CT_ERR_NULL);
    return 1;
}

static int test_init_zero_features(void)
{
    cm_input_ctx_t ctx;
    cm_input_feature_state_t states[3];
    ct_result_t rc = cm_input_init(&ctx, NULL, states, 0);
    ASSERT_EQ(rc, CT_ERR_SIZE);
    return 1;
}

static int test_init_feature_count_mismatch(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();  /* 3 features */
    cm_input_feature_state_t states[5];
    
    ct_result_t rc = cm_input_init(&ctx, &env, states, 5);  /* Mismatch */
    ASSERT_EQ(rc, CT_ERR_SIZE);
    
    return 1;
}

static int test_init_no_envelope(void)
{
    cm_input_ctx_t ctx;
    cm_input_feature_state_t states[3];
    
    ct_result_t rc = cm_input_init(&ctx, NULL, states, 3);
    ASSERT_EQ(rc, CT_OK);
    ASSERT(ctx.initialized);
    ASSERT(ctx.envelope == NULL);
    
    return 1;
}

/*============================================================================
 * Window Reset Tests
 *============================================================================*/

static int test_reset_window(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* Process some samples */
    int32_t values[3] = {0, 65536, 0};  /* All in range */
    cm_input_process(&ctx, values, NULL, NULL);
    cm_input_process(&ctx, values, NULL, NULL);
    
    ASSERT_EQ(ctx.window_samples, 2);
    
    /* Reset window */
    ct_result_t rc = cm_input_reset_window(&ctx, 42);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(ctx.window_id, 42);
    ASSERT_EQ(ctx.window_samples, 0);
    ASSERT_EQ(ctx.total_violations, 0);
    
    /* Check feature states are reset */
    ASSERT_EQ(states[0].sample_count, 0);
    ASSERT_EQ(states[0].violation_count, 0);
    
    return 1;
}

/*============================================================================
 * Range Checking Tests
 *============================================================================*/

static int test_check_feature_in_range(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* Feature 0: [-1.0, 1.0] */
    ASSERT(cm_input_check_feature(&ctx, 0, 0));           /* 0.0 - in range */
    ASSERT(cm_input_check_feature(&ctx, 0, -65536));      /* -1.0 - at min */
    ASSERT(cm_input_check_feature(&ctx, 0, 65536));       /* 1.0 - at max */
    ASSERT(cm_input_check_feature(&ctx, 0, 32768));       /* 0.5 - in range */
    
    return 1;
}

static int test_check_feature_out_of_range(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* Feature 0: [-1.0, 1.0] */
    ASSERT(!cm_input_check_feature(&ctx, 0, -65537));     /* Below min */
    ASSERT(!cm_input_check_feature(&ctx, 0, 65537));      /* Above max */
    ASSERT(!cm_input_check_feature(&ctx, 0, 131072));     /* 2.0 - out */
    
    return 1;
}

static int test_check_feature_no_envelope(void)
{
    cm_input_ctx_t ctx;
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, NULL, states, 3);  /* No envelope */
    
    /* Without envelope, all values should pass */
    ASSERT(cm_input_check_feature(&ctx, 0, 999999));
    ASSERT(cm_input_check_feature(&ctx, 0, -999999));
    
    return 1;
}

/*============================================================================
 * Process Tests
 *============================================================================*/

static int test_process_all_in_range(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    int32_t values[3] = {0, 65536, 0};  /* All in range */
    cm_input_result_t result;
    
    ct_result_t rc = cm_input_process(&ctx, values, &result, NULL);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.violations, 0);
    ASSERT_EQ(ctx.window_samples, 1);
    ASSERT_EQ(ctx.total_violations, 0);
    
    return 1;
}

static int test_process_one_violation(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* Feature 0 out of range (2.0 when max is 1.0) */
    int32_t values[3] = {131072, 65536, 0};
    cm_input_result_t result;
    
    ct_result_t rc = cm_input_process(&ctx, values, &result, NULL);
    
    ASSERT_EQ(rc, CT_ERR_RANGE);
    ASSERT_EQ(result.violations, 1);
    ASSERT_EQ(result.first_violation_feature, 0);
    ASSERT_EQ(result.first_violation_value_q16, 131072);
    ASSERT_EQ(result.first_violation_bound_q16, 65536);  /* max bound */
    ASSERT(result.is_upper_bound);
    
    return 1;
}

static int test_process_multiple_violations(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* All features out of range */
    int32_t values[3] = {131072, -65536, 65536};
    cm_input_result_t result;
    
    ct_result_t rc = cm_input_process(&ctx, values, &result, NULL);
    
    ASSERT_EQ(rc, CT_ERR_RANGE);
    ASSERT_EQ(result.violations, 3);
    /* First violation should be feature 0 */
    ASSERT_EQ(result.first_violation_feature, 0);
    
    return 1;
}

static int test_process_lower_bound_violation(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* Feature 0 below minimum (-2.0 when min is -1.0) */
    int32_t values[3] = {-131072, 65536, 0};
    cm_input_result_t result;
    
    ct_result_t rc = cm_input_process(&ctx, values, &result, NULL);
    
    ASSERT_EQ(rc, CT_ERR_RANGE);
    ASSERT(!result.is_upper_bound);
    ASSERT_EQ(result.first_violation_bound_q16, -65536);  /* min bound */
    
    return 1;
}

static int test_process_tracks_min_max(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* Process several samples */
    int32_t v1[3] = {32768, 0, 0};    /* 0.5, 0, 0 */
    int32_t v2[3] = {-32768, 0, 0};   /* -0.5, 0, 0 */
    int32_t v3[3] = {65536, 0, 0};    /* 1.0, 0, 0 */
    
    cm_input_process(&ctx, v1, NULL, NULL);
    cm_input_process(&ctx, v2, NULL, NULL);
    cm_input_process(&ctx, v3, NULL, NULL);
    
    /* Check min/max for feature 0 */
    cm_input_feature_state_t state;
    cm_input_get_feature_state(&ctx, 0, &state);
    
    ASSERT_EQ(state.min_observed_q16, -32768);
    ASSERT_EQ(state.max_observed_q16, 65536);
    ASSERT_EQ(state.sample_count, 3);
    
    return 1;
}

static int test_process_accumulates_violations(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* Process multiple samples with violations */
    int32_t v_good[3] = {0, 65536, 0};
    int32_t v_bad[3] = {131072, 65536, 0};  /* Feature 0 violation */
    
    cm_input_process(&ctx, v_good, NULL, NULL);
    cm_input_process(&ctx, v_bad, NULL, NULL);
    cm_input_process(&ctx, v_bad, NULL, NULL);
    cm_input_process(&ctx, v_good, NULL, NULL);
    
    ASSERT_EQ(cm_input_get_sample_count(&ctx), 4);
    ASSERT_EQ(cm_input_get_violations(&ctx), 2);
    
    /* Check per-feature violation count */
    cm_input_feature_state_t state;
    cm_input_get_feature_state(&ctx, 0, &state);
    ASSERT_EQ(state.violation_count, 2);
    
    return 1;
}

/*============================================================================
 * Histogram Accumulation Tests
 *============================================================================*/

static int test_histogram_accumulation(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_hist_envelope();
    cm_input_feature_state_t states[1];
    
    cm_input_init(&ctx, &env, states, 1);
    
    /* Process values in different bins */
    int32_t v0[1] = {32768};    /* 0.5 - bin 0 */
    int32_t v1[1] = {98304};    /* 1.5 - bin 1 */
    int32_t v2[1] = {163840};   /* 2.5 - bin 2 */
    int32_t v3[1] = {229376};   /* 3.5 - bin 3 */
    
    cm_input_process(&ctx, v0, NULL, NULL);
    cm_input_process(&ctx, v0, NULL, NULL);  /* Two in bin 0 */
    cm_input_process(&ctx, v1, NULL, NULL);
    cm_input_process(&ctx, v2, NULL, NULL);
    cm_input_process(&ctx, v3, NULL, NULL);
    
    /* Check histogram counts */
    cm_input_feature_state_t state;
    cm_input_get_feature_state(&ctx, 0, &state);
    
    ASSERT_EQ(state.hist_counts[0], 2);
    ASSERT_EQ(state.hist_counts[1], 1);
    ASSERT_EQ(state.hist_counts[2], 1);
    ASSERT_EQ(state.hist_counts[3], 1);
    
    return 1;
}

/*============================================================================
 * Drift Detection Tests
 *============================================================================*/

static int test_drift_no_drift(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_hist_envelope();
    cm_input_feature_state_t states[1];
    
    cm_input_init(&ctx, &env, states, 1);
    
    /* Create uniform distribution matching reference */
    int32_t v0[1] = {32768};    /* bin 0 */
    int32_t v1[1] = {98304};    /* bin 1 */
    int32_t v2[1] = {163840};   /* bin 2 */
    int32_t v3[1] = {229376};   /* bin 3 */
    
    for (int i = 0; i < 25; i++) {
        cm_input_process(&ctx, v0, NULL, NULL);
        cm_input_process(&ctx, v1, NULL, NULL);
        cm_input_process(&ctx, v2, NULL, NULL);
        cm_input_process(&ctx, v3, NULL, NULL);
    }
    
    /* Check drift - should be minimal */
    cm_drift_policy_t policy = {0};
    policy.enabled_detectors = CM_DRIFT_TV_ENABLED;
    policy.tv_threshold_q0_32 = 0x19999999;  /* ~0.1 */
    policy.epsilon_q0_32 = 1;
    
    cm_input_drift_result_t result;
    ct_fault_flags_t faults = {0};
    
    ct_result_t rc = cm_input_compute_drift(&ctx, &policy, &result, &faults);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.features_with_drift, 0);  /* No significant drift */
    
    return 1;
}

static int test_drift_detected(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_hist_envelope();
    cm_input_feature_state_t states[1];
    
    cm_input_init(&ctx, &env, states, 1);
    
    /* Create skewed distribution (all in bin 0) - very different from uniform ref */
    int32_t v0[1] = {32768};    /* bin 0 */
    
    for (int i = 0; i < 100; i++) {
        cm_input_process(&ctx, v0, NULL, NULL);
    }
    
    /* Check drift - should be significant */
    cm_drift_policy_t policy = {0};
    policy.enabled_detectors = CM_DRIFT_TV_ENABLED;
    policy.tv_threshold_q0_32 = 0x19999999;  /* ~0.1 */
    policy.epsilon_q0_32 = 1;
    
    cm_input_drift_result_t result;
    ct_fault_flags_t faults = {0};
    
    ct_result_t rc = cm_input_compute_drift(&ctx, &policy, &result, &faults);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.features_with_drift, 1);
    ASSERT(result.first_drift_result.flags & CM_DRIFT_TV_TRIGGERED);
    
    return 1;
}

/*============================================================================
 * Edge Cases
 *============================================================================*/

static int test_boundary_values(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* Test exact boundary values */
    int32_t v_min[3] = {-65536, 0, -32768};     /* All at minimums */
    int32_t v_max[3] = {65536, 131072, 32768};  /* All at maximums */
    
    cm_input_result_t result;
    
    ct_result_t rc = cm_input_process(&ctx, v_min, &result, NULL);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.violations, 0);
    
    rc = cm_input_process(&ctx, v_max, &result, NULL);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.violations, 0);
    
    return 1;
}

static int test_extreme_values(void)
{
    cm_input_ctx_t ctx;
    cm_input_envelope_t env = create_test_envelope();
    cm_input_feature_state_t states[3];
    
    cm_input_init(&ctx, &env, states, 3);
    
    /* Test extreme int32 values */
    int32_t v_extreme[3] = {INT32_MIN, INT32_MAX, 0};
    cm_input_result_t result;
    
    ct_result_t rc = cm_input_process(&ctx, v_extreme, &result, NULL);
    ASSERT_EQ(rc, CT_ERR_RANGE);
    ASSERT_EQ(result.violations, 2);  /* Features 0 and 1 out of range */
    
    return 1;
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("certifiable-monitor: Input Monitor Test Suite\n");
    printf("==============================================\n\n");
    
    printf("Initialization:\n");
    RUN_TEST("init_basic", test_init_basic);
    RUN_TEST("init_null_ctx", test_init_null_ctx);
    RUN_TEST("init_null_states", test_init_null_states);
    RUN_TEST("init_zero_features", test_init_zero_features);
    RUN_TEST("init_feature_count_mismatch", test_init_feature_count_mismatch);
    RUN_TEST("init_no_envelope", test_init_no_envelope);
    
    printf("\nWindow Reset:\n");
    RUN_TEST("reset_window", test_reset_window);
    
    printf("\nRange Checking:\n");
    RUN_TEST("check_feature_in_range", test_check_feature_in_range);
    RUN_TEST("check_feature_out_of_range", test_check_feature_out_of_range);
    RUN_TEST("check_feature_no_envelope", test_check_feature_no_envelope);
    
    printf("\nSample Processing:\n");
    RUN_TEST("process_all_in_range", test_process_all_in_range);
    RUN_TEST("process_one_violation", test_process_one_violation);
    RUN_TEST("process_multiple_violations", test_process_multiple_violations);
    RUN_TEST("process_lower_bound_violation", test_process_lower_bound_violation);
    RUN_TEST("process_tracks_min_max", test_process_tracks_min_max);
    RUN_TEST("process_accumulates_violations", test_process_accumulates_violations);
    
    printf("\nHistogram Accumulation:\n");
    RUN_TEST("histogram_accumulation", test_histogram_accumulation);
    
    printf("\nDrift Detection:\n");
    RUN_TEST("drift_no_drift", test_drift_no_drift);
    RUN_TEST("drift_detected", test_drift_detected);
    
    printf("\nEdge Cases:\n");
    RUN_TEST("boundary_values", test_boundary_values);
    RUN_TEST("extreme_values", test_extreme_values);
    
    printf("\n==============================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
