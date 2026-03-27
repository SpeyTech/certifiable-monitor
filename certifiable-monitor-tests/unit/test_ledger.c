/**
 * @file test_ledger.c
 * @brief Test Suite for Audit Ledger
 * @traceability CM-MATH-001 §6, CM-STRUCT-001 §2
 *
 * @details
 * Tests:
 * - Genesis binding (L_0 = H(tag || R || H_P))
 * - Chain integrity
 * - Tamper detection
 * - Entry serialization
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ct_types.h"
#include "cm_types.h"
#include "ledger.h"
#include "cm_audit.h"

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

/*============================================================================
 * Test Fixtures
 *============================================================================*/

/* Known test hashes for reproducibility */
static const uint8_t TEST_BUNDLE_ROOT[32] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38
};

static const uint8_t TEST_POLICY_HASH[32] = {
    0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
    0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
    0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
    0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8
};

/*============================================================================
 * Ledger Init Tests
 *============================================================================*/

TEST(ledger_init_clears_state)
{
    cm_ledger_ctx_t ctx;
    memset(&ctx, 0xFF, sizeof(ctx));  /* Fill with junk */
    
    ct_result_t rc = cm_ledger_init(&ctx);
    ASSERT(rc == CT_OK);
    ASSERT(ctx.seq_next == 0);
    ASSERT(ctx.initialized == false);
    
    return true;
}

TEST(ledger_init_null_returns_error)
{
    ct_result_t rc = cm_ledger_init(NULL);
    ASSERT(rc == CT_ERR_NULL);
    return true;
}

/*============================================================================
 * Genesis Tests
 *============================================================================*/

TEST(ledger_genesis_sets_initialized)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    ct_result_t rc = cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    ASSERT(rc == CT_OK);
    ASSERT(ctx.initialized == true);
    ASSERT(ctx.seq_next == 1);
    ASSERT(!ct_has_fault(&f));
    
    return true;
}

TEST(ledger_genesis_stores_bindings)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    ASSERT(memcmp(ctx.bundle_root, TEST_BUNDLE_ROOT, 32) == 0);
    ASSERT(memcmp(ctx.policy_hash, TEST_POLICY_HASH, 32) == 0);
    
    return true;
}

TEST(ledger_genesis_deterministic)
{
    cm_ledger_ctx_t ctx1, ctx2;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx1);
    cm_ledger_init(&ctx2);
    
    cm_ledger_genesis(&ctx1, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    cm_ledger_genesis(&ctx2, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    /* Same inputs must produce same L_0 */
    ASSERT(memcmp(ctx1.L_prev, ctx2.L_prev, 32) == 0);
    
    return true;
}

TEST(ledger_genesis_null_returns_error)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    cm_ledger_init(&ctx);
    
    ASSERT(cm_ledger_genesis(NULL, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f) == CT_ERR_NULL);
    ASSERT(cm_ledger_genesis(&ctx, NULL, TEST_POLICY_HASH, &f) == CT_ERR_NULL);
    ASSERT(cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, NULL, &f) == CT_ERR_NULL);
    
    return true;
}

/*============================================================================
 * Append Tests
 *============================================================================*/

TEST(ledger_append_increments_seq)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    uint8_t L[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    ASSERT(ctx.seq_next == 1);
    
    cm_ledger_append(&ctx, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L, &f);
    ASSERT(ctx.seq_next == 2);
    
    cm_ledger_append(&ctx, CM_EVENT_WINDOW_OK, 1, 0, NULL, 0, L, &f);
    ASSERT(ctx.seq_next == 3);
    
    return true;
}

TEST(ledger_append_updates_chain)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    uint8_t L0[32], L1[32], L2[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    memcpy(L0, ctx.L_prev, 32);
    
    cm_ledger_append(&ctx, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L1, &f);
    ASSERT(memcmp(ctx.L_prev, L1, 32) == 0);
    ASSERT(memcmp(L0, L1, 32) != 0);  /* Chain changed */
    
    cm_ledger_append(&ctx, CM_EVENT_WINDOW_OK, 1, 0, NULL, 0, L2, &f);
    ASSERT(memcmp(ctx.L_prev, L2, 32) == 0);
    ASSERT(memcmp(L1, L2, 32) != 0);  /* Chain changed again */
    
    return true;
}

TEST(ledger_append_without_genesis_fails)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    uint8_t L[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    /* Don't call genesis */
    
    ct_result_t rc = cm_ledger_append(&ctx, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L, &f);
    ASSERT(rc == CT_ERR_STATE);
    ASSERT(f.ledger_fail);
    
    return true;
}

TEST(ledger_append_deterministic)
{
    cm_ledger_ctx_t ctx1, ctx2;
    ct_fault_flags_t f;
    uint8_t L1[32], L2[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx1);
    cm_ledger_init(&ctx2);
    cm_ledger_genesis(&ctx1, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    cm_ledger_genesis(&ctx2, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    uint8_t payload[] = {0x42, 0x43, 0x44, 0x45};
    
    cm_ledger_append(&ctx1, CM_EVENT_VIOL_INPUT, 5, 1000, payload, 4, L1, &f);
    cm_ledger_append(&ctx2, CM_EVENT_VIOL_INPUT, 5, 1000, payload, 4, L2, &f);
    
    ASSERT(memcmp(L1, L2, 32) == 0);
    
    return true;
}

/*============================================================================
 * Convenience Entry Tests
 *============================================================================*/

TEST(ledger_append_window_ok)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    uint8_t L[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    ct_result_t rc = cm_ledger_append_window_ok(&ctx, 42, 256, 1000, L, &f);
    ASSERT(rc == CT_OK);
    ASSERT(!ct_has_fault(&f));
    
    return true;
}

TEST(ledger_append_violation)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    uint8_t L[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    ct_result_t rc = cm_ledger_append_violation(&ctx, 1, CM_VIOL_INPUT_RANGE, 
                                                 5, -100000, -65536, L, &f);
    ASSERT(rc == CT_OK);
    ASSERT(!ct_has_fault(&f));
    
    return true;
}

TEST(ledger_append_reaction)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    uint8_t L[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    ct_result_t rc = cm_ledger_append_reaction(&ctx, 1, CM_VIOL_INPUT_RANGE,
                                                CM_REACT_EMERGENCY_STOP, L, &f);
    ASSERT(rc == CT_OK);
    ASSERT(!ct_has_fault(&f));
    
    return true;
}

/*============================================================================
 * Chain Sensitivity Tests
 *============================================================================*/

TEST(ledger_chain_sensitive_to_payload)
{
    cm_ledger_ctx_t ctx1, ctx2;
    ct_fault_flags_t f;
    uint8_t L1[32], L2[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx1);
    cm_ledger_init(&ctx2);
    cm_ledger_genesis(&ctx1, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    cm_ledger_genesis(&ctx2, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    uint8_t payload1[] = {0x42};
    uint8_t payload2[] = {0x43};  /* Different payload */
    
    cm_ledger_append(&ctx1, CM_EVENT_WINDOW_OK, 0, 0, payload1, 1, L1, &f);
    cm_ledger_append(&ctx2, CM_EVENT_WINDOW_OK, 0, 0, payload2, 1, L2, &f);
    
    /* Different payload must produce different chain */
    ASSERT(memcmp(L1, L2, 32) != 0);
    
    return true;
}

TEST(ledger_chain_sensitive_to_bundle_root)
{
    cm_ledger_ctx_t ctx1, ctx2;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    uint8_t different_root[32];
    memcpy(different_root, TEST_BUNDLE_ROOT, 32);
    different_root[0] ^= 0x01;  /* Flip one bit */
    
    cm_ledger_init(&ctx1);
    cm_ledger_init(&ctx2);
    cm_ledger_genesis(&ctx1, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    cm_ledger_genesis(&ctx2, different_root, TEST_POLICY_HASH, &f);
    
    /* Genesis should differ */
    ASSERT(memcmp(ctx1.L_prev, ctx2.L_prev, 32) != 0);
    
    return true;
}

/*============================================================================
 * Get Functions Tests
 *============================================================================*/

TEST(ledger_get_digest)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    uint8_t L[32], L_check[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    cm_ledger_append(&ctx, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L, &f);
    
    ct_result_t rc = cm_ledger_get_digest(&ctx, L_check);
    ASSERT(rc == CT_OK);
    ASSERT(memcmp(L, L_check, 32) == 0);
    
    return true;
}

TEST(ledger_get_seq)
{
    cm_ledger_ctx_t ctx;
    ct_fault_flags_t f;
    uint8_t L[32];
    ct_clear_faults(&f);
    
    cm_ledger_init(&ctx);
    ASSERT_EQ(cm_ledger_get_seq(&ctx), 0);
    
    cm_ledger_genesis(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    ASSERT_EQ(cm_ledger_get_seq(&ctx), 1);
    
    cm_ledger_append(&ctx, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L, &f);
    ASSERT_EQ(cm_ledger_get_seq(&ctx), 2);
    
    return true;
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    printf("certifiable-monitor: Audit Ledger Test Suite\n");
    printf("=============================================\n\n");
    
    printf("Initialization:\n");
    RUN_TEST(ledger_init_clears_state);
    RUN_TEST(ledger_init_null_returns_error);
    
    printf("\nGenesis:\n");
    RUN_TEST(ledger_genesis_sets_initialized);
    RUN_TEST(ledger_genesis_stores_bindings);
    RUN_TEST(ledger_genesis_deterministic);
    RUN_TEST(ledger_genesis_null_returns_error);
    
    printf("\nAppend:\n");
    RUN_TEST(ledger_append_increments_seq);
    RUN_TEST(ledger_append_updates_chain);
    RUN_TEST(ledger_append_without_genesis_fails);
    RUN_TEST(ledger_append_deterministic);
    
    printf("\nConvenience Entries:\n");
    RUN_TEST(ledger_append_window_ok);
    RUN_TEST(ledger_append_violation);
    RUN_TEST(ledger_append_reaction);
    
    printf("\nChain Sensitivity:\n");
    RUN_TEST(ledger_chain_sensitive_to_payload);
    RUN_TEST(ledger_chain_sensitive_to_bundle_root);
    
    printf("\nAccessors:\n");
    RUN_TEST(ledger_get_digest);
    RUN_TEST(ledger_get_seq);
    
    printf("\n=============================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
