# CM-STRUCT-001

## certifiable-monitor: Data Structures (Phase 1)

**Status:** ✅ Draft

**Purpose:** Define core types for policy, ledger, and reaction mapping.

---

## 1. Policy Types

### 1.1 Histogram Specification

```c
typedef struct {
    uint32_t bin_count;
    int32_t *edges_q16;      /* length bin_count+1, Q16.16 */
    uint32_t *ref_counts;    /* length bin_count */
} cm_hist_spec_t;
```

### 1.2 Drift Policy

```c
typedef struct {
    int32_t tv_threshold_q0_32;
    int32_t jsd_threshold_q16_16;
    int32_t psi_threshold_q16_16;
    uint32_t enabled_detectors;  /* bitmask: TV=0x01, JSD=0x02, PSI=0x04 */
    uint32_t epsilon_q0_32;      /* smoothing for PSI */
} cm_drift_policy_t;
```

### 1.3 Input Envelope

```c
typedef struct {
    int32_t *min_q16;           /* per-feature minimum, Q16.16 */
    int32_t *max_q16;           /* per-feature maximum, Q16.16 */
    uint32_t feature_count;

    cm_hist_spec_t *hists;      /* feature_count elements or NULL */
} cm_input_envelope_t;
```

### 1.4 Activation Contract

```c
typedef struct {
    int32_t min_q16;            /* minimum bound, Q16.16 */
    int32_t max_q16;            /* maximum bound, Q16.16 */
    uint32_t tol_violations_q0_32;  /* allowed violation rate, Q0.32 */
    uint32_t max_overrange_q16;     /* absolute cap, Q16.16 */
} cm_activation_contract_t;

typedef struct {
    uint32_t layer_id;
    cm_activation_contract_t contract;
} cm_layer_contract_t;
```

### 1.5 Reaction Types

```c
typedef enum {
    CM_REACT_LOG_ONLY       = 0,
    CM_REACT_WARN_OPERATOR  = 1,
    CM_REACT_CLAMP_OUTPUT   = 2,
    CM_REACT_DEGRADE_MODE   = 3,
    CM_REACT_EMERGENCY_STOP = 4
} cm_reaction_t;
```

### 1.6 Violation Types

```c
typedef enum {
    CM_VIOL_INPUT_RANGE   = 1,
    CM_VIOL_INPUT_DRIFT   = 2,
    CM_VIOL_ACTIV_RANGE   = 3,
    CM_VIOL_ACTIV_SAT     = 4,
    CM_VIOL_OUTPUT_RANGE  = 5,
    CM_VIOL_OUTPUT_DRIFT  = 6,
    CM_VIOL_FAULT_BUDGET  = 7
} cm_violation_t;
```

### 1.7 Reaction Map Entry

```c
typedef struct {
    cm_violation_t violation;
    cm_reaction_t reaction;
} cm_reaction_map_entry_t;
```

### 1.8 Policy Root Structure

```c
typedef struct {
    uint32_t policy_version;     /* 1 */
    uint32_t window_size;        /* N samples */
    uint8_t bundle_root[32];     /* R (copied from loader) */
    uint8_t policy_hash[32];     /* H_P */

    cm_input_envelope_t input;
    cm_layer_contract_t *layers;
    uint32_t layer_count;

    cm_drift_policy_t drift;
    cm_reaction_map_entry_t *react_map;
    uint32_t react_map_count;
} cm_policy_t;
```

---

## 2. Ledger Types

### 2.1 Ledger Header

```c
typedef struct {
    uint64_t seq;               /* monotonic sequence number */
    uint64_t window_id;         /* window identifier */
    uint32_t event_type;        /* event type enum */
    uint32_t payload_len;       /* length of payload bytes */
    uint64_t time_tick;         /* 0 if count-window mode */
    uint8_t bundle_root[32];    /* R */
    uint8_t policy_hash[32];    /* H_P */
} cm_ledger_header_t;
```

**Size:** 8 + 8 + 4 + 4 + 8 + 32 + 32 = **96 bytes**

### 2.2 Ledger Entry

```c
typedef struct {
    cm_ledger_header_t hdr;
    const uint8_t *payload;     /* payload_len bytes */
} cm_ledger_entry_t;
```

### 2.3 Ledger Context

```c
typedef struct {
    uint8_t L_prev[32];         /* L_{t-1} */
    uint64_t seq_next;          /* next sequence number */
} cm_ledger_ctx_t;
```

---

## 3. Reaction Interface Types

```c
typedef struct {
    int32_t out_min_q16;        /* output clamp minimum, Q16.16 */
    int32_t out_max_q16;        /* output clamp maximum, Q16.16 */
    void *sys;                  /* opaque handle to system control plane */
} cm_react_ctx_t;
```

---

## 4. Event Type Enum

```c
typedef enum {
    CM_EVENT_GENESIS        = 0x00,
    CM_EVENT_WINDOW_OK      = 0x01,
    CM_EVENT_VIOL_INPUT     = 0x10,
    CM_EVENT_VIOL_ACTIV     = 0x11,
    CM_EVENT_VIOL_OUTPUT    = 0x12,
    CM_EVENT_VIOL_DRIFT     = 0x13,
    CM_EVENT_VIOL_FAULT     = 0x14,
    CM_EVENT_REACT_WARN     = 0x20,
    CM_EVENT_REACT_CLAMP    = 0x21,
    CM_EVENT_REACT_DEGRADE  = 0x22,
    CM_EVENT_REACT_STOP     = 0x23,
    CM_EVENT_CHECKPOINT     = 0x30
} cm_event_type_t;
```

---

## 5. Fault Flags (from certifiable-inference)

```c
typedef struct {
    uint32_t overflow    : 1;   /* Saturated high */
    uint32_t underflow   : 1;   /* Saturated low */
    uint32_t div_zero    : 1;   /* Division by zero */
    uint32_t domain      : 1;   /* Invalid input */
    uint32_t precision   : 1;   /* Precision loss detected */
    uint32_t _reserved   : 27;
} ct_fault_flags_t;
```

---

## 6. Hash Type

```c
typedef struct {
    uint8_t bytes[32];          /* SHA-256 digest */
} cm_hash_t;
```

---

## 7. Window Statistics

```c
typedef struct {
    uint64_t window_id;
    uint32_t sample_count;
    uint32_t *bin_counts;       /* length B */
    uint32_t bin_count;
} cm_window_stats_t;
```

---

## 8. Drift Result

```c
typedef struct {
    uint32_t tv_q0_32;          /* Total Variation in Q0.32 */
    int32_t jsd_q16_16;         /* Jensen-Shannon in Q16.16 */
    int32_t psi_q16_16;         /* PSI in Q16.16 */
    uint32_t flags;             /* which detectors triggered */
} cm_drift_result_t;
```

**Flags:**
```c
#define CM_DRIFT_TV_TRIGGERED   0x01
#define CM_DRIFT_JSD_TRIGGERED  0x02
#define CM_DRIFT_PSI_TRIGGERED  0x04
```

---

## 9. Monitor State

```c
typedef enum {
    CM_STATE_UNINIT     = 0,
    CM_STATE_INIT       = 1,
    CM_STATE_ENABLED    = 2,
    CM_STATE_ALARM      = 3,
    CM_STATE_DEGRADED   = 4,
    CM_STATE_STOPPED    = 5
} cm_monitor_state_t;

typedef struct {
    cm_monitor_state_t state;
    cm_policy_t *policy;
    cm_ledger_ctx_t ledger;
    cm_window_stats_t *input_stats;   /* per-feature */
    cm_window_stats_t *output_stats;
    uint64_t total_samples;
    uint64_t total_violations;
    ct_fault_flags_t accumulated_faults;
} cm_monitor_ctx_t;
```

---

## 10. Serialization Sizes

| Structure | Size (bytes) |
|-----------|--------------|
| cm_ledger_header_t | 96 |
| cm_hash_t | 32 |
| cm_activation_contract_t | 16 |
| cm_layer_contract_t | 20 |
| cm_drift_policy_t | 20 |
| cm_reaction_map_entry_t | 8 |

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
