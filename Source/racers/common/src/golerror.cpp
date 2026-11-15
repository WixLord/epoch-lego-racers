#include "golerror.h"

#include "decomp.h"

#ifdef BUILDING_LEGORACERS
#include "app/main.h"

#include <stdio.h>
#include <string.h>
#endif

#if defined(BUILDING_GOL)
#include "core/gol.h"

// GLOBAL: GOLDP 0x1006314c
FatalErrorMessageCBFN* g_fatalErrorMessage;
#endif

// FUNCTION: GOLDP 0x100070d0
// FUNCTION: LEGORACERS 0x0042f8b0
void GolFatalErrorMessage(const LegoChar* p_message, const LegoChar* p_file, LegoS32 p_line)
{
#ifdef BUILDING_LEGORACERS
	// Record the original failure before shutdown can obscure it.
	char path[MAX_PATH];
	DWORD length = GetModuleFileName(NULL, path, MAX_PATH);
	if (length > 0 && length < MAX_PATH) {
		char* slash = strrchr(path, '\\');
		const char* filename = "Reborn-errors.log";
		if (slash && static_cast<size_t>(slash + 1 - path) + strlen(filename) < MAX_PATH) {
			strcpy(slash + 1, filename);
			FILE* log = fopen(path, "a");
			if (log) {
				SYSTEMTIME time;
				GetLocalTime(&time);
				fprintf(
					log,
					"%04u-%02u-%02u %02u:%02u:%02u | %s | %s:%ld\n",
					time.wYear,
					time.wMonth,
					time.wDay,
					time.wHour,
					time.wMinute,
					time.wSecond,
					p_message ? p_message : "Unknown error",
					p_file ? p_file : "unknown",
					static_cast<long>(p_line)
				);
				fclose(log);
			}
		}
	}
	g_racers.Shutdown();
	MessageBox(NULL, p_message, "Fatal Error", MB_TOPMOST | MB_SETFOREGROUND | MB_ICONWARNING);
	g_racers.Destroy();
	exit(1);
#elif defined(BUILDING_GOL)
	g_fatalErrorMessage(p_message, p_file, p_line);
#endif
}

// FUNCTION: GOLDP 0x100070f0
// FUNCTION: LEGORACERS 0x0042f8f0
void GolFatalError(GolErrorCode p_code, const LegoChar* p_file, LegoS32 p_line)
{
	const LegoChar* message;

	switch (p_code) {
	case c_golErrorInvalidCondition:
		message = "Invalid error condition";
		break;
	case c_golErrorGeneral:
		message = "General error occurred";
		break;
	case c_golErrorInvalidParameter:
		message = "Invalid parameter detected";
		break;
	case c_golErrorInvalidValue:
		message = "Invalid value detected";
		break;
	case c_golErrorOutOfMemory:
		message = "Out of memory error occured";
		break;
	case c_golErrorCriticalResource:
		message = "Critical resource busy";
		break;
#ifdef BUILDING_LEGORACERS
	case c_golErrorAssertion:
		message = "Assertion Failed";
		break;
#endif
	default:
		message = "Unknown error condition occured";
		break;
	}

#ifdef BUILDING_LEGORACERS
	GolFatalErrorMessage(message, p_file, p_line);
#elif defined(BUILDING_GOL)
	g_fatalErrorMessage(message, p_file, p_line);
#endif
}
