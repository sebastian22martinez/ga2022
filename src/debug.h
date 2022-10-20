#pragma once

#include <stdint.h>

// Debugging Support

// Flags for debug_print().
typedef enum debug_print_t
{
	k_print_info = 1 << 0,
	k_print_warning = 1 << 1,
	k_print_error = 1 << 2,
} debug_print_t;

// Install unhandled exception handler.
// When unhandled exceptions are caught, will log an error and capture a memory dump.
void debug_install_exception_handler();

// Set mask of which types of prints will actually fire.
// See the debug_print().
void debug_set_print_mask(uint32_t mask);

// Log a message to the console.
// Message may be dropped if type is not in the active mask.
// See debug_set_print_mask.
void debug_print(uint32_t type, _Printf_format_string_ const char* format, ...);



//This function is called when non-freed memory is about to be leaked
//Prints the call stack and number of bytes lost.
void debug_backtrace_print(void* ptr, size_t size, int used, void* user);
