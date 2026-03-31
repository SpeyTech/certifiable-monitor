/**
 * @file test_policy.c
 * @brief Policy Parsing Test Suite
 * @traceability SRS-001-POLICY
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "policy.h"
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
 * Test JSON Primitives
 *============================================================================*/

static int test_json_get_int_basic(void)
{
    const char *json = "{\"version\": 42, \"name\": \"test\"}";
    int64_t val;
    
    cm_policy_result_t rc = cm_json_get_int(json, strlen(json), "version", &val);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT_EQ(val, 42);
    
    return 1;
}

static int test_json_get_int_negative(void)
{
    const char *json = "{\"offset\": -100}";
    int64_t val;
    
    cm_policy_result_t rc = cm_json_get_int(json, strlen(json), "offset", &val);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT_EQ(val, -100);
    
    return 1;
}

static int test_json_get_int_missing(void)
{
    const char *json = "{\"other\": 1}";
    int64_t val;
    
    cm_policy_result_t rc = cm_json_get_int(json, strlen(json), "missing", &val);
    ASSERT_EQ(rc, CM_POL_ERR_MISSING);
    
    return 1;
}

static int test_json_get_string_basic(void)
{
    const char *json = "{\"name\": \"hello world\"}";
    char buf[64];
    size_t len;
    
    cm_policy_result_t rc = cm_json_get_string(json, strlen(json), "name", 
                                               buf, sizeof(buf), &len);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT(strcmp(buf, "hello world") == 0);
    ASSERT_EQ(len, 11);
    
    return 1;
}

static int test_json_get_string_escaped(void)
{
    const char *json = "{\"msg\": \"line1\\nline2\"}";
    char buf[64];
    
    cm_policy_result_t rc = cm_json_get_string(json, strlen(json), "msg",
                                               buf, sizeof(buf), NULL);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT(strcmp(buf, "line1\nline2") == 0);
    
    return 1;
}

static int test_json_get_array_basic(void)
{
    const char *json = "{\"items\": [1, 2, 3]}";
    const char *arr_start;
    size_t arr_len;
    
    cm_policy_result_t rc = cm_json_get_array(json, strlen(json), "items",
                                              &arr_start, &arr_len);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT(arr_len > 0);
    
    return 1;
}

static int test_json_get_object_basic(void)
{
    const char *json = "{\"config\": {\"a\": 1, \"b\": 2}}";
    const char *obj_start;
    size_t obj_len;
    
    cm_policy_result_t rc = cm_json_get_object(json, strlen(json), "config",
                                               &obj_start, &obj_len);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT(obj_len > 0);
    
    return 1;
}

static int test_json_nested_object(void)
{
    const char *json = 
        "{"
        "  \"outer\": {"
        "    \"inner\": 99"
        "  }"
        "}";
    
    const char *obj_start;
    size_t obj_len;
    
    cm_policy_result_t rc = cm_json_get_object(json, strlen(json), "outer",
                                               &obj_start, &obj_len);
    ASSERT_EQ(rc, CM_POL_OK);
    
    /* Parse nested value */
    char nested_json[256];
    nested_json[0] = '{';
    memcpy(nested_json + 1, obj_start, obj_len);
    nested_json[obj_len + 1] = '}';
    nested_json[obj_len + 2] = '\0';
    
    int64_t inner_val;
    rc = cm_json_get_int(nested_json, obj_len + 2, "inner", &inner_val);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT_EQ(inner_val, 99);
    
    return 1;
}

/*============================================================================
 * Test Policy Initialization
 *============================================================================*/

static int test_policy_init_basic(void)
{
    cm_policy_t policy;
    
    cm_policy_result_t rc = cm_policy_init(&policy);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT_EQ(policy.policy_version, CM_POLICY_VERSION);
    ASSERT_EQ(policy.window_size, 256);
    ASSERT(policy.drift.enabled_detectors != 0);
    
    return 1;
}

static int test_policy_init_null(void)
{
    cm_policy_result_t rc = cm_policy_init(NULL);
    ASSERT_EQ(rc, CM_POL_ERR_NULL);
    
    return 1;
}

/*============================================================================
 * Test Policy Parsing
 *============================================================================*/

static int test_policy_parse_minimal(void)
{
    const char *json = 
        "{"
        "  \"policy_version\": 1,"
        "  \"window_size\": 128"
        "}";
    
    cm_policy_t policy;
    ct_fault_flags_t faults = {0};
    
    cm_policy_result_t rc = cm_policy_parse((const uint8_t *)json, strlen(json),
                                            &policy, &faults);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT_EQ(policy.policy_version, 1);
    ASSERT_EQ(policy.window_size, 128);
    
    return 1;
}

static int test_policy_parse_wrong_version(void)
{
    const char *json = 
        "{"
        "  \"policy_version\": 99,"
        "  \"window_size\": 256"
        "}";
    
    cm_policy_t policy;
    ct_fault_flags_t faults = {0};
    
    cm_policy_result_t rc = cm_policy_parse((const uint8_t *)json, strlen(json),
                                            &policy, &faults);
    ASSERT_EQ(rc, CM_POL_ERR_VERSION);
    
    return 1;
}

static int test_policy_parse_missing_version(void)
{
    const char *json = "{\"window_size\": 256}";
    
    cm_policy_t policy;
    ct_fault_flags_t faults = {0};
    
    cm_policy_result_t rc = cm_policy_parse((const uint8_t *)json, strlen(json),
                                            &policy, &faults);
    ASSERT_EQ(rc, CM_POL_ERR_MISSING);
    
    return 1;
}

static int test_policy_parse_with_drift(void)
{
    const char *json = 
        "{"
        "  \"policy_version\": 1,"
        "  \"window_size\": 256,"
        "  \"drift\": {"
        "    \"enabled\": [\"tv\", \"jsd\"],"
        "    \"tv_threshold_q0_32\": 429496729,"
        "    \"jsd_threshold_q16_16\": 6554,"
        "    \"epsilon_q0_32\": 10"
        "  }"
        "}";
    
    cm_policy_t policy;
    ct_fault_flags_t faults = {0};
    
    cm_policy_result_t rc = cm_policy_parse((const uint8_t *)json, strlen(json),
                                            &policy, &faults);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT(policy.drift.enabled_detectors & CM_DRIFT_TV_ENABLED);
    ASSERT(policy.drift.enabled_detectors & CM_DRIFT_JSD_ENABLED);
    ASSERT(!(policy.drift.enabled_detectors & CM_DRIFT_PSI_ENABLED));
    ASSERT_EQ(policy.drift.tv_threshold_q0_32, 429496729);
    ASSERT_EQ(policy.drift.jsd_threshold_q16_16, 6554);
    ASSERT_EQ(policy.drift.epsilon_q0_32, 10);
    
    return 1;
}

static int test_policy_parse_with_reactions(void)
{
    const char *json = 
        "{"
        "  \"policy_version\": 1,"
        "  \"window_size\": 256,"
        "  \"reaction_map\": ["
        "    {\"violation\": \"input_range\", \"action\": \"warn_operator\"},"
        "    {\"violation\": \"output_drift\", \"action\": \"emergency_stop\"}"
        "  ]"
        "}";
    
    cm_policy_t policy;
    ct_fault_flags_t faults = {0};
    
    cm_policy_result_t rc = cm_policy_parse((const uint8_t *)json, strlen(json),
                                            &policy, &faults);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT_EQ(policy.react_map_count, 2);
    ASSERT_EQ(policy.react_map[0].violation, CM_VIOL_INPUT_RANGE);
    ASSERT_EQ(policy.react_map[0].reaction, CM_REACT_WARN_OPERATOR);
    ASSERT_EQ(policy.react_map[1].violation, CM_VIOL_OUTPUT_DRIFT);
    ASSERT_EQ(policy.react_map[1].reaction, CM_REACT_EMERGENCY_STOP);
    
    return 1;
}

static int test_policy_parse_full(void)
{
    const char *json = 
        "{"
        "  \"policy_version\": 1,"
        "  \"window_size\": 512,"
        "  \"input\": {"
        "    \"feature_count\": 32"
        "  },"
        "  \"drift\": {"
        "    \"enabled\": [\"tv\", \"jsd\", \"psi\"],"
        "    \"tv_threshold_q0_32\": 100000000,"
        "    \"jsd_threshold_q16_16\": 3277,"
        "    \"psi_threshold_q16_16\": 6554,"
        "    \"epsilon_q0_32\": 1"
        "  },"
        "  \"reaction_map\": ["
        "    {\"violation\": \"input_range\", \"action\": \"clamp_and_log\"},"
        "    {\"violation\": \"fault_budget\", \"action\": \"emergency_stop\"}"
        "  ]"
        "}";
    
    cm_policy_t policy;
    ct_fault_flags_t faults = {0};
    
    cm_policy_result_t rc = cm_policy_parse((const uint8_t *)json, strlen(json),
                                            &policy, &faults);
    ASSERT_EQ(rc, CM_POL_OK);
    ASSERT_EQ(policy.policy_version, 1);
    ASSERT_EQ(policy.window_size, 512);
    ASSERT_EQ(policy.input.feature_count, 32);
    ASSERT(policy.drift.enabled_detectors == 
           (CM_DRIFT_TV_ENABLED | CM_DRIFT_JSD_ENABLED | CM_DRIFT_PSI_ENABLED));
    ASSERT_EQ(policy.react_map_count, 2);
    
    return 1;
}

/*============================================================================
 * Test Policy Hash
 *============================================================================*/

static int test_policy_hash_deterministic(void)
{
    const char *json = "{\"policy_version\": 1, \"window_size\": 256}";
    uint8_t hash1[CT_SHA256_SIZE];
    uint8_t hash2[CT_SHA256_SIZE];
    
    cm_policy_result_t rc1 = cm_policy_compute_hash((const uint8_t *)json, 
                                                    strlen(json), hash1);
    cm_policy_result_t rc2 = cm_policy_compute_hash((const uint8_t *)json,
                                                    strlen(json), hash2);
    
    ASSERT_EQ(rc1, CM_POL_OK);
    ASSERT_EQ(rc2, CM_POL_OK);
    ASSERT(memcmp(hash1, hash2, CT_SHA256_SIZE) == 0);
    
    return 1;
}

static int test_policy_hash_different_for_different_input(void)
{
    const char *json1 = "{\"policy_version\": 1, \"window_size\": 256}";
    const char *json2 = "{\"policy_version\": 1, \"window_size\": 512}";
    uint8_t hash1[CT_SHA256_SIZE];
    uint8_t hash2[CT_SHA256_SIZE];
    
    cm_policy_compute_hash((const uint8_t *)json1, strlen(json1), hash1);
    cm_policy_compute_hash((const uint8_t *)json2, strlen(json2), hash2);
    
    ASSERT(memcmp(hash1, hash2, CT_SHA256_SIZE) != 0);
    
    return 1;
}

static int test_policy_verify_hash_match(void)
{
    const char *json = "{\"policy_version\": 1, \"window_size\": 256}";
    cm_policy_t policy;
    ct_fault_flags_t faults = {0};
    
    cm_policy_result_t rc = cm_policy_parse((const uint8_t *)json, strlen(json),
                                            &policy, &faults);
    ASSERT_EQ(rc, CM_POL_OK);
    
    /* Hash should have been computed during parse */
    uint8_t expected_hash[CT_SHA256_SIZE];
    cm_policy_compute_hash((const uint8_t *)json, strlen(json), expected_hash);
    
    rc = cm_policy_verify_hash(&policy, expected_hash);
    ASSERT_EQ(rc, CM_POL_OK);
    
    return 1;
}

/*============================================================================
 * Test Policy Validation
 *============================================================================*/

static int test_policy_validate_valid(void)
{
    cm_policy_t policy;
    cm_policy_init(&policy);
    
    cm_policy_result_t rc = cm_policy_validate(&policy);
    ASSERT_EQ(rc, CM_POL_OK);
    
    return 1;
}

static int test_policy_validate_no_detectors(void)
{
    cm_policy_t policy;
    cm_policy_init(&policy);
    policy.drift.enabled_detectors = 0;  /* No detectors */
    
    cm_policy_result_t rc = cm_policy_validate(&policy);
    ASSERT_EQ(rc, CM_POL_ERR_SCHEMA);
    
    return 1;
}

static int test_policy_validate_zero_window(void)
{
    cm_policy_t policy;
    cm_policy_init(&policy);
    policy.window_size = 0;
    
    cm_policy_result_t rc = cm_policy_validate(&policy);
    ASSERT_EQ(rc, CM_POL_ERR_RANGE);
    
    return 1;
}

/*============================================================================
 * Test Reaction Lookup
 *============================================================================*/

static int test_policy_get_reaction_found(void)
{
    cm_policy_t policy;
    cm_policy_init(&policy);
    
    policy.react_map_count = 2;
    policy.react_map[0].violation = CM_VIOL_INPUT_RANGE;
    policy.react_map[0].reaction = CM_REACT_WARN_OPERATOR;
    policy.react_map[1].violation = CM_VIOL_FAULT_BUDGET;
    policy.react_map[1].reaction = CM_REACT_EMERGENCY_STOP;
    
    cm_reaction_t r = cm_policy_get_reaction(&policy, CM_VIOL_INPUT_RANGE);
    ASSERT_EQ(r, CM_REACT_WARN_OPERATOR);
    
    r = cm_policy_get_reaction(&policy, CM_VIOL_FAULT_BUDGET);
    ASSERT_EQ(r, CM_REACT_EMERGENCY_STOP);
    
    return 1;
}

static int test_policy_get_reaction_default(void)
{
    cm_policy_t policy;
    cm_policy_init(&policy);
    policy.react_map_count = 0;
    
    cm_reaction_t r = cm_policy_get_reaction(&policy, CM_VIOL_INPUT_DRIFT);
    ASSERT_EQ(r, CM_REACT_LOG_ONLY);
    
    return 1;
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("certifiable-monitor: Policy Test Suite\n");
    printf("======================================\n\n");
    
    printf("JSON Primitives:\n");
    RUN_TEST("json_get_int_basic", test_json_get_int_basic);
    RUN_TEST("json_get_int_negative", test_json_get_int_negative);
    RUN_TEST("json_get_int_missing", test_json_get_int_missing);
    RUN_TEST("json_get_string_basic", test_json_get_string_basic);
    RUN_TEST("json_get_string_escaped", test_json_get_string_escaped);
    RUN_TEST("json_get_array_basic", test_json_get_array_basic);
    RUN_TEST("json_get_object_basic", test_json_get_object_basic);
    RUN_TEST("json_nested_object", test_json_nested_object);
    
    printf("\nPolicy Initialization:\n");
    RUN_TEST("policy_init_basic", test_policy_init_basic);
    RUN_TEST("policy_init_null", test_policy_init_null);
    
    printf("\nPolicy Parsing:\n");
    RUN_TEST("policy_parse_minimal", test_policy_parse_minimal);
    RUN_TEST("policy_parse_wrong_version", test_policy_parse_wrong_version);
    RUN_TEST("policy_parse_missing_version", test_policy_parse_missing_version);
    RUN_TEST("policy_parse_with_drift", test_policy_parse_with_drift);
    RUN_TEST("policy_parse_with_reactions", test_policy_parse_with_reactions);
    RUN_TEST("policy_parse_full", test_policy_parse_full);
    
    printf("\nPolicy Hash:\n");
    RUN_TEST("policy_hash_deterministic", test_policy_hash_deterministic);
    RUN_TEST("policy_hash_different_for_different_input", test_policy_hash_different_for_different_input);
    RUN_TEST("policy_verify_hash_match", test_policy_verify_hash_match);
    
    printf("\nPolicy Validation:\n");
    RUN_TEST("policy_validate_valid", test_policy_validate_valid);
    RUN_TEST("policy_validate_no_detectors", test_policy_validate_no_detectors);
    RUN_TEST("policy_validate_zero_window", test_policy_validate_zero_window);
    
    printf("\nReaction Lookup:\n");
    RUN_TEST("policy_get_reaction_found", test_policy_get_reaction_found);
    RUN_TEST("policy_get_reaction_default", test_policy_get_reaction_default);
    
    printf("\n======================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
