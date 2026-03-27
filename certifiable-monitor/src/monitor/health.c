/**
 * @file health.c
 * @brief Health Monitor FSM Implementation
 * @traceability CM-ARCH-MATH-001 §6, SRS-005-HEALTH
 *
 * @details
 * Implements deterministic health monitoring state machine:
 * - States: UNINIT → INIT → ENABLED → ALARM → DEGRADED → STOPPED
 * - Fault budget tracking per window
 * - Violation-based state transitions
 *
 * All operations are integer-only with no dynamic allocation.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "health.h"
#include <string.h>

/*============================================================================
 * Default Budgets
 *============================================================================*/

static const cm_fault_budget_t default_budget = {
    .overflow_budget = 100,
    .underflow_budget = 100,
    .saturation_budget = 1000,
    .clamp_budget = 1000
};

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Initialize health monitor context
 * @traceability SRS-005-HEALTH-01
 */
ct_result_t cm_health_init(cm_health_ctx_t *ctx,
                           const cm_fault_budget_t *budget)
{
    if (!ctx) {
        return CT_ERR_NULL;
    }
    
    memset(ctx, 0, sizeof(cm_health_ctx_t));
    
    ctx->budget = budget ? budget : &default_budget;
    ctx->state = CM_STATE_INIT;
    ctx->window_id = 0;
    
    /* Reset counters */
    ctx->overflow_count = 0;
    ctx->underflow_count = 0;
    ctx->saturation_count = 0;
    ctx->clamp_count = 0;
    
    ctx->total_overflows = 0;
    ctx->total_underflows = 0;
    ctx->total_saturations = 0;
    ctx->total_clamps = 0;
    
    ctx->consecutive_alarm_windows = 0;
    ctx->alarm_threshold = 3;      /* Default: 3 consecutive alarms → degraded */
    ctx->degraded_threshold = 1;   /* Default: 1 violation in degraded → stop */
    
    ctx->initialized = true;
    
    return CT_OK;
}

/**
 * @brief Enable the health monitor
 * @traceability SRS-005-HEALTH-02
 */
ct_result_t cm_health_enable(cm_health_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    if (ctx->state == CM_STATE_STOPPED) {
        return CT_ERR_STATE;  /* Cannot enable from STOPPED */
    }
    
    if (ctx->state == CM_STATE_INIT) {
        ctx->state = CM_STATE_ENABLED;
    }
    
    return CT_OK;
}

/**
 * @brief Reset health monitor for new window
 * @traceability SRS-005-HEALTH-03
 */
ct_result_t cm_health_reset_window(cm_health_ctx_t *ctx, uint64_t window_id)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    ctx->window_id = window_id;
    
    /* Reset per-window counters */
    ctx->overflow_count = 0;
    ctx->underflow_count = 0;
    ctx->saturation_count = 0;
    ctx->clamp_count = 0;
    
    return CT_OK;
}

/*============================================================================
 * Fault Recording
 *============================================================================*/

/**
 * @brief Record fault flags from inference
 * @traceability CM-ARCH-MATH-001 §6
 */
ct_result_t cm_health_record_faults(cm_health_ctx_t *ctx,
                                    const ct_fault_flags_t *faults)
{
    if (!ctx || !ctx->initialized || !faults) {
        return CT_ERR_NULL;
    }
    
    if (faults->overflow) {
        ctx->overflow_count++;
        ctx->total_overflows++;
    }
    
    if (faults->underflow) {
        ctx->underflow_count++;
        ctx->total_underflows++;
    }
    
    return CT_OK;
}

/**
 * @brief Record a single saturation event
 */
ct_result_t cm_health_record_saturation(cm_health_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    ctx->saturation_count++;
    ctx->total_saturations++;
    
    return CT_OK;
}

/**
 * @brief Record a single clamp event
 */
ct_result_t cm_health_record_clamp(cm_health_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    ctx->clamp_count++;
    ctx->total_clamps++;
    
    return CT_OK;
}

/*============================================================================
 * Budget Checking
 *============================================================================*/

/**
 * @brief Check if any fault budget is exceeded
 * @traceability CM-ARCH-MATH-001 §6
 */
ct_result_t cm_health_check_budget(const cm_health_ctx_t *ctx,
                                   cm_health_result_t *result)
{
    if (!ctx || !ctx->initialized || !result) {
        return CT_ERR_NULL;
    }
    
    result->budget_exceeded = false;
    result->worst_violation = CM_VIOL_NONE;
    result->exceeded_count = 0;
    
    const cm_fault_budget_t *b = ctx->budget;
    
    if (ctx->overflow_count > b->overflow_budget) {
        result->budget_exceeded = true;
        result->exceeded_count++;
        if (result->worst_violation == CM_VIOL_NONE) {
            result->worst_violation = CM_VIOL_FAULT_BUDGET;
        }
    }
    
    if (ctx->underflow_count > b->underflow_budget) {
        result->budget_exceeded = true;
        result->exceeded_count++;
        if (result->worst_violation == CM_VIOL_NONE) {
            result->worst_violation = CM_VIOL_FAULT_BUDGET;
        }
    }
    
    if (ctx->saturation_count > b->saturation_budget) {
        result->budget_exceeded = true;
        result->exceeded_count++;
        if (result->worst_violation == CM_VIOL_NONE) {
            result->worst_violation = CM_VIOL_FAULT_BUDGET;
        }
    }
    
    if (ctx->clamp_count > b->clamp_budget) {
        result->budget_exceeded = true;
        result->exceeded_count++;
        if (result->worst_violation == CM_VIOL_NONE) {
            result->worst_violation = CM_VIOL_FAULT_BUDGET;
        }
    }
    
    return CT_OK;
}

/*============================================================================
 * State Machine
 *============================================================================*/

/**
 * @brief Process window end and update state if needed
 * @traceability SRS-005-HEALTH-04
 */
ct_result_t cm_health_process_window_end(cm_health_ctx_t *ctx,
                                         bool had_violations,
                                         cm_health_transition_t *transition)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    cm_health_transition_t local_trans;
    local_trans.old_state = ctx->state;
    local_trans.new_state = ctx->state;
    local_trans.transition_occurred = false;
    local_trans.trigger = CM_VIOL_NONE;
    
    switch (ctx->state) {
        case CM_STATE_UNINIT:
        case CM_STATE_INIT:
            /* No transitions from these states based on violations */
            break;
            
        case CM_STATE_ENABLED:
            if (had_violations) {
                ctx->state = CM_STATE_ALARM;
                ctx->consecutive_alarm_windows = 1;
                local_trans.new_state = CM_STATE_ALARM;
                local_trans.transition_occurred = true;
                local_trans.trigger = CM_VIOL_FAULT_BUDGET;
            }
            break;
            
        case CM_STATE_ALARM:
            if (had_violations) {
                ctx->consecutive_alarm_windows++;
                if (ctx->consecutive_alarm_windows >= ctx->alarm_threshold) {
                    ctx->state = CM_STATE_DEGRADED;
                    local_trans.new_state = CM_STATE_DEGRADED;
                    local_trans.transition_occurred = true;
                    local_trans.trigger = CM_VIOL_FAULT_BUDGET;
                }
            } else {
                /* Recovery: no violations, go back to ENABLED */
                ctx->state = CM_STATE_ENABLED;
                ctx->consecutive_alarm_windows = 0;
                local_trans.new_state = CM_STATE_ENABLED;
                local_trans.transition_occurred = true;
            }
            break;
            
        case CM_STATE_DEGRADED:
            if (had_violations) {
                /* Any violation in degraded → STOPPED */
                ctx->state = CM_STATE_STOPPED;
                local_trans.new_state = CM_STATE_STOPPED;
                local_trans.transition_occurred = true;
                local_trans.trigger = CM_VIOL_FAULT_BUDGET;
            }
            break;
            
        case CM_STATE_STOPPED:
            /* Terminal state, no transitions */
            break;
    }
    
    if (transition) {
        *transition = local_trans;
    }
    
    return CT_OK;
}

/**
 * @brief Trigger emergency stop
 * @traceability SRS-005-HEALTH-05
 */
ct_result_t cm_health_emergency_stop(cm_health_ctx_t *ctx,
                                     cm_health_transition_t *transition)
{
    if (!ctx || !ctx->initialized) {
        return CT_ERR_NULL;
    }
    
    cm_health_transition_t local_trans;
    local_trans.old_state = ctx->state;
    local_trans.trigger = CM_VIOL_FAULT_BUDGET;
    
    if (ctx->state != CM_STATE_STOPPED) {
        ctx->state = CM_STATE_STOPPED;
        local_trans.new_state = CM_STATE_STOPPED;
        local_trans.transition_occurred = true;
    } else {
        local_trans.new_state = CM_STATE_STOPPED;
        local_trans.transition_occurred = false;
    }
    
    if (transition) {
        *transition = local_trans;
    }
    
    return CT_OK;
}

/*============================================================================
 * State Queries
 *============================================================================*/

/**
 * @brief Get current state
 */
cm_monitor_state_t cm_health_get_state(const cm_health_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        return CM_STATE_UNINIT;
    }
    return ctx->state;
}

/**
 * @brief Check if monitor is operational
 */
bool cm_health_is_operational(const cm_health_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        return false;
    }
    return (ctx->state == CM_STATE_ENABLED || ctx->state == CM_STATE_ALARM);
}

/**
 * @brief Check if monitor is stopped
 */
bool cm_health_is_stopped(const cm_health_ctx_t *ctx)
{
    if (!ctx || !ctx->initialized) {
        return false;
    }
    return (ctx->state == CM_STATE_STOPPED);
}
