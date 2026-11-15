#ifndef VIDEOSETTINGS_H
#define VIDEOSETTINGS_H

#include "util/settingsfile.h"

#include <stdlib.h>

namespace VideoSettings
{
enum {
	c_resolutionCount = 10
};
static const char* const c_resolutions[c_resolutionCount] = {
	"desktop",
	"1280x720",
	"1920x1080",
	"2560x1440",
	"3440x1440",
	"3840x1600",
	"3840x2160",
	"5120x1440",
	"1920x1440",
	"2560x1920"
};
static const char* const c_resolutionLabels[c_resolutionCount] = {
	"DESKTOP",
	"1280 X 720",
	"1920 X 1080",
	"2560 X 1440",
	"3440 X 1440",
	"3840 X 1600",
	"3840 X 2160",
	"5120 X 1440",
	"1920 X 1440",
	"2560 X 1920"
};

inline void GetRaceResolution(unsigned long& p_width, unsigned long& p_height)
{
	SettingsFile settings("Reborn.ini");
	int choice = settings.ReadChoice("Video", "RaceResolution", c_resolutions, c_resolutionCount, 0);
	if (choice == 0) {
		DEVMODE mode = {};
		mode.dmSize = sizeof(mode);
		if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &mode) && mode.dmPelsWidth >= 640 &&
			mode.dmPelsHeight >= 480) {
			p_width = mode.dmPelsWidth;
			p_height = mode.dmPelsHeight;
			return;
		}
		choice = 2;
	}
	const char* value = c_resolutions[choice];
	p_width = static_cast<unsigned long>(atoi(value));
	p_height = static_cast<unsigned long>(atoi(strchr(value, 'x') + 1));
}
} // namespace VideoSettings

#endif
