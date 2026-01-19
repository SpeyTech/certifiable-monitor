/**
 * @file verify.h
 * @brief Ledger Verification API
 * @traceability CM-ARCH-MATH-001 §10, SRS-008-VERIFY
 *
 * @details
 * Provides offline verification of audit ledgers for post-incident analysis:
 * - Chain integrity verification (recompute L_0..L_t)
 * - Entry hash verification
 * - Policy and artifact binding verification
 * - Tamper/truncation detection
 * - Replay support for audits
 *
 * All operations are deterministic and produce bit-identical results
 * across x86, ARM, and RISC-V platforms.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef VERIFY_H
#define VERIFY_H

#include "cm_types.h"
#include "ct_types.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Verification Result Types
 *============================================================================*/

/**
 * @brief Verification error codes
 * @traceability SRS-008-VERIFY-01
 */
typedef enum {
    CM_VERIFY_OK              = 0,   /**< Verification successful */
    CM_VERIFY_ERR_NULL        = 1,   /**< Null pointer argument */
    CM_VERIFY_ERR_GENESIS     = 2,   /**< Genesis hash mismatch */
    CM_VERIFY_ERR_CHAIN       = 3,   /**< Chain hash mismatch at some entry */
    CM_VERIFY_ERR_ENTRY       = 4,   /**< Individual entry hash mismatch */
    CM_VERIFY_ERR_BINDING     = 5,   /**< Bundle/policy binding mismatch */
    CM_VERIFY_ERR_SEQUENCE    = 6,   /**< Sequence number discontinuity */
    CM_VERIFY_ERR_TRUNCATED   = 7,   /**< Chain appears truncated */
    CM_VERIFY_ERR_FINAL       = 8,   /**< Final digest mismatch */
    CM_VERIFY_ERR_EMPTY       = 9    /**< Empty entry array */
} cm_verify_result_t;

/**
 * @brief Detailed verification report
 * @traceability SRS-008-VERIFY-02
 */
typedef struct {
    cm_verify_result_t result;       /**< Overall result code */
    uint64_t           error_seq;    /**< Sequence number where error occurred */
    uint64_t           entries_verified; /**< Number of entries successfully verified */
    uint8_t            computed_L[CT_SHA256_SIZE]; /**< Final computed chain digest */
    bool               genesis_valid; /**< True if genesis hash matched */
    bool               bindings_valid; /**< True if all R/H_P bindings matched */
    bool               sequence_valid; /**< True if sequence numbers are monotonic */
} cm_verify_report_t;

/**
 * @brief Verification context for incremental verification
 * @traceability SRS-008-VERIFY-03
 */
typedef struct {
    uint8_t  L_curr[CT_SHA256_SIZE];  /**< Current chain digest */
    uint64_t seq_expected;            /**< Expected next sequence number */
    uint8_t  bundle_root[CT_SHA256_SIZE]; /**< Expected bundle root R */
    uint8_t  policy_hash[CT_SHA256_SIZE]; /**< Expected policy hash H_P */
    uint64_t entries_verified;        /**< Count of verified entries */
    bool     initialized;             /**< True if genesis verified */
} cm_verify_ctx_t;

/*============================================================================
 * Batch Verification Functions
 *============================================================================*/

/**
 * @brief Verify complete ledger chain from entries
 * @param entries Array of ledger entries (must be in order)
 * @param entry_count Number of entries
 * @param bundle_root Expected attestation root R
 * @param policy_hash Expected policy hash H_P
 * @param expected_L_final Expected final chain digest (may be NULL to skip)
 * @param report Output verification report
 * @return CM_VERIFY_OK on success
 * @traceability CM-ARCH-MATH-001 §10
 *
 * Recomputes:
 * 1. L_0 = H("CM:LEDGER:GENESIS:v1" || R || H_P)
 * 2. For each entry: e_t = H("CM:LEDGER:ENTRY:v1" || E_t)
 * 3. Chain: L_t = H("CM:LEDGER:v1" || L_{t-1} || e_t)
 * 4. Compares final L_t to expected_L_final (if provided)
 */
cm_verify_result_t cm_verify_chain(const cm_ledger_entry_t *entries,
                                   size_t entry_count,
                                   const uint8_t *bundle_root,
                                   const uint8_t *policy_hash,
                                   const uint8_t *expected_L_final,
                                   cm_verify_report_t *report);

/**
 * @brief Verify a single entry hash
 * @param entry Entry to verify
 * @param expected_hash Expected entry hash (32 bytes)
 * @return CM_VERIFY_OK if hash matches
 * @traceability SRS-008-VERIFY-04
 */
cm_verify_result_t cm_verify_entry_hash(const cm_ledger_entry_t *entry,
                                        const uint8_t *expected_hash);

/**
 * @brief Verify entry bindings match expected R and H_P
 * @param entry Entry to check
 * @param bundle_root Expected R
 * @param policy_hash Expected H_P
 * @return CM_VERIFY_OK if bindings match
 * @traceability SRS-008-VERIFY-05
 */
cm_verify_result_t cm_verify_entry_binding(const cm_ledger_entry_t *entry,
                                           const uint8_t *bundle_root,
                                           const uint8_t *policy_hash);

/**
 * @brief Verify genesis digest
 * @param bundle_root Attestation root R
 * @param policy_hash Policy hash H_P
 * @param expected_L0 Expected genesis digest
 * @return CM_VERIFY_OK if genesis matches
 * @traceability SRS-008-VERIFY-06
 */
cm_verify_result_t cm_verify_genesis(const uint8_t *bundle_root,
                                     const uint8_t *policy_hash,
                                     const uint8_t *expected_L0);

/**
 * @brief Compute genesis digest without comparison
 * @param bundle_root Attestation root R
 * @param policy_hash Policy hash H_P
 * @param L0_out Output genesis digest (32 bytes)
 * @return CM_VERIFY_OK on success
 */
cm_verify_result_t cm_verify_compute_genesis(const uint8_t *bundle_root,
                                             const uint8_t *policy_hash,
                                             uint8_t *L0_out);

/*============================================================================
 * Incremental Verification Functions
 *============================================================================*/

/**
 * @brief Initialize verification context
 * @param ctx Context to initialize
 * @param bundle_root Expected attestation root R
 * @param policy_hash Expected policy hash H_P
 * @return CM_VERIFY_OK on success
 * @traceability SRS-008-VERIFY-07
 *
 * Computes genesis L_0 and stores in context.
 */
cm_verify_result_t cm_verify_init(cm_verify_ctx_t *ctx,
                                  const uint8_t *bundle_root,
                                  const uint8_t *policy_hash);

/**
 * @brief Verify and incorporate a single entry
 * @param ctx Verification context
 * @param entry Entry to verify
 * @return CM_VERIFY_OK on success
 * @traceability SRS-008-VERIFY-08
 *
 * Checks:
 * - Entry binding (R, H_P)
 * - Sequence number continuity
 * - Updates chain digest L_t
 */
cm_verify_result_t cm_verify_entry(cm_verify_ctx_t *ctx,
                                   const cm_ledger_entry_t *entry);

/**
 * @brief Finalize incremental verification
 * @param ctx Verification context
 * @param expected_L_final Expected final digest (may be NULL)
 * @param report Output verification report
 * @return CM_VERIFY_OK on success
 * @traceability SRS-008-VERIFY-09
 */
cm_verify_result_t cm_verify_finalize(const cm_verify_ctx_t *ctx,
                                      const uint8_t *expected_L_final,
                                      cm_verify_report_t *report);

/**
 * @brief Get current chain digest from context
 * @param ctx Verification context
 * @param L_out Output buffer (32 bytes)
 * @return CM_VERIFY_OK on success
 */
cm_verify_result_t cm_verify_get_digest(const cm_verify_ctx_t *ctx,
                                        uint8_t *L_out);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Get human-readable verification result name
 * @param result Result code
 * @return Static string
 */
const char *cm_verify_result_name(cm_verify_result_t result);

/**
 * @brief Check if sequence numbers are monotonically increasing
 * @param entries Array of entries
 * @param entry_count Number of entries
 * @param start_seq Expected starting sequence number
 * @return CM_VERIFY_OK if monotonic
 * @traceability SRS-008-VERIFY-10
 */
cm_verify_result_t cm_verify_sequence_monotonic(const cm_ledger_entry_t *entries,
                                                size_t entry_count,
                                                uint64_t start_seq);

/**
 * @brief Compare two chain digests
 * @param L1 First digest
 * @param L2 Second digest
 * @return true if equal
 */
bool cm_verify_digests_equal(const uint8_t *L1, const uint8_t *L2);

#ifdef __cplusplus
}
#endif

#endif /* VERIFY_H */
