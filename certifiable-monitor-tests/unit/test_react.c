/**
 * @file test_react.c
 * @brief Reaction Handler Test Suite
 * @traceability CM-ARCH-MATH-001 §8, SRS-007-REACT
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "react.h"
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
 * Test Fixtures
 *============================================================================*/

static cm_reaction_map_entry_t test_react_map[] = {
    { CM_VIOL_INPUT_RANGE,  CM_REACT_WARN_OPERATOR },
    { CM_VIOL_INPUT_DRIFT,  CM_REACT_CLAMP_OUTPUT },
    { CM_VIOL_ACTIV_RANGE,  CM_REACT_DEGRADE_MODE },
    { CM_VIOL_OUTPUT_RANGE, CM_REACT_EMERGENCY_STOP },
    { CM_VIOL_FAULT_BUDGET, CM_REACT_EMERGENCY_STOP }
};

/*============================================================================
 * Initialization Tests
 *============================================================================*/

static int test_init_basic(void)
{
    cm_react_handler_t handler;
    ct_result_t rc = cm_react_init(&handler, test_react_map, 5);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(handler.initialized);
    ASSERT_EQ(handler.react_map_count, 5);
    ASSERT_EQ(handler.default_reaction, CM_REACT_LOG_ONLY);
    
    return 1;
}

static int test_init_null_handler(void)
{
    ct_result_t rc = cm_react_init(NULL, test_react_map, 5);
    ASSERT_EQ(rc, CT_ERR_NULL);
    return 1;
}

static int test_init_no_map(void)
{
    cm_react_handler_t handler;
    ct_result_t rc = cm_react_init(&handler, NULL, 0);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT(handler.initialized);
    ASSERT(handler.react_map == NULL);
    
    return 1;
}

static int test_set_default(void)
{
    cm_react_handler_t handler;
    cm_react_init(&handler, NULL, 0);
    
    ct_result_t rc = cm_react_set_default(&handler, CM_REACT_WARN_OPERATOR);
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(handler.default_reaction, CM_REACT_WARN_OPERATOR);
    
    return 1;
}

/*============================================================================
 * Lookup Tests
 *============================================================================*/

static int test_lookup_found(void)
{
    cm_react_handler_t handler;
    cm_react_init(&handler, test_react_map, 5);
    
    cm_reaction_t r = cm_react_lookup(&handler, CM_VIOL_INPUT_RANGE);
    ASSERT_EQ(r, CM_REACT_WARN_OPERATOR);
    
    r = cm_react_lookup(&handler, CM_VIOL_OUTPUT_RANGE);
    ASSERT_EQ(r, CM_REACT_EMERGENCY_STOP);
    
    return 1;
}

static int test_lookup_not_found(void)
{
    cm_react_handler_t handler;
    cm_react_init(&handler, test_react_map, 5);
    cm_react_set_default(&handler, CM_REACT_CLAMP_OUTPUT);
    
    /* ACTIV_SAT is not in the map */
    cm_reaction_t r = cm_react_lookup(&handler, CM_VIOL_ACTIV_SAT);
    ASSERT_EQ(r, CM_REACT_CLAMP_OUTPUT);  /* Should return default */
    
    return 1;
}

static int test_lookup_none(void)
{
    cm_react_handler_t handler;
    cm_react_init(&handler, test_react_map, 5);
    
    cm_reaction_t r = cm_react_lookup(&handler, CM_VIOL_NONE);
    ASSERT_EQ(r, CM_REACT_LOG_ONLY);
    
    return 1;
}

/*============================================================================
 * Process Tests
 *============================================================================*/

static int test_process_violation(void)
{
    cm_react_handler_t handler;
    cm_react_init(&handler, test_react_map, 5);
    
    cm_react_result_t result;
    ct_result_t rc = cm_react_process(&handler, CM_VIOL_INPUT_DRIFT, &result);
    
    ASSERT_EQ(rc, CT_OK);
    ASSERT_EQ(result.violation, CM_VIOL_INPUT_DRIFT);
    ASSERT_EQ(result.action, CM_REACT_CLAMP_OUTPUT);
    ASSERT(result.action_executed);
    
    return 1;
}

/*============================================================================
 * Query Tests
 *============================================================================*/

static int test_is_stop(void)
{
    ASSERT(cm_react_is_stop(CM_REACT_EMERGENCY_STOP));
    ASSERT(!cm_react_is_stop(CM_REACT_DEGRADE_MODE));
    ASSERT(!cm_react_is_stop(CM_REACT_LOG_ONLY));
    return 1;
}

static int test_is_degrade(void)
{
    ASSERT(cm_react_is_degrade(CM_REACT_DEGRADE_MODE));
    ASSERT(cm_react_is_degrade(CM_REACT_EMERGENCY_STOP));
    ASSERT(!cm_react_is_degrade(CM_REACT_CLAMP_OUTPUT));
    return 1;
}

static int test_is_clamp(void)
{
    ASSERT(cm_react_is_clamp(CM_REACT_CLAMP_OUTPUT));
    ASSERT(!cm_react_is_clamp(CM_REACT_DEGRADE_MODE));
    return 1;
}

/*============================================================================
 * Name Tests
 *============================================================================*/

static int test_react_names(void)
{
    ASSERT(strcmp(cm_react_name(CM_REACT_LOG_ONLY), "LOG_ONLY") == 0);
    ASSERT(strcmp(cm_react_name(CM_REACT_EMERGENCY_STOP), "EMERGENCY_STOP") == 0);
    return 1;
}

static int test_viol_names(void)
{
    ASSERT(strcmp(cm_viol_name(CM_VIOL_NONE), "NONE") == 0);
    ASSERT(strcmp(cm_viol_name(CM_VIOL_INPUT_RANGE), "INPUT_RANGE") == 0);
    ASSERT(strcmp(cm_viol_name(CM_VIOL_FAULT_BUDGET), "FAULT_BUDGET") == 0);
    return 1;
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("certifiable-monitor: Reaction Handler Test Suite\n");
    printf("================================================\n\n");
    
    printf("Initialization:\n");
    RUN_TEST("init_basic", test_init_basic);
    RUN_TEST("init_null_handler", test_init_null_handler);
    RUN_TEST("init_no_map", test_init_no_map);
    RUN_TEST("set_default", test_set_default);
    
    printf("\nLookup:\n");
    RUN_TEST("lookup_found", test_lookup_found);
    RUN_TEST("lookup_not_found", test_lookup_not_found);
    RUN_TEST("lookup_none", test_lookup_none);
    
    printf("\nProcess:\n");
    RUN_TEST("process_violation", test_process_violation);
    
    printf("\nQueries:\n");
    RUN_TEST("is_stop", test_is_stop);
    RUN_TEST("is_degrade", test_is_degrade);
    RUN_TEST("is_clamp", test_is_clamp);
    
    printf("\nNames:\n");
    RUN_TEST("react_names", test_react_names);
    RUN_TEST("viol_names", test_viol_names);
    
    printf("\n================================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
