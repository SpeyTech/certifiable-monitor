/**
 * @file input.h
 * @brief Input Monitor API
 * @traceability CM-ARCH-MATH-001 §3, SRS-002-INPUT
 *
 * @details
 * Provides deterministic input monitoring:
 * - Per-feature range checking against COE envelope
 * - Histogram accumulation for drift detection
 * - Violation counting and reporting
 *
 * All operations are integer-only and produce bit-identical results
 * across x86, ARM, and RISC-V platforms.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef INPUT_H
#define INPUT_H

#include "cm_types.h"
#include "ct_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Input Monitor Context
 *============================================================================*/

/**
 * @brief Per-feature monitoring state
 * @traceability CM-ARCH-MATH-001 §3.1
 */
typedef struct {
    uint32_t violation_count;               /**< Range violations this window */
    uint32_t sample_count;                  /**< Samples this window */
    uint32_t hist_counts[CM_MAX_BINS];      /**< Histogram bin counts */
    int32_t  min_observed_q16;              /**< Minimum observed value */
    int32_t  max_observed_q16;              /**< Maximum observed value */
} cm_input_feature_state_t;

/**
 * @brief Input monitor context
 * @traceability CM-ARCH-MATH-001 §3
 */
typedef struct {
    const cm_input_envelope_t *envelope;    /**< Policy envelope (not owned) */
    cm_input_feature_state_t  *features;    /**< Per-feature state (caller provides) */
    uint32_t                   feature_count; /**< Number of features */
    uint64_t                   window_id;   /**< Current window ID */
    uint32_t                   window_samples; /**< Samples in current window */
    uint32_t                   total_violations; /**< Total violations this window */
    bool                       initialized; /**< True if properly initialized */
} cm_input_ctx_t;

/**
 * @brief Input monitoring result for a single sample
 */
typedef struct {
    uint32_t violations;                    /**< Number of feature violations */
    uint32_t first_violation_feature;       /**< Index of first violating feature */
    int32_t  first_violation_value_q16;     /**< Value of first violation */
    int32_t  first_violation_bound_q16;     /**< Violated bound */
    bool     is_upper_bound;                /**< True if upper bound violated */
} cm_input_result_t;

/**
 * @brief Window drift result for input features
 */
typedef struct {
    uint32_t features_with_drift;           /**< Count of features with drift */
    uint32_t first_drift_feature;           /**< Index of first drifting feature */
    cm_drift_result_t first_drift_result;   /**< Drift metrics for first drifting feature */
} cm_input_drift_result_t;

/*============================================================================
 * Input Monitor Functions
 *============================================================================*/

/**
 * @brief Initialize input monitor context
 * @param ctx Context to initialize
 * @param envelope Input envelope from policy
 * @param feature_states Array of feature states (caller provides, length = feature_count)
 * @param feature_count Number of features to monitor
 * @return CT_OK on success
 * @traceability SRS-002-INPUT-01
 */
ct_result_t cm_input_init(cm_input_ctx_t *ctx,
                          const cm_input_envelope_t *envelope,
                          cm_input_feature_state_t *feature_states,
                          uint32_t feature_count);

/**
 * @brief Reset input monitor for new window
 * @param ctx Input monitor context
 * @param window_id New window identifier
 * @return CT_OK on success
 * @traceability SRS-002-INPUT-02
 */
ct_result_t cm_input_reset_window(cm_input_ctx_t *ctx, uint64_t window_id);

/**
 * @brief Process a single input vector
 * @param ctx Input monitor context
 * @param values Input feature values in Q16.16 (length = feature_count)
 * @param result Output result (may be NULL if not needed)
 * @param faults Fault flags
 * @return CT_OK on success, CT_ERR_RANGE if any violation detected
 * @traceability CM-ARCH-MATH-001 §3.1
 *
 * For each feature i:
 * - Checks values[i] against envelope bounds [min_q16[i], max_q16[i]]
 * - Accumulates histogram if envelope has histograms
 * - Updates violation count
 */
ct_result_t cm_input_process(cm_input_ctx_t *ctx,
                             const int32_t *values,
                             cm_input_result_t *result,
                             ct_fault_flags_t *faults);

/**
 * @brief Check if a single feature value is in range
 * @param ctx Input monitor context
 * @param feature_idx Feature index
 * @param value Value in Q16.16
 * @return true if in range, false otherwise
 * @traceability CM-ARCH-MATH-001 §3.1
 */
bool cm_input_check_feature(const cm_input_ctx_t *ctx,
                            uint32_t feature_idx,
                            int32_t value);

/**
 * @brief Get violation count for current window
 * @param ctx Input monitor context
 * @return Total violations across all features
 */
uint32_t cm_input_get_violations(const cm_input_ctx_t *ctx);

/**
 * @brief Get sample count for current window
 * @param ctx Input monitor context
 * @return Number of samples processed
 */
uint32_t cm_input_get_sample_count(const cm_input_ctx_t *ctx);

/**
 * @brief Compute drift for all features at end of window
 * @param ctx Input monitor context
 * @param drift_policy Drift detection policy
 * @param result Output drift result
 * @param faults Fault flags
 * @return CT_OK on success
 * @traceability CM-ARCH-MATH-001 §3.2
 *
 * Computes drift metrics (TV/JSD/PSI) for each feature with histograms,
 * comparing runtime counts to reference counts in envelope.
 */
ct_result_t cm_input_compute_drift(const cm_input_ctx_t *ctx,
                                   const cm_drift_policy_t *drift_policy,
                                   cm_input_drift_result_t *result,
                                   ct_fault_flags_t *faults);

/**
 * @brief Get per-feature statistics
 * @param ctx Input monitor context
 * @param feature_idx Feature index
 * @param state Output state (may be NULL to just check validity)
 * @return CT_OK on success, CT_ERR_RANGE if feature_idx out of bounds
 */
ct_result_t cm_input_get_feature_state(const cm_input_ctx_t *ctx,
                                       uint32_t feature_idx,
                                       cm_input_feature_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_H */
