/**
 * @file cm_types.h
 * @brief certifiable-monitor data types
 * @traceability CM-STRUCT-001 (All sections)
 *
 * @details
 * Defines all data structures for the certifiable-monitor system:
 * - Policy types (COE, drift, reactions)
 * - Ledger types (hash chain, entries)
 * - Monitor state machine
 * - Window statistics and drift results
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef CM_TYPES_H
#define CM_TYPES_H

#include "ct_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct cm_policy_s cm_policy_t;
typedef struct cm_ledger_ctx_s cm_ledger_ctx_t;
typedef struct cm_monitor_ctx_s cm_monitor_ctx_t;

/*============================================================================
 * Constants
 *============================================================================*/

/** @defgroup cm_constants Monitor Constants
 *  @{
 */

/** Maximum number of histogram bins */
#define CM_MAX_BINS           64

/** Maximum number of input features */
#define CM_MAX_FEATURES       64

/** Maximum number of monitored layers */
#define CM_MAX_LAYERS         32

/** Maximum reaction map entries */
#define CM_MAX_REACTIONS      32

/** Maximum policy JSON size */
#define CM_MAX_POLICY_SIZE    65536

/** Policy version */
#define CM_POLICY_VERSION     1

/** Ledger domain tags */
#define CM_LEDGER_GENESIS_TAG "CM:LEDGER:GENESIS:v1"
#define CM_LEDGER_ENTRY_TAG   "CM:LEDGER:ENTRY:v1"
#define CM_LEDGER_CHAIN_TAG   "CM:LEDGER:v1"
#define CM_POLICY_TAG         "CM:POLICY:v1"

/** LUT parameters (CM-MATH-001 Appendix A) */
#define CM_LUT_SIZE           512
#define CM_LUT_SHIFT          9

/** ln(2) in Q16.16 format: 0.693147... * 65536 = 45426 */
#define CM_LN2_Q16            45426

/** @} */

/*============================================================================
 * Section 1: Policy Types (CM-STRUCT-001 §1)
 *============================================================================*/

/** @defgroup policy_types Policy Types
 *  @{
 */

/**
 * @brief Histogram specification
 * @traceability CM-STRUCT-001 §1.1
 */
typedef struct {
    uint32_t bin_count;                    /**< Number of bins (B) */
    int32_t  edges_q16[CM_MAX_BINS + 1];   /**< Bin edges in Q16.16, length bin_count+1 */
    uint32_t ref_counts[CM_MAX_BINS];      /**< Reference counts from calibration */
} cm_hist_spec_t;

/**
 * @brief Drift detection policy
 * @traceability CM-STRUCT-001 §1.2
 */
typedef struct {
    uint32_t tv_threshold_q0_32;           /**< Total Variation threshold in Q0.32 */
    int32_t  jsd_threshold_q16_16;         /**< Jensen-Shannon threshold in Q16.16 */
    int32_t  psi_threshold_q16_16;         /**< PSI threshold in Q16.16 */
    uint32_t enabled_detectors;            /**< Bitmask: TV=0x01, JSD=0x02, PSI=0x04 */
    uint32_t epsilon_q0_32;                /**< Smoothing constant for PSI */
} cm_drift_policy_t;

/** Drift detector enable flags */
#define CM_DRIFT_TV_ENABLED   0x01
#define CM_DRIFT_JSD_ENABLED  0x02
#define CM_DRIFT_PSI_ENABLED  0x04

/**
 * @brief Input envelope specification
 * @traceability CM-STRUCT-001 §1.3
 */
typedef struct {
    uint32_t feature_count;                /**< Number of input features */
    int32_t  min_q16[CM_MAX_FEATURES];     /**< Per-feature minimum in Q16.16 */
    int32_t  max_q16[CM_MAX_FEATURES];     /**< Per-feature maximum in Q16.16 */
    bool     has_hists;                    /**< True if histograms are provided */
    cm_hist_spec_t hists[CM_MAX_FEATURES]; /**< Per-feature histograms (optional) */
} cm_input_envelope_t;

/**
 * @brief Activation contract for a single layer
 * @traceability CM-STRUCT-001 §1.4
 */
typedef struct {
    int32_t  min_q16;                      /**< Minimum bound in Q16.16 */
    int32_t  max_q16;                      /**< Maximum bound in Q16.16 */
    uint32_t tol_violations_q0_32;         /**< Allowed violation rate in Q0.32 */
    uint32_t max_overrange_q16;            /**< Maximum over-range magnitude in Q16.16 */
} cm_activation_contract_t;

/**
 * @brief Layer contract with identifier
 * @traceability CM-STRUCT-001 §1.4
 */
typedef struct {
    uint32_t layer_id;                     /**< Unique layer identifier */
    cm_activation_contract_t contract;     /**< Layer bounds contract */
} cm_layer_contract_t;

/**
 * @brief Reaction action types
 * @traceability CM-STRUCT-001 §1.5
 */
typedef enum {
    CM_REACT_LOG_ONLY       = 0,           /**< Record event, no action */
    CM_REACT_WARN_OPERATOR  = 1,           /**< Raise operator alert */
    CM_REACT_CLAMP_OUTPUT   = 2,           /**< Apply deterministic clamp */
    CM_REACT_DEGRADE_MODE   = 3,           /**< Switch to reduced capability */
    CM_REACT_EMERGENCY_STOP = 4            /**< Disable inference */
} cm_reaction_t;

/**
 * @brief Violation types
 * @traceability CM-STRUCT-001 §1.6
 */
typedef enum {
    CM_VIOL_NONE            = 0,           /**< No violation */
    CM_VIOL_INPUT_RANGE     = 1,           /**< Input outside envelope */
    CM_VIOL_INPUT_DRIFT     = 2,           /**< Input distribution drift */
    CM_VIOL_ACTIV_RANGE     = 3,           /**< Activation outside bounds */
    CM_VIOL_ACTIV_SAT       = 4,           /**< Activation saturation */
    CM_VIOL_OUTPUT_RANGE    = 5,           /**< Output outside bounds */
    CM_VIOL_OUTPUT_DRIFT    = 6,           /**< Output distribution drift */
    CM_VIOL_FAULT_BUDGET    = 7            /**< Fault budget exceeded */
} cm_violation_t;

/**
 * @brief Reaction map entry
 * @traceability CM-STRUCT-001 §1.7
 */
typedef struct {
    cm_violation_t violation;              /**< Violation type */
    cm_reaction_t  reaction;               /**< Corresponding action */
} cm_reaction_map_entry_t;

/**
 * @brief Output envelope specification
 * @traceability CM-STRUCT-001 §1.3 (output analog)
 */
typedef struct {
    uint32_t output_count;                 /**< Number of outputs */
    int32_t  min_q16[CM_MAX_FEATURES];     /**< Per-output minimum in Q16.16 */
    int32_t  max_q16[CM_MAX_FEATURES];     /**< Per-output maximum in Q16.16 */
    bool     has_hists;                    /**< True if histograms provided */
    cm_hist_spec_t hists[CM_MAX_FEATURES]; /**< Per-output histograms (optional) */
} cm_output_envelope_t;

/**
 * @brief Fault budget specification
 * @traceability CM-ARCH-MATH-001 §6
 */
typedef struct {
    uint32_t overflow_budget;              /**< Max overflow events per window */
    uint32_t underflow_budget;             /**< Max underflow events per window */
    uint32_t saturation_budget;            /**< Max saturation events per window */
    uint32_t clamp_budget;                 /**< Max clamp events per window */
} cm_fault_budget_t;

/**
 * @brief Complete policy structure (COE)
 * @traceability CM-STRUCT-001 §1.8
 */
struct cm_policy_s {
    uint32_t policy_version;               /**< Must be CM_POLICY_VERSION */
    uint32_t window_size;                  /**< N samples per window */
    uint8_t  bundle_root[CT_SHA256_SIZE];  /**< R (attestation root from loader) */
    uint8_t  policy_hash[CT_SHA256_SIZE];  /**< H_P (computed on load) */
    
    cm_input_envelope_t  input;            /**< Input envelope */
    cm_output_envelope_t output;           /**< Output envelope */
    
    cm_layer_contract_t  layers[CM_MAX_LAYERS]; /**< Layer contracts */
    uint32_t             layer_count;      /**< Number of monitored layers */
    
    cm_drift_policy_t    drift;            /**< Drift detection config */
    cm_fault_budget_t    fault_budget;     /**< Fault budget thresholds */
    
    cm_reaction_map_entry_t react_map[CM_MAX_REACTIONS]; /**< Reaction mapping */
    uint32_t             react_map_count;  /**< Number of reaction entries */
};

/** @} */

/*============================================================================
 * Section 2: Ledger Types (CM-STRUCT-001 §2)
 *============================================================================*/

/** @defgroup ledger_types Ledger Types
 *  @{
 */

/**
 * @brief Event type enumeration
 * @traceability CM-STRUCT-001 §4
 */
typedef enum {
    CM_EVENT_GENESIS        = 0x00,        /**< Ledger initialization */
    CM_EVENT_WINDOW_OK      = 0x01,        /**< Window completed, no violations */
    CM_EVENT_VIOL_INPUT     = 0x10,        /**< Input range violation */
    CM_EVENT_VIOL_ACTIV     = 0x11,        /**< Activation range violation */
    CM_EVENT_VIOL_OUTPUT    = 0x12,        /**< Output range violation */
    CM_EVENT_VIOL_DRIFT     = 0x13,        /**< Drift threshold exceeded */
    CM_EVENT_VIOL_FAULT     = 0x14,        /**< Fault budget exceeded */
    CM_EVENT_REACT_WARN     = 0x20,        /**< Warning issued */
    CM_EVENT_REACT_CLAMP    = 0x21,        /**< Output clamped */
    CM_EVENT_REACT_DEGRADE  = 0x22,        /**< Degraded mode entered */
    CM_EVENT_REACT_STOP     = 0x23,        /**< Emergency stop */
    CM_EVENT_CHECKPOINT     = 0x30         /**< Periodic checkpoint */
} cm_event_type_t;

/**
 * @brief Ledger entry header (fixed size)
 * @traceability CM-STRUCT-001 §2.1
 *
 * Size: 8 + 8 + 4 + 4 + 8 + 32 + 32 = 96 bytes
 */
typedef struct {
    uint64_t seq;                          /**< Monotonic sequence number */
    uint64_t window_id;                    /**< Window identifier */
    uint32_t event_type;                   /**< Event type enum */
    uint32_t payload_len;                  /**< Length of payload bytes */
    uint64_t time_tick;                    /**< Monotonic tick (0 if count-window) */
    uint8_t  bundle_root[CT_SHA256_SIZE];  /**< R */
    uint8_t  policy_hash[CT_SHA256_SIZE];  /**< H_P */
} cm_ledger_header_t;

/** Ledger header size for serialization */
#define CM_LEDGER_HEADER_SIZE 96

/**
 * @brief Ledger entry (header + payload)
 * @traceability CM-STRUCT-001 §2.2
 */
typedef struct {
    cm_ledger_header_t hdr;                /**< Fixed header */
    const uint8_t     *payload;            /**< Payload bytes (payload_len) */
} cm_ledger_entry_t;

/**
 * @brief Ledger context (maintains chain state)
 * @traceability CM-STRUCT-001 §2.3
 */
struct cm_ledger_ctx_s {
    uint8_t  L_prev[CT_SHA256_SIZE];       /**< L_{t-1} previous digest */
    uint64_t seq_next;                     /**< Next sequence number */
    uint8_t  bundle_root[CT_SHA256_SIZE];  /**< R (cached) */
    uint8_t  policy_hash[CT_SHA256_SIZE];  /**< H_P (cached) */
    bool     initialized;                  /**< True if genesis computed */
};

/** @} */

/*============================================================================
 * Section 3: Reaction Interface Types (CM-STRUCT-001 §3)
 *============================================================================*/

/** @defgroup react_types Reaction Types
 *  @{
 */

/**
 * @brief Reaction context
 * @traceability CM-STRUCT-001 §3
 */
typedef struct {
    int32_t  out_min_q16;                  /**< Output clamp minimum in Q16.16 */
    int32_t  out_max_q16;                  /**< Output clamp maximum in Q16.16 */
    void    *sys;                          /**< Opaque system control plane handle */
} cm_react_ctx_t;

/** @} */

/*============================================================================
 * Section 7: Window Statistics (CM-STRUCT-001 §7)
 *============================================================================*/

/** @defgroup window_types Window Types
 *  @{
 */

/**
 * @brief Window statistics
 * @traceability CM-STRUCT-001 §7
 */
typedef struct {
    uint64_t window_id;                    /**< Window identifier */
    uint32_t sample_count;                 /**< Samples in window */
    uint32_t bin_counts[CM_MAX_BINS];      /**< Histogram bin counts */
    uint32_t bin_count;                    /**< Number of bins (B) */
} cm_window_stats_t;

/** @} */

/*============================================================================
 * Section 8: Drift Result (CM-STRUCT-001 §8)
 *============================================================================*/

/** @defgroup drift_types Drift Types
 *  @{
 */

/** Drift trigger flags */
#define CM_DRIFT_TV_TRIGGERED   0x01
#define CM_DRIFT_JSD_TRIGGERED  0x02
#define CM_DRIFT_PSI_TRIGGERED  0x04

/**
 * @brief Drift computation result
 * @traceability CM-STRUCT-001 §8
 */
typedef struct {
    uint32_t tv_q0_32;                     /**< Total Variation in Q0.32 */
    int32_t  jsd_q16_16;                   /**< Jensen-Shannon in Q16.16 */
    int32_t  psi_q16_16;                   /**< PSI in Q16.16 */
    uint32_t flags;                        /**< Which detectors triggered */
} cm_drift_result_t;

/** @} */

/*============================================================================
 * Section 9: Monitor State (CM-STRUCT-001 §9)
 *============================================================================*/

/** @defgroup monitor_types Monitor Types
 *  @{
 */

/**
 * @brief Monitor state enumeration
 * @traceability CM-STRUCT-001 §9
 */
typedef enum {
    CM_STATE_UNINIT     = 0,               /**< Not initialized */
    CM_STATE_INIT       = 1,               /**< Initialized, not enabled */
    CM_STATE_ENABLED    = 2,               /**< Active monitoring */
    CM_STATE_ALARM      = 3,               /**< Violation detected */
    CM_STATE_DEGRADED   = 4,               /**< Reduced capability */
    CM_STATE_STOPPED    = 5                /**< Emergency stopped */
} cm_monitor_state_t;

/**
 * @brief Monitor context (main state machine)
 * @traceability CM-STRUCT-001 §9
 */
struct cm_monitor_ctx_s {
    cm_monitor_state_t   state;            /**< Current state */
    cm_policy_t         *policy;           /**< Loaded policy */
    cm_ledger_ctx_t      ledger;           /**< Ledger context */
    
    cm_window_stats_t   *input_stats;      /**< Per-feature input stats */
    cm_window_stats_t   *output_stats;     /**< Per-output stats */
    
    uint64_t             total_samples;    /**< Total samples processed */
    uint64_t             total_violations; /**< Total violations detected */
    uint64_t             current_window_id;/**< Current window ID */
    uint32_t             window_sample_count; /**< Samples in current window */
    
    ct_fault_flags_t     accumulated_faults; /**< Accumulated fault flags */
    
    /** Per-window fault counters */
    uint32_t overflow_count;
    uint32_t underflow_count;
    uint32_t saturation_count;
    uint32_t clamp_count;
};

/** @} */

/*============================================================================
 * Section 10: Payload Structures
 *============================================================================*/

/** @defgroup payload_types Payload Types
 *  @{
 */

/**
 * @brief Violation payload for ledger entries
 */
typedef struct {
    cm_violation_t violation;              /**< Violation type */
    uint32_t       feature_or_layer_id;    /**< Which feature/layer */
    int32_t        observed_q16;           /**< Observed value */
    int32_t        bound_q16;              /**< Violated bound */
} cm_viol_payload_t;

/**
 * @brief Drift payload for ledger entries
 */
typedef struct {
    uint32_t feature_id;                   /**< Which feature */
    uint32_t tv_q0_32;                     /**< Computed TV */
    int32_t  jsd_q16_16;                   /**< Computed JSD */
    int32_t  psi_q16_16;                   /**< Computed PSI */
    uint32_t triggered_flags;              /**< Which thresholds exceeded */
} cm_drift_payload_t;

/**
 * @brief Reaction payload for ledger entries
 */
typedef struct {
    cm_violation_t violation;              /**< Triggering violation */
    cm_reaction_t  action;                 /**< Action taken */
} cm_react_payload_t;

/**
 * @brief Window OK payload for ledger entries
 */
typedef struct {
    uint64_t window_id;                    /**< Window completed */
    uint32_t sample_count;                 /**< Samples in window */
    uint32_t max_tv_q0_32;                 /**< Max TV observed */
} cm_window_ok_payload_t;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CM_TYPES_H */
