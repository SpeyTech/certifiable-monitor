/**
 * @file policy.h
 * @brief COE Policy Parsing and Validation API
 * @traceability SRS-001-POLICY
 *
 * @details
 * Provides deterministic parsing of JSON policies using minimal
 * integer-only parser. Computes H_P using domain-separated SHA-256.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef POLICY_H
#define POLICY_H

#include "cm_types.h"
#include "ct_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Policy Error Codes
 *============================================================================*/

/** Policy-specific result codes */
typedef enum {
    CM_POL_OK               = 0,     /**< Success */
    CM_POL_ERR_NULL         = -1,    /**< NULL pointer */
    CM_POL_ERR_SIZE         = -2,    /**< Size exceeded */
    CM_POL_ERR_PARSE        = -3,    /**< JSON parse error */
    CM_POL_ERR_VERSION      = -4,    /**< Unsupported version */
    CM_POL_ERR_MISSING      = -5,    /**< Required field missing */
    CM_POL_ERR_RANGE        = -6,    /**< Value out of range */
    CM_POL_ERR_SCHEMA       = -7,    /**< Schema mismatch */
    CM_POL_ERR_DUPLICATE    = -8     /**< Duplicate entry */
} cm_policy_result_t;

/*============================================================================
 * Policy Parsing Functions
 *============================================================================*/

/**
 * @brief Parse policy from JSON bytes
 * @param json_bytes Raw JSON bytes (must be JCS canonical)
 * @param json_len Length of JSON
 * @param policy Output policy structure (caller provides)
 * @param faults Fault flags
 * @return CM_POL_OK on success, error code otherwise
 * @traceability SRS-001-POLICY-11, SRS-001-POLICY-12
 *
 * The parser is strict:
 * - Rejects unknown fields
 * - Validates all required fields
 * - Checks all value ranges
 */
cm_policy_result_t cm_policy_parse(const uint8_t *json_bytes,
                                   size_t json_len,
                                   cm_policy_t *policy,
                                   ct_fault_flags_t *faults);

/**
 * @brief Initialize policy with default values
 * @param policy Policy structure to initialize
 * @return CM_POL_OK on success
 */
cm_policy_result_t cm_policy_init(cm_policy_t *policy);

/**
 * @brief Compute policy hash H_P
 * @param json_bytes Canonical JSON bytes
 * @param json_len Length of JSON
 * @param hash_out Output hash (CT_SHA256_SIZE bytes)
 * @return CM_POL_OK on success
 * @traceability SRS-001-POLICY-09
 *
 * H_P = SHA256("CM:POLICY:v1" || json_bytes)
 */
cm_policy_result_t cm_policy_compute_hash(const uint8_t *json_bytes,
                                          size_t json_len,
                                          uint8_t *hash_out);

/**
 * @brief Verify policy hash matches expected
 * @param policy Loaded policy with policy_hash field
 * @param expected_hash Expected H_P
 * @return CM_POL_OK if match, error otherwise
 * @traceability SRS-001-POLICY-10
 */
cm_policy_result_t cm_policy_verify_hash(const cm_policy_t *policy,
                                         const uint8_t *expected_hash);

/*============================================================================
 * Policy Validation Functions
 *============================================================================*/

/**
 * @brief Validate policy structure is complete and consistent
 * @param policy Policy to validate
 * @return CM_POL_OK if valid
 * @traceability SRS-001-POLICY-05 through SRS-001-POLICY-08
 */
cm_policy_result_t cm_policy_validate(const cm_policy_t *policy);

/**
 * @brief Look up reaction for a violation type
 * @param policy Policy with reaction map
 * @param violation Violation type
 * @return Corresponding reaction, or CM_REACT_LOG_ONLY if not found
 * @traceability SRS-001-POLICY-08
 */
cm_reaction_t cm_policy_get_reaction(const cm_policy_t *policy,
                                     cm_violation_t violation);

/*============================================================================
 * JSON Primitives (Minimal Parser)
 *============================================================================*/

/**
 * @brief Extract integer value from JSON at key
 * @param json JSON string
 * @param json_len Length of JSON
 * @param key Key to search for (without quotes)
 * @param value_out Output integer value
 * @return CM_POL_OK if found and parsed
 */
cm_policy_result_t cm_json_get_int(const char *json, size_t json_len,
                                   const char *key, int64_t *value_out);

/**
 * @brief Extract string value from JSON at key
 * @param json JSON string
 * @param json_len Length of JSON
 * @param key Key to search for
 * @param buf_out Output buffer for string (without quotes)
 * @param buf_size Size of output buffer
 * @param len_out Actual length written
 * @return CM_POL_OK if found
 */
cm_policy_result_t cm_json_get_string(const char *json, size_t json_len,
                                      const char *key,
                                      char *buf_out, size_t buf_size,
                                      size_t *len_out);

/**
 * @brief Find start of array value for key
 * @param json JSON string
 * @param json_len Length of JSON
 * @param key Key to search for
 * @param array_start Output pointer to start of array (after '[')
 * @param array_len Output length of array content (before ']')
 * @return CM_POL_OK if found
 */
cm_policy_result_t cm_json_get_array(const char *json, size_t json_len,
                                     const char *key,
                                     const char **array_start,
                                     size_t *array_len);

/**
 * @brief Find start of object value for key
 * @param json JSON string
 * @param json_len Length of JSON
 * @param key Key to search for
 * @param obj_start Output pointer to start of object (after '{')
 * @param obj_len Output length of object content (before '}')
 * @return CM_POL_OK if found
 */
cm_policy_result_t cm_json_get_object(const char *json, size_t json_len,
                                      const char *key,
                                      const char **obj_start,
                                      size_t *obj_len);

#ifdef __cplusplus
}
#endif

#endif /* POLICY_H */
