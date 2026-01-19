# SRS-005-HEALTH

## Software Requirements Specification: Health Monitor FSM

**Document ID:** SRS-005-HEALTH  
**Version:** 1.0  
**Status:** ✅ Approved  
**Traceability:** CM-ARCH-MATH-001 §6

---

## 1. Purpose

This document specifies the software requirements for the Health Monitor FSM module (`src/monitor/health.c`), which provides deterministic state machine management for monitoring system health based on fault budgets and violation patterns.

---

## 2. Scope

The Health Monitor SHALL:
- Maintain a finite state machine with defined states and transitions
- Track fault events (overflow, underflow, saturation, clamp)
- Enforce fault budgets per monitoring window
- Transition to degraded/stopped states based on violations

---

## 3. Requirements

### 3.1 State Machine Definition

**SRS-005-HEALTH-01:** The monitor SHALL implement the following states:

| State | Value | Description |
|-------|-------|-------------|
| UNINIT | 0 | Not initialized |
| INIT | 1 | Initialized, not enabled |
| ENABLED | 2 | Active monitoring, healthy |
| ALARM | 3 | Violation detected, still operational |
| DEGRADED | 4 | Reduced capability mode |
| STOPPED | 5 | Emergency stopped, requires recovery |

**SRS-005-HEALTH-02:** Valid state transitions SHALL be:
```
UNINIT → INIT: cm_health_init()
INIT → ENABLED: cm_health_enable()
ENABLED → ALARM: Budget exceeded in window
ALARM → ENABLED: Window completes without violations
ALARM → DEGRADED: consecutive_alarm_windows >= alarm_threshold
DEGRADED → STOPPED: Any violation in degraded mode
Any → STOPPED: cm_health_emergency_stop()
```

**SRS-005-HEALTH-03:** The STOPPED state SHALL be terminal; `cm_health_enable()` SHALL return `CT_ERR_STATE` from STOPPED.

### 3.2 Initialization

**SRS-005-HEALTH-04:** The function `cm_health_init()` SHALL initialize the context with:
- Fault budget reference from policy (or defaults if NULL)
- All counters zeroed
- State set to INIT
- Default thresholds: alarm_threshold=3, degraded_threshold=1

**SRS-005-HEALTH-05:** The function `cm_health_enable()` SHALL transition from INIT to ENABLED.

**SRS-005-HEALTH-06:** The function `cm_health_reset_window()` SHALL:
- Update window_id
- Reset per-window fault counters to zero
- NOT reset cumulative counters or state

### 3.3 Fault Recording

**SRS-005-HEALTH-07:** The function `cm_health_record_faults()` SHALL increment appropriate counters based on fault flag bits:
- `overflow` flag → `overflow_count++`, `total_overflows++`
- `underflow` flag → `underflow_count++`, `total_underflows++`

**SRS-005-HEALTH-08:** The function `cm_health_record_saturation()` SHALL increment:
- `saturation_count++`
- `total_saturations++`

**SRS-005-HEALTH-09:** The function `cm_health_record_clamp()` SHALL increment:
- `clamp_count++`
- `total_clamps++`

### 3.4 Budget Checking

**SRS-005-HEALTH-10:** The function `cm_health_check_budget()` SHALL compare per-window counts against budget limits:
```
exceeded = (overflow_count > overflow_budget) ∨
           (underflow_count > underflow_budget) ∨
           (saturation_count > saturation_budget) ∨
           (clamp_count > clamp_budget)
```

**SRS-005-HEALTH-11:** The result SHALL report:
- Whether any budget was exceeded
- The most severe violation type
- Count of budgets exceeded

### 3.5 Window Processing

**SRS-005-HEALTH-12:** The function `cm_health_process_window_end()` SHALL:
- Update `consecutive_alarm_windows` counter
- Trigger state transitions based on violation history
- Return transition details (old state, new state, trigger)

**SRS-005-HEALTH-13:** State transition logic at window end:
```
If in ENABLED and had_violations:
    → ALARM, consecutive_alarm_windows = 1
If in ALARM and had_violations:
    consecutive_alarm_windows++
    If consecutive_alarm_windows >= alarm_threshold:
        → DEGRADED
If in ALARM and !had_violations:
    → ENABLED, consecutive_alarm_windows = 0
If in DEGRADED and had_violations:
    → STOPPED
```

### 3.6 Emergency Stop

**SRS-005-HEALTH-14:** The function `cm_health_emergency_stop()` SHALL:
- Transition immediately to STOPPED from any state
- Record the transition details
- Be idempotent (multiple calls are safe)

### 3.7 Query Functions

**SRS-005-HEALTH-15:** `cm_health_get_state()` SHALL return the current FSM state.

**SRS-005-HEALTH-16:** `cm_health_is_operational()` SHALL return true if state is ENABLED or ALARM.

**SRS-005-HEALTH-17:** `cm_health_is_stopped()` SHALL return true if state is STOPPED.

---

## 4. Error Handling

**SRS-005-HEALTH-18:** All functions SHALL return `CT_ERR_NULL` if passed null pointers for required arguments.

**SRS-005-HEALTH-19:** Functions requiring initialized context SHALL check `ctx->initialized`.

**SRS-005-HEALTH-20:** The monitor SHALL NOT allocate memory.

---

## 5. Determinism Requirements

**SRS-005-HEALTH-21:** All state transitions SHALL be deterministic based solely on inputs and current state.

**SRS-005-HEALTH-22:** Counter arithmetic SHALL use saturating semantics to prevent overflow.

**SRS-005-HEALTH-23:** All operations SHALL produce bit-identical results across platforms.

---

## 6. Traceability Matrix

| Requirement | Implementation | Test |
|-------------|----------------|------|
| SRS-005-HEALTH-01 | `cm_monitor_state_t` | `test_health_states_*` |
| SRS-005-HEALTH-02 | `cm_health_*` | `test_health_transitions_*` |
| SRS-005-HEALTH-04 | `cm_health_init()` | `test_health_init_*` |
| SRS-005-HEALTH-07 | `cm_health_record_faults()` | `test_health_faults_*` |
| SRS-005-HEALTH-10 | `cm_health_check_budget()` | `test_health_budget_*` |
| SRS-005-HEALTH-12 | `cm_health_process_window_end()` | `test_health_window_*` |
| SRS-005-HEALTH-14 | `cm_health_emergency_stop()` | `test_health_stop_*` |

---

*Copyright © 2026 The Murray Family Innovation Trust. All rights reserved.*
