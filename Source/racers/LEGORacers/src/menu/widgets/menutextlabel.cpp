#include "menu/widgets/menutextlabel.h"

#include "font/golfont.h"
#include "golstringtable.h"

DECOMP_SIZE_ASSERT(MenuTextLabel, 0x78)
DECOMP_SIZE_ASSERT(MenuTextLabel::CreateParams, 0x48)

// FUNCTION: LEGORACERS 0x0046f410
MenuTextLabel::MenuTextLabel()
{
	m_ownedText = NULL;
	Reset();
}

// FUNCTION: LEGORACERS 0x0046f490
MenuTextLabel::~MenuTextLabel()
{
	MenuWidget::Destroy();
	delete[] m_ownedText;
}

// FUNCTION: LEGORACERS 0x0046f4f0
void MenuTextLabel::Reset()
{
	m_string.Reset();
	m_stringTable = 0;
	m_font = NULL;
	m_style = 0;
	m_wrapWidth = 0;
	m_wrapped = 0;
	m_fitToBounds = FALSE;
	MenuWidget::Reset();
}

// FUNCTION: LEGORACERS 0x0046f520
LegoBool32 MenuTextLabel::Create(CreateParams* p_createParams, MenuStyleTable::TextStyle* p_style)
{
	Destroy();

	m_font = p_createParams->m_font;
	m_stringTable = p_createParams->m_stringTable;
	m_style = p_style;
	LegoS32 wrapWidth = p_createParams->m_wrapWidth;
	m_wrapWidth = wrapWidth;
	if (wrapWidth) {
		m_wrapped = TRUE;
	}

	if (CreateWidget(p_createParams)) {
		SetStringByIndex(p_createParams->m_stringId, 0);
		return TRUE;
	}

	return FALSE;
}

// FUNCTION: LEGORACERS 0x0046f580
void MenuTextLabel::SetStringByIndex(undefined2 p_stringId, undefined4 p_remeasure)
{
	GolString string;

	m_stringTable->CopyStringByIndex(&string, p_stringId);
	SetString(&string, p_remeasure);
}

// FUNCTION: LEGORACERS 0x0046f600
void MenuTextLabel::SetString(GolString* p_string, LegoBool32 p_remeasure)
{
	m_string.CopyFromGolString(p_string);
	m_string.ToUpperCase();

	LegoS32 width;
	LegoS32 height;

	if (!m_rect.m_right || !m_rect.m_bottom || p_remeasure) {
		m_string.FirstLine();
		m_rect.m_bottom = m_rect.m_top;
		m_rect.m_right = m_rect.m_left;

		for (LegoS32 i = 0; i < m_string.CountLines(); i++) {
			m_font->MeasureString(&m_string, &width, &height);

			LegoU32 right = m_rect.m_left + width;
			LegoU32 currentRight = m_rect.m_right;
			if (right > currentRight) {
				m_rect.m_right = right;
			}

			m_rect.m_bottom += height;
			m_string.NextLine();
		}

		m_string.FirstLine();
	}
}

// FUNCTION: LEGORACERS 0x0046f6b0
void MenuTextLabel::WrapText(LegoS32 p_rightMargin)
{
	LegoS32 right;
	LegoS32 left;

	if (m_parent) {
		right = m_parent->GetRect()->m_right - m_parent->GetRect()->m_left - p_rightMargin;
		left = m_rect.m_left;
	}
	else {
		right = m_rect.m_right;
		left = m_rect.m_left;
	}

	LegoS32 width = m_rect.m_right - left;
	m_rect.m_right = right;
	m_rect.m_left = right - width;
}

// FUNCTION: LEGORACERS 0x0046f6f0
MenuWidget* MenuTextLabel::DrawSelf(Rect* p_rect, Rect* p_arg)
{
	LegoS32 lineCount = m_string.CountLines();
	LegoFloat lineCountFloat = static_cast<LegoFloat>(lineCount);
	LegoFloat lineHeight = static_cast<LegoFloat>(m_rect.m_bottom - m_rect.m_top) / lineCountFloat;
	LegoFloat lineBottom = 0.0f;
	LegoS32 xOffset = p_arg->m_left - p_rect->m_left;
	LegoS32 yOffset = p_arg->m_top - p_rect->m_top;

	m_string.FirstLine();
	if (lineCount > 0) {
		LegoS32 lineTop = 0;
		for (; lineCount > 0; lineCount--) {
			Rect source;
			source.m_top = lineTop;
			source.m_left = 0;

			lineBottom += lineHeight;
			lineTop = (LegoS32) lineBottom;
			source.m_bottom = lineTop;
			source.m_right = m_rect.m_right - m_rect.m_left;

			Rect dest;
			MeasureText(m_font, &m_string, &source, &dest, 0);
			if (m_fitToBounds) {
				// Measure the unbounded text: MeasureText clamps each axis independently.
				LegoS32 width, height;
				m_font->MeasureString(&m_string, &width, &height);
				LegoFloat scale = 1.0f;
				if (width > source.m_right - source.m_left) {
					scale = static_cast<LegoFloat>(source.m_right - source.m_left) / width;
				}
				if (height > 0 && height * scale > source.m_bottom - source.m_top) {
					scale = static_cast<LegoFloat>(source.m_bottom - source.m_top) / height;
				}
				dest = Rect{0, 0, static_cast<LegoS32>(width * scale), static_cast<LegoS32>(height * scale)};
				CenterRectIn(&source, &dest);
			}
			dest.m_top += yOffset;
			dest.m_bottom += yOffset;
			dest.m_left += xOffset;
			dest.m_right += xOffset;
			DrawString(&dest, p_arg, m_font, &m_string, 0, 0);
			m_string.NextLine();
		}
	}

	return NULL;
}
