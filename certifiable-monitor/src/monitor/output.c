/**
 * @file output.c
 * @brief Output Monitor Implementation
 * @traceability CM-ARCH-MATH-001 §5, SRS-004-OUTPUT
 *
 * @details
 * Implements deterministic output monitoring:
 * - Per-output range checking against COE envelope
 * - Histogram accumulation for output drift detection
 * - Violation counting and reporting
 *
 * All operations are integer-only with no dynamic allocation.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "output.h"
#include "cm_detect.h"
#include <string.h>
#include <limits.h>

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Initialize output monitor context
 * @traceability SRS-004-OUTPUT-01
 */
ct_result_t cm_output_init(cm_output_ctx_t *ctx,
                           const cm_output_envelope_t *envelope,
                           cm_output_state_t *output_states,
                           uint32_t output_count)
{
    if (!ctx || !output_states) {
        return CT_ERR_NULL;
    }
    
    if (output_count == 0 || output_count > CM_MAX_FEATURES) {
        return CT_ERR_SIZE;
    }
    
    /* Envelope may be NULL if no bounds checking is needed */
    if (envelope && envelope->output_count != output_count) {
        return CT_ERR_SIZE;
    }
    
    ctx->envelope = envelope;
    ctx->outputs = output_states;
    ctx->output_count = output_count;
    ctx->window_id = 0;
    ctx->window_samples = 0;
    ctx->total_violations = 0;
    ctx->initialized = true;
    
    /* Initialize output states */
    for (uint32_t i = 0; i < output_count; i++) {
        memset(&output_states[i], 0, sizeof(cm_output_state_t));
        output_states[i].min_observed_q16 = INT32_MAX;
        output_states[i].max_observed_q16 = INT32_MIN;
    }
    
    return CT_OK;
}

/**
 * @brief Reset output monitor for new window
 * @traceability SRS-004-OUTPUT-02
 */
ct_result_t cm_output_reset_window(cm_output_ctx_t *ctx, uint64_t window_id)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    ctx->window_id = window_id;
    ctx->window_samples = 0;
    ctx->total_violations = 0;
    
    /* Reset per-output state */
    for (uint32_t i = 0; i < ctx->output_count; i++) {
        ctx->outputs[i].violation_count = 0;
        ctx->outputs[i].sample_count = 0;
        ctx->outputs[i].min_observed_q16 = INT32_MAX;
        ctx->outputs[i].max_observed_q16 = INT32_MIN;
        
        /* Zero histogram counts */
        memset(ctx->outputs[i].hist_counts, 0, sizeof(ctx->outputs[i].hist_counts));
    }
    
    return CT_OK;
}

/*============================================================================
 * Range Checking
 *============================================================================*/

/**
 * @brief Check if a single output value is in range
 * @traceability CM-ARCH-MATH-001 §5.1
 */
bool cm_output_check_value(const cm_output_ctx_t *ctx,
                           uint32_t output_idx,
                           int32_t value)
{
    if (!ctx || !ctx->initialized || !ctx->envelope) {
        return true;  /* No envelope = no checking */
    }
    
    if (output_idx >= ctx->output_count) {
        return false;
    }
    
    int32_t min_bound = ctx->envelope->min_q16[output_idx];
    int32_t max_bound = ctx->envelope->max_q16[output_idx];
    
    return (value >= min_bound) && (value <= max_bound);
}

/*============================================================================
 * Sample Processing
 *============================================================================*/

/**
 * @brief Process a single output vector
 * @traceability CM-ARCH-MATH-001 §5.1
 */
ct_result_t cm_output_process(cm_output_ctx_t *ctx,
                              const int32_t *values,
                              cm_output_result_t *result,
                              ct_fault_flags_t *faults)
{
    (void)faults;  /* Reserved for future use */
    
    if (!ctx || !ctx->initialized || !values) {
        return CT_ERR_NULL;
    }
    
    /* Initialize result */
    cm_output_result_t local_result = {0};
    local_result.violations = 0;
    local_result.first_violation_output = 0;
    local_result.first_violation_value_q16 = 0;
    local_result.first_violation_bound_q16 = 0;
    local_result.is_upper_bound = false;
    
    bool first_violation_recorded = false;
    
    /* Process each output */
    for (uint32_t i = 0; i < ctx->output_count; i++) {
        int32_t value = values[i];
        cm_output_state_t *state = &ctx->outputs[i];
        
        state->sample_count++;
        
        /* Update observed range */
        if (value < state->min_observed_q16) {
            state->min_observed_q16 = value;
        }
        if (value > state->max_observed_q16) {
            state->max_observed_q16 = value;
        }
        
        /* Range checking */
        if (ctx->envelope) {
            int32_t min_bound = ctx->envelope->min_q16[i];
            int32_t max_bound = ctx->envelope->max_q16[i];
            
            bool violation = false;
            bool upper = false;
            int32_t violated_bound = 0;
            
            if (value < min_bound) {
                violation = true;
                upper = false;
                violated_bound = min_bound;
            } else if (value > max_bound) {
                violation = true;
                upper = true;
                violated_bound = max_bound;
            }
            
            if (violation) {
                state->violation_count++;
                local_result.violations++;
                ctx->total_violations++;
                
                if (!first_violation_recorded) {
                    first_violation_recorded = true;
                    local_result.first_violation_output = i;
                    local_result.first_violation_value_q16 = value;
                    local_result.first_violation_bound_q16 = violated_bound;
                    local_result.is_upper_bound = upper;
                }
            }
            
            /* Histogram accumulation (if configured) */
            if (ctx->envelope->has_hists && i < ctx->envelope->output_count) {
                const cm_hist_spec_t *spec = &ctx->envelope->hists[i];
                if (spec->bin_count > 0) {
                    int32_t bin = cm_hist_assign_bin(value, spec);
                    if (bin >= 0 && (uint32_t)bin < CM_MAX_BINS) {
                        state->hist_counts[bin]++;
                    }
                }
            }
        }
    }
    
    ctx->window_samples++;
    
    if (result) {
        *result = local_result;
    }
    
    return (local_result.violations > 0) ? CT_ERR_RANGE : CT_OK;
}

/*============================================================================
 * Statistics Access
 *============================================================================*/

/**
 * @brief Get violation count for current window
 */
uint32_t cm_output_get_violations(const cm_output_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        return 0;
    }
    return ctx->total_violations;
}

/**
 * @brief Get sample count for current window
 */
uint32_t cm_output_get_sample_count(const cm_output_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        return 0;
    }
    return ctx->window_samples;
}

/**
 * @brief Get per-output statistics
 */
ct_result_t cm_output_get_state(const cm_output_ctx_t *ctx,
                                uint32_t output_idx,
                                cm_output_state_t *state)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    if (output_idx >= ctx->output_count) {
        return CT_ERR_RANGE;
    }
    
    if (state) {
        *state = ctx->outputs[output_idx];
    }
    
    return CT_OK;
}

/*============================================================================
 * Drift Computation
 *============================================================================*/

/**
 * @brief Compute drift for all outputs at end of window
 * @traceability CM-ARCH-MATH-001 §5.2
 */
ct_result_t cm_output_compute_drift(const cm_output_ctx_t *ctx,
                                    const cm_drift_policy_t *drift_policy,
                                    cm_output_drift_result_t *result,
                                    ct_fault_flags_t *faults)
{
    if (!ctx || !ctx->initialized || !drift_policy || !result) {
        return CT_ERR_NULL;
    }
    
    /* Initialize result */
    result->outputs_with_drift = 0;
    result->first_drift_output = 0;
    memset(&result->first_drift_result, 0, sizeof(result->first_drift_result));
    
    /* No envelope or no histograms = no drift checking */
    if (!ctx->envelope || !ctx->envelope->has_hists) {
        return CT_OK;
    }
    
    bool first_drift_recorded = false;
    
    /* Check drift for each output with histogram */
    for (uint32_t i = 0; i < ctx->output_count; i++) {
        const cm_hist_spec_t *spec = &ctx->envelope->hists[i];
        
        if (spec->bin_count == 0) {
            continue;  /* No histogram for this output */
        }
        
        const cm_output_state_t *state = &ctx->outputs[i];
        
        /* Compute drift between runtime and reference */
        cm_drift_result_t drift_result;
        ct_result_t rc = cm_detect_drift(
            state->hist_counts,
            spec->ref_counts,
            spec->bin_count,
            drift_policy,
            &drift_result,
            faults
        );
        
        if (rc != CT_OK && rc != CT_ERR_DOMAIN) {
            continue;
        }
        
        /* Check if any detector triggered */
        if (drift_result.flags != 0) {
            result->outputs_with_drift++;
            
            if (!first_drift_recorded) {
                first_drift_recorded = true;
                result->first_drift_output = i;
                result->first_drift_result = drift_result;
            }
        }
    }
    
    return CT_OK;
}
