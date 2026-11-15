// clang-format off
#include <windows.h>
#include <mmsystem.h>
// clang-format on

#include "app/main.h"

#include "app/legoracers.h"
#include "decomp.h"
#include "golerror.h"
#include "types.h"

#include <objbase.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>

// GLOBAL: LEGORACERS 0x004c47e4
HINSTANCE g_hInstance;

// GLOBAL: LEGORACERS 0x004c47e8
HINSTANCE g_hPrevInstance;

// GLOBAL: LEGORACERS 0x004c4a38
LegoRacers g_racers;

// GLOBAL: LEGORACERS 0x004c6ee8
LegoChar g_commandLine[256];

// GLOBAL: LEGORACERS 0x004c6fe8
LegoS32 g_argc;

// GLOBAL: LEGORACERS 0x004c6fec
LegoChar* g_argv[32];

// FUNCTION: LEGORACERS 0x00449ce0
void SplitCommand()
{
	LegoS32 offset = 0;
	g_argc = 0;

	if (!g_commandLine[0]) {
		return;
	}

	LegoS32 i;
	for (; g_argc < (LegoS32) sizeOfArray(g_argv); offset += i, g_argc++) {
		LegoChar* arg = &g_commandLine[offset];

		for (i = 0; arg[i] && arg[i] != ' '; i++) {
		}

		g_argv[g_argc] = arg;

		if (!g_commandLine[offset + i]) {
			g_argc++;
			break;
		}

		g_argv[g_argc][i++] = '\0';
	}
}

// FUNCTION: LEGORACERS 0x00449d50
int WINAPI WinMain(HINSTANCE p_hInstance, HINSTANCE p_hPrevInstance, LPSTR p_lpCmdLine, int p_nShowCmd)
{
	int result;

	// Remaster: also detect the unmodified original, which does not use our mutex.
	HANDLE instance = CreateMutex(NULL, FALSE, "Local\\LegoRacersReborn");
	if (!instance || GetLastError() == ERROR_ALREADY_EXISTS) {
		MessageBox(
			NULL,
			"LEGO Racers is already running. Close it before starting another copy.",
			"LEGO Racers",
			MB_OK
		);
		if (instance) {
			CloseHandle(instance);
		}
		return 0;
	}
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		MessageBox(NULL, "Could not check for another game instance. Please try again.", "LEGO Racers", MB_OK);
		CloseHandle(instance);
		return 1;
	}
	PROCESSENTRY32 entry = {};
	entry.dwSize = sizeof(entry);
	BOOL found = Process32First(snapshot, &entry);
	while (found) {
		if (entry.th32ProcessID != GetCurrentProcessId() &&
			(_stricmp(entry.szExeFile, "LEGORacers.exe") == 0 || _stricmp(entry.szExeFile, "RacersReborn.exe") == 0)) {
			CloseHandle(snapshot);
			CloseHandle(instance);
			MessageBox(
				NULL,
				"Another copy of LEGO Racers is running. Close it before starting this version.",
				"LEGO Racers",
				MB_OK
			);
			return 0;
		}
		found = Process32Next(snapshot, &entry);
	}
	CloseHandle(snapshot);

	CoInitialize(0);

	g_hInstance = p_hInstance;
	g_hPrevInstance = p_hPrevInstance;
	srand((unsigned int) timeGetTime);
	strncpy(g_commandLine, p_lpCmdLine, sizeof(g_commandLine));
	g_commandLine[sizeof(g_commandLine) - 1] = '\0';
	SplitCommand();

	result = GameMain(g_argc, g_argv);

	CoUninitialize();
	CloseHandle(instance);
	return result;
}
