/**
 * @file test_activation.c
 * @brief Activation Monitor Test Suite
 * @traceability CM-ARCH-MATH-001 §4, SRS-003-ACTIVATION
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "activation.h"
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

/* Create contracts for 2 layers */
static void create_test_contracts(cm_layer_contract_t *contracts)
{
    memset(contracts, 0, 2 * sizeof(cm_layer_contract_t));
    
    /* Layer 0: conv1, bounds [-1.0, 1.0], 1% violation rate tolerance */
    contracts[0].layer_id = 100;
    contracts[0].contract.min_q16 = -65536;    /* -1.0 */
    contracts[0].contract.max_q16 = 65536;     /* 1.0 */
    contracts[0].contract.tol_violations_q0_32 = 42949672;  /* ~1% */
    contracts[0].contract.max_overrange_q16 = 32768;  /* 0.5 max overrange */
    
    /* Layer 1: fc1, bounds [0.0, 2.0], 5% violation rate tolerance */
    contracts[1].layer_id = 200;
    contracts[1].contract.min_q16 = 0;         /* 0.0 */
    contracts[1].contract.max_q16 = 131072;    /* 2.0 */
    contracts[1].contract.tol_violations_q0_32 = 214748364;  /* ~5% */
    contracts[1].contract.max_overrange_q16 = 65536;  /* 1.0 max overrange */
}

/*============================================================================
 * Initialization Tests
 *============================================================================*/

static int test_init_basic(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    
    ct_result_t rc = cm_activ_init(&ctx, contracts, states, 2);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(ctx.initialized);
    ASSERT_EQ(ctx.layer_count, 2);
    ASSERT_EQ(ctx.window_samples, 0);
    ASSERT_EQ(states[0].layer_id, 100);
    ASSERT_EQ(states[1].layer_id, 200);
    
    return 1;
}

static int test_init_null_ctx(void)
{
    cm_activ_layer_state_t states[2];
    ct_result_t rc = cm_activ_init(NULL, NULL, states, 2);
    ASSERT_EQ(rc, CT_ERR_NULL);
    return 1;
}

static int test_init_null_states(void)
{
    cm_activ_ctx_t ctx;
    ct_result_t rc = cm_activ_init(&ctx, NULL, NULL, 2);
    ASSERT_EQ(rc, CT_ERR_NULL);
    return 1;
}

static int test_init_zero_layers(void)
{
    cm_activ_ctx_t ctx;
    cm_activ_layer_state_t states[2];
    ct_result_t rc = cm_activ_init(&ctx, NULL, states, 0);
    ASSERT_EQ(rc, CT_ERR_SIZE);
    return 1;
}

static int test_init_no_contracts(void)
{
    cm_activ_ctx_t ctx;
    cm_activ_layer_state_t states[2];
    
    ct_result_t rc = cm_activ_init(&ctx, NULL, states, 2);
    ASSERT_EQ(rc, CT_OK);
    ASSERT(ctx.initialized);
    ASSERT(ctx.contracts == NULL);
    /* Default layer IDs should be indices */
    ASSERT_EQ(states[0].layer_id, 0);
    ASSERT_EQ(states[1].layer_id, 1);
    
    return 1;
}

/*============================================================================
 * Window Reset Tests
 *============================================================================*/

static int test_reset_window(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Process some activations */
    int32_t values[4] = {0, 32768, 65536, -32768};  /* All in range */
    cm_activ_check_layer(&ctx, 0, values, 4, NULL, NULL);
    
    ASSERT(states[0].total_elements > 0);
    
    /* Reset window */
    ct_result_t rc = cm_activ_reset_window(&ctx, 42);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(ctx.window_id, 42);
    ASSERT_EQ(ctx.window_samples, 0);
    
    /* Check layer state is reset but layer_id preserved */
    ASSERT_EQ(states[0].total_elements, 0);
    ASSERT_EQ(states[0].violation_count, 0);
    ASSERT_EQ(states[0].layer_id, 100);
    
    return 1;
}

/*============================================================================
 * Layer Checking Tests
 *============================================================================*/

static int test_check_all_in_range(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Layer 0: [-1.0, 1.0] - all in range */
    int32_t values[4] = {0, 32768, -32768, 65536};
    cm_activ_layer_result_t result;
    
    ct_result_t rc = cm_activ_check_layer(&ctx, 0, values, 4, &result, NULL);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.violations, 0);
    ASSERT_EQ(states[0].total_elements, 4);
    
    return 1;
}

static int test_check_one_violation(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Layer 0: [-1.0, 1.0], 1% tolerance */
    /* Need 100+ elements to keep 1 violation under 1% */
    int32_t values[200];
    for (int i = 0; i < 200; i++) {
        values[i] = 0;  /* In range */
    }
    values[0] = 98304;  /* 1.5 - one value out of range */
    
    cm_activ_layer_result_t result;
    
    ct_result_t rc = cm_activ_check_layer(&ctx, 0, values, 200, &result, NULL);
    
    ASSERT_EQ(rc, CT_OK);  /* Within tolerance (0.5% < 1%) */
    ASSERT_EQ(result.violations, 1);
    ASSERT_EQ(states[0].violation_count, 1);
    
    /* Overrange should be 0.5 (32768) */
    ASSERT_EQ(result.max_overrange_q16, 32768);
    
    return 1;
}

static int test_check_multiple_violations(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Layer 0: [-1.0, 1.0] - multiple out of range */
    int32_t values[4] = {131072, -131072, 196608, -196608};  /* All 2.0 or -2.0 */
    cm_activ_layer_result_t result;
    
    ct_result_t rc = cm_activ_check_layer(&ctx, 0, values, 4, &result, NULL);
    
    /* All 4 values violate */
    ASSERT_EQ(result.violations, 4);
    /* Max overrange: 3.0 - 1.0 = 2.0 (131072) */
    ASSERT_EQ(result.max_overrange_q16, 131072);
    
    /* Should exceed tolerance since rate is 100% > 1% */
    ASSERT(result.rate_exceeded);
    ASSERT_EQ(rc, CT_ERR_RANGE);
    
    return 1;
}

static int test_check_lower_bound_violation(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Layer 1: [0.0, 2.0], 5% tolerance */
    /* Need 20+ elements to keep 1 violation under 5% */
    int32_t values[100];
    for (int i = 0; i < 100; i++) {
        values[i] = 65536;  /* In range (1.0) */
    }
    values[0] = -32768;  /* -0.5 - violates lower bound */
    
    cm_activ_layer_result_t result;
    
    ct_result_t rc = cm_activ_check_layer(&ctx, 1, values, 100, &result, NULL);
    
    ASSERT_EQ(rc, CT_OK);  /* Within tolerance (1% < 5%) */
    ASSERT_EQ(result.violations, 1);
    /* Overrange = 0.5 (32768) */
    ASSERT_EQ(result.max_overrange_q16, 32768);
    
    return 1;
}

static int test_check_tracks_min_max(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Process multiple batches */
    int32_t v1[2] = {32768, -32768};  /* 0.5, -0.5 */
    int32_t v2[2] = {65536, -65536};  /* 1.0, -1.0 */
    
    cm_activ_check_layer(&ctx, 0, v1, 2, NULL, NULL);
    cm_activ_check_layer(&ctx, 0, v2, 2, NULL, NULL);
    
    cm_activ_layer_state_t state;
    cm_activ_get_layer_state(&ctx, 0, &state);
    
    ASSERT_EQ(state.min_observed_q16, -65536);
    ASSERT_EQ(state.max_observed_q16, 65536);
    ASSERT_EQ(state.total_elements, 4);
    
    return 1;
}

static int test_check_invalid_layer(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    int32_t values[2] = {0, 0};
    ct_result_t rc = cm_activ_check_layer(&ctx, 5, values, 2, NULL, NULL);
    
    ASSERT_EQ(rc, CT_ERR_RANGE);
    
    return 1;
}

/*============================================================================
 * Saturation Tests
 *============================================================================*/

static int test_saturation_from_faults(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    int32_t values[2] = {0, 32768};
    ct_fault_flags_t faults = {0};
    faults.overflow = 1;
    
    cm_activ_check_layer(&ctx, 0, values, 2, NULL, &faults);
    
    ASSERT_EQ(states[0].saturation_count, 1);
    
    return 1;
}

static int test_record_saturation(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    ct_result_t rc = cm_activ_record_saturation(&ctx, 0);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(states[0].saturation_count, 1);
    
    cm_activ_record_saturation(&ctx, 0);
    cm_activ_record_saturation(&ctx, 1);
    
    ASSERT_EQ(states[0].saturation_count, 2);
    ASSERT_EQ(states[1].saturation_count, 1);
    
    return 1;
}

/*============================================================================
 * Violation Rate Tests
 *============================================================================*/

static int test_violation_rate_zero(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* All in range */
    int32_t values[100];
    for (int i = 0; i < 100; i++) {
        values[i] = 0;
    }
    
    cm_activ_check_layer(&ctx, 0, values, 100, NULL, NULL);
    
    uint32_t rate = cm_activ_get_violation_rate(&ctx, 0);
    ASSERT_EQ(rate, 0);
    ASSERT(!cm_activ_exceeds_tolerance(&ctx, 0));
    
    return 1;
}

static int test_violation_rate_100_percent(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* All out of range */
    int32_t values[4] = {131072, 131072, 131072, 131072};  /* All 2.0 */
    cm_activ_check_layer(&ctx, 0, values, 4, NULL, NULL);
    
    uint32_t rate = cm_activ_get_violation_rate(&ctx, 0);
    ASSERT_EQ(rate, UINT32_MAX);
    ASSERT(cm_activ_exceeds_tolerance(&ctx, 0));
    
    return 1;
}

static int test_violation_rate_partial(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* 1 out of 100 = 1% */
    int32_t values[100];
    for (int i = 0; i < 100; i++) {
        values[i] = 0;  /* In range */
    }
    values[0] = 131072;  /* Out of range */
    
    cm_activ_check_layer(&ctx, 0, values, 100, NULL, NULL);
    
    uint32_t rate = cm_activ_get_violation_rate(&ctx, 0);
    /* Should be ~1% which is 42949672 in Q0.32 */
    /* Actually 1/100 * 2^32 = 42949672.96 */
    ASSERT(rate > 40000000 && rate < 50000000);
    
    return 1;
}

/*============================================================================
 * Window Summary Tests
 *============================================================================*/

static int test_window_summary_no_violations(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Layer 0: [-1.0, 1.0] - all values in range */
    int32_t values0[4] = {0, 32768, -32768, 65536};
    cm_activ_check_layer(&ctx, 0, values0, 4, NULL, NULL);
    
    /* Layer 1: [0.0, 2.0] - all values in range (no negatives!) */
    int32_t values1[4] = {0, 32768, 65536, 131072};
    cm_activ_check_layer(&ctx, 1, values1, 4, NULL, NULL);
    
    cm_activ_result_t result;
    ct_result_t rc = cm_activ_get_window_summary(&ctx, &result);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.layers_with_violations, 0);
    ASSERT_EQ(result.total_saturations, 0);
    
    return 1;
}

static int test_window_summary_with_violations(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Layer 0: one violation */
    int32_t v0[4] = {0, 131072, 0, 0};  /* One out of range */
    cm_activ_check_layer(&ctx, 0, v0, 4, NULL, NULL);
    
    /* Layer 1: no violations */
    int32_t v1[4] = {65536, 65536, 65536, 65536};
    cm_activ_check_layer(&ctx, 1, v1, 4, NULL, NULL);
    
    /* Record saturation */
    cm_activ_record_saturation(&ctx, 0);
    
    cm_activ_result_t result;
    ct_result_t rc = cm_activ_get_window_summary(&ctx, &result);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.layers_with_violations, 1);
    ASSERT_EQ(result.first_violation_layer, 0);
    ASSERT_EQ(result.first_result.violations, 1);
    ASSERT_EQ(result.total_saturations, 1);
    
    return 1;
}

/*============================================================================
 * Layer Finding Tests
 *============================================================================*/

static int test_find_layer_found(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    uint32_t idx;
    ct_result_t rc = cm_activ_find_layer(&ctx, 200, &idx);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(idx, 1);
    
    return 1;
}

static int test_find_layer_not_found(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    uint32_t idx;
    ct_result_t rc = cm_activ_find_layer(&ctx, 999, &idx);
    
    ASSERT_EQ(rc, CT_ERR_RANGE);
    
    return 1;
}

/*============================================================================
 * Edge Cases
 *============================================================================*/

static int test_boundary_values(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Test exact boundary values for layer 0: [-1.0, 1.0] */
    int32_t values[2] = {-65536, 65536};  /* Exactly at bounds */
    cm_activ_layer_result_t result;
    
    ct_result_t rc = cm_activ_check_layer(&ctx, 0, values, 2, &result, NULL);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.violations, 0);  /* On boundary is valid */
    
    return 1;
}

static int test_max_overrange_exceeded(void)
{
    cm_activ_ctx_t ctx;
    cm_layer_contract_t contracts[2];
    cm_activ_layer_state_t states[2];
    
    create_test_contracts(contracts);
    cm_activ_init(&ctx, contracts, states, 2);
    
    /* Layer 0 max_overrange_q16 = 32768 (0.5) */
    /* Value 2.0 (131072) is 1.0 past bound = 65536 overrange > 32768 */
    int32_t values[100];
    for (int i = 0; i < 100; i++) {
        values[i] = 0;  /* In range */
    }
    values[0] = 131072;  /* Way out of range */
    
    cm_activ_layer_result_t result;
    ct_result_t rc = cm_activ_check_layer(&ctx, 0, values, 100, &result, NULL);
    
    /* Should fail because max overrange (65536) > limit (32768) */
    ASSERT(result.max_exceeded);
    ASSERT_EQ(rc, CT_ERR_RANGE);
    
    return 1;
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("certifiable-monitor: Activation Monitor Test Suite\n");
    printf("===================================================\n\n");
    
    printf("Initialization:\n");
    RUN_TEST("init_basic", test_init_basic);
    RUN_TEST("init_null_ctx", test_init_null_ctx);
    RUN_TEST("init_null_states", test_init_null_states);
    RUN_TEST("init_zero_layers", test_init_zero_layers);
    RUN_TEST("init_no_contracts", test_init_no_contracts);
    
    printf("\nWindow Reset:\n");
    RUN_TEST("reset_window", test_reset_window);
    
    printf("\nLayer Checking:\n");
    RUN_TEST("check_all_in_range", test_check_all_in_range);
    RUN_TEST("check_one_violation", test_check_one_violation);
    RUN_TEST("check_multiple_violations", test_check_multiple_violations);
    RUN_TEST("check_lower_bound_violation", test_check_lower_bound_violation);
    RUN_TEST("check_tracks_min_max", test_check_tracks_min_max);
    RUN_TEST("check_invalid_layer", test_check_invalid_layer);
    
    printf("\nSaturation Handling:\n");
    RUN_TEST("saturation_from_faults", test_saturation_from_faults);
    RUN_TEST("record_saturation", test_record_saturation);
    
    printf("\nViolation Rate:\n");
    RUN_TEST("violation_rate_zero", test_violation_rate_zero);
    RUN_TEST("violation_rate_100_percent", test_violation_rate_100_percent);
    RUN_TEST("violation_rate_partial", test_violation_rate_partial);
    
    printf("\nWindow Summary:\n");
    RUN_TEST("window_summary_no_violations", test_window_summary_no_violations);
    RUN_TEST("window_summary_with_violations", test_window_summary_with_violations);
    
    printf("\nLayer Finding:\n");
    RUN_TEST("find_layer_found", test_find_layer_found);
    RUN_TEST("find_layer_not_found", test_find_layer_not_found);
    
    printf("\nEdge Cases:\n");
    RUN_TEST("boundary_values", test_boundary_values);
    RUN_TEST("max_overrange_exceeded", test_max_overrange_exceeded);
    
    printf("\n===================================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
