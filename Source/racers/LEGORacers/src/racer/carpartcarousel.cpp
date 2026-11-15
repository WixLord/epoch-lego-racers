#include "racer/carpartcarousel.h"

#include "audio/soundgroupbinding.h"
#include "camera/golcamera.h"
#include "core/gol.h"
#include "decomp.h"
#include "golmath.h"
#include "golmodelbase.h"
#include "model/carbuildmodel.h"
#include "model/carpartset.h"
#include "model/legopiecelibrary.h"
#include "render/gold3drenderdevice.h"

#include <float.h>
#include <string.h>

DECOMP_SIZE_ASSERT(CarPartCarousel, 0x154)
DECOMP_SIZE_ASSERT(CarPartCarousel::CreateParams, 0x84)

// GLOBAL: LEGORACERS 0x004b02f8
extern const LegoFloat g_twoPi = 6.2831855f;

// GLOBAL: LEGORACERS 0x004b3c0c
static const LegoFloat g_maxFloat = FLT_MAX;

extern LegoFloat g_cosineTable[1024];

// FUNCTION: LEGORACERS 0x00485200
CarPartCarousel::CarPartCarousel()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x00485270
CarPartCarousel::~CarPartCarousel()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x004852c0
void CarPartCarousel::Reset()
{
	m_pieceLibrary = NULL;
	m_buildModel = NULL;
	m_colorTable = NULL;
	m_partSet = NULL;
	m_currentPieceType = 0;
	m_currentEntry = NULL;
	m_rotationAngle = 0.0f;
	m_gridBrowser = FALSE;
	m_gridRow = 0;
	::memset(m_choiceIndices, 0, sizeof(m_choiceIndices));
	MenuModelCarousel::Reset();
}

// FUNCTION: LEGORACERS 0x00485300
LegoBool32 CarPartCarousel::Create(CreateParams* p_createParams, MenuStyleTable::CarouselStyle* p_styleEntry)
{
	m_partSet = p_createParams->m_partSet;
	m_pieceLibrary = p_createParams->m_pieceLibrary;
	m_buildModel = p_createParams->m_buildModel;
	m_colorTable = p_createParams->m_colorTable;
	if (MenuModelCarousel::Create(p_createParams, p_styleEntry)) {
		InitializeItemModels();
	}

	return m_flags & 1;
}

// FUNCTION: LEGORACERS 0x00485350
LegoBool32 CarPartCarousel::Destroy()
{
	LegoBool32 result = TRUE;

	if (result & m_flags) {
		for (LegoS32 i = 0; i < m_slotCount; i++) {
			m_golExport->DestroyModel(m_items[i].m_model);
			m_items[i].m_model = NULL;
		}

		result = MenuModelCarousel::Destroy();
	}

	return result;
}

// FUNCTION: LEGORACERS 0x004853a0
LegoS32 CarPartCarousel::InitializeItemModels()
{
	LegoS32 maxHighPieceOffset = m_pieceLibrary->GetMaxHighPieceOffset();
	LegoS32 result = m_slotCount;

	for (LegoS32 i = 0; i < result; i++) {
		GolModelBase* model = GetItemModel(i);
		model->Allocate(m_renderer, 2, 3 * maxHighPieceOffset, maxHighPieceOffset, 100, 5);
		result = m_slotCount;
	}

	return result;
}

// FUNCTION: LEGORACERS 0x004853f0
void CarPartCarousel::RefreshChoiceIndices()
{
	m_currentEntry->FUN_004513d0(m_buildModel);
	m_currentEntry->FillChoiceIndices(m_choiceIndices, 0, 512);

	for (m_itemCount = 0; m_itemCount < 512; m_itemCount++) {
		if (m_choiceIndices[m_itemCount] < 0) {
			break;
		}
	}
}

// FUNCTION: LEGORACERS 0x00485440
void CarPartCarousel::SelectPartByType(LegoS32 p_pieceType)
{
	if (m_gridBrowser) {
		m_gridRow = 0;
	}
	if (p_pieceType == m_currentPieceType) {
		RefreshChoiceIndices();
		if (m_gridBrowser) {
			SetSelection(0);
		}
		return;
	}

	m_scrolling = 0;
	m_scrollDurationMs = 0;
	m_currentEntry = m_partSet->FindEntry(p_pieceType);
	m_currentPieceType = p_pieceType;
	RefreshChoiceIndices();
	SetSelection(0);
}

// FUNCTION: LEGORACERS 0x004854a0
void CarPartCarousel::RefreshItemModel(LegoS32 p_index)
{
	GolModelEntity* entity = GetItemEntity(p_index);
	GolModelBase* model = GetItemModel(p_index);
	LegoS32 choiceIndex =
		GetChoiceIndex(m_gridBrowser ? m_gridRow * 4 + p_index : WrapIndex(m_ringBaseIndex + p_index));
	LegoS32 pieceType;
	LegoS32 colorRecordIndex;

	m_currentEntry->GetChoice(choiceIndex, &pieceType, &colorRecordIndex);

	entity->SetPrimaryModel(model, g_maxFloat);

	LegoPieceLibrary::PieceRecord* pieceRecord = m_pieceLibrary->FindPieceRecord(pieceType, 1);
	m_buildModel->CenterOnPiece(pieceRecord, 0);
	m_buildModel->BuildPieceModel(entity, pieceRecord, 0, 0, 0, 0, colorRecordIndex, 239);
	MenuModelCarousel::RefreshItemModel(p_index);
}

// FUNCTION: LEGORACERS 0x00485550
void CarPartCarousel::LayoutItem(undefined4, GolModelEntity* p_entity)
{
	LegoFloat vectorX = g_cosineTable[c_vectorXCosineIndex];
	LegoFloat vectorZ = g_cosineTable[c_vectorZCosineIndex];
	GolVec3 axis = {1.0f, 0.0f, 0.0f};
	GolVec3 vector = {vectorX, 0.0f, vectorZ};

	GolMath::RotateAboutAxis(&axis, &axis, &vector, m_rotationAngle);
	p_entity->SetUpDirection(vector, axis);
}

// FUNCTION: LEGORACERS 0x004855c0
void CarPartCarousel::SetSelection(undefined4 p_index)
{
	if (m_gridBrowser) {
		if (p_index < static_cast<LegoU32>(m_itemCount)) {
			m_selectedIndex = p_index;
		}
		m_scrolling = 0;
		m_scrollDurationMs = 0;
		RefreshGridBrowser();
		return;
	}
	if (m_itemCount) {
		m_selectedIndex = p_index;
		m_ringBaseIndex = WrapIndex(p_index - m_focusedSlot);

		if (!m_scrolling) {
			if (m_itemCount >= m_slotCount - 1) {
				for (LegoS32 i = 0; i < m_slotCount; i++) {
					RefreshItemModel(i);
				}
			}
			else {
				for (LegoS32 i = 0; i < m_slotCount; i++) {
					GetItemEntity(i)->ResetModelState();
				}

				RefreshItemModel(m_focusedSlot);

				LegoS32 index = m_focusedSlot;
				LegoS32 previousVisibleIndex = index - 1;
				if (m_selectedIndex <= index) {
					index = m_selectedIndex;
				}
				index--;

				if (index >= 0) {
					LegoS32 count = index + 1;
					do {
						RefreshItemModel(previousVisibleIndex--);
					} while (--count);
				}

				LegoS32 baseIndex = static_cast<LegoS32>(m_focusedSlot);
				LegoS32 nextVisibleIndex = baseIndex + 1;
				LegoS32 count;
				if (m_selectedIndex >= baseIndex) {
					count = m_itemCount - m_selectedIndex;
				}
				else {
					count = m_slotCount - baseIndex - 1;
				}

				while (count--) {
					RefreshItemModel(nextVisibleIndex++);
				}
			}

			SnapToSelection();
		}
	}
}

// FUNCTION: LEGORACERS 0x004856c0
void CarPartCarousel::SelectChoice(LegoS32 p_pieceType, LegoS32 p_colorRecordIndex)
{
	LegoS32 result = 0;
	LegoS32 firstChoiceIndex = m_currentEntry->NormalizeChoiceIndex(0);
	LegoS32 choiceIndex = m_currentEntry->FindChoiceIndex(p_pieceType, p_colorRecordIndex);

	for (; firstChoiceIndex != choiceIndex; result++) {
		firstChoiceIndex = m_currentEntry->NormalizeChoiceIndex(firstChoiceIndex + 1);
	}

	SetSelection(result);
}

// FUNCTION: LEGORACERS 0x00485720
LegoS32 CarPartCarousel::ScrollNext()
{
	if (m_gridBrowser) {
		ScrollGridBrowser(1);
		return m_selectedIndex;
	}
	if (!m_itemCount) {
		return 0;
	}

	if (!m_scrolling) {
		if (m_selectedIndex >= m_itemCount - 1 && m_itemCount < m_slotCount - 1) {
			m_soundGroupBinding->PlaySoundByIndex(m_style->m_soundIds[2]);
			return m_selectedIndex;
		}

		MenuModelCarousel::ScrollNext();
		SetSelection(WrapIndex(m_selectedIndex + 1));

		if (m_itemCount >= m_slotCount - 1 ||
			m_itemCount - m_selectedIndex > m_slotCount - static_cast<LegoS32>(m_focusedSlot) - 1) {
			RefreshItemModel(m_slotCount - 1);
		}

		m_soundGroupBinding->PlaySoundByIndex(m_style->m_soundIds[0]);
	}

	return m_selectedIndex;
}

// FUNCTION: LEGORACERS 0x004857b0
LegoS32 CarPartCarousel::ScrollPrevious()
{
	if (m_gridBrowser) {
		ScrollGridBrowser(-1);
		return m_selectedIndex;
	}
	if (!m_itemCount) {
		return 0;
	}

	if (!m_scrolling) {
		if (!m_selectedIndex && m_itemCount < m_slotCount - 1) {
			m_soundGroupBinding->PlaySoundByIndex(m_style->m_soundIds[2]);
			return m_selectedIndex;
		}

		MenuModelCarousel::ScrollPrevious();
		SetSelection(WrapIndex(m_selectedIndex - 1));

		if (m_itemCount >= m_slotCount - 1 || m_selectedIndex > static_cast<LegoS32>(m_focusedSlot) - 1) {
			RefreshItemModel(0);
		}

		m_soundGroupBinding->PlaySoundByIndex(m_style->m_soundIds[1]);
	}

	return m_selectedIndex;
}

// FUNCTION: LEGORACERS 0x00485840
undefined4 CarPartCarousel::OnEvent(undefined4 p_elapsed)
{
	LegoFloat step = m_scrollStep;
	LegoFloat scaled = (LegoFloat) (LegoS32) p_elapsed;
	if ((m_rotationAngle += step * scaled) > g_twoPi) {
		m_rotationAngle -= g_twoPi;
	}

	return MenuModelCarousel::OnEvent(p_elapsed);
}

void CarPartCarousel::EnableGridBrowser()
{
	m_scrolling = 0;
	m_scrollDurationMs = 0;
	for (LegoS32 i = 0; i < m_slotCount; i++) {
		m_items[i].m_entity.ResetModelState();
		m_golExport->DestroyModel(m_items[i].m_model);
	}
	delete[] m_items;
	m_items = NULL;
	m_gridBrowser = TRUE;
	m_gridRow = 0;
	Rect rects[24];
	LegoS32 width = m_rect.m_right - m_rect.m_left, height = m_rect.m_bottom - m_rect.m_top;
	for (LegoS32 i = 0; i < 24; i++) {
		rects[i].m_left = (i % 4) * width / 4 + 4;
		rects[i].m_right = (i % 4 + 1) * width / 4 - 4;
		rects[i].m_top = (i / 4) * height / 6 + 4;
		rects[i].m_bottom = (i / 4 + 1) * height / 6 - 4;
	}
	UpdateViewport();
	m_camera->SetAspectRatio(static_cast<LegoFloat>(width) / height);
	m_camera->UpdateMatrices();
	PushCamera();
	CreateParams params;
	::memset(&params, 0, sizeof(params));
	params.m_slotCount = 24;
	params.m_focusedSlot = 0;
	params.m_slotRects = rects;
	CreateItems(&params);
	PopCamera();
	InitializeItemModels();
	for (LegoS32 i = 0; i < 24; i++) {
		m_items[i].m_positionZ = (height * .5f - (rects[i].m_top + rects[i].m_bottom) * .5f) * m_scaleY;
	}
	RefreshGridBrowser();
}

void CarPartCarousel::RefreshGridBrowser()
{
	if (!m_gridBrowser || !m_currentEntry) {
		return;
	}
	for (LegoS32 i = 0; i < m_slotCount; i++) {
		if (m_gridRow * 4 + i >= m_itemCount) {
			m_items[i].m_entity.ResetModelState();
			continue;
		}
		RefreshItemModel(i);
		GolVec3 position;
		m_items[i].m_entity.GetPosition(&position);
		LegoFloat farClip = m_cameraDistance - position.m_x + m_items[i].m_modelRadius * 2 + 10;
		if (farClip > m_camera->m_farClip) {
			m_camera->m_farClip = farClip;
			m_camera->m_flags |= GolCamera::c_flagProjectionDirty;
		}
		ColorRGBA color;
		color.m_alp = 255;
		color.m_red = color.m_grn = color.m_blu = (m_gridRow * 4 + i == m_selectedIndex) ? 200 : 120;
		m_items[i].m_primaryColor.m_color = color;
		color.m_red = color.m_grn = color.m_blu = 255;
		m_items[i].m_secondaryColor.m_color = color;
	}
	m_buildModel->UpdateOffset(TRUE);
}

void CarPartCarousel::ScrollGridBrowser(LegoS32 p_rows)
{
	LegoS32 maxRow = (m_itemCount + 3) / 4 - 6;
	if (maxRow < 0) {
		maxRow = 0;
	}
	LegoS32 row = m_gridRow + p_rows;
	if (row < 0) {
		row = 0;
	}
	if (row > maxRow) {
		row = maxRow;
	}
	if (row != m_gridRow) {
		m_gridRow = row;
		RefreshGridBrowser();
	}
}

LegoBool32 CarPartCarousel::PickGridChoice(LegoS32 p_x, LegoS32 p_y)
{
	Rect rect = *GetGlobalRect();
	if (p_x < rect.m_left || p_y < rect.m_top || p_x >= rect.m_right || p_y >= rect.m_bottom) {
		return FALSE;
	}
	LegoS32 col = (p_x - rect.m_left) * 4 / (rect.m_right - rect.m_left);
	LegoS32 row = (p_y - rect.m_top) * 6 / (rect.m_bottom - rect.m_top);
	LegoS32 choice = m_gridRow * 4 + row * 4 + col;
	if (choice >= m_itemCount) {
		return FALSE;
	}
	SetSelection(choice);
	return TRUE;
}

MenuWidget* CarPartCarousel::OnKeyDown(InputEventQueue::Event* p_event, undefined4 p_x, undefined4 p_y)
{
	if (!m_gridBrowser) {
		return MenuModelCarousel::OnKeyDown(p_event, p_x, p_y);
	}
	if (p_event->m_keyCode != InputDevice::c_sourceMouse || p_event->m_isRepeat || !HitTest(p_x, p_y)) {
		return NULL;
	}
	Rect rect = *GetGlobalRect();
	if (PickGridChoice(rect.m_left + p_x, rect.m_top + p_y) && m_notifyHandler) {
		m_notifyHandler->HandleKeyDown(this, p_event, p_x, p_y);
	}
	return this;
}
