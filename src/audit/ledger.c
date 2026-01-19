/**
 * @file ledger.c
 * @brief Hash-Chained Audit Ledger Implementation
 * @traceability CM-MATH-001 §6, CM-STRUCT-001 §2
 *
 * @details
 * Implements the tamper-evident flight recorder:
 * - Genesis: L_0 = H("CM:LEDGER:GENESIS:v1" || R || H_P)
 * - Entry hash: e_t = H("CM:LEDGER:ENTRY:v1" || E_t)
 * - Chain: L_t = H("CM:LEDGER:v1" || L_{t-1} || e_t)
 *
 * All serialization is little-endian with no padding for determinism.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "ledger.h"
#include "cm_audit.h"
#include "cm_types.h"
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * @brief Write uint64 as little-endian bytes
 */
static void write_le64(uint8_t *out, uint64_t val)
{
    out[0] = (uint8_t)(val);
    out[1] = (uint8_t)(val >> 8);
    out[2] = (uint8_t)(val >> 16);
    out[3] = (uint8_t)(val >> 24);
    out[4] = (uint8_t)(val >> 32);
    out[5] = (uint8_t)(val >> 40);
    out[6] = (uint8_t)(val >> 48);
    out[7] = (uint8_t)(val >> 56);
}

/**
 * @brief Write uint32 as little-endian bytes
 */
static void write_le32(uint8_t *out, uint32_t val)
{
    out[0] = (uint8_t)(val);
    out[1] = (uint8_t)(val >> 8);
    out[2] = (uint8_t)(val >> 16);
    out[3] = (uint8_t)(val >> 24);
}

/*============================================================================
 * Ledger Context Management
 *============================================================================*/

/**
 * @brief Initialize ledger context
 * @traceability CM-MATH-001 §6
 */
ct_result_t cm_ledger_init(cm_ledger_ctx_t *ctx)
{
    if (!ctx) return CT_ERR_NULL;
    
    memset(ctx->L_prev, 0, CT_SHA256_SIZE);
    ctx->seq_next = 0;
    memset(ctx->bundle_root, 0, CT_SHA256_SIZE);
    memset(ctx->policy_hash, 0, CT_SHA256_SIZE);
    ctx->initialized = false;
    
    return CT_OK;
}

/**
 * @brief Compute genesis entry L_0
 * @traceability CM-MATH-001 §6.2
 *
 * L_0 = H("CM:LEDGER:GENESIS:v1" || R || H_P)
 */
ct_result_t cm_ledger_genesis(cm_ledger_ctx_t *ctx,
                              const uint8_t *bundle_root,
                              const uint8_t *policy_hash,
                              ct_fault_flags_t *faults)
{
    (void)faults;  /* Genesis cannot fault */
    
    if (!ctx || !bundle_root || !policy_hash) {
        return CT_ERR_NULL;
    }
    
    /* Store R and H_P for future entries */
    memcpy(ctx->bundle_root, bundle_root, CT_SHA256_SIZE);
    memcpy(ctx->policy_hash, policy_hash, CT_SHA256_SIZE);
    
    /* Compute L_0 = H(tag || R || H_P) */
    cm_sha256_ctx_t sha;
    cm_sha256_init(&sha);
    
    /* Hash the domain tag */
    cm_sha256_update(&sha, (const uint8_t *)CM_LEDGER_GENESIS_TAG,
                     strlen(CM_LEDGER_GENESIS_TAG));
    
    /* Hash R */
    cm_sha256_update(&sha, bundle_root, CT_SHA256_SIZE);
    
    /* Hash H_P */
    cm_sha256_update(&sha, policy_hash, CT_SHA256_SIZE);
    
    /* Output L_0 */
    cm_sha256_final(&sha, ctx->L_prev);
    
    ctx->seq_next = 1;  /* First real entry will be seq 1 */
    ctx->initialized = true;
    
    return CT_OK;
}

/*============================================================================
 * Entry Creation
 *============================================================================*/

/**
 * @brief Serialize ledger entry header
 * @traceability CM-STRUCT-001 §2.1
 *
 * Layout (96 bytes, little-endian, no padding):
 * - seq: 8 bytes
 * - window_id: 8 bytes
 * - event_type: 4 bytes
 * - payload_len: 4 bytes
 * - time_tick: 8 bytes
 * - bundle_root: 32 bytes
 * - policy_hash: 32 bytes
 */
ct_result_t cm_ledger_serialize_header(const cm_ledger_header_t *hdr, uint8_t *out)
{
    if (!hdr || !out) return CT_ERR_NULL;
    
    uint8_t *p = out;
    
    write_le64(p, hdr->seq);           p += 8;
    write_le64(p, hdr->window_id);     p += 8;
    write_le32(p, hdr->event_type);    p += 4;
    write_le32(p, hdr->payload_len);   p += 4;
    write_le64(p, hdr->time_tick);     p += 8;
    memcpy(p, hdr->bundle_root, 32);   p += 32;
    memcpy(p, hdr->policy_hash, 32);   /* p += 32; */
    
    return CT_OK;
}

/**
 * @brief Compute entry hash
 * @traceability CM-MATH-001 §6.2
 *
 * e_t = H("CM:LEDGER:ENTRY:v1" || E_t)
 * where E_t = Header || Payload
 */
ct_result_t cm_ledger_hash_entry(const cm_ledger_entry_t *entry,
                                 uint8_t *entry_hash,
                                 ct_fault_flags_t *faults)
{
    (void)faults;
    
    if (!entry || !entry_hash) return CT_ERR_NULL;
    
    /* Serialize header */
    uint8_t hdr_bytes[CM_LEDGER_HEADER_SIZE];
    ct_result_t rc = cm_ledger_serialize_header(&entry->hdr, hdr_bytes);
    if (rc != CT_OK) return rc;
    
    /* Hash: tag || header || payload */
    cm_sha256_ctx_t sha;
    cm_sha256_init(&sha);
    
    cm_sha256_update(&sha, (const uint8_t *)CM_LEDGER_ENTRY_TAG,
                     strlen(CM_LEDGER_ENTRY_TAG));
    cm_sha256_update(&sha, hdr_bytes, CM_LEDGER_HEADER_SIZE);
    
    if (entry->payload && entry->hdr.payload_len > 0) {
        cm_sha256_update(&sha, entry->payload, entry->hdr.payload_len);
    }
    
    cm_sha256_final(&sha, entry_hash);
    
    return CT_OK;
}

/**
 * @brief Append entry to ledger chain
 * @traceability CM-MATH-001 §6.2
 *
 * L_t = H("CM:LEDGER:v1" || L_{t-1} || e_t)
 */
ct_result_t cm_ledger_append(cm_ledger_ctx_t *ctx,
                             cm_event_type_t event_type,
                             uint64_t window_id,
                             uint64_t time_tick,
                             const uint8_t *payload,
                             uint32_t payload_len,
                             uint8_t *L_out,
                             ct_fault_flags_t *faults)
{
    if (!ctx || !L_out) return CT_ERR_NULL;
    if (!ctx->initialized) {
        if (faults) faults->ledger_fail = 1;
        return CT_ERR_STATE;
    }
    
    /* Build entry header */
    cm_ledger_header_t hdr;
    hdr.seq = ctx->seq_next;
    hdr.window_id = window_id;
    hdr.event_type = (uint32_t)event_type;
    hdr.payload_len = payload_len;
    hdr.time_tick = time_tick;
    memcpy(hdr.bundle_root, ctx->bundle_root, CT_SHA256_SIZE);
    memcpy(hdr.policy_hash, ctx->policy_hash, CT_SHA256_SIZE);
    
    /* Build entry */
    cm_ledger_entry_t entry;
    entry.hdr = hdr;
    entry.payload = payload;
    
    /* Compute entry hash e_t */
    uint8_t e_t[CT_SHA256_SIZE];
    ct_result_t rc = cm_ledger_hash_entry(&entry, e_t, faults);
    if (rc != CT_OK) return rc;
    
    /* Compute chain digest L_t = H(tag || L_{t-1} || e_t) */
    cm_sha256_ctx_t sha;
    cm_sha256_init(&sha);
    
    cm_sha256_update(&sha, (const uint8_t *)CM_LEDGER_CHAIN_TAG,
                     strlen(CM_LEDGER_CHAIN_TAG));
    cm_sha256_update(&sha, ctx->L_prev, CT_SHA256_SIZE);
    cm_sha256_update(&sha, e_t, CT_SHA256_SIZE);
    
    cm_sha256_final(&sha, L_out);
    
    /* Update context */
    memcpy(ctx->L_prev, L_out, CT_SHA256_SIZE);
    ctx->seq_next++;
    
    return CT_OK;
}

/*============================================================================
 * Convenience Entry Creators
 *============================================================================*/

/**
 * @brief Append window-OK entry
 */
ct_result_t cm_ledger_append_window_ok(cm_ledger_ctx_t *ctx,
                                       uint64_t window_id,
                                       uint32_t sample_count,
                                       uint32_t max_tv,
                                       uint8_t *L_out,
                                       ct_fault_flags_t *faults)
{
    /* Build payload: window_id (8) + sample_count (4) + max_tv (4) = 16 bytes */
    uint8_t payload[16];
    write_le64(payload, window_id);
    write_le32(payload + 8, sample_count);
    write_le32(payload + 12, max_tv);
    
    return cm_ledger_append(ctx, CM_EVENT_WINDOW_OK, window_id, 0,
                            payload, sizeof(payload), L_out, faults);
}

/**
 * @brief Append violation entry
 */
ct_result_t cm_ledger_append_violation(cm_ledger_ctx_t *ctx,
                                       uint64_t window_id,
                                       cm_violation_t violation,
                                       uint32_t feature_or_layer,
                                       int32_t observed,
                                       int32_t bound,
                                       uint8_t *L_out,
                                       ct_fault_flags_t *faults)
{
    /* Build payload: violation (4) + feature (4) + observed (4) + bound (4) = 16 bytes */
    uint8_t payload[16];
    write_le32(payload, (uint32_t)violation);
    write_le32(payload + 4, feature_or_layer);
    write_le32(payload + 8, (uint32_t)observed);
    write_le32(payload + 12, (uint32_t)bound);
    
    /* Map violation type to event type */
    cm_event_type_t event;
    switch (violation) {
        case CM_VIOL_INPUT_RANGE:
        case CM_VIOL_INPUT_DRIFT:
            event = CM_EVENT_VIOL_INPUT;
            break;
        case CM_VIOL_ACTIV_RANGE:
        case CM_VIOL_ACTIV_SAT:
            event = CM_EVENT_VIOL_ACTIV;
            break;
        case CM_VIOL_OUTPUT_RANGE:
        case CM_VIOL_OUTPUT_DRIFT:
            event = CM_EVENT_VIOL_OUTPUT;
            break;
        case CM_VIOL_FAULT_BUDGET:
            event = CM_EVENT_VIOL_FAULT;
            break;
        default:
            event = CM_EVENT_VIOL_INPUT;
            break;
    }
    
    return cm_ledger_append(ctx, event, window_id, 0,
                            payload, sizeof(payload), L_out, faults);
}

/**
 * @brief Append drift entry
 */
ct_result_t cm_ledger_append_drift(cm_ledger_ctx_t *ctx,
                                   uint64_t window_id,
                                   uint32_t feature_id,
                                   const cm_drift_result_t *result,
                                   uint8_t *L_out,
                                   ct_fault_flags_t *faults)
{
    if (!result) return CT_ERR_NULL;
    
    /* Build payload: feature (4) + tv (4) + jsd (4) + psi (4) + flags (4) = 20 bytes */
    uint8_t payload[20];
    write_le32(payload, feature_id);
    write_le32(payload + 4, result->tv_q0_32);
    write_le32(payload + 8, (uint32_t)result->jsd_q16_16);
    write_le32(payload + 12, (uint32_t)result->psi_q16_16);
    write_le32(payload + 16, result->flags);
    
    return cm_ledger_append(ctx, CM_EVENT_VIOL_DRIFT, window_id, 0,
                            payload, sizeof(payload), L_out, faults);
}

/**
 * @brief Append reaction entry
 */
ct_result_t cm_ledger_append_reaction(cm_ledger_ctx_t *ctx,
                                      uint64_t window_id,
                                      cm_violation_t violation,
                                      cm_reaction_t action,
                                      uint8_t *L_out,
                                      ct_fault_flags_t *faults)
{
    /* Build payload: violation (4) + action (4) = 8 bytes */
    uint8_t payload[8];
    write_le32(payload, (uint32_t)violation);
    write_le32(payload + 4, (uint32_t)action);
    
    /* Map action to event type */
    cm_event_type_t event;
    switch (action) {
        case CM_REACT_WARN_OPERATOR:
            event = CM_EVENT_REACT_WARN;
            break;
        case CM_REACT_CLAMP_OUTPUT:
            event = CM_EVENT_REACT_CLAMP;
            break;
        case CM_REACT_DEGRADE_MODE:
            event = CM_EVENT_REACT_DEGRADE;
            break;
        case CM_REACT_EMERGENCY_STOP:
            event = CM_EVENT_REACT_STOP;
            break;
        default:
            event = CM_EVENT_REACT_WARN;
            break;
    }
    
    return cm_ledger_append(ctx, event, window_id, 0,
                            payload, sizeof(payload), L_out, faults);
}

/*============================================================================
 * Verification
 *============================================================================*/

/**
 * @brief Verify chain integrity from entries
 * @traceability CM-ARCH-MATH-001 §10
 */
ct_result_t cm_ledger_verify_chain(const cm_ledger_entry_t *entries,
                                   size_t entry_count,
                                   const uint8_t *bundle_root,
                                   const uint8_t *policy_hash,
                                   const uint8_t *expected_L_final,
                                   ct_fault_flags_t *faults)
{
    if (!entries || !bundle_root || !policy_hash || !expected_L_final) {
        return CT_ERR_NULL;
    }
    
    /* Recompute L_0 */
    uint8_t L_curr[CT_SHA256_SIZE];
    cm_sha256_ctx_t sha;
    
    cm_sha256_init(&sha);
    cm_sha256_update(&sha, (const uint8_t *)CM_LEDGER_GENESIS_TAG,
                     strlen(CM_LEDGER_GENESIS_TAG));
    cm_sha256_update(&sha, bundle_root, CT_SHA256_SIZE);
    cm_sha256_update(&sha, policy_hash, CT_SHA256_SIZE);
    cm_sha256_final(&sha, L_curr);
    
    /* Recompute chain */
    for (size_t i = 0; i < entry_count; i++) {
        /* Verify entry binding matches expected R and H_P */
        if (memcmp(entries[i].hdr.bundle_root, bundle_root, CT_SHA256_SIZE) != 0 ||
            memcmp(entries[i].hdr.policy_hash, policy_hash, CT_SHA256_SIZE) != 0) {
            if (faults) faults->hash_fail = 1;
            return CT_ERR_HASH;
        }
        
        /* Compute entry hash e_i */
        uint8_t e_i[CT_SHA256_SIZE];
        ct_result_t rc = cm_ledger_hash_entry(&entries[i], e_i, faults);
        if (rc != CT_OK) return rc;
        
        /* Compute L_i = H(tag || L_{i-1} || e_i) */
        cm_sha256_init(&sha);
        cm_sha256_update(&sha, (const uint8_t *)CM_LEDGER_CHAIN_TAG,
                         strlen(CM_LEDGER_CHAIN_TAG));
        cm_sha256_update(&sha, L_curr, CT_SHA256_SIZE);
        cm_sha256_update(&sha, e_i, CT_SHA256_SIZE);
        cm_sha256_final(&sha, L_curr);
    }
    
    /* Compare final digest */
    if (memcmp(L_curr, expected_L_final, CT_SHA256_SIZE) != 0) {
        if (faults) faults->hash_fail = 1;
        return CT_ERR_HASH;
    }
    
    return CT_OK;
}

/**
 * @brief Get current chain digest
 */
ct_result_t cm_ledger_get_digest(const cm_ledger_ctx_t *ctx, uint8_t *L_out)
{
    if (!ctx || !L_out) return CT_ERR_NULL;
    memcpy(L_out, ctx->L_prev, CT_SHA256_SIZE);
    return CT_OK;
}

/**
 * @brief Get next sequence number
 */
uint64_t cm_ledger_get_seq(const cm_ledger_ctx_t *ctx)
{
    if (!ctx) return 0;
    return ctx->seq_next;
}
