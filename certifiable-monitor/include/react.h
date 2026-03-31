/**
 * @file react.h
 * @brief Reaction Handler API
 * @traceability CM-ARCH-MATH-001 §8, SRS-007-REACT
 *
 * @details
 * Provides deterministic reaction handling:
 * - Policy lookup for violation → action mapping
 * - Action execution (log, warn, clamp, degrade, stop)
 * - Ledger event emission
 *
 * All operations are integer-only and produce bit-identical results
 * across x86, ARM, and RISC-V platforms.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef REACT_H
#define REACT_H

#include "cm_types.h"
#include "ct_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Reaction Context
 *============================================================================*/

/**
 * @brief Reaction handler context
 * @traceability CM-ARCH-MATH-001 §8
 */
typedef struct {
    const cm_reaction_map_entry_t *react_map; /**< Reaction mapping table */
    uint32_t react_map_count;                 /**< Number of entries in map */
    cm_reaction_t default_reaction;           /**< Fallback if no mapping found */
    bool initialized;                         /**< True if properly initialized */
} cm_react_handler_t;

/**
 * @brief Reaction execution result
 */
typedef struct {
    cm_violation_t violation;                 /**< Input violation type */
    cm_reaction_t action;                     /**< Action taken */
    bool action_executed;                     /**< True if action was executed */
    bool ledger_event_emitted;                /**< True if event was logged */
} cm_react_result_t;

/*============================================================================
 * Reaction Handler Functions
 *============================================================================*/

/**
 * @brief Initialize reaction handler
 * @param handler Handler to initialize
 * @param react_map Reaction mapping table from policy
 * @param map_count Number of entries in mapping table
 * @return CT_OK on success
 * @traceability SRS-007-REACT-01
 */
ct_result_t cm_react_init(cm_react_handler_t *handler,
                          const cm_reaction_map_entry_t *react_map,
                          uint32_t map_count);

/**
 * @brief Set default reaction for unmapped violations
 * @param handler Reaction handler
 * @param reaction Default reaction
 * @return CT_OK on success
 */
ct_result_t cm_react_set_default(cm_react_handler_t *handler,
                                 cm_reaction_t reaction);

/**
 * @brief Look up reaction for a violation type
 * @param handler Reaction handler
 * @param violation Violation type
 * @return Corresponding reaction, or default if not found
 * @traceability CM-ARCH-MATH-001 §8.1
 */
cm_reaction_t cm_react_lookup(const cm_react_handler_t *handler,
                              cm_violation_t violation);

/**
 * @brief Process a violation and determine action
 * @param handler Reaction handler
 * @param violation Violation that occurred
 * @param result Output result (may be NULL)
 * @return CT_OK on success
 * @traceability CM-ARCH-MATH-001 §8.2
 */
ct_result_t cm_react_process(const cm_react_handler_t *handler,
                             cm_violation_t violation,
                             cm_react_result_t *result);

/**
 * @brief Check if reaction requires stopping inference
 * @param reaction Reaction to check
 * @return true if reaction is EMERGENCY_STOP
 */
bool cm_react_is_stop(cm_reaction_t reaction);

/**
 * @brief Check if reaction requires degraded mode
 * @param reaction Reaction to check
 * @return true if reaction is DEGRADE_MODE or EMERGENCY_STOP
 */
bool cm_react_is_degrade(cm_reaction_t reaction);

/**
 * @brief Check if reaction requires output clamping
 * @param reaction Reaction to check
 * @return true if reaction is CLAMP_OUTPUT
 */
bool cm_react_is_clamp(cm_reaction_t reaction);

/**
 * @brief Get reaction name as string (for logging)
 * @param reaction Reaction type
 * @return Static string name
 */
const char *cm_react_name(cm_reaction_t reaction);

/**
 * @brief Get violation name as string (for logging)
 * @param violation Violation type
 * @return Static string name
 */
const char *cm_viol_name(cm_violation_t violation);

#ifdef __cplusplus
}
#endif

#endif /* REACT_H */
