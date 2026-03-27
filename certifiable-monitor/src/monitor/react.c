/**
 * @file react.c
 * @brief Reaction Handler Implementation
 * @traceability CM-ARCH-MATH-001 §8, SRS-007-REACT
 *
 * @details
 * Implements deterministic reaction handling:
 * - Policy lookup for violation → action mapping
 * - Action determination based on policy
 *
 * All operations are integer-only with no dynamic allocation.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "react.h"
#include <string.h>

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Initialize reaction handler
 * @traceability SRS-007-REACT-01
 */
ct_result_t cm_react_init(cm_react_handler_t *handler,
                          const cm_reaction_map_entry_t *react_map,
                          uint32_t map_count)
{
    if (!handler) {
        return CT_ERR_NULL;
    }
    
    if (map_count > CM_MAX_REACTIONS) {
        return CT_ERR_SIZE;
    }
    
    handler->react_map = react_map;  /* May be NULL */
    handler->react_map_count = map_count;
    handler->default_reaction = CM_REACT_LOG_ONLY;
    handler->initialized = true;
    
    return CT_OK;
}

/**
 * @brief Set default reaction for unmapped violations
 */
ct_result_t cm_react_set_default(cm_react_handler_t *handler,
                                 cm_reaction_t reaction)
{
    if (!handler || !handler->initialized) {
        return CT_ERR_NULL;
    }
    
    handler->default_reaction = reaction;
    return CT_OK;
}

/*============================================================================
 * Reaction Lookup
 *============================================================================*/

/**
 * @brief Look up reaction for a violation type
 * @traceability CM-ARCH-MATH-001 §8.1
 */
cm_reaction_t cm_react_lookup(const cm_react_handler_t *handler,
                              cm_violation_t violation)
{
    if (!handler || !handler->initialized) {
        return CM_REACT_LOG_ONLY;
    }
    
    if (violation == CM_VIOL_NONE) {
        return CM_REACT_LOG_ONLY;
    }
    
    /* Linear search through mapping table */
    if (handler->react_map) {
        for (uint32_t i = 0; i < handler->react_map_count; i++) {
            if (handler->react_map[i].violation == violation) {
                return handler->react_map[i].reaction;
            }
        }
    }
    
    return handler->default_reaction;
}

/**
 * @brief Process a violation and determine action
 * @traceability CM-ARCH-MATH-001 §8.2
 */
ct_result_t cm_react_process(const cm_react_handler_t *handler,
                             cm_violation_t violation,
                             cm_react_result_t *result)
{
    if (!handler || !handler->initialized) {
        return CT_ERR_NULL;
    }
    
    cm_react_result_t local_result;
    local_result.violation = violation;
    local_result.action = cm_react_lookup(handler, violation);
    local_result.action_executed = true;
    local_result.ledger_event_emitted = false;  /* Caller must handle ledger */
    
    if (result) {
        *result = local_result;
    }
    
    return CT_OK;
}

/*============================================================================
 * Reaction Queries
 *============================================================================*/

/**
 * @brief Check if reaction requires stopping inference
 */
bool cm_react_is_stop(cm_reaction_t reaction)
{
    return (reaction == CM_REACT_EMERGENCY_STOP);
}

/**
 * @brief Check if reaction requires degraded mode
 */
bool cm_react_is_degrade(cm_reaction_t reaction)
{
    return (reaction == CM_REACT_DEGRADE_MODE || 
            reaction == CM_REACT_EMERGENCY_STOP);
}

/**
 * @brief Check if reaction requires output clamping
 */
bool cm_react_is_clamp(cm_reaction_t reaction)
{
    return (reaction == CM_REACT_CLAMP_OUTPUT);
}

/*============================================================================
 * String Conversions (for logging/debugging)
 *============================================================================*/

/**
 * @brief Get reaction name as string
 */
const char *cm_react_name(cm_reaction_t reaction)
{
    switch (reaction) {
        case CM_REACT_LOG_ONLY:       return "LOG_ONLY";
        case CM_REACT_WARN_OPERATOR:  return "WARN_OPERATOR";
        case CM_REACT_CLAMP_OUTPUT:   return "CLAMP_OUTPUT";
        case CM_REACT_DEGRADE_MODE:   return "DEGRADE_MODE";
        case CM_REACT_EMERGENCY_STOP: return "EMERGENCY_STOP";
        default:                      return "UNKNOWN";
    }
}

/**
 * @brief Get violation name as string
 */
const char *cm_viol_name(cm_violation_t violation)
{
    switch (violation) {
        case CM_VIOL_NONE:         return "NONE";
        case CM_VIOL_INPUT_RANGE:  return "INPUT_RANGE";
        case CM_VIOL_INPUT_DRIFT:  return "INPUT_DRIFT";
        case CM_VIOL_ACTIV_RANGE:  return "ACTIV_RANGE";
        case CM_VIOL_ACTIV_SAT:    return "ACTIV_SAT";
        case CM_VIOL_OUTPUT_RANGE: return "OUTPUT_RANGE";
        case CM_VIOL_OUTPUT_DRIFT: return "OUTPUT_DRIFT";
        case CM_VIOL_FAULT_BUDGET: return "FAULT_BUDGET";
        default:                   return "UNKNOWN";
    }
}
