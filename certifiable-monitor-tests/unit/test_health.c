/**
 * @file test_health.c
 * @brief Health Monitor FSM Test Suite
 * @traceability CM-ARCH-MATH-001 §6, SRS-005-HEALTH
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "health.h"
#include "cm_types.h"
#include "ct_types.h"
#include <stdio.h>
#include <string.h>

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
 * Initialization Tests
 *============================================================================*/

static int test_init_basic(void)
{
    cm_health_ctx_t ctx;
    ct_result_t rc = cm_health_init(&ctx, NULL);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(ctx.initialized);
    ASSERT_EQ(ctx.state, CM_STATE_INIT);
    ASSERT_EQ(ctx.overflow_count, 0);
    
    return 1;
}

static int test_init_null_ctx(void)
{
    ct_result_t rc = cm_health_init(NULL, NULL);
    ASSERT_EQ(rc, CT_ERR_NULL);
    return 1;
}

static int test_init_with_budget(void)
{
    cm_health_ctx_t ctx;
    cm_fault_budget_t budget = {
        .overflow_budget = 10,
        .underflow_budget = 10,
        .saturation_budget = 100,
        .clamp_budget = 100
    };
    
    ct_result_t rc = cm_health_init(&ctx, &budget);
    ASSERT_EQ(rc, CT_OK);
    ASSERT(ctx.budget == &budget);
    
    return 1;
}

/*============================================================================
 * Enable Tests
 *============================================================================*/

static int test_enable_from_init(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    
    ASSERT_EQ(ctx.state, CM_STATE_INIT);
    
    ct_result_t rc = cm_health_enable(&ctx);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(ctx.state, CM_STATE_ENABLED);
    
    return 1;
}

static int test_enable_idempotent(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    cm_health_enable(&ctx);
    
    /* Enable again - should stay ENABLED */
    ct_result_t rc = cm_health_enable(&ctx);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(ctx.state, CM_STATE_ENABLED);
    
    return 1;
}

/*============================================================================
 * Fault Recording Tests
 *============================================================================*/

static int test_record_faults(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    
    ct_fault_flags_t faults = {0};
    faults.overflow = 1;
    
    ct_result_t rc = cm_health_record_faults(&ctx, &faults);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(ctx.overflow_count, 1);
    ASSERT_EQ(ctx.total_overflows, 1);
    
    /* Record more */
    cm_health_record_faults(&ctx, &faults);
    ASSERT_EQ(ctx.overflow_count, 2);
    ASSERT_EQ(ctx.total_overflows, 2);
    
    return 1;
}

static int test_record_saturation(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    
    cm_health_record_saturation(&ctx);
    cm_health_record_saturation(&ctx);
    
    ASSERT_EQ(ctx.saturation_count, 2);
    ASSERT_EQ(ctx.total_saturations, 2);
    
    return 1;
}

static int test_record_clamp(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    
    cm_health_record_clamp(&ctx);
    
    ASSERT_EQ(ctx.clamp_count, 1);
    ASSERT_EQ(ctx.total_clamps, 1);
    
    return 1;
}

/*============================================================================
 * Budget Check Tests
 *============================================================================*/

static int test_budget_not_exceeded(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);  /* Default budgets */
    cm_health_enable(&ctx);
    
    /* Record some faults, but below budget */
    for (int i = 0; i < 50; i++) {
        cm_health_record_saturation(&ctx);
    }
    
    cm_health_result_t result;
    ct_result_t rc = cm_health_check_budget(&ctx, &result);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(!result.budget_exceeded);
    ASSERT_EQ(result.exceeded_count, 0);
    
    return 1;
}

static int test_budget_exceeded(void)
{
    cm_health_ctx_t ctx;
    cm_fault_budget_t budget = {
        .overflow_budget = 5,
        .underflow_budget = 5,
        .saturation_budget = 10,
        .clamp_budget = 10
    };
    cm_health_init(&ctx, &budget);
    cm_health_enable(&ctx);
    
    /* Exceed saturation budget */
    for (int i = 0; i < 15; i++) {
        cm_health_record_saturation(&ctx);
    }
    
    cm_health_result_t result;
    ct_result_t rc = cm_health_check_budget(&ctx, &result);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(result.budget_exceeded);
    ASSERT_EQ(result.worst_violation, CM_VIOL_FAULT_BUDGET);
    
    return 1;
}

/*============================================================================
 * State Machine Tests
 *============================================================================*/

static int test_enabled_to_alarm(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    cm_health_enable(&ctx);
    
    ASSERT_EQ(ctx.state, CM_STATE_ENABLED);
    
    cm_health_transition_t trans;
    ct_result_t rc = cm_health_process_window_end(&ctx, true, &trans);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(trans.transition_occurred);
    ASSERT_EQ(trans.old_state, CM_STATE_ENABLED);
    ASSERT_EQ(trans.new_state, CM_STATE_ALARM);
    ASSERT_EQ(ctx.state, CM_STATE_ALARM);
    
    return 1;
}

static int test_alarm_to_enabled(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    cm_health_enable(&ctx);
    
    /* Go to ALARM */
    cm_health_process_window_end(&ctx, true, NULL);
    ASSERT_EQ(ctx.state, CM_STATE_ALARM);
    
    /* Recover - no violations */
    cm_health_transition_t trans;
    cm_health_process_window_end(&ctx, false, &trans);
    
    ASSERT(trans.transition_occurred);
    ASSERT_EQ(trans.new_state, CM_STATE_ENABLED);
    ASSERT_EQ(ctx.state, CM_STATE_ENABLED);
    
    return 1;
}

static int test_alarm_to_degraded(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    cm_health_enable(&ctx);
    
    /* Go to ALARM and keep having violations */
    /* Default threshold is 3 consecutive alarm windows */
    cm_health_process_window_end(&ctx, true, NULL);  /* 1 */
    ASSERT_EQ(ctx.state, CM_STATE_ALARM);
    
    cm_health_process_window_end(&ctx, true, NULL);  /* 2 */
    ASSERT_EQ(ctx.state, CM_STATE_ALARM);
    
    cm_health_transition_t trans;
    cm_health_process_window_end(&ctx, true, &trans);  /* 3 → DEGRADED */
    
    ASSERT(trans.transition_occurred);
    ASSERT_EQ(trans.new_state, CM_STATE_DEGRADED);
    ASSERT_EQ(ctx.state, CM_STATE_DEGRADED);
    
    return 1;
}

static int test_degraded_to_stopped(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    cm_health_enable(&ctx);
    
    /* Fast-forward to DEGRADED */
    cm_health_process_window_end(&ctx, true, NULL);
    cm_health_process_window_end(&ctx, true, NULL);
    cm_health_process_window_end(&ctx, true, NULL);
    ASSERT_EQ(ctx.state, CM_STATE_DEGRADED);
    
    /* Any violation in DEGRADED → STOPPED */
    cm_health_transition_t trans;
    cm_health_process_window_end(&ctx, true, &trans);
    
    ASSERT(trans.transition_occurred);
    ASSERT_EQ(trans.new_state, CM_STATE_STOPPED);
    ASSERT_EQ(ctx.state, CM_STATE_STOPPED);
    
    return 1;
}

static int test_emergency_stop(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    cm_health_enable(&ctx);
    
    cm_health_transition_t trans;
    ct_result_t rc = cm_health_emergency_stop(&ctx, &trans);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(trans.transition_occurred);
    ASSERT_EQ(trans.new_state, CM_STATE_STOPPED);
    ASSERT_EQ(ctx.state, CM_STATE_STOPPED);
    
    return 1;
}

/*============================================================================
 * Query Tests
 *============================================================================*/

static int test_is_operational(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    
    ASSERT(!cm_health_is_operational(&ctx));  /* INIT */
    
    cm_health_enable(&ctx);
    ASSERT(cm_health_is_operational(&ctx));  /* ENABLED */
    
    cm_health_process_window_end(&ctx, true, NULL);
    ASSERT(cm_health_is_operational(&ctx));  /* ALARM */
    
    return 1;
}

static int test_is_stopped(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    cm_health_enable(&ctx);
    
    ASSERT(!cm_health_is_stopped(&ctx));
    
    cm_health_emergency_stop(&ctx, NULL);
    ASSERT(cm_health_is_stopped(&ctx));
    
    return 1;
}

/*============================================================================
 * Window Reset Tests
 *============================================================================*/

static int test_window_reset(void)
{
    cm_health_ctx_t ctx;
    cm_health_init(&ctx, NULL);
    cm_health_enable(&ctx);
    
    /* Accumulate faults */
    cm_health_record_saturation(&ctx);
    cm_health_record_saturation(&ctx);
    cm_health_record_clamp(&ctx);
    
    ASSERT_EQ(ctx.saturation_count, 2);
    ASSERT_EQ(ctx.clamp_count, 1);
    
    /* Reset window */
    ct_result_t rc = cm_health_reset_window(&ctx, 42);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(ctx.window_id, 42);
    
    /* Per-window counters reset */
    ASSERT_EQ(ctx.saturation_count, 0);
    ASSERT_EQ(ctx.clamp_count, 0);
    
    /* But totals preserved */
    ASSERT_EQ(ctx.total_saturations, 2);
    ASSERT_EQ(ctx.total_clamps, 1);
    
    return 1;
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("certifiable-monitor: Health Monitor FSM Test Suite\n");
    printf("==================================================\n\n");
    
    printf("Initialization:\n");
    RUN_TEST("init_basic", test_init_basic);
    RUN_TEST("init_null_ctx", test_init_null_ctx);
    RUN_TEST("init_with_budget", test_init_with_budget);
    
    printf("\nEnable:\n");
    RUN_TEST("enable_from_init", test_enable_from_init);
    RUN_TEST("enable_idempotent", test_enable_idempotent);
    
    printf("\nFault Recording:\n");
    RUN_TEST("record_faults", test_record_faults);
    RUN_TEST("record_saturation", test_record_saturation);
    RUN_TEST("record_clamp", test_record_clamp);
    
    printf("\nBudget Check:\n");
    RUN_TEST("budget_not_exceeded", test_budget_not_exceeded);
    RUN_TEST("budget_exceeded", test_budget_exceeded);
    
    printf("\nState Machine:\n");
    RUN_TEST("enabled_to_alarm", test_enabled_to_alarm);
    RUN_TEST("alarm_to_enabled", test_alarm_to_enabled);
    RUN_TEST("alarm_to_degraded", test_alarm_to_degraded);
    RUN_TEST("degraded_to_stopped", test_degraded_to_stopped);
    RUN_TEST("emergency_stop", test_emergency_stop);
    
    printf("\nQueries:\n");
    RUN_TEST("is_operational", test_is_operational);
    RUN_TEST("is_stopped", test_is_stopped);
    
    printf("\nWindow Reset:\n");
    RUN_TEST("window_reset", test_window_reset);
    
    printf("\n==================================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
