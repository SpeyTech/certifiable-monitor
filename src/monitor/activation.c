/**
 * @file activation.c
 * @brief Activation Monitor Implementation
 * @traceability CM-ARCH-MATH-001 §4, SRS-003-ACTIVATION
 *
 * @details
 * Implements deterministic activation monitoring:
 * - Per-layer bounds checking against COE contracts
 * - Saturation detection from inference engine fault flags
 * - Violation rate calculation per window
 * - Maximum over-range magnitude tracking
 *
 * All operations are integer-only with no dynamic allocation.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "activation.h"
#include "dvm.h"
#include <string.h>
#include <limits.h>

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Initialize activation monitor context
 * @traceability SRS-003-ACTIVATION-01
 */
ct_result_t cm_activ_init(cm_activ_ctx_t *ctx,
                          const cm_layer_contract_t *contracts,
                          cm_activ_layer_state_t *layer_states,
                          uint32_t layer_count)
{
    if (!ctx || !layer_states) {
        return CT_ERR_NULL;
    }
    
    if (layer_count == 0 || layer_count > CM_MAX_LAYERS) {
        return CT_ERR_SIZE;
    }
    
    ctx->contracts = contracts;  /* May be NULL if no contracts */
    ctx->layers = layer_states;
    ctx->layer_count = layer_count;
    ctx->window_id = 0;
    ctx->window_samples = 0;
    ctx->initialized = true;
    
    /* Initialize layer states */
    for (uint32_t i = 0; i < layer_count; i++) {
        memset(&layer_states[i], 0, sizeof(cm_activ_layer_state_t));
        
        /* Copy layer_id from contract if available */
        if (contracts) {
            layer_states[i].layer_id = contracts[i].layer_id;
        } else {
            layer_states[i].layer_id = i;  /* Default to index */
        }
        
        layer_states[i].min_observed_q16 = INT32_MAX;
        layer_states[i].max_observed_q16 = INT32_MIN;
    }
    
    return CT_OK;
}

/**
 * @brief Reset activation monitor for new window
 * @traceability SRS-003-ACTIVATION-02
 */
ct_result_t cm_activ_reset_window(cm_activ_ctx_t *ctx, uint64_t window_id)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    ctx->window_id = window_id;
    ctx->window_samples = 0;
    
    /* Reset per-layer state */
    for (uint32_t i = 0; i < ctx->layer_count; i++) {
        uint32_t layer_id = ctx->layers[i].layer_id;  /* Preserve layer_id */
        
        ctx->layers[i].total_elements = 0;
        ctx->layers[i].violation_count = 0;
        ctx->layers[i].saturation_count = 0;
        ctx->layers[i].max_overrange_q16 = 0;
        ctx->layers[i].min_observed_q16 = INT32_MAX;
        ctx->layers[i].max_observed_q16 = INT32_MIN;
        ctx->layers[i].layer_id = layer_id;
    }
    
    return CT_OK;
}

/*============================================================================
 * Layer Checking
 *============================================================================*/

/**
 * @brief Check activations for a single layer
 * @traceability CM-ARCH-MATH-001 §4
 */
ct_result_t cm_activ_check_layer(cm_activ_ctx_t *ctx,
                                 uint32_t layer_idx,
                                 const int32_t *values,
                                 uint32_t count,
                                 cm_activ_layer_result_t *result,
                                 const ct_fault_flags_t *faults)
{
    if (!ctx || !ctx->initialized || !values) {
        return CT_ERR_NULL;
    }
    
    if (layer_idx >= ctx->layer_count) {
        return CT_ERR_RANGE;
    }
    
    cm_activ_layer_state_t *state = &ctx->layers[layer_idx];
    
    /* Get contract bounds (if available) */
    int32_t min_bound = INT32_MIN;
    int32_t max_bound = INT32_MAX;
    uint32_t tol_rate = UINT32_MAX;  /* Default: allow any rate */
    uint32_t max_over_limit = UINT32_MAX;
    
    if (ctx->contracts) {
        const cm_activation_contract_t *contract = &ctx->contracts[layer_idx].contract;
        min_bound = contract->min_q16;
        max_bound = contract->max_q16;
        tol_rate = contract->tol_violations_q0_32;
        max_over_limit = contract->max_overrange_q16;
    }
    
    /* Initialize local result */
    cm_activ_layer_result_t local_result = {0};
    local_result.violations = 0;
    local_result.max_overrange_q16 = 0;
    local_result.rate_exceeded = false;
    local_result.max_exceeded = false;
    
    ct_fault_flags_t dummy_faults = {0};
    
    /* Process each activation value */
    for (uint32_t i = 0; i < count; i++) {
        int32_t value = values[i];
        
        state->total_elements++;
        
        /* Update observed range */
        if (value < state->min_observed_q16) {
            state->min_observed_q16 = value;
        }
        if (value > state->max_observed_q16) {
            state->max_observed_q16 = value;
        }
        
        /* Check bounds */
        int32_t overrange = 0;
        
        if (value < min_bound) {
            state->violation_count++;
            local_result.violations++;
            overrange = dvm_abs(min_bound - value, &dummy_faults);
        } else if (value > max_bound) {
            state->violation_count++;
            local_result.violations++;
            overrange = dvm_abs(value - max_bound, &dummy_faults);
        }
        
        /* Track maximum overrange */
        if (overrange > local_result.max_overrange_q16) {
            local_result.max_overrange_q16 = overrange;
        }
        if (overrange > state->max_overrange_q16) {
            state->max_overrange_q16 = overrange;
        }
    }
    
    /* Count saturations from fault flags */
    if (faults && (faults->overflow || faults->underflow)) {
        state->saturation_count++;
    }
    
    /* Check rate tolerance */
    if (state->total_elements > 0 && tol_rate < UINT32_MAX) {
        uint32_t actual_rate = cm_activ_get_violation_rate(ctx, layer_idx);
        local_result.rate_exceeded = (actual_rate > tol_rate);
    }
    
    /* Check max overrange limit */
    if (max_over_limit < UINT32_MAX) {
        local_result.max_exceeded = ((uint32_t)state->max_overrange_q16 > max_over_limit);
    }
    
    if (result) {
        *result = local_result;
    }
    
    /* Return error if tolerance exceeded */
    if (local_result.rate_exceeded || local_result.max_exceeded) {
        return CT_ERR_RANGE;
    }
    
    return CT_OK;
}

/**
 * @brief Record saturation event for a layer
 * @traceability CM-ARCH-MATH-001 §4
 */
ct_result_t cm_activ_record_saturation(cm_activ_ctx_t *ctx,
                                       uint32_t layer_idx)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    if (layer_idx >= ctx->layer_count) {
        return CT_ERR_RANGE;
    }
    
    ctx->layers[layer_idx].saturation_count++;
    
    return CT_OK;
}

/*============================================================================
 * Statistics Access
 *============================================================================*/

/**
 * @brief Get window summary for all layers
 */
ct_result_t cm_activ_get_window_summary(const cm_activ_ctx_t *ctx,
                                        cm_activ_result_t *result)
{
    if (!ctx || !ctx->initialized || !result) {
        return CT_ERR_NULL;
    }
    
    result->layers_with_violations = 0;
    result->first_violation_layer = 0;
    result->total_saturations = 0;
    memset(&result->first_result, 0, sizeof(result->first_result));
    
    bool first_recorded = false;
    
    for (uint32_t i = 0; i < ctx->layer_count; i++) {
        const cm_activ_layer_state_t *state = &ctx->layers[i];
        
        result->total_saturations += state->saturation_count;
        
        if (state->violation_count > 0) {
            result->layers_with_violations++;
            
            if (!first_recorded) {
                first_recorded = true;
                result->first_violation_layer = i;
                result->first_result.violations = state->violation_count;
                result->first_result.max_overrange_q16 = state->max_overrange_q16;
                result->first_result.rate_exceeded = cm_activ_exceeds_tolerance(ctx, i);
                
                /* Check max limit */
                if (ctx->contracts) {
                    uint32_t limit = ctx->contracts[i].contract.max_overrange_q16;
                    result->first_result.max_exceeded = 
                        ((uint32_t)state->max_overrange_q16 > limit);
                }
            }
        }
    }
    
    return CT_OK;
}

/**
 * @brief Get per-layer statistics
 */
ct_result_t cm_activ_get_layer_state(const cm_activ_ctx_t *ctx,
                                     uint32_t layer_idx,
                                     cm_activ_layer_state_t *state)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    if (layer_idx >= ctx->layer_count) {
        return CT_ERR_RANGE;
    }
    
    if (state) {
        *state = ctx->layers[layer_idx];
    }
    
    return CT_OK;
}

/**
 * @brief Compute violation rate for a layer
 * @traceability CM-ARCH-MATH-001 §4
 */
uint32_t cm_activ_get_violation_rate(const cm_activ_ctx_t *ctx,
                                     uint32_t layer_idx)
{
    if (!ctx || !ctx->initialized || layer_idx >= ctx->layer_count) {
        return 0;
    }
    
    const cm_activ_layer_state_t *state = &ctx->layers[layer_idx];
    
    if (state->total_elements == 0) {
        return 0;
    }
    
    /* Compute rate in Q0.32: (violations * 2^32) / total_elements */
    /* Handle edge case where violations == total_elements */
    if (state->violation_count >= state->total_elements) {
        return UINT32_MAX;
    }
    
    uint64_t num = (uint64_t)state->violation_count << 32;
    uint32_t rate = (uint32_t)(num / state->total_elements);
    
    return rate;
}

/**
 * @brief Check if layer exceeds its tolerance
 */
bool cm_activ_exceeds_tolerance(const cm_activ_ctx_t *ctx,
                                uint32_t layer_idx)
{
    if (!ctx || !ctx->initialized || !ctx->contracts) {
        return false;
    }
    
    if (layer_idx >= ctx->layer_count) {
        return false;
    }
    
    uint32_t tol = ctx->contracts[layer_idx].contract.tol_violations_q0_32;
    uint32_t rate = cm_activ_get_violation_rate(ctx, layer_idx);
    
    return (rate > tol);
}

/**
 * @brief Find layer index by layer_id
 */
ct_result_t cm_activ_find_layer(const cm_activ_ctx_t *ctx,
                                uint32_t layer_id,
                                uint32_t *layer_idx)
{
    if (!ctx || !ctx->initialized || !layer_idx) {
        return CT_ERR_NULL;
    }
    
    for (uint32_t i = 0; i < ctx->layer_count; i++) {
        if (ctx->layers[i].layer_id == layer_id) {
            *layer_idx = i;
            return CT_OK;
        }
    }
    
    return CT_ERR_RANGE;  /* Not found */
}
