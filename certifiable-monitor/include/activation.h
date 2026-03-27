/**
 * @file activation.h
 * @brief Activation Monitor API
 * @traceability CM-ARCH-MATH-001 §4, SRS-003-ACTIVATION
 *
 * @details
 * Provides deterministic activation monitoring for neural network layers:
 * - Per-layer bounds checking against COE contracts
 * - Saturation detection from inference engine fault flags
 * - Violation rate calculation per window
 * - Maximum over-range magnitude tracking
 *
 * All operations are integer-only and produce bit-identical results
 * across x86, ARM, and RISC-V platforms.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef ACTIVATION_H
#define ACTIVATION_H

#include "cm_types.h"
#include "ct_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Activation Monitor Context
 *============================================================================*/

/**
 * @brief Per-layer monitoring state
 * @traceability CM-ARCH-MATH-001 §4
 */
typedef struct {
    uint32_t layer_id;                      /**< Layer identifier */
    uint64_t total_elements;                /**< Total elements checked this window */
    uint32_t violation_count;               /**< Bounds violations this window */
    uint32_t saturation_count;              /**< Saturation events this window */
    int32_t  max_overrange_q16;             /**< Maximum over-range magnitude Q16.16 */
    int32_t  min_observed_q16;              /**< Minimum observed value */
    int32_t  max_observed_q16;              /**< Maximum observed value */
} cm_activ_layer_state_t;

/**
 * @brief Activation monitor context
 * @traceability CM-ARCH-MATH-001 §4
 */
typedef struct {
    const cm_layer_contract_t *contracts;   /**< Layer contracts (not owned) */
    cm_activ_layer_state_t    *layers;      /**< Per-layer state (caller provides) */
    uint32_t                   layer_count; /**< Number of monitored layers */
    uint64_t                   window_id;   /**< Current window ID */
    uint32_t                   window_samples; /**< Samples (inferences) in window */
    bool                       initialized; /**< True if properly initialized */
} cm_activ_ctx_t;

/**
 * @brief Per-layer result from checking activations
 */
typedef struct {
    uint32_t violations;                    /**< Number of bound violations */
    int32_t  max_overrange_q16;             /**< Maximum over-range magnitude */
    bool     rate_exceeded;                 /**< True if violation rate exceeds tolerance */
    bool     max_exceeded;                  /**< True if max overrange exceeds limit */
} cm_activ_layer_result_t;

/**
 * @brief Window summary result for all layers
 */
typedef struct {
    uint32_t layers_with_violations;        /**< Count of layers with violations */
    uint32_t first_violation_layer;         /**< Index of first violating layer */
    cm_activ_layer_result_t first_result;   /**< Result for first violating layer */
    uint32_t total_saturations;             /**< Total saturations across all layers */
} cm_activ_result_t;

/*============================================================================
 * Activation Monitor Functions
 *============================================================================*/

/**
 * @brief Initialize activation monitor context
 * @param ctx Context to initialize
 * @param contracts Layer contracts from policy (may be NULL for no bounds checking)
 * @param layer_states Array of layer states (caller provides)
 * @param layer_count Number of layers to monitor
 * @return CT_OK on success
 * @traceability SRS-003-ACTIVATION-01
 */
ct_result_t cm_activ_init(cm_activ_ctx_t *ctx,
                          const cm_layer_contract_t *contracts,
                          cm_activ_layer_state_t *layer_states,
                          uint32_t layer_count);

/**
 * @brief Reset activation monitor for new window
 * @param ctx Activation monitor context
 * @param window_id New window identifier
 * @return CT_OK on success
 * @traceability SRS-003-ACTIVATION-02
 */
ct_result_t cm_activ_reset_window(cm_activ_ctx_t *ctx, uint64_t window_id);

/**
 * @brief Check activations for a single layer
 * @param ctx Activation monitor context
 * @param layer_idx Layer index (0 to layer_count-1)
 * @param values Activation values in Q16.16
 * @param count Number of activation values
 * @param result Output result (may be NULL)
 * @param faults Fault flags from inference engine (may be NULL)
 * @return CT_OK on success, CT_ERR_RANGE if tolerance exceeded
 * @traceability CM-ARCH-MATH-001 §4
 *
 * For each activation value:
 * - Checks against layer contract bounds [min_q16, max_q16]
 * - Computes over-range magnitude for violations
 * - Counts saturations from fault flags
 */
ct_result_t cm_activ_check_layer(cm_activ_ctx_t *ctx,
                                 uint32_t layer_idx,
                                 const int32_t *values,
                                 uint32_t count,
                                 cm_activ_layer_result_t *result,
                                 const ct_fault_flags_t *faults);

/**
 * @brief Record saturation event for a layer
 * @param ctx Activation monitor context
 * @param layer_idx Layer index
 * @return CT_OK on success
 * @traceability CM-ARCH-MATH-001 §4
 *
 * Called by inference engine when saturation/clamp occurs.
 */
ct_result_t cm_activ_record_saturation(cm_activ_ctx_t *ctx,
                                       uint32_t layer_idx);

/**
 * @brief Get window summary for all layers
 * @param ctx Activation monitor context
 * @param result Output summary result
 * @return CT_OK on success
 */
ct_result_t cm_activ_get_window_summary(const cm_activ_ctx_t *ctx,
                                        cm_activ_result_t *result);

/**
 * @brief Get per-layer statistics
 * @param ctx Activation monitor context
 * @param layer_idx Layer index
 * @param state Output state (may be NULL to just check validity)
 * @return CT_OK on success, CT_ERR_RANGE if layer_idx out of bounds
 */
ct_result_t cm_activ_get_layer_state(const cm_activ_ctx_t *ctx,
                                     uint32_t layer_idx,
                                     cm_activ_layer_state_t *state);

/**
 * @brief Compute violation rate for a layer
 * @param ctx Activation monitor context
 * @param layer_idx Layer index
 * @return Violation rate in Q0.32 (violations / total_elements)
 * @traceability CM-ARCH-MATH-001 §4
 */
uint32_t cm_activ_get_violation_rate(const cm_activ_ctx_t *ctx,
                                     uint32_t layer_idx);

/**
 * @brief Check if layer exceeds its tolerance
 * @param ctx Activation monitor context
 * @param layer_idx Layer index
 * @return true if violation_rate > tol_violations_q0_32
 */
bool cm_activ_exceeds_tolerance(const cm_activ_ctx_t *ctx,
                                uint32_t layer_idx);

/**
 * @brief Find layer index by layer_id
 * @param ctx Activation monitor context
 * @param layer_id Layer identifier to find
 * @param layer_idx Output layer index
 * @return CT_OK on success, CT_ERR_RANGE if not found
 */
ct_result_t cm_activ_find_layer(const cm_activ_ctx_t *ctx,
                                uint32_t layer_id,
                                uint32_t *layer_idx);

#ifdef __cplusplus
}
#endif

#endif /* ACTIVATION_H */
