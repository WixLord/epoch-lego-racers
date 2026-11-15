#ifndef DIAGNOSTICLOG_H
#define DIAGNOSTICLOG_H
#include <stdio.h>
#include <string.h>
#include <windows.h>

// Append beside the executable, independent of the process working directory.
inline void AppendDiagnosticLog(const char* filename, const char* message)
{
	char path[MAX_PATH];
	DWORD length = GetModuleFileName(NULL, path, MAX_PATH);
	if (!length || length >= MAX_PATH) {
		return;
	}
	char* slash = strrchr(path, '\\');
	if (!slash || static_cast<size_t>(slash + 1 - path) + strlen(filename) >= MAX_PATH) {
		return;
	}
	strcpy(slash + 1, filename);
	FILE* file = fopen(path, "a");
	if (!file) {
		return;
	}
	fprintf(file, "%s\n", message);
	fclose(file);
}
#endif
