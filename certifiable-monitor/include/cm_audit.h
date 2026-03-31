/**
 * @file cm_audit.h
 * @brief SHA-256 and Domain-Separated Hashing API
 * @traceability CM-MATH-001 §7
 *
 * @details
 * Provides cryptographic hashing for:
 * - Ledger hash chain
 * - Policy hash H_P
 * - Genesis binding L_0
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef CM_AUDIT_H
#define CM_AUDIT_H

#include "ct_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * SHA-256 Context
 *============================================================================*/

/**
 * @brief SHA-256 context for incremental hashing
 * @traceability CM-MATH-001 §7
 */
typedef struct {
    uint32_t state[8];     /**< Hash state (H0..H7) */
    uint64_t count;        /**< Total bits processed */
    uint8_t  buffer[64];   /**< Block buffer */
} cm_sha256_ctx_t;

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * @brief Initialize SHA-256 context
 * @param ctx Context to initialize
 */
void cm_sha256_init(cm_sha256_ctx_t *ctx);

/**
 * @brief Update hash with data
 * @param ctx Context
 * @param data Input data
 * @param len Length of data in bytes
 */
void cm_sha256_update(cm_sha256_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief Finalize hash and output digest
 * @param ctx Context
 * @param digest Output buffer (32 bytes)
 */
void cm_sha256_final(cm_sha256_ctx_t *ctx, uint8_t *digest);

/**
 * @brief One-shot SHA-256 hash
 * @param data Input data
 * @param len Length of data
 * @param digest Output buffer (32 bytes)
 */
void cm_sha256(const uint8_t *data, size_t len, uint8_t *digest);

/**
 * @brief Domain-separated hash: H(tag || LE64(len) || data)
 * @param tag Domain separation tag (null-terminated)
 * @param data Payload data
 * @param len Length of payload
 * @param digest Output buffer (32 bytes)
 * @traceability CM-MATH-001 §7.2
 */
void cm_sha256_domain(const char *tag, const uint8_t *data, size_t len, uint8_t *digest);

#ifdef __cplusplus
}
#endif

#endif /* CM_AUDIT_H */
