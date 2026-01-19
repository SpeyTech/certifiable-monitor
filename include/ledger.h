/**
 * @file ledger.h
 * @brief Hash-Chained Audit Ledger API
 * @traceability CM-MATH-001 §6, CM-STRUCT-001 §2
 *
 * @details
 * Provides tamper-evident flight recorder functionality:
 * - Genesis entry binds to bundle attestation root R and policy hash H_P
 * - Each entry hashes previous chain digest L_{t-1}
 * - Tampering/truncation is detectable via hash verification
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef CM_LEDGER_H
#define CM_LEDGER_H

#include "ct_types.h"
#include "cm_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Ledger Context Management
 *============================================================================*/

/**
 * @brief Initialize ledger context
 * @param ctx Ledger context to initialize
 * @return CT_OK on success
 * @traceability CM-MATH-001 §6
 */
ct_result_t cm_ledger_init(cm_ledger_ctx_t *ctx);

/**
 * @brief Compute genesis entry L_0
 * @param ctx Ledger context
 * @param bundle_root Attestation root R from deployment bundle
 * @param policy_hash Policy hash H_P
 * @param faults Fault flags
 * @return CT_OK on success
 * @traceability CM-MATH-001 §6.2
 *
 * Genesis: L_0 = H("CM:LEDGER:GENESIS:v1" || R || H_P)
 */
ct_result_t cm_ledger_genesis(cm_ledger_ctx_t *ctx,
                              const uint8_t *bundle_root,
                              const uint8_t *policy_hash,
                              ct_fault_flags_t *faults);

/*============================================================================
 * Entry Creation
 *============================================================================*/

/**
 * @brief Serialize ledger entry header to bytes
 * @param hdr Header to serialize
 * @param out Output buffer (CM_LEDGER_HEADER_SIZE bytes)
 * @return CT_OK on success
 * @traceability CM-STRUCT-001 §2.1
 *
 * Serializes to little-endian with no padding.
 */
ct_result_t cm_ledger_serialize_header(const cm_ledger_header_t *hdr, uint8_t *out);

/**
 * @brief Compute entry hash e_t = H("CM:LEDGER:ENTRY:v1" || E_t)
 * @param entry Entry to hash
 * @param entry_hash Output hash (32 bytes)
 * @param faults Fault flags
 * @return CT_OK on success
 * @traceability CM-MATH-001 §6.2
 */
ct_result_t cm_ledger_hash_entry(const cm_ledger_entry_t *entry,
                                 uint8_t *entry_hash,
                                 ct_fault_flags_t *faults);

/**
 * @brief Append entry to ledger chain
 * @param ctx Ledger context (updated with new L_t)
 * @param event_type Event type
 * @param window_id Current window ID
 * @param time_tick Monotonic tick (0 for count-window mode)
 * @param payload Payload bytes (may be NULL if payload_len is 0)
 * @param payload_len Length of payload
 * @param L_out Output chain digest L_t (32 bytes)
 * @param faults Fault flags
 * @return CT_OK on success
 * @traceability CM-MATH-001 §6.2
 *
 * Chain: L_t = H("CM:LEDGER:v1" || L_{t-1} || e_t)
 */
ct_result_t cm_ledger_append(cm_ledger_ctx_t *ctx,
                             cm_event_type_t event_type,
                             uint64_t window_id,
                             uint64_t time_tick,
                             const uint8_t *payload,
                             uint32_t payload_len,
                             uint8_t *L_out,
                             ct_fault_flags_t *faults);

/*============================================================================
 * Convenience Entry Creators
 *============================================================================*/

/**
 * @brief Append window-OK entry
 * @param ctx Ledger context
 * @param window_id Window that completed
 * @param sample_count Samples in window
 * @param max_tv Maximum TV observed
 * @param L_out Output chain digest
 * @param faults Fault flags
 * @return CT_OK on success
 */
ct_result_t cm_ledger_append_window_ok(cm_ledger_ctx_t *ctx,
                                       uint64_t window_id,
                                       uint32_t sample_count,
                                       uint32_t max_tv,
                                       uint8_t *L_out,
                                       ct_fault_flags_t *faults);

/**
 * @brief Append violation entry
 * @param ctx Ledger context
 * @param window_id Current window
 * @param violation Violation type
 * @param feature_or_layer Which feature/layer
 * @param observed Observed value
 * @param bound Violated bound
 * @param L_out Output chain digest
 * @param faults Fault flags
 * @return CT_OK on success
 */
ct_result_t cm_ledger_append_violation(cm_ledger_ctx_t *ctx,
                                       uint64_t window_id,
                                       cm_violation_t violation,
                                       uint32_t feature_or_layer,
                                       int32_t observed,
                                       int32_t bound,
                                       uint8_t *L_out,
                                       ct_fault_flags_t *faults);

/**
 * @brief Append drift entry
 * @param ctx Ledger context
 * @param window_id Current window
 * @param feature_id Which feature
 * @param result Drift computation result
 * @param L_out Output chain digest
 * @param faults Fault flags
 * @return CT_OK on success
 */
ct_result_t cm_ledger_append_drift(cm_ledger_ctx_t *ctx,
                                   uint64_t window_id,
                                   uint32_t feature_id,
                                   const cm_drift_result_t *result,
                                   uint8_t *L_out,
                                   ct_fault_flags_t *faults);

/**
 * @brief Append reaction entry
 * @param ctx Ledger context
 * @param window_id Current window
 * @param violation Triggering violation
 * @param action Action taken
 * @param L_out Output chain digest
 * @param faults Fault flags
 * @return CT_OK on success
 */
ct_result_t cm_ledger_append_reaction(cm_ledger_ctx_t *ctx,
                                      uint64_t window_id,
                                      cm_violation_t violation,
                                      cm_reaction_t action,
                                      uint8_t *L_out,
                                      ct_fault_flags_t *faults);

/*============================================================================
 * Verification
 *============================================================================*/

/**
 * @brief Verify chain integrity from entries
 * @param entries Array of ledger entries
 * @param entry_count Number of entries
 * @param bundle_root Expected R
 * @param policy_hash Expected H_P
 * @param expected_L_final Expected final digest
 * @param faults Fault flags
 * @return CT_OK if chain is valid, CT_ERR_HASH if tampered
 * @traceability CM-ARCH-MATH-001 §10
 */
ct_result_t cm_ledger_verify_chain(const cm_ledger_entry_t *entries,
                                   size_t entry_count,
                                   const uint8_t *bundle_root,
                                   const uint8_t *policy_hash,
                                   const uint8_t *expected_L_final,
                                   ct_fault_flags_t *faults);

/**
 * @brief Get current chain digest
 * @param ctx Ledger context
 * @param L_out Output buffer (32 bytes)
 * @return CT_OK on success
 */
ct_result_t cm_ledger_get_digest(const cm_ledger_ctx_t *ctx, uint8_t *L_out);

/**
 * @brief Get next sequence number
 * @param ctx Ledger context
 * @return Next sequence number
 */
uint64_t cm_ledger_get_seq(const cm_ledger_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CM_LEDGER_H */
