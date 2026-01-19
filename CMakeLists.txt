cmake_minimum_required(VERSION 3.10)

project(certifiable-monitor
    VERSION 1.0.0
    DESCRIPTION "Deterministic runtime monitoring for safety-critical ML"
    LANGUAGES C
)

# C99 strict compliance
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Strict warnings (certification grade)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow -Wconversion -Wstrict-prototypes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffp-contract=off")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-fast-math")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-common -O2")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# DVM Source files (Deterministic Virtual Machine core)
set(DVM_SOURCES
    src/dvm/primitives.c
    src/dvm/sha256.c
)

# Monitor Source files
set(MONITOR_SOURCES
    src/monitor/detectors.c
    src/monitor/policy.c
    src/monitor/input.c
    src/monitor/activation.c
    src/monitor/output.c
    src/monitor/health.c
    src/monitor/react.c
)

# Audit Source files
set(AUDIT_SOURCES
    src/audit/ledger.c
    src/audit/verify.c
)

# Main library
add_library(certifiable_monitor STATIC
    ${DVM_SOURCES}
    ${MONITOR_SOURCES}
    ${AUDIT_SOURCES}
)

# Tests
enable_testing()

# Test: DVM Primitives
add_executable(test_primitives tests/unit/test_primitives.c)
target_link_libraries(test_primitives certifiable_monitor)
add_test(NAME test_primitives COMMAND test_primitives)

# Test: Audit Ledger
add_executable(test_ledger tests/unit/test_ledger.c)
target_link_libraries(test_ledger certifiable_monitor)
add_test(NAME test_ledger COMMAND test_ledger)

# Test: Drift Detectors
add_executable(test_detectors tests/unit/test_detectors.c)
target_link_libraries(test_detectors certifiable_monitor)
add_test(NAME test_detectors COMMAND test_detectors)

# Test: Policy Parser
add_executable(test_policy tests/unit/test_policy.c)
target_link_libraries(test_policy certifiable_monitor)
add_test(NAME test_policy COMMAND test_policy)

# Test: Input Monitor
add_executable(test_input tests/unit/test_input.c)
target_link_libraries(test_input certifiable_monitor)
add_test(NAME test_input COMMAND test_input)

# Test: Activation Monitor
add_executable(test_activation tests/unit/test_activation.c)
target_link_libraries(test_activation certifiable_monitor)
add_test(NAME test_activation COMMAND test_activation)

# Test: Output Monitor
add_executable(test_output tests/unit/test_output.c)
target_link_libraries(test_output certifiable_monitor)
add_test(NAME test_output COMMAND test_output)

# Test: Health FSM
add_executable(test_health tests/unit/test_health.c)
target_link_libraries(test_health certifiable_monitor)
add_test(NAME test_health COMMAND test_health)

# Test: Reaction Handler
add_executable(test_react tests/unit/test_react.c)
target_link_libraries(test_react certifiable_monitor)
add_test(NAME test_react COMMAND test_react)

# Test: Verification
add_executable(test_verify tests/unit/test_verify.c)
target_link_libraries(test_verify certifiable_monitor)
add_test(NAME test_verify COMMAND test_verify)

# Test: Bit-Identity (Cross-Platform Verification)
add_executable(test_bit_identity tests/unit/test_bit_identity.c)
target_link_libraries(test_bit_identity certifiable_monitor)
add_test(NAME test_bit_identity COMMAND test_bit_identity)

# Custom target for all tests
add_custom_target(test-all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_primitives test_ledger test_detectors test_policy test_input test_activation test_output test_health test_react test_verify test_bit_identity
)

# Install targets
install(TARGETS certifiable_monitor
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
