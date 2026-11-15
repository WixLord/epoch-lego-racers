#include "menu/screens/carbuildscreen.h"

#include "audio/soundgroupbinding.h"
#include "audio/soundnode.h"
#include "camera/golcamera.h"
#include "golhashtable.h"
#include "golmaterial.h"
#include "golstream.h"
#include "input/inputdevice.h"
#include "input/inputmanager.h"
#include "input/mousedevice.h"
#include "menu/menugamecontext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DECOMP_SIZE_ASSERT(CarBuildScreen, 0x3c34)

// GLOBAL: LEGORACERS 0x004af668
LegoFloat g_carBuildPreviewMouseScale = 0.01f;

// GLOBAL: LEGORACERS 0x004c1d12
const LegoU16 g_carBuildTextIds[] = {0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0};

// GLOBAL: LEGORACERS 0x004c1d38
const LegoChar g_carBuildHelpFontName[8] = {'f', 'o', 'n', 't', '_', 'h', 'l', 'p'};

// GLOBAL: LEGORACERS 0x004c1d78
const LegoS32 g_carBuildDragHorizontalOffsets[] = {-1, -1, 0,  1,  1,  1,  0,  -1, -1, -1, -1, 0,  1,  1,  1,  0,
												   0,  -1, -1, -1, 0,  1,  1,  1,  1,  0,  -1, -1, -1, 0,  1,  1,
												   1,  1,  0,  -1, -1, -1, 0,  1,  1,  1,  1,  0,  -1, -1, -1, 0,
												   0,  1,  1,  1,  0,  -1, -1, -1, -1, 0,  1,  1,  1,  0,  -1, -1};

// GLOBAL: LEGORACERS 0x004c1e78
const LegoS32 g_carBuildDragVerticalOffsets[] = {0,  1,  1,  1,  0,  -1, -1, -1, -1, 0,  1,  1,  1,  0,  -1, -1,
												 -1, -1, 0,  1,  1,  1,  0,  -1, -1, -1, -1, 0,  1,  1,  1,  0,
												 0,  -1, -1, -1, 0,  1,  1,  1,  1,  0,  -1, -1, -1, 0,  1,  1,
												 1,  1,  0,  -1, -1, -1, 0,  1,  1,  1,  1,  0,  -1, -1, -1, 0};

// FUNCTION: LEGORACERS 0x004736b0
CarBuildScreen::CarBuildScreen()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x004737b0
CarBuildScreen::~CarBuildScreen()
{
	m_helpStrings.ReleaseOwnedBuffers();
	Destroy();
}

// FUNCTION: LEGORACERS 0x004738a0
void CarBuildScreen::Reset()
{
	m_doubleClickMs = 0;
	m_dragDelayMs = 0;
	m_mouseOrbit = FALSE;
	m_paletteOpen = FALSE;
	m_lastMouseX = m_lastMouseY = -1;
	m_orbitTravel = 0;
	CarModelScreenBase::Reset();
}

// FUNCTION: LEGORACERS 0x004738c0
void CarBuildScreen::CreateWidgets()
{
	CarModelScreenBase::CreateWidgets();
	CreateHotspotButton(&m_movePad, 0xb5, 0xb5);
	CreateButton(&m_rotateButton, 0xb2, 0xb0);
	CreateButton(&m_placeButton, 0xb1, 0xb0);
	CreateButton(&m_undoButton, 0xb3, 0xb0);
	CreateHotspotButton(&m_viewPad, 0xb4, 0xb4);

	MenuButton* glyph0 = &m_pieceViewRegion;
	CreateButton(glyph0, 0xb8, 0xb0);

	MenuButton* glyph1 = &m_carViewRegion;
	CreateButton(glyph1, 0xb7, 0xb0);

	glyph0->ClearFlags(MenuIcon::c_flagSelected);
	glyph1->ClearFlags(MenuIcon::c_flagSelected);
	CreateButton(&m_doneButton, 0x3f, 0xb0);
}

// FUNCTION: LEGORACERS 0x004739a0
LegoBool32 CarBuildScreen::Initialize(MenuGameContext* p_context, MenuScreenCreateParams* p_createParams)
{
	if (!CarModelScreenBase::Initialize(p_context, p_createParams)) {
		return FALSE;
	}

	p_context->m_saveSystem.GetGameState().SetLanguageResourcePath();
	m_helpStrings.UseOwnedBuffers();
	m_helpStrings.Load("carbuild.srf");

	if (g_hashTable) {
		g_hashTable->SetCurrentEntryFromString("MENUDATA");
	}

	m_categorySelector.Select(4);
	ArrangeBuilder();
	return m_initialized;
}

void CarBuildScreen::ArrangeBuilder()
{
	m_rootIcon.Deselect(0);
	m_rootIcon.ClearFocus();
	m_hoverIcon = NULL;
	m_selectedIcon = NULL;
	m_movePad.DetachFromParent();
	m_viewPad.DetachFromParent();
	m_placeButton.DetachFromParent();
	m_rotateButton.DetachFromParent();
	m_pieceViewRegion.DetachFromParent();
	m_carViewRegion.DetachFromParent();
	m_movePad.RemoveFromParent();
	m_viewPad.RemoveFromParent();
	m_placeButton.RemoveFromParent();
	m_rotateButton.RemoveFromParent();
	m_pieceViewRegion.RemoveFromParent();
	m_carViewRegion.RemoveFromParent();
	// Preserve the original full-screen LEGO background.
	// Keep selector objects alive for existing theme callbacks, but replace
	// their old arrangement with the shared box layout and their own arrows.
	m_categorySelector.DetachFromParent();
	m_pieceSelector.DetachFromParent();
	m_categorySelector.RemoveFromParent();
	m_pieceSelector.RemoveFromParent();
	Rect undoRect = *m_undoButton.GetRect();
	LegoFloat undoAspect =
		static_cast<LegoFloat>(undoRect.m_right - undoRect.m_left) / (undoRect.m_bottom - undoRect.m_top);
	Rect themeRect = *m_categoryCarousel.GetFirstChild()->GetRect();
	LegoFloat themeAspect =
		static_cast<LegoFloat>(themeRect.m_right - themeRect.m_left) / (themeRect.m_bottom - themeRect.m_top);
	MenuButton* arrows[4] = {
		&m_categorySelector.GetPrevButton(),
		&m_categorySelector.GetNextButton(),
		&m_pieceSelector.GetPrevButton(),
		&m_pieceSelector.GetNextButton()
	};
	for (LegoS32 i = 0; i < 4; i++) {
		arrows[i]->AttachToParent(GetRootIcon());
		arrows[i]->SetIconEventHandler(this);
		arrows[i]->Enable(0);
	}
	m_builderLayout.Create(GetRootIcon(), MenuBox::c_horizontal, 12, 12);
	m_browserColumn.Create(&m_builderLayout, MenuBox::c_vertical, 0, 10);
	m_previewColumn.Create(&m_builderLayout, MenuBox::c_vertical, 0, 10);
	m_themeRow.Create(&m_browserColumn, MenuBox::c_horizontal, 0, 8);
	m_toolbar.Create(&m_previewColumn, MenuBox::c_horizontal, 0, 10);
	m_paintColumn.Create(&m_toolbar, MenuBox::c_vertical, 4, 4);
	m_paintRow.Create(NULL, MenuBox::c_horizontal, 10, 8);
	m_hintRow.Create(&m_previewColumn, MenuBox::c_horizontal, 0, 0);
	MenuTextLabel* labels[3] = {&m_paintLabel, &m_browserLabel, &m_builderHint};
	const char* text[3] = {
		"Chassis color...",
		"Scroll for more bricks",
		"Click: place / R: rotate / Right drag: orbit / Wheel: zoom / Right click + Delete: remove"
	};
	for (LegoS32 i = 0; i < 3; i++) {
		MenuTextLabel::CreateParams params;
		::memset(&params, 0, sizeof(params));
		params.m_golExport = m_golExport;
		params.m_renderer = m_renderer;
		params.m_font = GetHelpFont(1);
		params.m_stringTable = &m_helpStrings;
		params.m_color.m_color.m_red = params.m_color.m_color.m_grn = params.m_color.m_color.m_blu =
			params.m_color.m_color.m_alp = 255;
		labels[i]->Create(&params, NULL);
		labels[i]->SetText(text[i]);
		labels[i]->FitToBounds();
	}
	m_paintCount = m_context->m_colorTable.GetColorCount();
	if (m_paintCount > 32) {
		m_paintCount = 32;
	}
	for (LegoS32 i = 0; i < m_paintCount; i++) {
		MenuColorBox::CreateParams params;
		::memset(&params, 0, sizeof(params));
		params.m_golExport = m_golExport;
		params.m_renderer = m_renderer;
		const GolMaterialColor& color =
			m_context->m_colorTable.GetMaterial(m_context->m_colorTable.GetMaterialIndexForColorRecord(i))
				->GetDiffuse();
		params.m_color.m_color.m_red = color.m_red;
		params.m_color.m_color.m_grn = color.m_grn;
		params.m_color.m_color.m_blu = color.m_blu;
		params.m_color.m_color.m_alp = 255;
		m_paintSwatches[i].Create(&params);
		m_paintRow.Add(&m_paintSwatches[i]);
	}
	m_paintColumn.Add(&m_paintLabel, 0, 1);

	m_browserFooter.Create(&m_browserColumn, MenuBox::c_horizontal, 0, 8);
	Rect bounds = *GetRootIcon()->GetRect();
	LegoS32 height = bounds.m_bottom - bounds.m_top;
	LegoS32 header = height / 10;
	m_builderLayout.Add(&m_browserColumn, 0, 3);
	m_builderLayout.Add(&m_previewColumn, 0, 7);
	m_browserColumn.Add(&m_themeRow, header);
	m_browserColumn.Add(&m_partCarousel);
	m_browserColumn.Add(&m_browserFooter, header / 2);
	m_themeRow.Add(&m_categorySelector.GetPrevButton(), header / 2, 1, 1);
	m_themeRow.Add(&m_categoryCarousel, 0, 1, themeAspect);
	m_themeRow.Add(&m_categorySelector.GetNextButton(), header / 2, 1, 1);
	m_browserFooter.Add(&m_pieceSelector.GetPrevButton(), header / 2, 1, 1);
	m_browserFooter.Add(&m_browserLabel);
	m_browserFooter.Add(&m_pieceSelector.GetNextButton(), header / 2, 1, 1);
	m_toolbar.Add(&m_undoButton, header, 1, undoAspect);
	m_toolbar.Add(&m_paintColumn);
	m_toolbar.Add(&m_doneButton, header, 1, 1);
	m_previewColumn.Add(&m_toolbar, header);
	MenuWidget* frame = m_sceneView.GetParent()->GetParent();
	m_previewColumn.Add(frame);
	m_hintRow.Add(&m_builderHint);
	m_previewColumn.Add(&m_hintRow, header / 3);
	bounds.m_left = bounds.m_top = 0;
	m_builderLayout.SetRect(&bounds);
	Rect scene = *m_sceneView.GetParent()->GetRect();
	scene.m_right -= scene.m_left;
	scene.m_bottom -= scene.m_top;
	scene.m_left = scene.m_top = 0;
	m_sceneView.SetRect(&scene);
	m_sceneView.GetCamera()->SetAspectRatio(static_cast<LegoFloat>(scene.m_right) / scene.m_bottom);
	Rect viewport = *m_sceneView.GetGlobalRect();
	m_sceneView.GetCamera()->SetViewport(&viewport);
	Rect category = *m_categoryCarousel.GetRect();
	category.m_right -= category.m_left;
	category.m_bottom -= category.m_top;
	category.m_left = category.m_top = 0;
	for (MenuWidget* child = m_categoryCarousel.GetFirstChild(); child; child = child->GetNextSibling()) {
		child->SetRect(&category);
		m_categoryCarousel.LayoutItem(child);
	}
	m_categoryCarousel.SnapToSelection();
	// A popup is the only overlay here; its contents still use the shared box layout.
	MenuColorBox::CreateParams palette;
	::memset(&palette, 0, sizeof(palette));
	palette.m_golExport = m_golExport;
	palette.m_renderer = m_renderer;
	palette.m_parent = GetRootIcon();
	palette.m_flags = 1;
	palette.m_color.m_color.m_red = 12;
	palette.m_color.m_color.m_grn = 12;
	palette.m_color.m_color.m_blu = 90;
	palette.m_color.m_color.m_alp = 255;
	Rect paint = *m_paintColumn.GetGlobalRect();
	palette.m_rect = Rect{paint.m_left, paint.m_bottom + 8, paint.m_right, paint.m_bottom + header};
	m_paletteBackground.Create(&palette);
	m_paintRow.SetParent(&m_paletteBackground);
	Rect paletteContent = {0, 0, paint.m_right - paint.m_left, header - 8};
	m_paintRow.SetRect(&paletteContent);
	m_paletteBackground.ClearFlags(2);
	m_paletteOpen = FALSE;
	palette.m_parent = &m_paintColumn;
	palette.m_rect = Rect{0, 0, paint.m_right - paint.m_left, paint.m_bottom - paint.m_top};
	m_paintButtonBackground.Create(&palette);
	m_paintLabel.SetParent(&m_paintColumn); // Label above the button background.
	m_partCarousel.EnableGridBrowser();
	m_partPlacement.PitchViewAnalog(0);
}

// FUNCTION: LEGORACERS 0x00473a20 FOLDED
LegoBool32 CarBuildScreen::PointInRect(Rect* p_rect, LegoS32 p_x, LegoS32 p_y)
{
	return p_x >= p_rect->m_left && p_x <= p_rect->m_right && p_y >= p_rect->m_top && p_y <= p_rect->m_bottom;
}

// FUNCTION: LEGORACERS 0x00473a50
LegoBool32 CarBuildScreen::HandleSceneClick(InputEventQueue::Event* p_event, undefined4 p_cursorX, undefined4 p_cursorY)
{
	if (p_event->m_isRepeat) {
		return FALSE;
	}

	LegoS32 x = static_cast<LegoS32>(p_cursorX);
	LegoS32 y = static_cast<LegoS32>(p_cursorY);

	if (!PointInRect(m_sceneView.GetGlobalRect(), x, y)) {
		return FALSE;
	}

	if (PointInRect(m_pieceViewRegion.GetRect(), x, y)) {
		if (m_doubleClickMs) {
			if (m_mode == c_modeBrowse) {
				if (m_partPlacement.CommitPiece()) {
					m_nextMode = c_modeBusy;
					m_carouselAction = 4;
				}

				m_doubleClickMs = 0;
				return TRUE;
			}
		}

		m_nextMode = c_modePieceView;
		m_doubleClickMs = c_carBuildClickDelay;
		m_rootIcon.SetFocus();

		if (m_partPlacement.GetFocusedPane() != 1) {
			m_partPlacement.FocusPiece();
		}

		return TRUE;
	}

	if (PointInRect(m_carViewRegion.GetRect(), x, y)) {
		m_nextMode = c_modeCarView;
		m_rootIcon.SetFocus();

		if (m_partPlacement.GetFocusedPane() != 2) {
			m_partPlacement.FocusCar();
		}

		return TRUE;
	}

	return FALSE;
}

// FUNCTION: LEGORACERS 0x00473b80
void CarBuildScreen::HandleCursorDrag(LegoS32 p_deltaX, LegoS32 p_deltaY)
{
	if (m_dragDelayMs) {
		return;
	}

	LegoS32 keyCode = c_carBuildRegionSource;
	if (p_deltaX >= c_carBuildDragThreshold) {
		if (p_deltaY >= c_carBuildDragThreshold) {
			keyCode = c_carBuildRegionSource | 4;
		}
		else if (p_deltaY > -c_carBuildDragThreshold) {
			keyCode = c_carBuildRegionSource | 3;
		}
		else {
			keyCode = c_carBuildRegionSource | 2;
		}
	}
	else if (p_deltaX <= -c_carBuildDragThreshold) {
		if (p_deltaY >= c_carBuildDragThreshold) {
			keyCode = c_carBuildRegionSource | 6;
		}
		else if (p_deltaY <= -c_carBuildDragThreshold) {
			keyCode = c_carBuildRegionSource | 8;
		}
		else {
			keyCode = c_carBuildRegionSource | 7;
		}
	}
	else if (p_deltaY >= c_carBuildDragThreshold) {
		keyCode = c_carBuildRegionSource | 5;
	}
	else if (p_deltaY <= -c_carBuildDragThreshold) {
		keyCode = c_carBuildRegionSource | 1;
	}

	LegoS32 regionId = keyCode & InputDevice::c_keyCodeMask;
	if (!regionId) {
		return;
	}

	LegoS32 categoryIndex = m_partPlacement.GetViewSlot();
	LegoS32 index = regionId - 1;
	MovePieceByDrag(
		g_carBuildDragHorizontalOffsets[categoryIndex * 8 + index],
		g_carBuildDragVerticalOffsets[categoryIndex * 8 + index],
		index & 1,
		0
	);
	m_dragDelayMs = c_carBuildDragDelay;
}

// FUNCTION: LEGORACERS 0x00473c40
void CarBuildScreen::EnterBrowseMode()
{
	if (m_movePad.GetStateFlags() & 4) {
		m_movePad.Unfocus(0);
	}

	if (m_viewPad.GetStateFlags() & 4) {
		m_viewPad.Unfocus(0);
	}

	m_cursor->SetCursorEnabled(TRUE);
}

// FUNCTION: LEGORACERS 0x00473c90
void CarBuildScreen::EnterPieceViewMode()
{
	m_categorySelector.Disable(0);
	m_pieceSelector.Disable(0);
	m_viewPad.Disable(0);
	m_placeButton.Disable(0);
	m_rotateButton.Disable(0);
	m_undoButton.Disable(0);
	m_doneButton.Disable(0);
	m_movePad.Select(5);
}

// FUNCTION: LEGORACERS 0x00473d20
void CarBuildScreen::ExitPieceViewMode()
{
	if (m_movePad.GetStateFlags() & 4) {
		m_movePad.Unfocus(5);
	}

	m_categorySelector.Enable(0);
	m_pieceSelector.Enable(0);
	m_viewPad.Enable(0);
	m_placeButton.Enable(0);
	m_rotateButton.Enable(0);
	m_undoButton.Enable(0);
	m_doneButton.Enable(0);
	m_cursor->SetCursorEnabled(TRUE);
}

// FUNCTION: LEGORACERS 0x00473dc0
void CarBuildScreen::EnterCarViewMode()
{
	m_categorySelector.Disable(0);
	m_pieceSelector.Disable(0);
	m_movePad.Disable(0);
	m_placeButton.Disable(0);
	m_rotateButton.Disable(0);
	m_undoButton.Disable(0);
	m_viewPad.Select(5);
	m_cursor->SetCursorEnabled(FALSE);
}

// FUNCTION: LEGORACERS 0x00473e50
void CarBuildScreen::ExitCarViewMode()
{
	if (m_viewPad.GetStateFlags() & 4) {
		m_viewPad.Unfocus(5);
	}

	m_categorySelector.Enable(0);
	m_pieceSelector.Enable(0);
	m_movePad.Enable(0);
	m_placeButton.Enable(0);
	m_rotateButton.Enable(0);
	m_undoButton.Enable(0);
	m_cursor->SetCursorEnabled(TRUE);
}

// FUNCTION: LEGORACERS 0x004164c0 FOLDED
void CarBuildScreen::ExitBrowseMode()
{
}

// FUNCTION: LEGORACERS 0x004164c0 FOLDED
void CarBuildScreen::EnterBusyMode()
{
}

// FUNCTION: LEGORACERS 0x004164c0 FOLDED
void CarBuildScreen::ExitBusyMode()
{
}

// FUNCTION: LEGORACERS 0x00473ee0
LegoBool32 CarBuildScreen::HandleBuildKey(MenuWidget*, InputEventQueue::Event* p_event, undefined4, undefined4)
{
	return FALSE;
}

// FUNCTION: LEGORACERS 0x00474330
LegoBool32 CarBuildScreen::HandleViewModeKey(MenuWidget*, InputEventQueue::Event* p_event, undefined4, undefined4)
{
	return FALSE;
}

// FUNCTION: LEGORACERS 0x004743f0
LegoBool32 CarBuildScreen::HandleMouseButton(
	InputEventQueue::Event* p_event,
	undefined4 p_cursorX,
	undefined4 p_cursorY
)
{
	return FALSE;
}

// FUNCTION: LEGORACERS 0x00474470
LegoBool32 CarBuildScreen::RouteWidgetKeyUp(
	MenuWidget* p_source,
	InputEventQueue::Event* p_event,
	undefined4 p_cursorX,
	undefined4 p_cursorY
)
{
	if (p_source == GetRootIcon()) {
		return HandleMouseButton(p_event, p_cursorX, p_cursorY);
	}

	if (p_source == &m_partCarousel) {
		OnWidgetValueChanged(&m_pieceSelector);
		return TRUE;
	}

	return FALSE;
}

// FUNCTION: LEGORACERS 0x004744c0
LegoBool32 CarBuildScreen::HandleKeyDown(
	MenuWidget* p_source,
	InputEventQueue::Event* p_event,
	undefined4 p_cursorX,
	undefined4 p_cursorY
)
{
	if (m_paletteOpen && p_event->m_keyCode != c_carBuildMouseButton0) {
		if (p_event->m_keyCode == (InputDevice::c_sourceKeyboard | 1)) {
			m_paletteOpen = FALSE;
			m_paletteBackground.ClearFlags(2);
		}
		return TRUE;
	}
	if (m_paletteOpen) {
		p_source = GetRootIcon();
	}
	if (p_source == GetRootIcon() && m_mode != c_modeBusy && !m_navPending) {
		if (p_event->m_keyCode == c_carBuildMouseButton0 && !p_event->m_isRepeat) {
			if (m_paletteOpen) {
				for (LegoS32 i = 0; i < m_paintCount; i++) {
					if (PointInRect(m_paintSwatches[i].GetGlobalRect(), p_cursorX, p_cursorY)) {
						SetChassisPaint(i);
					}
				}
				m_paletteOpen = FALSE;
				m_paletteBackground.ClearFlags(2);
				return TRUE;
			}
			if (PointInRect(m_paintColumn.GetGlobalRect(), p_cursorX, p_cursorY)) {
				m_paletteOpen = TRUE;
				m_paletteBackground.SetFlags(2);
				return TRUE;
			}
			if (PointInRect(m_partCarousel.GetGlobalRect(), p_cursorX, p_cursorY)) {
				if (m_partCarousel.PickGridChoice(p_cursorX, p_cursorY)) {
					OnWidgetValueChanged(&m_pieceSelector);
				}
				return TRUE;
			}
			if (PointInRect(m_categorySelector.GetPrevButton().GetGlobalRect(), p_cursorX, p_cursorY)) {
				m_categoryCarousel.ScrollPrevious();
				OnWidgetValueChanged(&m_categorySelector);
				return TRUE;
			}
			if (PointInRect(m_categorySelector.GetNextButton().GetGlobalRect(), p_cursorX, p_cursorY)) {
				m_categoryCarousel.ScrollNext();
				OnWidgetValueChanged(&m_categorySelector);
				return TRUE;
			}
			if (PointInRect(m_pieceSelector.GetPrevButton().GetGlobalRect(), p_cursorX, p_cursorY)) {
				m_partCarousel.ScrollGridBrowser(-5);
				return TRUE;
			}
			if (PointInRect(m_pieceSelector.GetNextButton().GetGlobalRect(), p_cursorX, p_cursorY)) {
				m_partCarousel.ScrollGridBrowser(5);
				return TRUE;
			}
		}

		if (p_event->m_keyCode == (InputDevice::c_sourceKeyboard | 0x13) && !p_event->m_isRepeat) {
			m_partPlacement.RotatePiece();
			return TRUE;
		}
		if (p_event->m_keyCode == c_carBuildKeyboardDelete && !p_event->m_isRepeat) {
			m_partPlacement.DeleteSelectedBrick();
			return TRUE;
		}
		if (PointInRect(m_sceneView.GetGlobalRect(), p_cursorX, p_cursorY)) {
			if (p_event->m_keyCode == c_carBuildMouseButton1) {
				if (!p_event->m_isRepeat) {
					m_mouseOrbit = TRUE;
					m_orbitTravel = 0;
					m_lastMouseX = p_cursorX;
					m_lastMouseY = p_cursorY;
				}
				return TRUE;
			}
			if (p_event->m_keyCode == c_carBuildMouseButton0) {
				if (!p_event->m_isRepeat && m_partPlacement.PlaceAtCursor(p_cursorX, p_cursorY, TRUE)) {
					m_nextMode = c_modeBusy;
					m_carouselAction = 4;
				}
				return TRUE;
			}
		}
	}

	if (m_mode != c_modeBusy) {
		if (!m_navPending) {
			CarBuildScreenBase::HandleKeyDown(p_source, p_event, p_cursorX, p_cursorY);
			if (p_source != GetRootIcon() || !HandleBuildKey(p_source, p_event, p_cursorX, p_cursorY)) {
				switch (m_mode) {
				case c_modeBrowse:
					return RouteWidgetKeyUp(p_source, p_event, p_cursorX, p_cursorY);
				case c_modeCarView:
					return FALSE;
				case c_modePieceView:
					return FALSE;
				default:
					return FALSE;
				}
			}
		}
	}

	return TRUE;
}

// FUNCTION: LEGORACERS 0x00474550
LegoBool32 CarBuildScreen::HandleKeyUp(
	MenuWidget* p_source,
	InputEventQueue::Event* p_event,
	undefined4 p_cursorX,
	undefined4 p_cursorY
)
{
	if (p_event->m_keyCode == c_carBuildMouseButton1 && m_mouseOrbit) {
		m_mouseOrbit = FALSE;
		if (m_orbitTravel < 5 && PointInRect(m_sceneView.GetGlobalRect(), p_cursorX, p_cursorY)) {
			m_partPlacement.SelectAtCursor(p_cursorX, p_cursorY);
		}
		return TRUE;
	}
	if (p_event->m_keyCode == c_carBuildMouseButton0 &&
		PointInRect(m_sceneView.GetGlobalRect(), p_cursorX, p_cursorY)) {
		return TRUE;
	}

	if (m_mode == c_modeBusy) {
		return TRUE;
	}

	CarBuildScreenBase::HandleKeyUp(p_source, p_event, p_cursorX, p_cursorY);
	if (p_source == GetRootIcon() && HandleViewModeKey(p_source, p_event, p_cursorX, p_cursorY)) {
		return TRUE;
	}

	if ((p_event->m_keyCode & InputDevice::c_sourceMask) == InputDevice::c_sourceMouse) {
		m_nextMode = c_modeBrowse;
	}

	return FALSE;
}

// FUNCTION: LEGORACERS 0x004745e0
void CarBuildScreen::OnWidgetFocused(MenuWidget* p_source)
{
	if (p_source == GetRootIcon()) {
		switch (m_nextMode) {
		case 2:
			m_soundGroupBinding->PlaySoundByIndex(5);
			break;
		case 3:
			m_soundGroupBinding->PlaySoundByIndex(17);
			break;
		default:
			return;
		}

		m_cursor->SetCursorEnabled(FALSE);
	}

	if (p_source == &m_movePad) {
		m_cursor->SetCursorEnabled(FALSE);
	}
}

// FUNCTION: LEGORACERS 0x00474640
void CarBuildScreen::OnWidgetUnfocused(MenuWidget* p_source)
{
	if (p_source == GetRootIcon()) {
		switch (m_mode) {
		case c_modeCarView:
			m_partPlacement.SnapViewRotation();
			m_partPlacement.SnapViewPitch();
			m_soundGroupBinding->PlaySoundByIndex(5);
			break;
		case c_modePieceView:
			m_soundGroupBinding->PlaySoundByIndex(17);
			break;
		default:
			return;
		}

		m_cursor->SetCursorEnabled(TRUE);
	}

	if (p_source == &m_movePad) {
		m_cursor->SetCursorEnabled(TRUE);
	}
}

// FUNCTION: LEGORACERS 0x004746c0
undefined4 CarBuildScreen::OnWidgetKeyDown(MenuWidget* p_source, void*, undefined4 p_cursorX, undefined4 p_cursorY)
{
	if (p_source == GetRootIcon()) {
		switch (m_mode) {
		case c_modeCarView:
			HandleViewDrag(p_cursorX, p_cursorY);
			break;
		case c_modePieceView:
			HandleCursorDrag(p_cursorX, p_cursorY);
			return 1;
		}
	}

	return 0;
}

// FUNCTION: LEGORACERS 0x00474710
void CarBuildScreen::OnIconFocused(MenuIcon* p_icon)
{
	m_clickedWidget = NULL;
	if (p_icon == &m_movePad) {
		m_nextMode = c_modePieceView;
	}
	else if (p_icon == &m_viewPad) {
		m_nextMode = c_modeCarView;
	}
}

// FUNCTION: LEGORACERS 0x00474750
void CarBuildScreen::OnIconUnfocused(MenuWidget* p_source)
{
	if (m_paletteOpen) {
		return;
	}
	if (m_mode != c_modeBusy) {
		m_clickedWidget = p_source;
		m_nextMode = c_modeBrowse;

		if (p_source == &m_categorySelector.GetPrevButton() || p_source == &m_categorySelector.GetNextButton()) {
			if (p_source == &m_categorySelector.GetPrevButton()) {
				m_categoryCarousel.ScrollPrevious();
			}
			else {
				m_categoryCarousel.ScrollNext();
			}
			OnWidgetValueChanged(&m_categorySelector);
			return;
		}
		if (p_source == &m_pieceSelector.GetPrevButton() || p_source == &m_pieceSelector.GetNextButton()) {
			m_partCarousel.ScrollGridBrowser(p_source == &m_pieceSelector.GetPrevButton() ? -5 : 5);
			return;
		}
		if (p_source == &m_placeButton) {
			if (m_partPlacement.CommitPiece()) {
				m_nextMode = c_modeBusy;
				m_carouselAction = 4;
			}
			return;
		}
		else if (p_source == &m_rotateButton) {
			m_partPlacement.RotatePiece();
			m_soundGroupBinding->PlaySoundByIndex(0x16);
			return;
		}
		else if (p_source == &m_undoButton) {
			if (UndoPiece()) {
				m_nextMode = c_modeResetView;
			}
			return;
		}
		else if (p_source == &m_doneButton) {
			m_nextMenuId = 0x11;
			m_navPending = 1;
			return;
		}
	}

	CarModelScreenBase::OnIconUnfocused(p_source);
}

// FUNCTION: LEGORACERS 0x00474820
void CarBuildScreen::OnWidgetValueChanged(MenuWidget* p_source)
{
	LegoU32 sound = 0;

	if (p_source == &m_movePad) {
		LegoU32 index = m_movePad.GetHotspotIndex();
		LegoS32 categoryIndex = m_partPlacement.GetViewSlot();

		if (index--) {
			MovePieceByDrag(
				g_carBuildDragHorizontalOffsets[index + (categoryIndex * 8)],
				g_carBuildDragVerticalOffsets[index + (categoryIndex * 8)],
				0x10,
				index & 1
			);
		}

		m_dragDelayMs = c_carBuildDragDelay;
	}
	else if (p_source == &m_viewPad) {
		switch (m_viewPad.GetHotspotIndex()) {
		case 1:
			if (m_partPlacement.PitchViewStep(1, TRUE)) {
				sound = 6;
			}
			break;
		case 3:
			if (m_partPlacement.RotateViewStep(1, TRUE)) {
				sound = 6;
			}
			break;
		case 5:
			if (m_partPlacement.PitchViewStep(-1, TRUE)) {
				sound = 6;
			}
			break;
		case 7:
			if (m_partPlacement.RotateViewStep(-1, TRUE)) {
				sound = 6;
			}
			break;
		case 9:
			m_partPlacement.BeginViewReset(TRUE);
			break;
		}

		m_soundGroupBinding->PlaySoundByIndex(sound & 0xffff);
	}
	else {
		CarModelScreenBase::OnWidgetValueChanged(p_source);
	}
}

// FUNCTION: LEGORACERS 0x00474940
void CarBuildScreen::UpdateHoverRegions()
{
	MenuInputDispatcher::Cursor* cursor = m_cursor;
	if (!cursor || !cursor->m_isCursorVisible) {
		return;
	}

	Rect rect;
	m_sceneView.GetEntityScreenRect(m_partPlacement.GetCarGroup(), &rect);
	m_carViewRegion.SetRect(&rect);
	m_sceneView.GetEntityScreenRect(m_partPlacement.GetPieceEntity(), &rect);
	m_pieceViewRegion.SetRect(&rect);

	LegoS32 x = cursor->m_cursorX + cursor->m_originX;
	LegoS32 y = cursor->m_cursorY + cursor->m_originY;

	if (PointInRect(m_sceneView.GetGlobalRect(), x, y)) {
		if (PointInRect(m_pieceViewRegion.GetRect(), x, y)) {
			if (m_partPlacement.GetFocusedPane() != 1) {
				m_hoverIcon = NULL;
				m_carViewRegion.Deselect(6);
				m_pieceViewRegion.Select(6);
				m_partPlacement.FocusPiece();
			}
			return;
		}

		if (PointInRect(m_carViewRegion.GetRect(), x, y)) {
			if (m_partPlacement.GetFocusedPane() != 2) {
				m_hoverIcon = NULL;
				m_pieceViewRegion.Deselect(6);
				m_carViewRegion.Select(6);
				m_partPlacement.FocusCar();
			}
			return;
		}
	}

	if ((m_pieceViewRegion.GetStateFlags() & MenuIcon::c_flagSelected) ||
		(m_carViewRegion.GetStateFlags() & MenuIcon::c_flagSelected)) {
		m_pieceViewRegion.Deselect(7);
		m_carViewRegion.Deselect(7);
		m_hoverIcon = NULL;
		CarModelScreenBase::Update(0);
		m_hoverIcon = m_rootIcon.FindSelectedLeaf();
		m_selectedIcon = m_hoverIcon;
	}

	m_partPlacement.ClearFocusPane();
}

// FUNCTION: LEGORACERS 0x00474b10
LegoBool32 CarBuildScreen::Update(undefined4 p_elapsed)
{
	if (!m_paletteOpen && m_cursor && m_mode != c_modeBusy && !m_navPending) {
		LegoS32 x = m_cursor->m_originX + m_cursor->m_cursorX, y = m_cursor->m_originY + m_cursor->m_cursorY;
		VisualStateColor paintColor;
		paintColor.m_packed = 0xffffffff;
		if (PointInRect(m_paintColumn.GetGlobalRect(), x, y)) {
			paintColor.m_color.m_blu = 0;
		}
		m_paintLabel.SetColor(&paintColor);
		MouseDevice* mouse = m_context->m_inputBindings.GetInputManager()->GetMouse();
		if (m_mouseOrbit && m_lastMouseX >= 0) {
			LegoS32 dx = x - m_lastMouseX, dy = y - m_lastMouseY;
			m_orbitTravel += abs(dx) + abs(dy);
			HandleViewDrag(dx, dy);
		}
		else if (PointInRect(m_partCarousel.GetGlobalRect(), x, y)) {
			if (mouse) {
				LegoFloat wheel = mouse->GetAxisValue(DirectInputDevice::c_axisZ);
				if (wheel != 0) {
					m_partCarousel.ScrollGridBrowser(wheel > 0 ? -1 : 1);
				}
			}
		}
		else if (PointInRect(m_sceneView.GetGlobalRect(), x, y)) {
			if (mouse) {
				LegoFloat wheel = mouse->GetAxisValue(DirectInputDevice::c_axisZ);
				if (wheel != 0) {
					m_partPlacement.ZoomView(wheel / 120.0f);
				}
			}
			if (x != m_lastMouseX || y != m_lastMouseY) {
				m_partPlacement.PlaceAtCursor(x, y, FALSE);
			}
		}
		m_lastMouseX = x;
		m_lastMouseY = y;
	}

	// Mouse placement and selection replace the old hover-region controls.

	if (p_elapsed >= m_doubleClickMs) {
		m_doubleClickMs = 0;
	}
	else {
		m_doubleClickMs -= p_elapsed;
	}

	if (p_elapsed >= m_dragDelayMs) {
		m_dragDelayMs = 0;
	}
	else {
		m_dragDelayMs -= p_elapsed;
	}

	return CarModelScreenBase::Update(p_elapsed);
}

// FUNCTION: LEGORACERS 0x00474b70
GolString* CarBuildScreen::GetHelpString(undefined4 p_index)
{
	// Legacy movement hints no longer describe the mouse-driven builder.
	return NULL;
}

// FUNCTION: LEGORACERS 0x00474ba0
GolFont* CarBuildScreen::GetHelpFont(undefined4 p_helpStringId)
{
	if (!p_helpStringId) {
		return NULL;
	}

	LegoChar fontName[sizeof(g_carBuildHelpFontName)];
	::memcpy(fontName, g_carBuildHelpFontName, sizeof(fontName));
	return m_renderer->FindFontByName(fontName);
}

// FUNCTION: LEGORACERS 0x004774e0
void CarBuildScreen::HandleViewDrag(LegoS32 p_deltaX, LegoS32 p_deltaY)
{
	if (m_mode == c_modeBusy) {
		return;
	}

	if (p_deltaX) {
		m_partPlacement.RotateViewAnalog(-(static_cast<LegoFloat>(p_deltaX) * g_carBuildPreviewMouseScale));
	}

	if (p_deltaY) {
		m_partPlacement.PitchViewAnalog(static_cast<LegoFloat>(p_deltaY) * g_carBuildPreviewMouseScale);
	}
}

// FUNCTION: LEGORACERS 0x00477540
LegoBool32 CarBuildScreen::UndoPiece()
{
	if (m_mode != c_modeBusy) {
		LegoS32 carSetPartId;
		LegoS32 pieceType;
		LegoS32 colorRecordIndex;
		m_partPlacement.UndoLastPiece(&carSetPartId, &pieceType, &colorRecordIndex);

		if (carSetPartId) {
			CarPartCarousel* partCarousel = &m_partCarousel;
			partCarousel->SelectPartByType(carSetPartId);
			partCarousel->SelectChoice(pieceType, colorRecordIndex);

			LegoS32 categoryIndex = m_context->m_partSet.GetSelectedEntry()->GetIndex();
			if (categoryIndex >= 0 && categoryIndex < 12) {
				m_categoryCarousel.SelectChild(&m_categoryIcons[categoryIndex]);
			}
			m_partPlacement.SelectPieceChoice(partCarousel->GetChoiceIndex(partCarousel->GetSelectedIndex()));
			m_nextMode = c_modeResetView;
			return TRUE;
		}
	}

	return FALSE;
}

void CarBuildScreen::SetChassisPaint(LegoS32 p_index)
{
	CarBuildModel& build = m_context->m_carBuildModel;
	CarBuildModel::PieceList& pieces = build.GetPieceList();
	if (!pieces.m_entryCount || p_index < 0 || p_index >= m_paintCount) {
		return;
	}
	pieces.m_chassisPaintEnabled = TRUE;
	pieces.m_entries[0].m_colorRecordIndex = p_index;
	pieces.RebuildGrid();
	build.UpdateOffset(TRUE);
	build.RebuildModel(1, 0x7f);
	LegoChar chassis[9];
	m_context->m_saveSystem.GetActiveRecord().GetChassisName(chassis);
	const GolMaterialColor& color =
		m_context->m_colorTable.GetMaterial(m_context->m_colorTable.GetMaterialIndexForColorRecord(p_index))
			->GetDiffuse();
	m_context->m_chassisModels.ApplyChassisPaint(m_context->m_chassisModels.GetPrimaryModel(chassis), &color);
	m_context->m_saveSystem.GetActiveRecord().MarkCarModified();
	char text[80];
	sprintf(text, "Chassis color: %s...", m_context->m_colorTable.GetColorName(p_index));
	m_paintLabel.SetText(text);
	m_paintColumn.Arrange();
}
