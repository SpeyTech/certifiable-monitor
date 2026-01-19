/**
 * @file cm_detect.h
 * @brief Drift Detection API (TV, JSD, PSI)
 * @traceability CM-MATH-001 §2-4
 *
 * @details
 * Provides deterministic drift detection using:
 * - Total Variation Distance (TV)
 * - Jensen-Shannon Divergence (JSD)
 * - Population Stability Index (PSI)
 *
 * All computations use fixed-point arithmetic with LUT-based log2.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#ifndef CM_DETECT_H
#define CM_DETECT_H

#include "ct_types.h"
#include "cm_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Histogram Operations
 *============================================================================*/

/**
 * @brief Assign a value to a histogram bin
 * @param value Input value in Q16.16
 * @param spec Histogram specification with bin edges
 * @return Bin index (0 to bin_count-1), or -1 if outside all bins
 * @traceability CM-MATH-001 §1.1
 */
int32_t cm_hist_assign_bin(int32_t value, const cm_hist_spec_t *spec);

/**
 * @brief Increment histogram bin count for a value
 * @param value Input value in Q16.16
 * @param spec Histogram specification
 * @param counts Current bin counts (modified)
 * @return CT_OK on success, error code otherwise
 * @traceability CM-MATH-001 §1.2
 */
ct_result_t cm_hist_accumulate(int32_t value, const cm_hist_spec_t *spec,
                               uint32_t *counts);

/**
 * @brief Compute total count from histogram
 * @param counts Bin counts
 * @param bin_count Number of bins
 * @return Sum of all counts
 */
uint64_t cm_hist_total(const uint32_t *counts, uint32_t bin_count);

/*============================================================================
 * Probability Normalization
 *============================================================================*/

/**
 * @brief Normalize counts to Q0.32 probabilities
 * @param counts Input bin counts
 * @param bin_count Number of bins
 * @param total Total count (sum of counts)
 * @param probs Output probabilities in Q0.32 (caller provides array)
 * @param faults Fault flags
 * @return CT_OK on success
 * @traceability CM-MATH-001 §1.3
 *
 * Each p_b = floor((counts[b] * 2^32) / total)
 */
ct_result_t cm_hist_normalize(const uint32_t *counts, uint32_t bin_count,
                              uint64_t total, uint32_t *probs,
                              ct_fault_flags_t *faults);

/*============================================================================
 * Total Variation Distance
 *============================================================================*/

/**
 * @brief Compute Total Variation distance between two distributions
 * @param p Runtime distribution in Q0.32
 * @param q Reference distribution in Q0.32
 * @param bin_count Number of bins
 * @param faults Fault flags
 * @return TV(p,q) in Q0.32, range [0, 2^32] representing [0, 1]
 * @traceability CM-MATH-001 §2
 *
 * TV(p,q) = (1/2) * sum_b |p_b - q_b|
 */
uint32_t cm_detect_tv(const uint32_t *p, const uint32_t *q,
                      uint32_t bin_count, ct_fault_flags_t *faults);

/*============================================================================
 * Jensen-Shannon Divergence
 *============================================================================*/

/**
 * @brief Compute Jensen-Shannon Divergence between two distributions
 * @param p Runtime distribution in Q0.32
 * @param q Reference distribution in Q0.32
 * @param bin_count Number of bins
 * @param faults Fault flags
 * @return JSD(p,q) in Q16.16, range [0, 65536] representing [0, 1]
 * @traceability CM-MATH-001 §3
 *
 * JSD(p,q) = (1/2) * KL(p||m) + (1/2) * KL(q||m)
 * where m = (p + q) / 2
 *
 * Uses LUT-based log2 for deterministic computation.
 */
int32_t cm_detect_jsd(const uint32_t *p, const uint32_t *q,
                      uint32_t bin_count, ct_fault_flags_t *faults);

/*============================================================================
 * Population Stability Index
 *============================================================================*/

/**
 * @brief Compute Population Stability Index between two distributions
 * @param p Runtime distribution in Q0.32
 * @param q Reference distribution in Q0.32
 * @param bin_count Number of bins
 * @param epsilon Smoothing constant in Q0.32 (to avoid log(0))
 * @param faults Fault flags
 * @return PSI(p,q) in Q16.16
 * @traceability CM-MATH-001 §4
 *
 * PSI(p,q) = sum_b (p_b - q_b) * ln(p_b / q_b)
 *
 * Uses epsilon smoothing: p_b = max(p_b, epsilon)
 */
int32_t cm_detect_psi(const uint32_t *p, const uint32_t *q,
                      uint32_t bin_count, uint32_t epsilon,
                      ct_fault_flags_t *faults);

/*============================================================================
 * Combined Drift Evaluation
 *============================================================================*/

/**
 * @brief Evaluate all enabled drift detectors
 * @param runtime_counts Runtime window bin counts
 * @param ref_counts Reference (calibration) bin counts
 * @param bin_count Number of bins
 * @param policy Drift policy with thresholds and enabled flags
 * @param result Output drift result
 * @param faults Fault flags
 * @return CT_OK on success
 * @traceability CM-MATH-001 §5
 */
ct_result_t cm_detect_drift(const uint32_t *runtime_counts,
                            const uint32_t *ref_counts,
                            uint32_t bin_count,
                            const cm_drift_policy_t *policy,
                            cm_drift_result_t *result,
                            ct_fault_flags_t *faults);

/*============================================================================
 * Range Checking
 *============================================================================*/

/**
 * @brief Check if value is within bounds
 * @param value Value to check in Q16.16
 * @param min_q16 Minimum bound in Q16.16
 * @param max_q16 Maximum bound in Q16.16
 * @return true if min <= value <= max
 * @traceability CM-MATH-001 §3.1
 */
bool cm_in_range(int32_t value, int32_t min_q16, int32_t max_q16);

/**
 * @brief Count range violations in a vector
 * @param values Input values in Q16.16
 * @param count Number of values
 * @param min_q16 Minimum bound
 * @param max_q16 Maximum bound
 * @return Number of violations
 * @traceability CM-ARCH-MATH-001 §3.1
 */
uint32_t cm_count_violations(const int32_t *values, uint32_t count,
                             int32_t min_q16, int32_t max_q16);

/**
 * @brief Compute maximum over-range magnitude
 * @param values Input values in Q16.16
 * @param count Number of values
 * @param min_q16 Minimum bound
 * @param max_q16 Maximum bound
 * @param faults Fault flags
 * @return Maximum distance outside bounds in Q16.16
 * @traceability CM-ARCH-MATH-001 §4
 */
int32_t cm_max_overrange(const int32_t *values, uint32_t count,
                         int32_t min_q16, int32_t max_q16,
                         ct_fault_flags_t *faults);

#ifdef __cplusplus
}
#endif

#endif /* CM_DETECT_H */
