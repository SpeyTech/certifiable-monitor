/**
 * @file primitives.c
 * @brief Deterministic Virtual Machine (DVM) Primitive Operations
 * @traceability CM-MATH-001 (All sections)
 *
 * @details
 * Implements the core DVM operations:
 * - Saturating arithmetic (add, sub, mul, clamp)
 * - Round-to-nearest-even shift
 * - Fixed-point division
 * - LUT-based log2 for JSD/PSI computations
 *
 * All operations are integer-only and produce bit-identical results
 * across x86, ARM, and RISC-V platforms. No FPU instructions are used.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "dvm.h"
#include "cm_types.h"
#include <stdint.h>
#include <limits.h>

/*============================================================================
 * LUT Constants (CM-MATH-001 Appendix A)
 *============================================================================*/

/**
 * @brief Log2 Look-Up Table for x in [1.0, 2.0)
 * @traceability CM-MATH-001 §3 (LUT specification)
 *
 * Size: 512 entries (2^9)
 * Format: uint16_t storing fractional part of log2(1 + i/512) in Q0.16
 * Generated via: floor(log2(1.0 + i/512.0) * 65536)
 * 
 * Entry i represents log2(1.0 + i/512.0) for i in [0, 511]
 * The fractional part is in [0, 1) since log2(1.x) is in [0, 1)
 */
static const uint16_t LOG2_LUT[CM_LUT_SIZE] = {
        0,   184,   368,   552,   735,   918,  1101,  1283,
     1465,  1647,  1828,  2009,  2190,  2370,  2550,  2730,
     2909,  3088,  3266,  3445,  3622,  3800,  3977,  4154,
     4331,  4507,  4683,  4858,  5034,  5209,  5383,  5558,
     5731,  5905,  6078,  6251,  6424,  6597,  6769,  6940,
     7112,  7283,  7454,  7624,  7794,  7964,  8134,  8303,
     8472,  8641,  8809,  8977,  9145,  9313,  9480,  9647,
     9813,  9980, 10146, 10311, 10477, 10642, 10807, 10971,
    11136, 11300, 11463, 11627, 11790, 11953, 12115, 12278,
    12440, 12602, 12763, 12924, 13085, 13246, 13406, 13566,
    13726, 13886, 14045, 14204, 14363, 14521, 14680, 14838,
    14995, 15153, 15310, 15467, 15624, 15780, 15936, 16092,
    16248, 16403, 16558, 16713, 16868, 17022, 17176, 17330,
    17484, 17637, 17790, 17943, 18096, 18248, 18400, 18552,
    18704, 18855, 19006, 19157, 19308, 19458, 19608, 19758,
    19908, 20058, 20207, 20356, 20505, 20653, 20801, 20950,
    21097, 21245, 21392, 21540, 21686, 21833, 21980, 22126,
    22272, 22418, 22563, 22709, 22854, 22999, 23143, 23288,
    23432, 23576, 23720, 23863, 24007, 24150, 24293, 24436,
    24578, 24720, 24862, 25004, 25146, 25287, 25429, 25570,
    25710, 25851, 25991, 26132, 26272, 26411, 26551, 26690,
    26829, 26968, 27107, 27246, 27384, 27522, 27660, 27798,
    27935, 28073, 28210, 28347, 28483, 28620, 28756, 28892,
    29028, 29164, 29300, 29435, 29570, 29705, 29840, 29974,
    30109, 30243, 30377, 30511, 30644, 30778, 30911, 31044,
    31177, 31310, 31442, 31575, 31707, 31839, 31971, 32102,
    32234, 32365, 32496, 32627, 32757, 32888, 33018, 33148,
    33278, 33408, 33538, 33667, 33796, 33925, 34054, 34183,
    34312, 34440, 34568, 34696, 34824, 34952, 35079, 35207,
    35334, 35461, 35588, 35714, 35841, 35967, 36093, 36219,
    36345, 36471, 36596, 36721, 36847, 36972, 37096, 37221,
    37346, 37470, 37594, 37718, 37842, 37966, 38089, 38212,
    38336, 38459, 38582, 38704, 38827, 38949, 39071, 39193,
    39315, 39437, 39559, 39680, 39801, 39923, 40044, 40164,
    40285, 40406, 40526, 40646, 40766, 40886, 41006, 41126,
    41245, 41364, 41483, 41602, 41721, 41840, 41959, 42077,
    42195, 42313, 42431, 42549, 42667, 42784, 42902, 43019,
    43136, 43253, 43370, 43486, 43603, 43719, 43836, 43952,
    44068, 44183, 44299, 44415, 44530, 44645, 44760, 44875,
    44990, 45105, 45219, 45334, 45448, 45562, 45676, 45790,
    45904, 46017, 46131, 46244, 46357, 46470, 46583, 46696,
    46808, 46921, 47033, 47145, 47257, 47369, 47481, 47593,
    47704, 47816, 47927, 48038, 48149, 48260, 48371, 48482,
    48592, 48703, 48813, 48923, 49033, 49143, 49253, 49362,
    49472, 49581, 49690, 49800, 49909, 50017, 50126, 50235,
    50343, 50452, 50560, 50668, 50776, 50884, 50992, 51099,
    51207, 51314, 51421, 51528, 51635, 51742, 51849, 51956,
    52062, 52169, 52275, 52381, 52487, 52593, 52699, 52805,
    52910, 53016, 53121, 53226, 53331, 53436, 53541, 53646,
    53751, 53855, 53960, 54064, 54168, 54272, 54376, 54480,
    54584, 54687, 54791, 54894, 54998, 55101, 55204, 55307,
    55410, 55512, 55615, 55717, 55820, 55922, 56024, 56126,
    56228, 56330, 56432, 56533, 56635, 56736, 56837, 56939,
    57040, 57141, 57242, 57342, 57443, 57543, 57644, 57744,
    57844, 57944, 58044, 58144, 58244, 58344, 58443, 58543,
    58642, 58742, 58841, 58940, 59039, 59138, 59236, 59335,
    59433, 59532, 59630, 59728, 59827, 59925, 60023, 60120,
    60218, 60316, 60413, 60511, 60608, 60705, 60802, 60899,
    60996, 61093, 61190, 61286, 61383, 61479, 61576, 61672,
    61768, 61864, 61960, 62056, 62152, 62247, 62343, 62438,
    62534, 62629, 62724, 62819, 62914, 63009, 63104, 63199,
    63293, 63388, 63482, 63576, 63671, 63765, 63859, 63953,
    64047, 64140, 64234, 64327, 64421, 64514, 64608, 64701,
    64794, 64887, 64980, 65073, 65165, 65258, 65351, 65443
};

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Count leading zeros (portable implementation)
 * @traceability CM-MATH-001 §3
 */
uint32_t cm_clz32(uint32_t x)
{
    uint32_t n = 0;
    if (x == 0) return 32;
    if ((x & 0xFFFF0000U) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF000000U) == 0) { n += 8;  x <<= 8;  }
    if ((x & 0xF0000000U) == 0) { n += 4;  x <<= 4;  }
    if ((x & 0xC0000000U) == 0) { n += 2;  x <<= 2;  }
    if ((x & 0x80000000U) == 0) { n += 1; }
    return n;
}

/*============================================================================
 * Basic Arithmetic with Saturation
 *============================================================================*/

/**
 * @brief 64-bit addition with saturation
 * @traceability CM-MATH-001 §DVM-ADD64
 */
int64_t dvm_add64(int64_t a, int64_t b, ct_fault_flags_t *faults)
{
    /* Check for overflow without triggering UB */
    /* Overflow: a > 0, b > 0, a > INT64_MAX - b */
    if (a > 0 && b > 0 && a > INT64_MAX - b) {
        if (faults) faults->overflow = 1;
        return INT64_MAX;
    }
    /* Underflow: a < 0, b < 0, a < INT64_MIN - b */
    if (a < 0 && b < 0 && a < INT64_MIN - b) {
        if (faults) faults->underflow = 1;
        return INT64_MIN;
    }
    
    return a + b;
}

/**
 * @brief 64-bit subtraction with saturation
 * @traceability CM-MATH-001 §DVM-SUB64
 */
int64_t dvm_sub64(int64_t a, int64_t b, ct_fault_flags_t *faults)
{
    /* Overflow: a > 0, b < 0, a > INT64_MAX + b (equivalently a - b > INT64_MAX) */
    if (a > 0 && b < 0 && a > INT64_MAX + b) {
        if (faults) faults->overflow = 1;
        return INT64_MAX;
    }
    /* Underflow: a < 0, b > 0, a < INT64_MIN + b (equivalently a - b < INT64_MIN) */
    if (a < 0 && b > 0 && a < INT64_MIN + b) {
        if (faults) faults->underflow = 1;
        return INT64_MIN;
    }
    
    return a - b;
}

/**
 * @brief 32×32→64 multiply
 * @traceability CM-MATH-001 §DVM-MUL64
 */
int64_t dvm_mul64(int32_t a, int32_t b)
{
    /* No overflow possible: int32 * int32 always fits in int64 */
    return (int64_t)a * (int64_t)b;
}

/**
 * @brief Clamp 64-bit to 32-bit signed range
 * @traceability CM-MATH-001 §DVM-CLAMP32
 */
int32_t dvm_clamp32(int64_t x, ct_fault_flags_t *faults)
{
    if (x > INT32_MAX) {
        if (faults) faults->overflow = 1;
        return INT32_MAX;
    }
    if (x < INT32_MIN) {
        if (faults) faults->underflow = 1;
        return INT32_MIN;
    }
    return (int32_t)x;
}

/**
 * @brief Clamp 64-bit to unsigned 32-bit range
 * @traceability CM-MATH-001 §DVM-CLAMP32U
 */
uint32_t dvm_clamp32u(int64_t x, ct_fault_flags_t *faults)
{
    if (x > (int64_t)UINT32_MAX) {
        if (faults) faults->overflow = 1;
        return UINT32_MAX;
    }
    if (x < 0) {
        if (faults) faults->underflow = 1;
        return 0;
    }
    return (uint32_t)x;
}

/**
 * @brief Absolute value with saturation
 * @traceability CM-MATH-001 §DVM-ABS
 */
int32_t dvm_abs(int32_t x, ct_fault_flags_t *faults)
{
    if (x == INT32_MIN) {
        /* |INT32_MIN| > INT32_MAX, must saturate */
        if (faults) faults->overflow = 1;
        return INT32_MAX;
    }
    return (x < 0) ? -x : x;
}

/*============================================================================
 * Round-to-Nearest-Even Shift
 *============================================================================*/

/**
 * @brief Arithmetic right shift with round-to-nearest-even
 * @traceability CM-MATH-001 §DVM-ROUND-SHIFT-RNE
 */
int32_t dvm_round_shift_rne(int64_t x, uint32_t shift, ct_fault_flags_t *faults)
{
    if (shift > 62) {
        if (faults) faults->domain = 1;
        return 0;
    }
    if (shift == 0) {
        return dvm_clamp32(x, faults);
    }
    
    int64_t half = (int64_t)1 << (shift - 1);
    int64_t mask = ((int64_t)1 << shift) - 1;
    int64_t frac = x & mask;
    int64_t quot = x >> shift;  /* Arithmetic shift for signed */
    
    int64_t result;
    if (frac < half) {
        /* Round down */
        result = quot;
    } else if (frac > half) {
        /* Round up */
        result = quot + 1;
    } else {
        /* Exactly halfway: round to even */
        result = quot + (quot & 1);
    }
    
    return dvm_clamp32(result, faults);
}

/*============================================================================
 * Fixed-Point Division
 *============================================================================*/

/**
 * @brief Q16.16 fixed-point division
 * @traceability CM-MATH-001 §DVM-DIV-Q
 */
int32_t dvm_div_q16(int32_t num, int32_t den, ct_fault_flags_t *faults)
{
    if (den == 0) {
        if (faults) faults->div_zero = 1;
        return (num >= 0) ? INT32_MAX : INT32_MIN;
    }
    
    /* Widen numerator and shift left by 16 for Q16.16 */
    int64_t wide_num = (int64_t)num << CT_Q16_SHIFT;
    
    /* Add half denominator for rounding */
    if (den > 0) {
        wide_num += den / 2;
    } else {
        wide_num -= (-den) / 2;
    }
    
    int64_t result = wide_num / den;
    
    return dvm_clamp32(result, faults);
}

/*============================================================================
 * Fixed-Point Math (LUT-based)
 *============================================================================*/

/**
 * @brief Compute ratio a/b in Q16.16 format
 * @traceability CM-MATH-001 §2.2
 */
int32_t cm_ratio_q16(uint32_t a, uint32_t b, ct_fault_flags_t *faults)
{
    if (b == 0) {
        if (faults) faults->div_zero = 1;
        return INT32_MAX;  /* Saturate to max */
    }
    
    uint64_t num = (uint64_t)a << CT_Q16_SHIFT;
    /* Add half denominator for rounding */
    num += (b >> 1);
    
    uint64_t res = num / b;
    
    /* Saturate to int32 max */
    if (res > (uint64_t)INT32_MAX) {
        if (faults) faults->overflow = 1;
        return INT32_MAX;
    }
    
    return (int32_t)res;
}

/**
 * @brief Compute log2(x) in Q16.16 format using LUT
 * @traceability CM-MATH-001 §3
 *
 * Algorithm:
 * 1. Find MSB position to get integer part k
 * 2. Normalize mantissa to [1.0, 2.0) range
 * 3. Use top 9 bits of fractional part for LUT index
 * 4. Use remaining bits for linear interpolation
 *
 * For x in Q16.16:
 * - x = 65536 represents 1.0
 * - x = 131072 represents 2.0
 * - log2(1.0) = 0
 * - log2(2.0) = 1.0 = 65536 in Q16.16
 */
int32_t cm_log2_q16(uint32_t x, ct_fault_flags_t *faults)
{
    if (x == 0) {
        if (faults) faults->domain = 1;
        return INT32_MIN;  /* -infinity representation */
    }
    
    /* 1. Find integer part: k = floor(log2(x_real)) where x_real = x / 2^16 */
    /* This equals floor(log2(x)) - 16 */
    uint32_t lz = cm_clz32(x);
    int32_t msb_pos = 31 - (int32_t)lz;  /* Position of highest set bit (0-31) */
    
    /* Integer part of log2(x_real) */
    int32_t k = msb_pos - CT_Q16_SHIFT;  /* Subtract 16 to account for Q16.16 */
    int32_t int_part = k << CT_Q16_SHIFT;  /* k in Q16.16 */
    
    /* 2. Compute mantissa m in [1, 2) */
    /* x = 2^msb_pos * (1 + frac) where frac in [0, 1) */
    /* We need the fractional bits after the MSB */
    
    /* Shift so the MSB is at bit 31, then mask it out to get fraction */
    /* After shift: bits 30..0 represent the fractional part */
    uint32_t shifted = x << (31 - (uint32_t)msb_pos);
    uint32_t frac_bits = shifted & 0x7FFFFFFFU;  /* Remove implicit leading 1 */
    
    /* 3. Extract LUT index from top 9 bits of frac_bits */
    /* frac_bits is in [0, 0x7FFFFFFF), representing [0, 1) */
    /* Top 9 bits (30..22) give index 0..511 */
    uint32_t idx = frac_bits >> 22;
    if (idx >= CM_LUT_SIZE) idx = CM_LUT_SIZE - 1;  /* Safety clamp */
    
    /* 4. Extract interpolation fraction (bits 21..6, i.e., 16 bits) */
    uint32_t interp_frac = (frac_bits >> 6) & 0xFFFFU;
    
    /* 5. LUT lookup and linear interpolation */
    /* LUT[i] = floor(log2(1 + i/512) * 65536) */
    int32_t y0 = (int32_t)LOG2_LUT[idx];
    int32_t y1 = (idx < CM_LUT_SIZE - 1) ? (int32_t)LOG2_LUT[idx + 1] : CT_Q16_ONE;
    
    int32_t delta = y1 - y0;
    int32_t interp = (delta * (int32_t)interp_frac) >> 16;
    
    int32_t frac_part = y0 + interp;
    
    return int_part + frac_part;
}

/**
 * @brief Compute ln(x) in Q16.16 using log2
 * @traceability CM-MATH-001 §3
 *
 * ln(x) = log2(x) * ln(2)
 * ln(2) ≈ 0.693147 = 45426 in Q16.16
 */
int32_t cm_ln_q16(uint32_t x, ct_fault_flags_t *faults)
{
    int32_t lg2 = cm_log2_q16(x, faults);
    
    /* Multiply by ln(2) in Q16.16 */
    int64_t res = ((int64_t)lg2 * CM_LN2_Q16) >> CT_Q16_SHIFT;
    
    return dvm_clamp32(res, faults);
}
