/**
 * @file health.h
 * @brief Health Monitor FSM API
 * @traceability CM-ARCH-MATH-001 §6, SRS-005-HEALTH
 *
 * @details
 * Provides deterministic health monitoring state machine:
 * - States: UNINIT -> INIT -> ENABLED -> ALARM -> DEGRADED -> STOPPED
 * - Fault budget tracking per window
 * - Violation-based state transitions
 * - Window boundary handling
 *
 * All operations are integer-only and produce bit-identical results
 * across x86, ARM, and RISC-V platforms.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef HEALTH_H
#define HEALTH_H

#include "cm_types.h"
#include "ct_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Health Monitor Context
 *============================================================================*/

/**
 * @brief Health monitor context
 * @traceability CM-ARCH-MATH-001 §6
 */
typedef struct {
    cm_monitor_state_t state;               /**< Current FSM state */
    const cm_fault_budget_t *budget;        /**< Fault budget from policy */
    uint64_t window_id;                     /**< Current window ID */
    
    /* Per-window fault counters */
    uint32_t overflow_count;                /**< Overflow events this window */
    uint32_t underflow_count;               /**< Underflow events this window */
    uint32_t saturation_count;              /**< Saturation events this window */
    uint32_t clamp_count;                   /**< Clamp events this window */
    
    /* Cumulative counters */
    uint64_t total_overflows;               /**< Total overflow events */
    uint64_t total_underflows;              /**< Total underflow events */
    uint64_t total_saturations;             /**< Total saturation events */
    uint64_t total_clamps;                  /**< Total clamp events */
    
    /* State tracking */
    uint32_t consecutive_alarm_windows;     /**< Windows with violations in a row */
    uint32_t alarm_threshold;               /**< Windows before degrading */
    uint32_t degraded_threshold;            /**< Violations in degraded before stop */
    
    bool initialized;                       /**< True if properly initialized */
} cm_health_ctx_t;

/**
 * @brief Health check result
 */
typedef struct {
    bool budget_exceeded;                   /**< True if any budget exceeded */
    cm_violation_t worst_violation;         /**< Most severe violation type */
    uint32_t exceeded_count;                /**< Number of budgets exceeded */
} cm_health_result_t;

/**
 * @brief State transition result
 */
typedef struct {
    cm_monitor_state_t old_state;           /**< State before transition */
    cm_monitor_state_t new_state;           /**< State after transition */
    bool transition_occurred;               /**< True if state changed */
    cm_violation_t trigger;                 /**< Violation that triggered change */
} cm_health_transition_t;

/*============================================================================
 * Health Monitor Functions
 *============================================================================*/

/**
 * @brief Initialize health monitor context
 * @param ctx Context to initialize
 * @param budget Fault budget from policy (may be NULL for defaults)
 * @return CT_OK on success
 * @traceability SRS-005-HEALTH-01
 */
ct_result_t cm_health_init(cm_health_ctx_t *ctx,
                           const cm_fault_budget_t *budget);

/**
 * @brief Enable the health monitor
 * @param ctx Health monitor context
 * @return CT_OK on success
 * @traceability SRS-005-HEALTH-02
 */
ct_result_t cm_health_enable(cm_health_ctx_t *ctx);

/**
 * @brief Reset health monitor for new window
 * @param ctx Health monitor context
 * @param window_id New window identifier
 * @return CT_OK on success
 * @traceability SRS-005-HEALTH-03
 */
ct_result_t cm_health_reset_window(cm_health_ctx_t *ctx, uint64_t window_id);

/**
 * @brief Record fault flags from inference
 * @param ctx Health monitor context
 * @param faults Fault flags from inference engine
 * @return CT_OK on success
 * @traceability CM-ARCH-MATH-001 §6
 */
ct_result_t cm_health_record_faults(cm_health_ctx_t *ctx,
                                    const ct_fault_flags_t *faults);

/**
 * @brief Record a single saturation event
 * @param ctx Health monitor context
 * @return CT_OK on success
 */
ct_result_t cm_health_record_saturation(cm_health_ctx_t *ctx);

/**
 * @brief Record a single clamp event
 * @param ctx Health monitor context
 * @return CT_OK on success
 */
ct_result_t cm_health_record_clamp(cm_health_ctx_t *ctx);

/**
 * @brief Check if any fault budget is exceeded
 * @param ctx Health monitor context
 * @param result Output check result
 * @return CT_OK on success
 * @traceability CM-ARCH-MATH-001 §6
 */
ct_result_t cm_health_check_budget(const cm_health_ctx_t *ctx,
                                   cm_health_result_t *result);

/**
 * @brief Process window end and update state if needed
 * @param ctx Health monitor context
 * @param had_violations True if window had any violations
 * @param transition Output transition result (may be NULL)
 * @return CT_OK on success
 * @traceability SRS-005-HEALTH-04
 */
ct_result_t cm_health_process_window_end(cm_health_ctx_t *ctx,
                                         bool had_violations,
                                         cm_health_transition_t *transition);

/**
 * @brief Trigger emergency stop
 * @param ctx Health monitor context
 * @param transition Output transition result (may be NULL)
 * @return CT_OK on success
 * @traceability SRS-005-HEALTH-05
 */
ct_result_t cm_health_emergency_stop(cm_health_ctx_t *ctx,
                                     cm_health_transition_t *transition);

/**
 * @brief Get current state
 * @param ctx Health monitor context
 * @return Current FSM state
 */
cm_monitor_state_t cm_health_get_state(const cm_health_ctx_t *ctx);

/**
 * @brief Check if monitor is operational (ENABLED or ALARM)
 * @param ctx Health monitor context
 * @return true if monitoring is active
 */
bool cm_health_is_operational(const cm_health_ctx_t *ctx);

/**
 * @brief Check if monitor is stopped
 * @param ctx Health monitor context
 * @return true if in STOPPED state
 */
bool cm_health_is_stopped(const cm_health_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* HEALTH_H */
