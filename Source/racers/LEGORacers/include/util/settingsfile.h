#ifndef SETTINGSFILE_H
#define SETTINGSFILE_H

#include <string.h>
#include <windows.h>

// Settings are relative to the executable, independent of a shortcut's working directory.
class SettingsFile {
public:
	explicit SettingsFile(const char* p_name)
	{
		m_path[0] = 0;
		DWORD length = GetModuleFileName(NULL, m_path, MAX_PATH);
		m_path[MAX_PATH - 1] = 0;
		char* slash = strrchr(m_path, '\\');
		m_valid = length > 0 && length < MAX_PATH && slash &&
				  static_cast<size_t>(slash + 1 - m_path) + strlen(p_name) < MAX_PATH;
		if (m_valid) {
			strcpy(slash + 1, p_name);
		}
	}

	int ReadChoice(const char* p_section, const char* p_key, const char* const* p_values, int p_count, int p_default)
		const
	{
		char value[128];
		if (!m_valid) {
			return p_default;
		}
		GetPrivateProfileString(p_section, p_key, "", value, sizeof(value), m_path);
		for (int i = 0; i < p_count; ++i) {
			if (_stricmp(value, p_values[i]) == 0) {
				return i;
			}
		}
		return p_default;
	}

	bool Write(const char* p_section, const char* p_key, const char* p_value) const
	{
		return m_valid && WritePrivateProfileString(p_section, p_key, p_value, m_path) != FALSE;
	}

private:
	char m_path[MAX_PATH];
	bool m_valid;
};

#endif
