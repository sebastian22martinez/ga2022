#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>

static uint32_t s_mask = 0xffffffff;

static LONG debug_exception_handler(LPEXCEPTION_POINTERS info)
{
	// XXX: MS uses 0xE06D7363 to indicate C++ language exception.
	// We're just to going to ignore them. Sometimes Vulkan throws them on startup?
	// https://devblogs.microsoft.com/oldnewthing/20100730-00/?p=13273
	if (info->ExceptionRecord->ExceptionCode == 0xE06D7363)
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}

	debug_print(k_print_error, "Caught exception!\n");

	HANDLE file = CreateFile(L"ga2022-crash.dmp", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION mini_exception = { 0 };
		mini_exception.ThreadId = GetCurrentThreadId();
		mini_exception.ExceptionPointers = info;
		mini_exception.ClientPointers = FALSE;

		MiniDumpWriteDump(GetCurrentProcess(),
			GetCurrentProcessId(),
			file,
			MiniDumpWithThreadInfo,
			&mini_exception,
			NULL,
			NULL);

		CloseHandle(file);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

void debug_install_exception_handler()
{
	AddVectoredExceptionHandler(TRUE, debug_exception_handler);
}

void debug_set_print_mask(uint32_t mask)
{
	s_mask = mask;
}

void debug_print(uint32_t type, _Printf_format_string_ const char* format, ...)
{
	if ((s_mask & type) == 0)
	{
		return;
	}

	va_list args;
	va_start(args, format);
	char buffer[256];
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	OutputDebugStringA(buffer);

	DWORD bytes = (DWORD)strlen(buffer);
	DWORD written = 0;
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleA(out, buffer, bytes, &written, NULL);
}

void debug_backtrace_print(void* ptr, size_t size, int used, void* user)
{
	#define MAX_POOL 10

	//Prints size of the leak
	printf("Memory leak of size %d bytes with callstack:\n", (unsigned int)size);

	SymSetOptions(0x00000002 | 0x00000004);

	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);

	void* maxPool[10];
	unsigned short stacks = CaptureStackBackTrace(4, MAX_POOL, maxPool, NULL);
	char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;

	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYM_NAME;

	int toF = 0;

	//Prints call stack, and stops at the main frame.
	for (int i = 0; i < MAX_POOL; i++)
	{
		SymFromAddr(process, (DWORD64)(maxPool[i]), 0, pSymbol);
		printf("[%d] %s \n", i, pSymbol->Name);
		int ee = strcmp(pSymbol->Name, "main");

		if (toF == 1) { break; }
		if (ee) { toF = 1; }
	}
}
