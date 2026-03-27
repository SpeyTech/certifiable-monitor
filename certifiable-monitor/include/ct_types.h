/**
 * @file ct_types.h
 * @brief Core types shared across certifiable-* projects
 * @traceability CT-STRUCT-001 (Framework types)
 *
 * @details
 * Defines the fundamental types used throughout the certifiable framework:
 * - Fixed-point formats (Q16.16, Q0.32, Q16.48)
 * - Fault flags for deterministic error propagation
 * - Hash types for cryptographic operations
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef CT_TYPES_H
#define CT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Fixed-Point Constants
 *============================================================================*/

/** @defgroup fixed_point Fixed-Point Constants
 *  @{
 */

/** Q16.16 format constants */
#define CT_Q16_SHIFT      16
#define CT_Q16_ONE        (1 << CT_Q16_SHIFT)       /**< 65536 = 1.0 */
#define CT_Q16_HALF       (1 << (CT_Q16_SHIFT - 1)) /**< 32768 = 0.5 */
#define CT_Q16_MAX        INT32_MAX                  /**< 0x7FFFFFFF */
#define CT_Q16_MIN        INT32_MIN                  /**< 0x80000000 */
#define CT_Q16_EPS        1                          /**< Smallest positive */

/** Q0.32 format constants (probability representation) */
#define CT_Q0_32_SHIFT    32
#define CT_Q0_32_ONE      ((uint64_t)1 << CT_Q0_32_SHIFT)  /**< 1.0 */
#define CT_Q0_32_HALF     ((uint64_t)1 << (CT_Q0_32_SHIFT - 1))

/** Q16.48 format constants (accumulators) */
#define CT_Q16_48_SHIFT   48

/** @} */

/*============================================================================
 * Fault Flags
 *============================================================================*/

/** @defgroup fault_flags Fault Flags
 *  @{
 */

/**
 * @brief Fault flags for deterministic error propagation
 * @traceability CM-STRUCT-001 §5
 *
 * Any operation that detects an anomaly sets the appropriate flag.
 * Faults propagate through the computation chain and invalidate results.
 */
typedef struct {
    uint32_t overflow    : 1;  /**< Saturated high during operation */
    uint32_t underflow   : 1;  /**< Saturated low during operation */
    uint32_t div_zero    : 1;  /**< Division by zero attempted */
    uint32_t domain      : 1;  /**< Invalid input domain (e.g., log of 0) */
    uint32_t precision   : 1;  /**< Precision loss detected */
    uint32_t input_range : 1;  /**< Input outside envelope */
    uint32_t output_range: 1;  /**< Output outside envelope */
    uint32_t policy_fail : 1;  /**< Policy validation failed */
    uint32_t hash_fail   : 1;  /**< Hash verification failed */
    uint32_t ledger_fail : 1;  /**< Ledger integrity failed */
    uint32_t _reserved   : 22; /**< Reserved for future use */
} ct_fault_flags_t;

/**
 * @brief Check if any fault flag is set
 * @param f Pointer to fault flags
 * @return true if any fault is active
 */
static inline bool ct_has_fault(const ct_fault_flags_t *f) {
    return f->overflow || f->underflow || f->div_zero || 
           f->domain || f->precision || f->input_range ||
           f->output_range || f->policy_fail || f->hash_fail ||
           f->ledger_fail;
}

/**
 * @brief Clear all fault flags
 * @param f Pointer to fault flags
 */
static inline void ct_clear_faults(ct_fault_flags_t *f) {
    f->overflow = 0;
    f->underflow = 0;
    f->div_zero = 0;
    f->domain = 0;
    f->precision = 0;
    f->input_range = 0;
    f->output_range = 0;
    f->policy_fail = 0;
    f->hash_fail = 0;
    f->ledger_fail = 0;
}

/**
 * @brief Merge fault flags (OR operation)
 * @param dst Destination flags (modified)
 * @param src Source flags to merge
 */
static inline void ct_merge_faults(ct_fault_flags_t *dst, const ct_fault_flags_t *src) {
    dst->overflow    |= src->overflow;
    dst->underflow   |= src->underflow;
    dst->div_zero    |= src->div_zero;
    dst->domain      |= src->domain;
    dst->precision   |= src->precision;
    dst->input_range |= src->input_range;
    dst->output_range|= src->output_range;
    dst->policy_fail |= src->policy_fail;
    dst->hash_fail   |= src->hash_fail;
    dst->ledger_fail |= src->ledger_fail;
}

/** @} */

/*============================================================================
 * Hash Types
 *============================================================================*/

/** @defgroup hash_types Hash Types
 *  @{
 */

/** SHA-256 digest size in bytes */
#define CT_SHA256_SIZE    32

/**
 * @brief SHA-256 hash container
 * @traceability CM-STRUCT-001 §6
 */
typedef struct {
    uint8_t bytes[CT_SHA256_SIZE];
} ct_hash_t;

/**
 * @brief Compare two hashes for equality
 * @param a First hash
 * @param b Second hash
 * @return true if hashes are identical
 */
static inline bool ct_hash_equal(const ct_hash_t *a, const ct_hash_t *b) {
    for (size_t i = 0; i < CT_SHA256_SIZE; i++) {
        if (a->bytes[i] != b->bytes[i]) return false;
    }
    return true;
}

/**
 * @brief Set hash to zero
 * @param h Hash to clear
 */
static inline void ct_hash_clear(ct_hash_t *h) {
    for (size_t i = 0; i < CT_SHA256_SIZE; i++) {
        h->bytes[i] = 0;
    }
}

/** @} */

/*============================================================================
 * Result Types
 *============================================================================*/

/** @defgroup result_types Result Types
 *  @{
 */

/**
 * @brief Generic result codes
 */
typedef enum {
    CT_OK             = 0,   /**< Success */
    CT_ERR_NULL       = -1,  /**< Null pointer */
    CT_ERR_OVERFLOW   = -2,  /**< Overflow detected */
    CT_ERR_UNDERFLOW  = -3,  /**< Underflow detected */
    CT_ERR_DIV_ZERO   = -4,  /**< Division by zero */
    CT_ERR_DOMAIN     = -5,  /**< Invalid input domain */
    CT_ERR_RANGE      = -6,  /**< Value out of range */
    CT_ERR_SIZE       = -7,  /**< Invalid size */
    CT_ERR_FORMAT     = -8,  /**< Invalid format */
    CT_ERR_HASH       = -9,  /**< Hash mismatch */
    CT_ERR_STATE      = -10, /**< Invalid state */
    CT_ERR_BUFFER     = -11, /**< Buffer too small */
    CT_ERR_PARSE      = -12, /**< Parse error */
    CT_ERR_POLICY     = -13  /**< Policy violation */
} ct_result_t;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CT_TYPES_H */
