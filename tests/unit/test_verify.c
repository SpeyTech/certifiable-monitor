/**
 * @file test_verify.c
 * @brief Test Suite for Ledger Verification
 * @traceability CM-ARCH-MATH-001 §10, SRS-008-VERIFY
 *
 * @details
 * Tests:
 * - Genesis verification
 * - Entry binding verification
 * - Chain integrity verification
 * - Tamper/truncation detection
 * - Incremental verification
 * - Sequence monotonicity
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
#include "verify.h"
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

/* Different hashes for mismatch testing */
static const uint8_t WRONG_HASH[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

/**
 * @brief Create a test entry with proper bindings
 */
static void create_test_entry(cm_ledger_entry_t *entry,
                              uint64_t seq,
                              uint64_t window_id,
                              cm_event_type_t event_type,
                              const uint8_t *payload,
                              uint32_t payload_len)
{
    entry->hdr.seq = seq;
    entry->hdr.window_id = window_id;
    entry->hdr.event_type = (uint32_t)event_type;
    entry->hdr.payload_len = payload_len;
    entry->hdr.time_tick = 0;
    memcpy(entry->hdr.bundle_root, TEST_BUNDLE_ROOT, 32);
    memcpy(entry->hdr.policy_hash, TEST_POLICY_HASH, 32);
    entry->payload = payload;
}

/*============================================================================
 * Genesis Verification Tests
 *============================================================================*/

TEST(verify_compute_genesis_basic)
{
    uint8_t L0[32];
    
    cm_verify_result_t vr = cm_verify_compute_genesis(TEST_BUNDLE_ROOT,
                                                       TEST_POLICY_HASH,
                                                       L0);
    ASSERT(vr == CM_VERIFY_OK);
    
    /* Verify it's non-zero */
    bool all_zero = true;
    for (int i = 0; i < 32; i++) {
        if (L0[i] != 0) all_zero = false;
    }
    ASSERT(!all_zero);
    
    return true;
}

TEST(verify_compute_genesis_deterministic)
{
    uint8_t L0_a[32], L0_b[32];
    
    cm_verify_compute_genesis(TEST_BUNDLE_ROOT, TEST_POLICY_HASH, L0_a);
    cm_verify_compute_genesis(TEST_BUNDLE_ROOT, TEST_POLICY_HASH, L0_b);
    
    ASSERT(memcmp(L0_a, L0_b, 32) == 0);
    
    return true;
}

TEST(verify_compute_genesis_null_returns_error)
{
    uint8_t L0[32];
    
    ASSERT(cm_verify_compute_genesis(NULL, TEST_POLICY_HASH, L0) == CM_VERIFY_ERR_NULL);
    ASSERT(cm_verify_compute_genesis(TEST_BUNDLE_ROOT, NULL, L0) == CM_VERIFY_ERR_NULL);
    ASSERT(cm_verify_compute_genesis(TEST_BUNDLE_ROOT, TEST_POLICY_HASH, NULL) == CM_VERIFY_ERR_NULL);
    
    return true;
}

TEST(verify_genesis_matches_ledger)
{
    /* Create ledger and compute genesis */
    cm_ledger_ctx_t ledger;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    cm_ledger_init(&ledger);
    cm_ledger_genesis(&ledger, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    /* Get ledger's L_0 */
    uint8_t ledger_L0[32];
    cm_ledger_get_digest(&ledger, ledger_L0);
    
    /* Verify matches */
    cm_verify_result_t vr = cm_verify_genesis(TEST_BUNDLE_ROOT,
                                               TEST_POLICY_HASH,
                                               ledger_L0);
    ASSERT(vr == CM_VERIFY_OK);
    
    return true;
}

TEST(verify_genesis_wrong_hash_fails)
{
    uint8_t L0[32];
    cm_verify_compute_genesis(TEST_BUNDLE_ROOT, TEST_POLICY_HASH, L0);
    
    /* Tamper with expected hash */
    uint8_t tampered[32];
    memcpy(tampered, L0, 32);
    tampered[0] ^= 0xFF;
    
    cm_verify_result_t vr = cm_verify_genesis(TEST_BUNDLE_ROOT,
                                               TEST_POLICY_HASH,
                                               tampered);
    ASSERT(vr == CM_VERIFY_ERR_GENESIS);
    
    return true;
}

/*============================================================================
 * Entry Binding Tests
 *============================================================================*/

TEST(verify_entry_binding_valid)
{
    cm_ledger_entry_t entry;
    create_test_entry(&entry, 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_result_t vr = cm_verify_entry_binding(&entry,
                                                     TEST_BUNDLE_ROOT,
                                                     TEST_POLICY_HASH);
    ASSERT(vr == CM_VERIFY_OK);
    
    return true;
}

TEST(verify_entry_binding_wrong_bundle_root)
{
    cm_ledger_entry_t entry;
    create_test_entry(&entry, 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_result_t vr = cm_verify_entry_binding(&entry,
                                                     WRONG_HASH,
                                                     TEST_POLICY_HASH);
    ASSERT(vr == CM_VERIFY_ERR_BINDING);
    
    return true;
}

TEST(verify_entry_binding_wrong_policy_hash)
{
    cm_ledger_entry_t entry;
    create_test_entry(&entry, 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_result_t vr = cm_verify_entry_binding(&entry,
                                                     TEST_BUNDLE_ROOT,
                                                     WRONG_HASH);
    ASSERT(vr == CM_VERIFY_ERR_BINDING);
    
    return true;
}

TEST(verify_entry_binding_null)
{
    cm_ledger_entry_t entry;
    create_test_entry(&entry, 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    
    ASSERT(cm_verify_entry_binding(NULL, TEST_BUNDLE_ROOT, TEST_POLICY_HASH) == CM_VERIFY_ERR_NULL);
    ASSERT(cm_verify_entry_binding(&entry, NULL, TEST_POLICY_HASH) == CM_VERIFY_ERR_NULL);
    ASSERT(cm_verify_entry_binding(&entry, TEST_BUNDLE_ROOT, NULL) == CM_VERIFY_ERR_NULL);
    
    return true;
}

/*============================================================================
 * Sequence Monotonicity Tests
 *============================================================================*/

TEST(verify_sequence_monotonic_valid)
{
    cm_ledger_entry_t entries[3];
    create_test_entry(&entries[0], 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    create_test_entry(&entries[1], 2, 1, CM_EVENT_WINDOW_OK, NULL, 0);
    create_test_entry(&entries[2], 3, 2, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_result_t vr = cm_verify_sequence_monotonic(entries, 3, 1);
    ASSERT(vr == CM_VERIFY_OK);
    
    return true;
}

TEST(verify_sequence_monotonic_gap)
{
    cm_ledger_entry_t entries[3];
    create_test_entry(&entries[0], 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    create_test_entry(&entries[1], 3, 1, CM_EVENT_WINDOW_OK, NULL, 0);  /* Gap: skipped 2 */
    create_test_entry(&entries[2], 4, 2, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_result_t vr = cm_verify_sequence_monotonic(entries, 3, 1);
    ASSERT(vr == CM_VERIFY_ERR_SEQUENCE);
    
    return true;
}

TEST(verify_sequence_monotonic_wrong_start)
{
    cm_ledger_entry_t entries[2];
    create_test_entry(&entries[0], 5, 0, CM_EVENT_WINDOW_OK, NULL, 0);  /* Wrong start */
    create_test_entry(&entries[1], 6, 1, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_result_t vr = cm_verify_sequence_monotonic(entries, 2, 1);
    ASSERT(vr == CM_VERIFY_ERR_SEQUENCE);
    
    return true;
}

TEST(verify_sequence_monotonic_empty)
{
    cm_verify_result_t vr = cm_verify_sequence_monotonic(NULL, 0, 1);
    ASSERT(vr == CM_VERIFY_OK);  /* Empty is trivially valid */
    
    return true;
}

/*============================================================================
 * Incremental Verification Tests
 *============================================================================*/

TEST(verify_init_computes_genesis)
{
    cm_verify_ctx_t ctx;
    
    cm_verify_result_t vr = cm_verify_init(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH);
    ASSERT(vr == CM_VERIFY_OK);
    ASSERT(ctx.initialized == true);
    ASSERT(ctx.seq_expected == 1);
    ASSERT(ctx.entries_verified == 0);
    
    return true;
}

TEST(verify_init_null_returns_error)
{
    cm_verify_ctx_t ctx;
    
    ASSERT(cm_verify_init(NULL, TEST_BUNDLE_ROOT, TEST_POLICY_HASH) == CM_VERIFY_ERR_NULL);
    ASSERT(cm_verify_init(&ctx, NULL, TEST_POLICY_HASH) == CM_VERIFY_ERR_NULL);
    ASSERT(cm_verify_init(&ctx, TEST_BUNDLE_ROOT, NULL) == CM_VERIFY_ERR_NULL);
    
    return true;
}

TEST(verify_entry_incremental_success)
{
    /* Create ledger and append an entry */
    cm_ledger_ctx_t ledger;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    cm_ledger_init(&ledger);
    cm_ledger_genesis(&ledger, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    uint8_t L1[32];
    cm_ledger_append(&ledger, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L1, &f);
    
    /* Now verify incrementally */
    cm_verify_ctx_t vctx;
    cm_verify_init(&vctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH);
    
    /* Create matching entry */
    cm_ledger_entry_t entry;
    create_test_entry(&entry, 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_result_t vr = cm_verify_entry(&vctx, &entry);
    ASSERT(vr == CM_VERIFY_OK);
    ASSERT(vctx.entries_verified == 1);
    ASSERT(vctx.seq_expected == 2);
    
    /* Verify final digest matches */
    uint8_t verify_L[32];
    cm_verify_get_digest(&vctx, verify_L);
    ASSERT(memcmp(verify_L, L1, 32) == 0);
    
    return true;
}

TEST(verify_entry_wrong_sequence)
{
    cm_verify_ctx_t ctx;
    cm_verify_init(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH);
    
    /* Entry with wrong sequence (expecting 1, got 5) */
    cm_ledger_entry_t entry;
    create_test_entry(&entry, 5, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_result_t vr = cm_verify_entry(&ctx, &entry);
    ASSERT(vr == CM_VERIFY_ERR_SEQUENCE);
    
    return true;
}

TEST(verify_entry_wrong_binding)
{
    cm_verify_ctx_t ctx;
    cm_verify_init(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH);
    
    /* Entry with wrong binding */
    cm_ledger_entry_t entry;
    entry.hdr.seq = 1;
    entry.hdr.window_id = 0;
    entry.hdr.event_type = CM_EVENT_WINDOW_OK;
    entry.hdr.payload_len = 0;
    entry.hdr.time_tick = 0;
    memcpy(entry.hdr.bundle_root, WRONG_HASH, 32);  /* Wrong! */
    memcpy(entry.hdr.policy_hash, TEST_POLICY_HASH, 32);
    entry.payload = NULL;
    
    cm_verify_result_t vr = cm_verify_entry(&ctx, &entry);
    ASSERT(vr == CM_VERIFY_ERR_BINDING);
    
    return true;
}

TEST(verify_finalize_success)
{
    cm_verify_ctx_t ctx;
    cm_verify_init(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH);
    
    cm_ledger_entry_t entry;
    create_test_entry(&entry, 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    cm_verify_entry(&ctx, &entry);
    
    cm_verify_report_t report;
    cm_verify_result_t vr = cm_verify_finalize(&ctx, NULL, &report);
    
    ASSERT(vr == CM_VERIFY_OK);
    ASSERT(report.result == CM_VERIFY_OK);
    ASSERT(report.entries_verified == 1);
    ASSERT(report.genesis_valid == true);
    ASSERT(report.bindings_valid == true);
    ASSERT(report.sequence_valid == true);
    
    return true;
}

TEST(verify_finalize_wrong_final_digest)
{
    cm_verify_ctx_t ctx;
    cm_verify_init(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH);
    
    cm_ledger_entry_t entry;
    create_test_entry(&entry, 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    cm_verify_entry(&ctx, &entry);
    
    cm_verify_report_t report;
    cm_verify_result_t vr = cm_verify_finalize(&ctx, WRONG_HASH, &report);
    
    ASSERT(vr == CM_VERIFY_ERR_FINAL);
    ASSERT(report.result == CM_VERIFY_ERR_FINAL);
    
    return true;
}

/*============================================================================
 * Batch Chain Verification Tests
 *============================================================================*/

TEST(verify_chain_empty)
{
    cm_verify_report_t report;
    
    cm_verify_result_t vr = cm_verify_chain(NULL, 0,
                                             TEST_BUNDLE_ROOT,
                                             TEST_POLICY_HASH,
                                             NULL, &report);
    ASSERT(vr == CM_VERIFY_OK);
    ASSERT(report.entries_verified == 0);
    
    return true;
}

TEST(verify_chain_single_entry)
{
    /* Create ledger and append entry */
    cm_ledger_ctx_t ledger;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    cm_ledger_init(&ledger);
    cm_ledger_genesis(&ledger, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    uint8_t L1[32];
    cm_ledger_append(&ledger, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L1, &f);
    
    /* Create matching entry for verification */
    cm_ledger_entry_t entries[1];
    create_test_entry(&entries[0], 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_report_t report;
    cm_verify_result_t vr = cm_verify_chain(entries, 1,
                                             TEST_BUNDLE_ROOT,
                                             TEST_POLICY_HASH,
                                             L1, &report);
    ASSERT(vr == CM_VERIFY_OK);
    ASSERT(report.entries_verified == 1);
    ASSERT(memcmp(report.computed_L, L1, 32) == 0);
    
    return true;
}

TEST(verify_chain_multiple_entries)
{
    /* Create ledger with multiple entries */
    cm_ledger_ctx_t ledger;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    cm_ledger_init(&ledger);
    cm_ledger_genesis(&ledger, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    uint8_t L1[32], L2[32], L3[32];
    cm_ledger_append(&ledger, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L1, &f);
    cm_ledger_append(&ledger, CM_EVENT_WINDOW_OK, 1, 0, NULL, 0, L2, &f);
    cm_ledger_append(&ledger, CM_EVENT_VIOL_INPUT, 2, 0, NULL, 0, L3, &f);
    
    /* Create matching entries */
    cm_ledger_entry_t entries[3];
    create_test_entry(&entries[0], 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    create_test_entry(&entries[1], 2, 1, CM_EVENT_WINDOW_OK, NULL, 0);
    create_test_entry(&entries[2], 3, 2, CM_EVENT_VIOL_INPUT, NULL, 0);
    
    cm_verify_report_t report;
    cm_verify_result_t vr = cm_verify_chain(entries, 3,
                                             TEST_BUNDLE_ROOT,
                                             TEST_POLICY_HASH,
                                             L3, &report);
    ASSERT(vr == CM_VERIFY_OK);
    ASSERT(report.entries_verified == 3);
    
    return true;
}

TEST(verify_chain_tampered_entry)
{
    /* Create ledger with entries */
    cm_ledger_ctx_t ledger;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    cm_ledger_init(&ledger);
    cm_ledger_genesis(&ledger, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    uint8_t L1[32], L2[32];
    cm_ledger_append(&ledger, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L1, &f);
    cm_ledger_append(&ledger, CM_EVENT_WINDOW_OK, 1, 0, NULL, 0, L2, &f);
    
    /* Create entries with tampered binding */
    cm_ledger_entry_t entries[2];
    create_test_entry(&entries[0], 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    
    /* Tamper second entry's binding */
    entries[1].hdr.seq = 2;
    entries[1].hdr.window_id = 1;
    entries[1].hdr.event_type = CM_EVENT_WINDOW_OK;
    entries[1].hdr.payload_len = 0;
    entries[1].hdr.time_tick = 0;
    memcpy(entries[1].hdr.bundle_root, WRONG_HASH, 32);  /* Tampered! */
    memcpy(entries[1].hdr.policy_hash, TEST_POLICY_HASH, 32);
    entries[1].payload = NULL;
    
    cm_verify_report_t report;
    cm_verify_result_t vr = cm_verify_chain(entries, 2,
                                             TEST_BUNDLE_ROOT,
                                             TEST_POLICY_HASH,
                                             L2, &report);
    ASSERT(vr == CM_VERIFY_ERR_BINDING);
    ASSERT(report.error_seq == 2);
    ASSERT(report.entries_verified == 1);
    
    return true;
}

TEST(verify_chain_wrong_final)
{
    cm_ledger_ctx_t ledger;
    ct_fault_flags_t f;
    ct_clear_faults(&f);
    
    cm_ledger_init(&ledger);
    cm_ledger_genesis(&ledger, TEST_BUNDLE_ROOT, TEST_POLICY_HASH, &f);
    
    uint8_t L1[32];
    cm_ledger_append(&ledger, CM_EVENT_WINDOW_OK, 0, 0, NULL, 0, L1, &f);
    
    cm_ledger_entry_t entries[1];
    create_test_entry(&entries[0], 1, 0, CM_EVENT_WINDOW_OK, NULL, 0);
    
    cm_verify_report_t report;
    cm_verify_result_t vr = cm_verify_chain(entries, 1,
                                             TEST_BUNDLE_ROOT,
                                             TEST_POLICY_HASH,
                                             WRONG_HASH, &report);  /* Wrong expected final */
    ASSERT(vr == CM_VERIFY_ERR_FINAL);
    
    return true;
}

TEST(verify_chain_null_args)
{
    cm_verify_report_t report;
    
    ASSERT(cm_verify_chain(NULL, 0, NULL, TEST_POLICY_HASH, NULL, &report) == CM_VERIFY_ERR_NULL);
    ASSERT(cm_verify_chain(NULL, 0, TEST_BUNDLE_ROOT, NULL, NULL, &report) == CM_VERIFY_ERR_NULL);
    
    return true;
}

/*============================================================================
 * Utility Function Tests
 *============================================================================*/

TEST(verify_result_name_all_codes)
{
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_OK), "OK") == 0);
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_ERR_NULL), "NULL_POINTER") == 0);
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_ERR_GENESIS), "GENESIS_MISMATCH") == 0);
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_ERR_CHAIN), "CHAIN_MISMATCH") == 0);
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_ERR_ENTRY), "ENTRY_MISMATCH") == 0);
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_ERR_BINDING), "BINDING_MISMATCH") == 0);
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_ERR_SEQUENCE), "SEQUENCE_ERROR") == 0);
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_ERR_TRUNCATED), "TRUNCATED") == 0);
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_ERR_FINAL), "FINAL_MISMATCH") == 0);
    ASSERT(strcmp(cm_verify_result_name(CM_VERIFY_ERR_EMPTY), "EMPTY_ENTRIES") == 0);
    ASSERT(strcmp(cm_verify_result_name((cm_verify_result_t)99), "UNKNOWN") == 0);
    
    return true;
}

TEST(verify_digests_equal_same)
{
    uint8_t L1[32], L2[32];
    memset(L1, 0xAB, 32);
    memset(L2, 0xAB, 32);
    
    ASSERT(cm_verify_digests_equal(L1, L2) == true);
    
    return true;
}

TEST(verify_digests_equal_different)
{
    uint8_t L1[32], L2[32];
    memset(L1, 0xAB, 32);
    memset(L2, 0xCD, 32);
    
    ASSERT(cm_verify_digests_equal(L1, L2) == false);
    
    return true;
}

TEST(verify_digests_equal_null)
{
    uint8_t L[32];
    memset(L, 0xAB, 32);
    
    ASSERT(cm_verify_digests_equal(NULL, L) == false);
    ASSERT(cm_verify_digests_equal(L, NULL) == false);
    ASSERT(cm_verify_digests_equal(NULL, NULL) == false);
    
    return true;
}

TEST(verify_get_digest_success)
{
    cm_verify_ctx_t ctx;
    cm_verify_init(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH);
    
    uint8_t L[32];
    cm_verify_result_t vr = cm_verify_get_digest(&ctx, L);
    ASSERT(vr == CM_VERIFY_OK);
    
    /* Should match genesis computed by init */
    uint8_t L0[32];
    cm_verify_compute_genesis(TEST_BUNDLE_ROOT, TEST_POLICY_HASH, L0);
    ASSERT(memcmp(L, L0, 32) == 0);
    
    return true;
}

TEST(verify_get_digest_null)
{
    cm_verify_ctx_t ctx;
    uint8_t L[32];
    
    ASSERT(cm_verify_get_digest(NULL, L) == CM_VERIFY_ERR_NULL);
    
    cm_verify_init(&ctx, TEST_BUNDLE_ROOT, TEST_POLICY_HASH);
    ASSERT(cm_verify_get_digest(&ctx, NULL) == CM_VERIFY_ERR_NULL);
    
    return true;
}

/*============================================================================
 * Main Entry Point
 *============================================================================*/

int main(void)
{
    printf("\n=== certifiable-monitor: Verification Test Suite ===\n\n");
    
    printf("Genesis Verification:\n");
    RUN_TEST(verify_compute_genesis_basic);
    RUN_TEST(verify_compute_genesis_deterministic);
    RUN_TEST(verify_compute_genesis_null_returns_error);
    RUN_TEST(verify_genesis_matches_ledger);
    RUN_TEST(verify_genesis_wrong_hash_fails);
    
    printf("\nEntry Binding Verification:\n");
    RUN_TEST(verify_entry_binding_valid);
    RUN_TEST(verify_entry_binding_wrong_bundle_root);
    RUN_TEST(verify_entry_binding_wrong_policy_hash);
    RUN_TEST(verify_entry_binding_null);
    
    printf("\nSequence Monotonicity:\n");
    RUN_TEST(verify_sequence_monotonic_valid);
    RUN_TEST(verify_sequence_monotonic_gap);
    RUN_TEST(verify_sequence_monotonic_wrong_start);
    RUN_TEST(verify_sequence_monotonic_empty);
    
    printf("\nIncremental Verification:\n");
    RUN_TEST(verify_init_computes_genesis);
    RUN_TEST(verify_init_null_returns_error);
    RUN_TEST(verify_entry_incremental_success);
    RUN_TEST(verify_entry_wrong_sequence);
    RUN_TEST(verify_entry_wrong_binding);
    RUN_TEST(verify_finalize_success);
    RUN_TEST(verify_finalize_wrong_final_digest);
    
    printf("\nBatch Chain Verification:\n");
    RUN_TEST(verify_chain_empty);
    RUN_TEST(verify_chain_single_entry);
    RUN_TEST(verify_chain_multiple_entries);
    RUN_TEST(verify_chain_tampered_entry);
    RUN_TEST(verify_chain_wrong_final);
    RUN_TEST(verify_chain_null_args);
    
    printf("\nUtility Functions:\n");
    RUN_TEST(verify_result_name_all_codes);
    RUN_TEST(verify_digests_equal_same);
    RUN_TEST(verify_digests_equal_different);
    RUN_TEST(verify_digests_equal_null);
    RUN_TEST(verify_get_digest_success);
    RUN_TEST(verify_get_digest_null);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}
