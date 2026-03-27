/**
 * @file output.h
 * @brief Output Monitor API
 * @traceability CM-ARCH-MATH-001 §5, SRS-004-OUTPUT
 *
 * @details
 * Provides deterministic output monitoring:
 * - Per-output range checking against COE envelope
 * - Histogram accumulation for output drift detection
 * - Violation counting and reporting
 * - Classification distribution tracking (optional)
 *
 * All operations are integer-only and produce bit-identical results
 * across x86, ARM, and RISC-V platforms.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef OUTPUT_H
#define OUTPUT_H

#include "cm_types.h"
#include "ct_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Output Monitor Context
 *============================================================================*/

/**
 * @brief Per-output monitoring state
 * @traceability CM-ARCH-MATH-001 §5.1
 */
typedef struct {
    uint32_t violation_count;               /**< Range violations this window */
    uint32_t sample_count;                  /**< Samples this window */
    uint32_t hist_counts[CM_MAX_BINS];      /**< Histogram bin counts */
    int32_t  min_observed_q16;              /**< Minimum observed value */
    int32_t  max_observed_q16;              /**< Maximum observed value */
} cm_output_state_t;

/**
 * @brief Output monitor context
 * @traceability CM-ARCH-MATH-001 §5
 */
typedef struct {
    const cm_output_envelope_t *envelope;   /**< Policy envelope (not owned) */
    cm_output_state_t          *outputs;    /**< Per-output state (caller provides) */
    uint32_t                    output_count; /**< Number of outputs */
    uint64_t                    window_id;  /**< Current window ID */
    uint32_t                    window_samples; /**< Samples in current window */
    uint32_t                    total_violations; /**< Total violations this window */
    bool                        initialized; /**< True if properly initialized */
} cm_output_ctx_t;

/**
 * @brief Output monitoring result for a single sample
 */
typedef struct {
    uint32_t violations;                    /**< Number of output violations */
    uint32_t first_violation_output;        /**< Index of first violating output */
    int32_t  first_violation_value_q16;     /**< Value of first violation */
    int32_t  first_violation_bound_q16;     /**< Violated bound */
    bool     is_upper_bound;                /**< True if upper bound violated */
} cm_output_result_t;

/**
 * @brief Window drift result for outputs
 */
typedef struct {
    uint32_t outputs_with_drift;            /**< Count of outputs with drift */
    uint32_t first_drift_output;            /**< Index of first drifting output */
    cm_drift_result_t first_drift_result;   /**< Drift metrics for first drifting output */
} cm_output_drift_result_t;

/*============================================================================
 * Output Monitor Functions
 *============================================================================*/

/**
 * @brief Initialize output monitor context
 * @param ctx Context to initialize
 * @param envelope Output envelope from policy
 * @param output_states Array of output states (caller provides)
 * @param output_count Number of outputs to monitor
 * @return CT_OK on success
 * @traceability SRS-004-OUTPUT-01
 */
ct_result_t cm_output_init(cm_output_ctx_t *ctx,
                           const cm_output_envelope_t *envelope,
                           cm_output_state_t *output_states,
                           uint32_t output_count);

/**
 * @brief Reset output monitor for new window
 * @param ctx Output monitor context
 * @param window_id New window identifier
 * @return CT_OK on success
 * @traceability SRS-004-OUTPUT-02
 */
ct_result_t cm_output_reset_window(cm_output_ctx_t *ctx, uint64_t window_id);

/**
 * @brief Process a single output vector
 * @param ctx Output monitor context
 * @param values Output values in Q16.16 (length = output_count)
 * @param result Output result (may be NULL if not needed)
 * @param faults Fault flags
 * @return CT_OK on success, CT_ERR_RANGE if any violation detected
 * @traceability CM-ARCH-MATH-001 §5.1
 */
ct_result_t cm_output_process(cm_output_ctx_t *ctx,
                              const int32_t *values,
                              cm_output_result_t *result,
                              ct_fault_flags_t *faults);

/**
 * @brief Check if a single output value is in range
 * @param ctx Output monitor context
 * @param output_idx Output index
 * @param value Value in Q16.16
 * @return true if in range, false otherwise
 * @traceability CM-ARCH-MATH-001 §5.1
 */
bool cm_output_check_value(const cm_output_ctx_t *ctx,
                           uint32_t output_idx,
                           int32_t value);

/**
 * @brief Get violation count for current window
 * @param ctx Output monitor context
 * @return Total violations across all outputs
 */
uint32_t cm_output_get_violations(const cm_output_ctx_t *ctx);

/**
 * @brief Get sample count for current window
 * @param ctx Output monitor context
 * @return Number of samples processed
 */
uint32_t cm_output_get_sample_count(const cm_output_ctx_t *ctx);

/**
 * @brief Compute drift for all outputs at end of window
 * @param ctx Output monitor context
 * @param drift_policy Drift detection policy
 * @param result Output drift result
 * @param faults Fault flags
 * @return CT_OK on success
 * @traceability CM-ARCH-MATH-001 §5.2
 */
ct_result_t cm_output_compute_drift(const cm_output_ctx_t *ctx,
                                    const cm_drift_policy_t *drift_policy,
                                    cm_output_drift_result_t *result,
                                    ct_fault_flags_t *faults);

/**
 * @brief Get per-output statistics
 * @param ctx Output monitor context
 * @param output_idx Output index
 * @param state Output state (may be NULL to just check validity)
 * @return CT_OK on success, CT_ERR_RANGE if output_idx out of bounds
 */
ct_result_t cm_output_get_state(const cm_output_ctx_t *ctx,
                                uint32_t output_idx,
                                cm_output_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* OUTPUT_H */
