/**
 * @file verify.c
 * @brief Ledger Verification Implementation
 * @traceability CM-ARCH-MATH-001 §10, SRS-008-VERIFY
 *
 * @details
 * Implements offline verification of audit ledgers:
 * - Recomputes genesis L_0 from R and H_P
 * - Verifies chain integrity by replaying entries
 * - Detects tampering, truncation, and binding violations
 *
 * All operations are integer-only with no dynamic allocation.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "verify.h"
#include "ledger.h"
#include "cm_audit.h"
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * @brief Compute L_0 = H("CM:LEDGER:GENESIS:v1" || R || H_P)
 */
static void compute_genesis_internal(const uint8_t *bundle_root,
                                     const uint8_t *policy_hash,
                                     uint8_t *L0_out)
{
    cm_sha256_ctx_t sha;
    cm_sha256_init(&sha);
    
    cm_sha256_update(&sha, (const uint8_t *)CM_LEDGER_GENESIS_TAG,
                     strlen(CM_LEDGER_GENESIS_TAG));
    cm_sha256_update(&sha, bundle_root, CT_SHA256_SIZE);
    cm_sha256_update(&sha, policy_hash, CT_SHA256_SIZE);
    
    cm_sha256_final(&sha, L0_out);
}

/**
 * @brief Compute chain advancement L_t = H("CM:LEDGER:v1" || L_{t-1} || e_t)
 */
static void compute_chain_step(const uint8_t *L_prev,
                               const uint8_t *entry_hash,
                               uint8_t *L_out)
{
    cm_sha256_ctx_t sha;
    cm_sha256_init(&sha);
    
    cm_sha256_update(&sha, (const uint8_t *)CM_LEDGER_CHAIN_TAG,
                     strlen(CM_LEDGER_CHAIN_TAG));
    cm_sha256_update(&sha, L_prev, CT_SHA256_SIZE);
    cm_sha256_update(&sha, entry_hash, CT_SHA256_SIZE);
    
    cm_sha256_final(&sha, L_out);
}

/**
 * @brief Initialize report with defaults
 */
static void init_report(cm_verify_report_t *report)
{
    if (!report) return;
    
    report->result = CM_VERIFY_OK;
    report->error_seq = 0;
    report->entries_verified = 0;
    memset(report->computed_L, 0, CT_SHA256_SIZE);
    report->genesis_valid = false;
    report->bindings_valid = true;
    report->sequence_valid = true;
}

/*============================================================================
 * Genesis Verification
 *============================================================================*/

/**
 * @brief Compute genesis digest without comparison
 * @traceability SRS-008-VERIFY-06
 */
cm_verify_result_t cm_verify_compute_genesis(const uint8_t *bundle_root,
                                             const uint8_t *policy_hash,
                                             uint8_t *L0_out)
{
    if (!bundle_root || !policy_hash || !L0_out) {
        return CM_VERIFY_ERR_NULL;
    }
    
    compute_genesis_internal(bundle_root, policy_hash, L0_out);
    return CM_VERIFY_OK;
}

/**
 * @brief Verify genesis digest
 * @traceability SRS-008-VERIFY-06
 */
cm_verify_result_t cm_verify_genesis(const uint8_t *bundle_root,
                                     const uint8_t *policy_hash,
                                     const uint8_t *expected_L0)
{
    if (!bundle_root || !policy_hash || !expected_L0) {
        return CM_VERIFY_ERR_NULL;
    }
    
    uint8_t computed_L0[CT_SHA256_SIZE];
    compute_genesis_internal(bundle_root, policy_hash, computed_L0);
    
    if (memcmp(computed_L0, expected_L0, CT_SHA256_SIZE) != 0) {
        return CM_VERIFY_ERR_GENESIS;
    }
    
    return CM_VERIFY_OK;
}

/*============================================================================
 * Entry Verification
 *============================================================================*/

/**
 * @brief Verify entry bindings match expected R and H_P
 * @traceability SRS-008-VERIFY-05
 */
cm_verify_result_t cm_verify_entry_binding(const cm_ledger_entry_t *entry,
                                           const uint8_t *bundle_root,
                                           const uint8_t *policy_hash)
{
    if (!entry || !bundle_root || !policy_hash) {
        return CM_VERIFY_ERR_NULL;
    }
    
    if (memcmp(entry->hdr.bundle_root, bundle_root, CT_SHA256_SIZE) != 0) {
        return CM_VERIFY_ERR_BINDING;
    }
    
    if (memcmp(entry->hdr.policy_hash, policy_hash, CT_SHA256_SIZE) != 0) {
        return CM_VERIFY_ERR_BINDING;
    }
    
    return CM_VERIFY_OK;
}

/**
 * @brief Verify a single entry hash
 * @traceability SRS-008-VERIFY-04
 */
cm_verify_result_t cm_verify_entry_hash(const cm_ledger_entry_t *entry,
                                        const uint8_t *expected_hash)
{
    if (!entry || !expected_hash) {
        return CM_VERIFY_ERR_NULL;
    }
    
    uint8_t computed_hash[CT_SHA256_SIZE];
    ct_fault_flags_t faults;
    ct_clear_faults(&faults);
    
    ct_result_t rc = cm_ledger_hash_entry(entry, computed_hash, &faults);
    if (rc != CT_OK) {
        return CM_VERIFY_ERR_ENTRY;
    }
    
    if (memcmp(computed_hash, expected_hash, CT_SHA256_SIZE) != 0) {
        return CM_VERIFY_ERR_ENTRY;
    }
    
    return CM_VERIFY_OK;
}

/**
 * @brief Check if sequence numbers are monotonically increasing
 * @traceability SRS-008-VERIFY-10
 */
cm_verify_result_t cm_verify_sequence_monotonic(const cm_ledger_entry_t *entries,
                                                size_t entry_count,
                                                uint64_t start_seq)
{
    if (!entries && entry_count > 0) {
        return CM_VERIFY_ERR_NULL;
    }
    
    for (size_t i = 0; i < entry_count; i++) {
        uint64_t expected = start_seq + i;
        if (entries[i].hdr.seq != expected) {
            return CM_VERIFY_ERR_SEQUENCE;
        }
    }
    
    return CM_VERIFY_OK;
}

/*============================================================================
 * Incremental Verification
 *============================================================================*/

/**
 * @brief Initialize verification context
 * @traceability SRS-008-VERIFY-07
 */
cm_verify_result_t cm_verify_init(cm_verify_ctx_t *ctx,
                                  const uint8_t *bundle_root,
                                  const uint8_t *policy_hash)
{
    if (!ctx || !bundle_root || !policy_hash) {
        return CM_VERIFY_ERR_NULL;
    }
    
    /* Store expected bindings */
    memcpy(ctx->bundle_root, bundle_root, CT_SHA256_SIZE);
    memcpy(ctx->policy_hash, policy_hash, CT_SHA256_SIZE);
    
    /* Compute genesis L_0 */
    compute_genesis_internal(bundle_root, policy_hash, ctx->L_curr);
    
    /* Initialize state */
    ctx->seq_expected = 1;  /* First entry after genesis is seq 1 */
    ctx->entries_verified = 0;
    ctx->initialized = true;
    
    return CM_VERIFY_OK;
}

/**
 * @brief Verify and incorporate a single entry
 * @traceability SRS-008-VERIFY-08
 */
cm_verify_result_t cm_verify_entry(cm_verify_ctx_t *ctx,
                                   const cm_ledger_entry_t *entry)
{
    if (!ctx || !entry) {
        return CM_VERIFY_ERR_NULL;
    }
    
    if (!ctx->initialized) {
        return CM_VERIFY_ERR_NULL;
    }
    
    /* Check binding */
    cm_verify_result_t vr = cm_verify_entry_binding(entry,
                                                     ctx->bundle_root,
                                                     ctx->policy_hash);
    if (vr != CM_VERIFY_OK) {
        return vr;
    }
    
    /* Check sequence continuity */
    if (entry->hdr.seq != ctx->seq_expected) {
        return CM_VERIFY_ERR_SEQUENCE;
    }
    
    /* Compute entry hash e_t */
    uint8_t e_t[CT_SHA256_SIZE];
    ct_fault_flags_t faults;
    ct_clear_faults(&faults);
    
    ct_result_t rc = cm_ledger_hash_entry(entry, e_t, &faults);
    if (rc != CT_OK) {
        return CM_VERIFY_ERR_ENTRY;
    }
    
    /* Compute chain step L_t = H(tag || L_{t-1} || e_t) */
    uint8_t L_new[CT_SHA256_SIZE];
    compute_chain_step(ctx->L_curr, e_t, L_new);
    
    /* Update context */
    memcpy(ctx->L_curr, L_new, CT_SHA256_SIZE);
    ctx->seq_expected++;
    ctx->entries_verified++;
    
    return CM_VERIFY_OK;
}

/**
 * @brief Finalize incremental verification
 * @traceability SRS-008-VERIFY-09
 */
cm_verify_result_t cm_verify_finalize(const cm_verify_ctx_t *ctx,
                                      const uint8_t *expected_L_final,
                                      cm_verify_report_t *report)
{
    if (!ctx) {
        return CM_VERIFY_ERR_NULL;
    }
    
    if (report) {
        init_report(report);
        report->entries_verified = ctx->entries_verified;
        memcpy(report->computed_L, ctx->L_curr, CT_SHA256_SIZE);
        report->genesis_valid = true;
        report->bindings_valid = true;
        report->sequence_valid = true;
    }
    
    /* Check final digest if provided */
    if (expected_L_final) {
        if (memcmp(ctx->L_curr, expected_L_final, CT_SHA256_SIZE) != 0) {
            if (report) {
                report->result = CM_VERIFY_ERR_FINAL;
            }
            return CM_VERIFY_ERR_FINAL;
        }
    }
    
    if (report) {
        report->result = CM_VERIFY_OK;
    }
    
    return CM_VERIFY_OK;
}

/**
 * @brief Get current chain digest from context
 */
cm_verify_result_t cm_verify_get_digest(const cm_verify_ctx_t *ctx,
                                        uint8_t *L_out)
{
    if (!ctx || !L_out) {
        return CM_VERIFY_ERR_NULL;
    }
    
    if (!ctx->initialized) {
        return CM_VERIFY_ERR_NULL;
    }
    
    memcpy(L_out, ctx->L_curr, CT_SHA256_SIZE);
    return CM_VERIFY_OK;
}

/*============================================================================
 * Batch Verification
 *============================================================================*/

/**
 * @brief Verify complete ledger chain from entries
 * @traceability CM-ARCH-MATH-001 §10
 */
cm_verify_result_t cm_verify_chain(const cm_ledger_entry_t *entries,
                                   size_t entry_count,
                                   const uint8_t *bundle_root,
                                   const uint8_t *policy_hash,
                                   const uint8_t *expected_L_final,
                                   cm_verify_report_t *report)
{
    if (report) {
        init_report(report);
    }
    
    /* Null checks */
    if (!bundle_root || !policy_hash) {
        if (report) report->result = CM_VERIFY_ERR_NULL;
        return CM_VERIFY_ERR_NULL;
    }
    
    if (entry_count > 0 && !entries) {
        if (report) report->result = CM_VERIFY_ERR_NULL;
        return CM_VERIFY_ERR_NULL;
    }
    
    /* Initialize verification context */
    cm_verify_ctx_t ctx;
    cm_verify_result_t vr = cm_verify_init(&ctx, bundle_root, policy_hash);
    if (vr != CM_VERIFY_OK) {
        if (report) report->result = vr;
        return vr;
    }
    
    if (report) {
        report->genesis_valid = true;
    }
    
    /* Process each entry */
    for (size_t i = 0; i < entry_count; i++) {
        vr = cm_verify_entry(&ctx, &entries[i]);
        
        if (vr == CM_VERIFY_ERR_BINDING) {
            if (report) {
                report->result = CM_VERIFY_ERR_BINDING;
                report->error_seq = entries[i].hdr.seq;
                report->bindings_valid = false;
                report->entries_verified = i;
                memcpy(report->computed_L, ctx.L_curr, CT_SHA256_SIZE);
            }
            return CM_VERIFY_ERR_BINDING;
        }
        
        if (vr == CM_VERIFY_ERR_SEQUENCE) {
            if (report) {
                report->result = CM_VERIFY_ERR_SEQUENCE;
                report->error_seq = entries[i].hdr.seq;
                report->sequence_valid = false;
                report->entries_verified = i;
                memcpy(report->computed_L, ctx.L_curr, CT_SHA256_SIZE);
            }
            return CM_VERIFY_ERR_SEQUENCE;
        }
        
        if (vr != CM_VERIFY_OK) {
            if (report) {
                report->result = vr;
                report->error_seq = entries[i].hdr.seq;
                report->entries_verified = i;
                memcpy(report->computed_L, ctx.L_curr, CT_SHA256_SIZE);
            }
            return vr;
        }
    }
    
    /* Finalize */
    return cm_verify_finalize(&ctx, expected_L_final, report);
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Get human-readable verification result name
 */
const char *cm_verify_result_name(cm_verify_result_t result)
{
    switch (result) {
        case CM_VERIFY_OK:           return "OK";
        case CM_VERIFY_ERR_NULL:     return "NULL_POINTER";
        case CM_VERIFY_ERR_GENESIS:  return "GENESIS_MISMATCH";
        case CM_VERIFY_ERR_CHAIN:    return "CHAIN_MISMATCH";
        case CM_VERIFY_ERR_ENTRY:    return "ENTRY_MISMATCH";
        case CM_VERIFY_ERR_BINDING:  return "BINDING_MISMATCH";
        case CM_VERIFY_ERR_SEQUENCE: return "SEQUENCE_ERROR";
        case CM_VERIFY_ERR_TRUNCATED:return "TRUNCATED";
        case CM_VERIFY_ERR_FINAL:    return "FINAL_MISMATCH";
        case CM_VERIFY_ERR_EMPTY:    return "EMPTY_ENTRIES";
        default:                     return "UNKNOWN";
    }
}

/**
 * @brief Compare two chain digests
 */
bool cm_verify_digests_equal(const uint8_t *L1, const uint8_t *L2)
{
    if (!L1 || !L2) {
        return false;
    }
    return memcmp(L1, L2, CT_SHA256_SIZE) == 0;
}
