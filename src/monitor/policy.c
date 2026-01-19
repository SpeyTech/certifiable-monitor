/**
 * @file policy.c
 * @brief COE Policy Parsing and Validation Implementation
 * @traceability SRS-001-POLICY
 *
 * @details
 * Implements deterministic JSON parsing using integer-only algorithms.
 * No external JSON libraries - all parsing is hand-coded for certification.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "policy.h"
#include "cm_audit.h"
#include <string.h>
#include <stdbool.h>

/*============================================================================
 * Internal Constants
 *============================================================================*/

/** Maximum key length for JSON parsing */
#define MAX_KEY_LEN 64

/** Maximum string value length */
#define MAX_STRING_LEN 256

/*============================================================================
 * JSON Parsing Primitives
 *============================================================================*/

/**
 * @brief Skip whitespace in JSON
 */
static const char *skip_ws(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

/**
 * @brief Find key in JSON object (top level only)
 * @param json JSON string starting at '{'
 * @param json_len Length of JSON
 * @param key Key to find (without quotes)
 * @param value_start Output: start of value (after ':')
 * @return true if found
 */
static bool find_key(const char *json, size_t json_len, const char *key,
                     const char **value_start)
{
    if (!json || json_len == 0 || !key || !value_start) {
        return false;
    }
    
    const char *p = json;
    const char *end = json + json_len;
    size_t key_len = strlen(key);
    
    /* Skip to first '{' */
    while (p < end && *p != '{') p++;
    if (p >= end) return false;
    p++;  /* Skip '{' */
    
    int depth = 1;
    
    while (p < end && depth > 0) {
        p = skip_ws(p, end);
        if (p >= end) break;
        
        if (*p == '}') {
            depth--;
            p++;
            continue;
        }
        
        if (*p == '{') {
            depth++;
            p++;
            continue;
        }
        
        if (*p == '[') {
            /* Skip array */
            int arr_depth = 1;
            p++;
            while (p < end && arr_depth > 0) {
                if (*p == '[') arr_depth++;
                else if (*p == ']') arr_depth--;
                p++;
            }
            continue;
        }
        
        if (*p == ']') {
            p++;
            continue;
        }
        
        if (*p == ',') {
            p++;
            continue;
        }
        
        /* At top level (depth == 1), look for key */
        if (depth == 1 && *p == '"') {
            p++;  /* Skip opening quote */
            const char *key_start = p;
            
            /* Find closing quote */
            while (p < end && *p != '"') p++;
            if (p >= end) return false;
            
            size_t found_len = (size_t)(p - key_start);
            p++;  /* Skip closing quote */
            
            /* Skip to colon */
            p = skip_ws(p, end);
            if (p >= end || *p != ':') continue;
            p++;  /* Skip colon */
            
            /* Skip whitespace after colon */
            p = skip_ws(p, end);
            
            /* Check if this is our key */
            if (found_len == key_len && 
                memcmp(key_start, key, key_len) == 0) {
                *value_start = p;
                return true;
            }
            
            /* Not our key - skip value */
            if (*p == '"') {
                /* String value */
                p++;
                while (p < end && *p != '"') {
                    if (*p == '\\' && p + 1 < end) p++;
                    p++;
                }
                if (p < end) p++;  /* Skip closing quote */
            } else if (*p == '{') {
                /* Object value - skip nested */
                int obj_depth = 1;
                p++;
                while (p < end && obj_depth > 0) {
                    if (*p == '{') obj_depth++;
                    else if (*p == '}') obj_depth--;
                    else if (*p == '"') {
                        /* Skip string inside object */
                        p++;
                        while (p < end && *p != '"') {
                            if (*p == '\\' && p + 1 < end) p++;
                            p++;
                        }
                    }
                    p++;
                }
            } else if (*p == '[') {
                /* Array value - skip */
                int arr_depth = 1;
                p++;
                while (p < end && arr_depth > 0) {
                    if (*p == '[') arr_depth++;
                    else if (*p == ']') arr_depth--;
                    else if (*p == '"') {
                        p++;
                        while (p < end && *p != '"') {
                            if (*p == '\\' && p + 1 < end) p++;
                            p++;
                        }
                    }
                    p++;
                }
            } else {
                /* Number, true, false, null - skip until delimiter */
                while (p < end && *p != ',' && *p != '}' && *p != ']' &&
                       *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
                    p++;
                }
            }
        } else {
            p++;  /* Skip unknown character */
        }
    }
    
    return false;
}

/**
 * @brief Parse integer from JSON value position
 */
static bool parse_int(const char *p, const char *end, int64_t *out)
{
    if (!p || p >= end || !out) return false;
    
    p = skip_ws(p, end);
    if (p >= end) return false;
    
    bool negative = false;
    if (*p == '-') {
        negative = true;
        p++;
    }
    
    if (p >= end || *p < '0' || *p > '9') return false;
    
    int64_t value = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        p++;
    }
    
    *out = negative ? -value : value;
    return true;
}

/**
 * @brief Parse string from JSON value position
 */
static bool parse_string(const char *p, const char *end,
                         char *buf, size_t buf_size, size_t *len_out)
{
    if (!p || p >= end || !buf || buf_size == 0) return false;
    
    p = skip_ws(p, end);
    if (p >= end || *p != '"') return false;
    p++;  /* Skip opening quote */
    
    size_t len = 0;
    while (p < end && *p != '"' && len < buf_size - 1) {
        if (*p == '\\' && p + 1 < end) {
            p++;
            switch (*p) {
                case 'n': buf[len++] = '\n'; break;
                case 't': buf[len++] = '\t'; break;
                case 'r': buf[len++] = '\r'; break;
                case '"': buf[len++] = '"'; break;
                case '\\': buf[len++] = '\\'; break;
                default: buf[len++] = *p; break;
            }
        } else {
            buf[len++] = *p;
        }
        p++;
    }
    buf[len] = '\0';
    
    if (len_out) *len_out = len;
    return (*p == '"');
}

/*============================================================================
 * Public JSON API
 *============================================================================*/

cm_policy_result_t cm_json_get_int(const char *json, size_t json_len,
                                   const char *key, int64_t *value_out)
{
    if (!json || !key || !value_out) {
        return CM_POL_ERR_NULL;
    }
    
    const char *value_start;
    if (!find_key(json, json_len, key, &value_start)) {
        return CM_POL_ERR_MISSING;
    }
    
    if (!parse_int(value_start, json + json_len, value_out)) {
        return CM_POL_ERR_PARSE;
    }
    
    return CM_POL_OK;
}

cm_policy_result_t cm_json_get_string(const char *json, size_t json_len,
                                      const char *key,
                                      char *buf_out, size_t buf_size,
                                      size_t *len_out)
{
    if (!json || !key || !buf_out) {
        return CM_POL_ERR_NULL;
    }
    
    const char *value_start;
    if (!find_key(json, json_len, key, &value_start)) {
        return CM_POL_ERR_MISSING;
    }
    
    if (!parse_string(value_start, json + json_len, buf_out, buf_size, len_out)) {
        return CM_POL_ERR_PARSE;
    }
    
    return CM_POL_OK;
}

cm_policy_result_t cm_json_get_array(const char *json, size_t json_len,
                                     const char *key,
                                     const char **array_start,
                                     size_t *array_len)
{
    if (!json || !key || !array_start || !array_len) {
        return CM_POL_ERR_NULL;
    }
    
    const char *value_start;
    if (!find_key(json, json_len, key, &value_start)) {
        return CM_POL_ERR_MISSING;
    }
    
    const char *p = skip_ws(value_start, json + json_len);
    if (p >= json + json_len || *p != '[') {
        return CM_POL_ERR_SCHEMA;
    }
    
    p++;  /* Skip '[' */
    *array_start = p;
    
    /* Find matching ']' */
    int depth = 1;
    while (p < json + json_len && depth > 0) {
        if (*p == '[') depth++;
        else if (*p == ']') depth--;
        else if (*p == '"') {
            p++;
            while (p < json + json_len && *p != '"') {
                if (*p == '\\' && p + 1 < json + json_len) p++;
                p++;
            }
        }
        if (depth > 0) p++;
    }
    
    *array_len = (size_t)(p - *array_start - 1);  /* Exclude ']' */
    return CM_POL_OK;
}

cm_policy_result_t cm_json_get_object(const char *json, size_t json_len,
                                      const char *key,
                                      const char **obj_start,
                                      size_t *obj_len)
{
    if (!json || !key || !obj_start || !obj_len) {
        return CM_POL_ERR_NULL;
    }
    
    const char *value_start;
    if (!find_key(json, json_len, key, &value_start)) {
        return CM_POL_ERR_MISSING;
    }
    
    const char *p = skip_ws(value_start, json + json_len);
    if (p >= json + json_len || *p != '{') {
        return CM_POL_ERR_SCHEMA;
    }
    
    p++;  /* Skip '{' */
    *obj_start = p;
    
    /* Find matching '}' */
    int depth = 1;
    while (p < json + json_len && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        else if (*p == '"') {
            p++;
            while (p < json + json_len && *p != '"') {
                if (*p == '\\' && p + 1 < json + json_len) p++;
                p++;
            }
        }
        if (depth > 0) p++;
    }
    
    *obj_len = (size_t)(p - *obj_start - 1);  /* Exclude '}' */
    return CM_POL_OK;
}

/*============================================================================
 * Policy Initialization
 *============================================================================*/

cm_policy_result_t cm_policy_init(cm_policy_t *policy)
{
    if (!policy) {
        return CM_POL_ERR_NULL;
    }
    
    memset(policy, 0, sizeof(*policy));
    policy->policy_version = CM_POLICY_VERSION;
    policy->window_size = 256;  /* Default */
    
    /* Default drift settings */
    policy->drift.enabled_detectors = CM_DRIFT_TV_ENABLED;
    policy->drift.tv_threshold_q0_32 = 0x19999999;  /* ~0.1 */
    policy->drift.jsd_threshold_q16_16 = 6554;      /* ~0.1 */
    policy->drift.psi_threshold_q16_16 = 13107;     /* ~0.2 */
    policy->drift.epsilon_q0_32 = 1;
    
    return CM_POL_OK;
}

/*============================================================================
 * Policy Hash Computation
 *============================================================================*/

cm_policy_result_t cm_policy_compute_hash(const uint8_t *json_bytes,
                                          size_t json_len,
                                          uint8_t *hash_out)
{
    if (!json_bytes || !hash_out) {
        return CM_POL_ERR_NULL;
    }
    
    /* H_P = SHA256("CM:POLICY:v1" || json_bytes) */
    cm_sha256_domain(CM_POLICY_TAG, json_bytes, json_len, hash_out);
    
    return CM_POL_OK;
}

cm_policy_result_t cm_policy_verify_hash(const cm_policy_t *policy,
                                         const uint8_t *expected_hash)
{
    if (!policy || !expected_hash) {
        return CM_POL_ERR_NULL;
    }
    
    if (memcmp(policy->policy_hash, expected_hash, CT_SHA256_SIZE) != 0) {
        return CM_POL_ERR_SCHEMA;  /* Hash mismatch */
    }
    
    return CM_POL_OK;
}

/*============================================================================
 * Policy Parsing
 *============================================================================*/

/**
 * @brief Parse violation type string
 */
static cm_violation_t parse_violation_type(const char *str)
{
    if (strcmp(str, "input_range") == 0) return CM_VIOL_INPUT_RANGE;
    if (strcmp(str, "input_drift") == 0) return CM_VIOL_INPUT_DRIFT;
    if (strcmp(str, "activation_range") == 0) return CM_VIOL_ACTIV_RANGE;
    if (strcmp(str, "activation_saturation") == 0) return CM_VIOL_ACTIV_SAT;
    if (strcmp(str, "output_range") == 0) return CM_VIOL_OUTPUT_RANGE;
    if (strcmp(str, "output_drift") == 0) return CM_VIOL_OUTPUT_DRIFT;
    if (strcmp(str, "fault_budget") == 0) return CM_VIOL_FAULT_BUDGET;
    return CM_VIOL_NONE;
}

/**
 * @brief Parse reaction type string
 */
static cm_reaction_t parse_reaction_type(const char *str)
{
    if (strcmp(str, "log_only") == 0) return CM_REACT_LOG_ONLY;
    if (strcmp(str, "warn_operator") == 0) return CM_REACT_WARN_OPERATOR;
    if (strcmp(str, "clamp_and_log") == 0) return CM_REACT_CLAMP_OUTPUT;
    if (strcmp(str, "degrade_mode") == 0) return CM_REACT_DEGRADE_MODE;
    if (strcmp(str, "emergency_stop") == 0) return CM_REACT_EMERGENCY_STOP;
    return CM_REACT_LOG_ONLY;
}

/**
 * @brief Parse drift enabled array
 */
static uint32_t parse_enabled_detectors(const char *array_start, size_t array_len)
{
    uint32_t flags = 0;
    const char *p = array_start;
    const char *end = array_start + array_len;
    
    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end) break;
        
        if (*p == '"') {
            char buf[32];
            size_t len;
            if (parse_string(p, end, buf, sizeof(buf), &len)) {
                if (strcmp(buf, "tv") == 0) flags |= CM_DRIFT_TV_ENABLED;
                else if (strcmp(buf, "jsd") == 0) flags |= CM_DRIFT_JSD_ENABLED;
                else if (strcmp(buf, "psi") == 0) flags |= CM_DRIFT_PSI_ENABLED;
            }
            /* Skip to after string */
            p++;
            while (p < end && *p != '"') {
                if (*p == '\\' && p + 1 < end) p++;
                p++;
            }
            if (p < end) p++;
        } else if (*p == ',') {
            p++;
        } else {
            p++;
        }
    }
    
    return flags;
}

cm_policy_result_t cm_policy_parse(const uint8_t *json_bytes,
                                   size_t json_len,
                                   cm_policy_t *policy,
                                   ct_fault_flags_t *faults)
{
    (void)faults;  /* Reserved for future use */
    
    if (!json_bytes || !policy) {
        return CM_POL_ERR_NULL;
    }
    
    if (json_len > CM_MAX_POLICY_SIZE) {
        return CM_POL_ERR_SIZE;
    }
    
    /* Initialize policy */
    cm_policy_result_t rc = cm_policy_init(policy);
    if (rc != CM_POL_OK) return rc;
    
    const char *json = (const char *)json_bytes;
    int64_t int_val;
    
    /* Parse policy_version (required) */
    rc = cm_json_get_int(json, json_len, "policy_version", &int_val);
    if (rc != CM_POL_OK) return CM_POL_ERR_MISSING;
    if (int_val != CM_POLICY_VERSION) return CM_POL_ERR_VERSION;
    policy->policy_version = (uint32_t)int_val;
    
    /* Parse window_size (required) */
    rc = cm_json_get_int(json, json_len, "window_size", &int_val);
    if (rc != CM_POL_OK) return CM_POL_ERR_MISSING;
    if (int_val < 1 || int_val > INT32_MAX) return CM_POL_ERR_RANGE;
    policy->window_size = (uint32_t)int_val;
    
    /* Parse drift object (optional but expected) */
    const char *drift_start;
    size_t drift_len;
    rc = cm_json_get_object(json, json_len, "drift", &drift_start, &drift_len);
    if (rc == CM_POL_OK) {
        /* Wrap drift object for nested parsing */
        char drift_json[1024];
        if (drift_len + 2 < sizeof(drift_json)) {
            drift_json[0] = '{';
            memcpy(drift_json + 1, drift_start, drift_len);
            drift_json[drift_len + 1] = '}';
            drift_json[drift_len + 2] = '\0';
            size_t drift_json_len = drift_len + 2;
            
            /* Parse enabled array */
            const char *enabled_start;
            size_t enabled_len;
            if (cm_json_get_array(drift_json, drift_json_len, "enabled",
                                  &enabled_start, &enabled_len) == CM_POL_OK) {
                policy->drift.enabled_detectors = 
                    parse_enabled_detectors(enabled_start, enabled_len);
            }
            
            /* Parse thresholds */
            if (cm_json_get_int(drift_json, drift_json_len, 
                               "tv_threshold_q0_32", &int_val) == CM_POL_OK) {
                policy->drift.tv_threshold_q0_32 = (uint32_t)int_val;
            }
            if (cm_json_get_int(drift_json, drift_json_len,
                               "jsd_threshold_q16_16", &int_val) == CM_POL_OK) {
                policy->drift.jsd_threshold_q16_16 = (int32_t)int_val;
            }
            if (cm_json_get_int(drift_json, drift_json_len,
                               "psi_threshold_q16_16", &int_val) == CM_POL_OK) {
                policy->drift.psi_threshold_q16_16 = (int32_t)int_val;
            }
            if (cm_json_get_int(drift_json, drift_json_len,
                               "epsilon_q0_32", &int_val) == CM_POL_OK) {
                policy->drift.epsilon_q0_32 = (uint32_t)int_val;
            }
        }
    }
    
    /* Parse input object (optional) */
    const char *input_start;
    size_t input_len;
    rc = cm_json_get_object(json, json_len, "input", &input_start, &input_len);
    if (rc == CM_POL_OK) {
        char input_json[2048];
        if (input_len + 2 < sizeof(input_json)) {
            input_json[0] = '{';
            memcpy(input_json + 1, input_start, input_len);
            input_json[input_len + 1] = '}';
            input_json[input_len + 2] = '\0';
            size_t input_json_len = input_len + 2;
            
            if (cm_json_get_int(input_json, input_json_len,
                               "feature_count", &int_val) == CM_POL_OK) {
                if (int_val > CM_MAX_FEATURES) return CM_POL_ERR_RANGE;
                policy->input.feature_count = (uint32_t)int_val;
            }
        }
    }
    
    /* Parse reaction_map array (optional) */
    const char *react_start;
    size_t react_len;
    rc = cm_json_get_array(json, json_len, "reaction_map", &react_start, &react_len);
    if (rc == CM_POL_OK) {
        /* Parse each reaction entry - simplified parsing */
        const char *p = react_start;
        const char *end = react_start + react_len;
        uint32_t count = 0;
        
        while (p < end && count < CM_MAX_REACTIONS) {
            p = skip_ws(p, end);
            if (p >= end) break;
            
            if (*p == '{') {
                /* Find end of object */
                int depth = 1;
                const char *obj_start = p + 1;
                p++;
                while (p < end && depth > 0) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                    else if (*p == '"') {
                        p++;
                        while (p < end && *p != '"') {
                            if (*p == '\\' && p + 1 < end) p++;
                            p++;
                        }
                    }
                    if (depth > 0) p++;
                }
                size_t obj_len = (size_t)(p - obj_start);
                
                /* Parse this reaction entry */
                char entry_json[256];
                if (obj_len + 2 < sizeof(entry_json)) {
                    entry_json[0] = '{';
                    memcpy(entry_json + 1, obj_start, obj_len);
                    entry_json[obj_len + 1] = '}';
                    entry_json[obj_len + 2] = '\0';
                    
                    char viol_str[64], act_str[64];
                    if (cm_json_get_string(entry_json, obj_len + 2, "violation",
                                          viol_str, sizeof(viol_str), NULL) == CM_POL_OK &&
                        cm_json_get_string(entry_json, obj_len + 2, "action",
                                          act_str, sizeof(act_str), NULL) == CM_POL_OK) {
                        policy->react_map[count].violation = parse_violation_type(viol_str);
                        policy->react_map[count].reaction = parse_reaction_type(act_str);
                        count++;
                    }
                }
                p++;  /* Skip '}' */
            } else if (*p == ',') {
                p++;
            } else {
                p++;
            }
        }
        policy->react_map_count = count;
    }
    
    /* Compute policy hash */
    cm_policy_compute_hash(json_bytes, json_len, policy->policy_hash);
    
    return CM_POL_OK;
}

/*============================================================================
 * Policy Validation
 *============================================================================*/

cm_policy_result_t cm_policy_validate(const cm_policy_t *policy)
{
    if (!policy) {
        return CM_POL_ERR_NULL;
    }
    
    /* Check version */
    if (policy->policy_version != CM_POLICY_VERSION) {
        return CM_POL_ERR_VERSION;
    }
    
    /* Check window size */
    if (policy->window_size == 0) {
        return CM_POL_ERR_RANGE;
    }
    
    /* Check at least one drift detector is enabled */
    if (policy->drift.enabled_detectors == 0) {
        return CM_POL_ERR_SCHEMA;
    }
    
    /* Check layer contracts for duplicates */
    for (uint32_t i = 0; i < policy->layer_count; i++) {
        for (uint32_t j = i + 1; j < policy->layer_count; j++) {
            if (policy->layers[i].layer_id == policy->layers[j].layer_id) {
                return CM_POL_ERR_DUPLICATE;
            }
        }
    }
    
    return CM_POL_OK;
}

/*============================================================================
 * Reaction Lookup
 *============================================================================*/

cm_reaction_t cm_policy_get_reaction(const cm_policy_t *policy,
                                     cm_violation_t violation)
{
    if (!policy) {
        return CM_REACT_LOG_ONLY;
    }
    
    for (uint32_t i = 0; i < policy->react_map_count; i++) {
        if (policy->react_map[i].violation == violation) {
            return policy->react_map[i].reaction;
        }
    }
    
    return CM_REACT_LOG_ONLY;  /* Default */
}
