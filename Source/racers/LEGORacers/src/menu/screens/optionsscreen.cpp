#include "menu/screens/optionsscreen.h"

#include "golstring.h"
#include "menu/menugamecontext.h"
#include "menu/menuscreencreateparams.h"
#include "util/settingsfile.h"
#include "util/videosettings.h"

#include <string.h>

// Remaster fork: this screen intentionally extends the retail class layout.
namespace
{
const char* const g_graphicsTitles[] = {"RACE RESOLUTION", "ANTI-ALIASING", "FILTERING", "V-SYNC", "SHADOWS", "BLOOM", "AMBIENT OCCLUSION"};
const char* const g_graphicsKeys[] =
	{"Resolution", "Antialiasing", "Filtering", "ForceVerticalSync", "Shadows", "Bloom", "AmbientOcclusion"};
const char* const g_graphicsValues[7][7] = {
	{"640x480", "1024x768", "1280x960", "1600x1200", "1920x1440", "2560x1920", "3840x2880"},
	{"off", "2x", "4x", "8x"},
	{"appdriven", "bilinear", "trilinear", "4", "8", "16"},
	{"false", "true"},
	{"off", "cars", "scene"},
	{"off", "subtle", "medium", "strong"},
	{"off", "subtle", "medium", "strong"}
};
const char* const g_graphicsLabels[7][7] = {
	{"640 X 480", "1024 X 768", "1280 X 960", "1600 X 1200", "1920 X 1440", "2560 X 1920", "3840 X 2880"},
	{"OFF", "2X MSAA", "4X MSAA", "8X MSAA"},
	{"ORIGINAL", "BILINEAR", "TRILINEAR", "4X ANISOTROPIC", "8X ANISOTROPIC", "16X ANISOTROPIC"},
	{"OFF", "ON"},
	{"OFF", "CARS ONLY", "FULL SCENE"},
	{"OFF", "SUBTLE", "MEDIUM", "STRONG"},
	{"OFF", "SUBTLE", "MEDIUM", "STRONG"}
};
const LegoS32 g_graphicsCounts[] = {VideoSettings::c_resolutionCount, 4, 6, 2, 3, 4, 4};
const LegoS32 g_graphicsDefaults[] = {2, 2, 5, 1, 1, 1, 0};
} // namespace

OptionsScreen::OptionsScreen()
{
	Reset();
}

OptionsScreen::~OptionsScreen()
{
	Destroy();
}

void OptionsScreen::Reset()
{
	m_driverCount = 0;
	::memset(m_driverNameBuffers, 0, sizeof(m_driverNameBuffers));
	OptionsScreenBase::Reset();
}

void OptionsScreen::CreateVideoWidgets()
{
	CreateTextButton(&m_videoButton, 0xee, 0x42, 0x12);

	// Use the game's selector and label styles. Only the containing flow layout is new.
	CreateCarousel(&m_driverCarousel, 0xf6, 0x3b);
	CreateSelector(&m_driverSelector, &m_driverCarousel, 0xf5, 0x4c);
	Rect selectorRect = *m_driverSelector.GetRect();
	MenuWidget* parent = m_driverSelector.GetParent();
	LegoS32 width = selectorRect.m_right - selectorRect.m_left;
	LegoS32 height = selectorRect.m_bottom - selectorRect.m_top;
	// Reserve space for all seven rows and the footer inside the 416px panel.
	if (height > 54) height = 54;
	m_driverSelector.Destroy();

	MenuWidget::CreateParams panelParams = {};
	panelParams.m_parent = parent;
	panelParams.m_flags = 1;
	panelParams.m_eventHandler = this;
	panelParams.m_golExport = m_golExport;
	panelParams.m_renderer = m_renderer;
	panelParams.m_id = 0x7000;
	// Anchor the column at the original video selector; children flow vertically.
	panelParams.m_rect = selectorRect;
	panelParams.m_rect.m_top = 0;
	panelParams.m_rect.m_right = panelParams.m_rect.m_left + 580;
	panelParams.m_rect.m_bottom = 416;
	m_graphicsPanel.Create(&panelParams, FALSE, 0);

	SettingsFile settings("dgVoodoo.conf");
	SettingsFile gameSettings("Reborn.ini");
    const char* renderers[]={"legacy","d3d11"};
    bool native=gameSettings.ReadChoice("Video","Renderer",renderers,2,0)==1;
	for (LegoS32 row = 0; row < 7; ++row) {
		MenuWidget::CreateParams rowParams = panelParams;
		rowParams.m_parent = &m_graphicsPanel;
		rowParams.m_id = static_cast<undefined2>(0x7010 + row);
		rowParams.m_rect = Rect{0, 0, 580, height};
		m_graphicsRows[row].Create(&rowParams, TRUE, 20);
		m_graphicsPanel.Append(&m_graphicsRows[row], 580, height);

		CreateTextLabel(&m_graphicsTitles[row], 0x102, 0x37, 0x5b);
		m_graphicsTitles[row].SetText(g_graphicsTitles[row]);
		m_graphicsTitles[row].FitToBounds();
		m_graphicsRows[row].Append(&m_graphicsTitles[row], 240, height);

		CreateCarousel(&m_graphicsCarousels[row], 0xf6, 0x3b);
		CreateSelector(&m_graphicsSelectors[row], &m_graphicsCarousels[row], 0xf5, 0x4c);
		m_graphicsRows[row].Append(&m_graphicsSelectors[row], width, height);

		for (LegoS32 choice = 0; choice < g_graphicsCounts[row]; ++choice) {
			CreateTextLabel(&m_graphicsValues[row][choice], 0xf7, 0xf5, 0x12);
			m_graphicsValues[row][choice].SetText(
				row == 0 ? VideoSettings::c_resolutionLabels[choice] : g_graphicsLabels[row][choice]
			);
			m_graphicsValues[row][choice].FitToBounds();
			Rect valueBounds = {0, 0, 236, height};
			m_graphicsValues[row][choice].SetRect(&valueBounds);
			m_graphicsCarousels[row].AddItem(&m_graphicsValues[row][choice]);
		}
		m_savedGraphicsChoices[row] = settings.ReadChoice(
			"DirectX",
			g_graphicsKeys[row],
			g_graphicsValues[row],
			(row == 0 ? 7 : g_graphicsCounts[row]),
			g_graphicsDefaults[row]
		);
		if (row == 0) {
			m_savedGraphicsChoices[row] = gameSettings.ReadChoice(
				"Video",
				"RaceResolution",
				VideoSettings::c_resolutions,
				VideoSettings::c_resolutionCount,
				0
			);
		}
		if (row >= 4 || (native && row > 0)) {
			m_savedGraphicsChoices[row] = gameSettings.ReadChoice(
				"Video",
				g_graphicsKeys[row],
				g_graphicsValues[row],
				g_graphicsCounts[row],
				(row < 4 ? m_savedGraphicsChoices[row] : g_graphicsDefaults[row])
			);
		}
		m_graphicsCarousels[row].SetSelection(m_savedGraphicsChoices[row]);
	}
	CreateTextLabel(&m_graphicsFooter, 0x102, 0x37, 0x5b);
	m_graphicsFooter.SetText(native ? "BACK TO SAVE. RACE SETTINGS APPLY NEXT RACE." : "BACK TO SAVE. RESTART TO APPLY AA AND FILTERING.");
	m_graphicsFooter.FitToBounds();
	m_graphicsPanel.Append(&m_graphicsFooter, 580, 18);
	HideVideoWidgets();
}

LegoBool32 OptionsScreen::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	return OptionsScreenBase::Initialize(p_context, p_createParams) != FALSE;
}

void OptionsScreen::OnIconUnfocused(MenuWidget* p_widget)
{
	if (p_widget == &m_videoButton) {
		m_nextPage = 3;
	}
	OptionsScreenBase::OnIconUnfocused(p_widget);
	if (m_nextMenuId != 0xffff) {
		m_navPending = TRUE;
	}
	m_clickedWidget = p_widget;
}

void OptionsScreen::ShowMainButtons()
{
	m_infoLabel.SetFlags(2);
	m_videoButton.SetFlags(2);
	OptionsScreenBase::ShowMainButtons();
}

void OptionsScreen::HideMainButtons()
{
	m_videoButton.ClearFlags(2);
	OptionsScreenBase::HideMainButtons();
}

void OptionsScreen::ShowVideoWidgets()
{
	m_graphicsPanel.SetFlags(2);
	for (LegoS32 row = 0; row < 7; ++row) {
		m_graphicsSelectors[row].SetFlags(2);
	}
	m_infoLabel.ClearFlags(2);
	m_backButton.SetTextByIndex(0x10);
	m_graphicsSelectors[0].Select(4);
}

void OptionsScreen::HideVideoWidgets()
{
	m_graphicsPanel.ClearFlags(2);
	for (LegoS32 row = 0; row < 7; ++row) {
		m_graphicsSelectors[row].ClearFlags(2);
	}
}

void OptionsScreen::ApplyPageChange()
{
	if (m_page == 3) {
		if (!SaveVideoOptions()) {
			m_nextPage = 3;
			return;
		}
		HideVideoWidgets();
	}
	if (m_nextPage == 3) {
		ShowVideoWidgets();
	}
	OptionsScreenBase::ApplyPageChange();
}

LegoBool32 OptionsScreen::SaveVideoOptions()
{
	// dgVoodoo reads these settings at startup. Do not recreate the live device here.
	SettingsFile settings("dgVoodoo.conf");
	SettingsFile gameSettings("Reborn.ini");
    const char* renderers[]={"legacy","d3d11"};
    bool native=gameSettings.ReadChoice("Video","Renderer",renderers,2,0)==1;
	bool success = true;
	for (LegoS32 row = 0; row < 7; ++row) {
		LegoS32 selected = static_cast<LegoS32>(m_graphicsCarousels[row].GetSelectedIndex());
		if (selected < 0 || selected >= g_graphicsCounts[row]) {
			success = false;
			continue;
		}
		if (selected != m_savedGraphicsChoices[row]) {
			bool written =
				row == 0 ? gameSettings.Write("Video", "RaceResolution", VideoSettings::c_resolutions[selected])
						 : ((row >= 4 || native) ? gameSettings.Write("Video", g_graphicsKeys[row], g_graphicsValues[row][selected])
									 : settings.Write("DirectX", g_graphicsKeys[row], g_graphicsValues[row][selected]));
			if (written) {
				m_savedGraphicsChoices[row] = selected;
			}
			else {
				success = false;
			}
		}
	}
	if (!success) {
		m_graphicsFooter.SetText("SAVE FAILED. CHECK GAME FOLDER PERMISSIONS.");
	}
	return success;
}
