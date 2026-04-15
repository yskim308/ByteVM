#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Uncomment for bytecode disassembly and per-instruction execution tracing.
// #define DEBUG_PRINT_CODE
// #define DEBUG_TRACE_EXECUTION

// Uncomment for stress test mode for the garbage collector
#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC

typedef uint8_t Byte;

#endif
