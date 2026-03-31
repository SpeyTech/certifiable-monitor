/**
 * @file detectors.c
 * @brief Drift Detection Implementation (TV, JSD, PSI)
 * @traceability CM-MATH-001 §2-4
 *
 * @details
 * Implements deterministic drift detection using fixed-point arithmetic:
 * - Total Variation Distance (TV) - no logarithms, safest detector
 * - Jensen-Shannon Divergence (JSD) - uses LUT log2
 * - Population Stability Index (PSI) - uses LUT log2 with epsilon smoothing
 *
 * All operations are integer-only and produce bit-identical results
 * across x86, ARM, and RISC-V platforms.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "cm_detect.h"
#include "dvm.h"
#include "cm_types.h"
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * Histogram Operations
 *============================================================================*/

/**
 * @brief Assign a value to a histogram bin
 * @traceability CM-MATH-001 §1.1
 *
 * Bin assignment:
 * - x in [e_{b-1}, e_b) -> bin b-1 (0-indexed)
 * - x in [e_{B-1}, e_B] -> bin B-1 (last bin includes right edge)
 */
int32_t cm_hist_assign_bin(int32_t value, const cm_hist_spec_t *spec)
{
    if (!spec || spec->bin_count == 0) {
        return -1;
    }
    
    uint32_t B = spec->bin_count;
    
    /* Check below first edge */
    if (value < spec->edges_q16[0]) {
        return -1;  /* Below histogram range */
    }
    
    /* Check above last edge */
    if (value > spec->edges_q16[B]) {
        return -1;  /* Above histogram range */
    }
    
    /* Binary search for correct bin */
    /* bins are [edge[i], edge[i+1]) except last bin is [edge[B-1], edge[B]] */
    for (uint32_t b = 0; b < B; b++) {
        int32_t left = spec->edges_q16[b];
        int32_t right = spec->edges_q16[b + 1];
        
        if (b == B - 1) {
            /* Last bin includes right edge */
            if (value >= left && value <= right) {
                return (int32_t)b;
            }
        } else {
            /* Normal bin: [left, right) */
            if (value >= left && value < right) {
                return (int32_t)b;
            }
        }
    }
    
    return -1;  /* Should not reach here */
}

/**
 * @brief Increment histogram bin count for a value
 * @traceability CM-MATH-001 §1.2
 */
ct_result_t cm_hist_accumulate(int32_t value, const cm_hist_spec_t *spec,
                               uint32_t *counts)
{
    if (!spec || !counts) {
        return CT_ERR_NULL;
    }
    
    int32_t bin = cm_hist_assign_bin(value, spec);
    if (bin < 0) {
        return CT_ERR_RANGE;  /* Value outside histogram */
    }
    
    counts[bin]++;
    return CT_OK;
}

/**
 * @brief Compute total count from histogram
 */
uint64_t cm_hist_total(const uint32_t *counts, uint32_t bin_count)
{
    if (!counts) return 0;
    
    uint64_t total = 0;
    for (uint32_t b = 0; b < bin_count; b++) {
        total += counts[b];
    }
    return total;
}

/*============================================================================
 * Probability Normalization
 *============================================================================*/

/**
 * @brief Normalize counts to Q0.32 probabilities
 * @traceability CM-MATH-001 §1.3
 */
ct_result_t cm_hist_normalize(const uint32_t *counts, uint32_t bin_count,
                              uint64_t total, uint32_t *probs,
                              ct_fault_flags_t *faults)
{
    if (!counts || !probs) {
        return CT_ERR_NULL;
    }
    if (total == 0) {
        if (faults) faults->div_zero = 1;
        return CT_ERR_DIV_ZERO;
    }
    
    /* p_b = floor((counts[b] * 2^32) / total) */
    /* Special case: when counts[b] == total, result is 2^32 which overflows */
    /* In that case, use UINT32_MAX (0xFFFFFFFF) as closest representable value */
    for (uint32_t b = 0; b < bin_count; b++) {
        if (counts[b] == total) {
            /* All mass in this bin - use max representable value */
            probs[b] = UINT32_MAX;
        } else if (counts[b] == 0) {
            probs[b] = 0;
        } else {
            uint64_t num = (uint64_t)counts[b] << 32;
            uint64_t result = num / total;
            /* Cap at UINT32_MAX to prevent overflow on edge cases */
            probs[b] = (result > UINT32_MAX) ? UINT32_MAX : (uint32_t)result;
        }
    }
    
    return CT_OK;
}

/*============================================================================
 * Total Variation Distance
 *============================================================================*/

/**
 * @brief Compute Total Variation distance
 * @traceability CM-MATH-001 §2
 *
 * TV(p,q) = (1/2) * sum_b |p_b - q_b|
 *
 * Since p_b, q_b are in Q0.32, we compute:
 * S = sum_b |p_b - q_b|  (may exceed 32 bits, use 64-bit accumulator)
 * TV = S / 2 (right shift by 1)
 *
 * Result is in Q0.32: 0 = no drift, UINT32_MAX = maximum drift
 */
uint32_t cm_detect_tv(const uint32_t *p, const uint32_t *q,
                      uint32_t bin_count, ct_fault_flags_t *faults)
{
    (void)faults;  /* TV cannot overflow with valid probability inputs */
    
    if (!p || !q || bin_count == 0) {
        return 0;
    }
    
    uint64_t sum = 0;
    
    for (uint32_t b = 0; b < bin_count; b++) {
        /* Compute |p_b - q_b| using signed arithmetic */
        int64_t diff = (int64_t)p[b] - (int64_t)q[b];
        if (diff < 0) diff = -diff;
        sum += (uint64_t)diff;
    }
    
    /* TV = S / 2 */
    /* Maximum sum can be 2 * 2^32 for completely disjoint distributions */
    /* After dividing by 2, result can be 2^32 which overflows uint32_t */
    /* Cap at UINT32_MAX to represent maximum TV */
    uint64_t tv64 = sum >> 1;
    uint32_t tv = (tv64 > UINT32_MAX) ? UINT32_MAX : (uint32_t)tv64;
    
    return tv;
}

/*============================================================================
 * Jensen-Shannon Divergence
 *============================================================================*/

/**
 * @brief Compute Jensen-Shannon Divergence
 * @traceability CM-MATH-001 §3
 *
 * JSD(p,q) = (1/2) * KL(p||m) + (1/2) * KL(q||m)
 * where m = (p + q) / 2
 *
 * KL(p||m) = sum_b p_b * log2(p_b / m_b)
 *
 * Algorithm:
 * 1. For each bin, compute m_b = (p_b + q_b) / 2
 * 2. Compute r_p = p_b / m_b in Q16.16
 * 3. Compute log2(r_p) using LUT
 * 4. Multiply p_b * log2(r_p), accumulate
 * 5. Repeat for q||m
 * 6. Scale by 1/2
 *
 * Zero handling: if p_b = 0, contribution is 0 (by continuity)
 *                if m_b = 0, then p_b = q_b = 0, so contribution is 0
 */
int32_t cm_detect_jsd(const uint32_t *p, const uint32_t *q,
                      uint32_t bin_count, ct_fault_flags_t *faults)
{
    if (!p || !q || bin_count == 0) {
        return 0;
    }
    
    int64_t kl_p_m = 0;  /* KL(p||m) accumulator in Q16.48 */
    int64_t kl_q_m = 0;  /* KL(q||m) accumulator in Q16.48 */
    
    for (uint32_t b = 0; b < bin_count; b++) {
        uint32_t p_b = p[b];
        uint32_t q_b = q[b];
        
        /* m_b = (p_b + q_b) / 2 */
        uint32_t m_b = (p_b >> 1) + (q_b >> 1);
        /* Handle odd case */
        if ((p_b & 1) && (q_b & 1)) m_b++;
        else if ((p_b & 1) || (q_b & 1)) m_b += (p_b + q_b) & 1 ? 1 : 0;
        
        /* Skip if m_b = 0 (implies p_b = q_b = 0) */
        if (m_b == 0) continue;
        
        /* KL(p||m) contribution: p_b * log2(p_b / m_b) */
        if (p_b > 0) {
            /* r_p = p_b / m_b in Q16.16 */
            /* p_b and m_b are in Q0.32, so p_b/m_b is dimensionless */
            /* We want log2(p_b/m_b) */
            /* Scale: convert p_b from Q0.32 to Q16.16 by shifting */
            /* p_b in Q0.32 means p_b / 2^32 is the actual probability */
            /* r_p = p_b / m_b (both in Q0.32, ratio is unitless) */
            /* For cm_log2_q16, input is Q16.16 where 65536 = 1.0 */
            /* So r_p * 65536 = (p_b / m_b) * 65536 = (p_b * 65536) / m_b */
            
            uint64_t r_scaled = ((uint64_t)p_b << 16) / m_b;
            if (r_scaled > UINT32_MAX) r_scaled = UINT32_MAX;
            
            int32_t log_r = cm_log2_q16((uint32_t)r_scaled, faults);
            
            /* Contribution: p_b * log_r */
            /* p_b is Q0.32, log_r is Q16.16 */
            /* Product is Q16.48 */
            /* But we need to scale p_b to be meaningful */
            /* p_b / 2^32 is actual prob, so (p_b * log_r) / 2^32 is contribution */
            /* Accumulate in higher precision, scale at end */
            int64_t contrib = ((int64_t)p_b * log_r) >> 16;
            kl_p_m += contrib;
        }
        
        /* KL(q||m) contribution: q_b * log2(q_b / m_b) */
        if (q_b > 0) {
            uint64_t r_scaled = ((uint64_t)q_b << 16) / m_b;
            if (r_scaled > UINT32_MAX) r_scaled = UINT32_MAX;
            
            int32_t log_r = cm_log2_q16((uint32_t)r_scaled, faults);
            
            int64_t contrib = ((int64_t)q_b * log_r) >> 16;
            kl_q_m += contrib;
        }
    }
    
    /* JSD = (KL(p||m) + KL(q||m)) / 2 */
    /* kl_p_m and kl_q_m are in Q0.32 (scaled probability * Q16.16 log >> 16) */
    /* Actually let's reconsider the scaling... */
    /* p_b is Q0.32, log_r is Q16.16 */
    /* p_b * log_r is Q16.48, then >>16 gives Q0.32 */
    /* sum over all bins gives Q0.32 + some integer bits for sum */
    /* JSD theoretical max is 1.0 (in nats) or log2(2) = 1.0 in bits */
    /* So we should have JSD in [0, 1], output in Q16.16 means [0, 65536] */
    
    int64_t jsd_sum = kl_p_m + kl_q_m;
    
    /* Divide by 2 */
    jsd_sum >>= 1;
    
    /* Scale from Q0.32 to Q16.16: >> 16 */
    int32_t jsd = (int32_t)(jsd_sum >> 16);
    
    /* Clamp to valid range [0, 65536] */
    if (jsd < 0) jsd = 0;
    if (jsd > CT_Q16_ONE) jsd = CT_Q16_ONE;
    
    return jsd;
}

/*============================================================================
 * Population Stability Index
 *============================================================================*/

/**
 * @brief Compute Population Stability Index
 * @traceability CM-MATH-001 §4
 *
 * PSI(p,q) = sum_b (p_b - q_b) * ln(p_b / q_b)
 *
 * Uses epsilon smoothing: p_b = max(p_b, epsilon)
 * Uses ln(x) = log2(x) * ln(2)
 */
int32_t cm_detect_psi(const uint32_t *p, const uint32_t *q,
                      uint32_t bin_count, uint32_t epsilon,
                      ct_fault_flags_t *faults)
{
    if (!p || !q || bin_count == 0) {
        return 0;
    }
    
    int64_t psi_sum = 0;  /* Accumulator */
    
    for (uint32_t b = 0; b < bin_count; b++) {
        /* Apply epsilon smoothing */
        uint32_t p_b = p[b];
        uint32_t q_b = q[b];
        
        if (p_b < epsilon) p_b = epsilon;
        if (q_b < epsilon) q_b = epsilon;
        
        /* d_b = p_b - q_b (signed) */
        int64_t d_b = (int64_t)p_b - (int64_t)q_b;
        
        /* log_ratio = ln(p_b / q_b) = log2(p_b/q_b) * ln(2) */
        /* Compute p_b / q_b in Q16.16 */
        uint64_t r_scaled = ((uint64_t)p_b << 16) / q_b;
        if (r_scaled > UINT32_MAX) r_scaled = UINT32_MAX;
        if (r_scaled == 0) r_scaled = 1;  /* Avoid log(0) */
        
        int32_t log2_r = cm_log2_q16((uint32_t)r_scaled, faults);
        
        /* ln(r) = log2(r) * ln(2), ln(2) = 45426 in Q16.16 */
        int64_t ln_r = ((int64_t)log2_r * CM_LN2_Q16) >> CT_Q16_SHIFT;
        
        /* Contribution: d_b * ln_r */
        /* d_b is Q0.32, ln_r is Q16.16 */
        /* Product in Q16.48, scale to Q16.16 by >> 32 */
        int64_t contrib = (d_b * ln_r) >> 32;
        
        psi_sum += contrib;
    }
    
    /* PSI is unbounded in principle, but clamp to int32 range */
    if (psi_sum > INT32_MAX) {
        if (faults) faults->overflow = 1;
        return INT32_MAX;
    }
    if (psi_sum < INT32_MIN) {
        if (faults) faults->underflow = 1;
        return INT32_MIN;
    }
    
    return (int32_t)psi_sum;
}

/*============================================================================
 * Combined Drift Evaluation
 *============================================================================*/

/**
 * @brief Evaluate all enabled drift detectors
 * @traceability CM-MATH-001 §5
 */
ct_result_t cm_detect_drift(const uint32_t *runtime_counts,
                            const uint32_t *ref_counts,
                            uint32_t bin_count,
                            const cm_drift_policy_t *policy,
                            cm_drift_result_t *result,
                            ct_fault_flags_t *faults)
{
    if (!runtime_counts || !ref_counts || !policy || !result) {
        return CT_ERR_NULL;
    }
    if (bin_count == 0 || bin_count > CM_MAX_BINS) {
        return CT_ERR_SIZE;
    }
    
    /* Initialize result */
    result->tv_q0_32 = 0;
    result->jsd_q16_16 = 0;
    result->psi_q16_16 = 0;
    result->flags = 0;
    
    /* Compute totals */
    uint64_t runtime_total = cm_hist_total(runtime_counts, bin_count);
    uint64_t ref_total = cm_hist_total(ref_counts, bin_count);
    
    if (runtime_total == 0 || ref_total == 0) {
        if (faults) faults->domain = 1;
        return CT_ERR_DOMAIN;
    }
    
    /* Normalize to probabilities */
    uint32_t p[CM_MAX_BINS];
    uint32_t q[CM_MAX_BINS];
    
    ct_result_t rc = cm_hist_normalize(runtime_counts, bin_count, runtime_total, p, faults);
    if (rc != CT_OK) return rc;
    
    rc = cm_hist_normalize(ref_counts, bin_count, ref_total, q, faults);
    if (rc != CT_OK) return rc;
    
    /* Compute enabled detectors */
    if (policy->enabled_detectors & CM_DRIFT_TV_ENABLED) {
        result->tv_q0_32 = cm_detect_tv(p, q, bin_count, faults);
        
        if (result->tv_q0_32 > policy->tv_threshold_q0_32) {
            result->flags |= CM_DRIFT_TV_TRIGGERED;
        }
    }
    
    if (policy->enabled_detectors & CM_DRIFT_JSD_ENABLED) {
        result->jsd_q16_16 = cm_detect_jsd(p, q, bin_count, faults);
        
        if (result->jsd_q16_16 > policy->jsd_threshold_q16_16) {
            result->flags |= CM_DRIFT_JSD_TRIGGERED;
        }
    }
    
    if (policy->enabled_detectors & CM_DRIFT_PSI_ENABLED) {
        result->psi_q16_16 = cm_detect_psi(p, q, bin_count, policy->epsilon_q0_32, faults);
        
        if (result->psi_q16_16 > policy->psi_threshold_q16_16) {
            result->flags |= CM_DRIFT_PSI_TRIGGERED;
        }
    }
    
    return CT_OK;
}

/*============================================================================
 * Range Checking
 *============================================================================*/

/**
 * @brief Check if value is within bounds
 * @traceability CM-MATH-001 §3.1
 */
bool cm_in_range(int32_t value, int32_t min_q16, int32_t max_q16)
{
    return (value >= min_q16) && (value <= max_q16);
}

/**
 * @brief Count range violations in a vector
 * @traceability CM-ARCH-MATH-001 §3.1
 */
uint32_t cm_count_violations(const int32_t *values, uint32_t count,
                             int32_t min_q16, int32_t max_q16)
{
    if (!values) return 0;
    
    uint32_t violations = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (values[i] < min_q16 || values[i] > max_q16) {
            violations++;
        }
    }
    return violations;
}

/**
 * @brief Compute maximum over-range magnitude
 * @traceability CM-ARCH-MATH-001 §4
 */
int32_t cm_max_overrange(const int32_t *values, uint32_t count,
                         int32_t min_q16, int32_t max_q16,
                         ct_fault_flags_t *faults)
{
    if (!values) return 0;
    
    int32_t max_over = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        int32_t val = values[i];
        int32_t over = 0;
        
        if (val < min_q16) {
            /* Below minimum */
            over = dvm_abs(min_q16 - val, faults);
        } else if (val > max_q16) {
            /* Above maximum */
            over = dvm_abs(val - max_q16, faults);
        }
        
        if (over > max_over) {
            max_over = over;
        }
    }
    
    return max_over;
}
