#ifndef MENUTEXTLABEL_H
#define MENUTEXTLABEL_H

#include "compat.h"
#include "decomp.h"
#include "golstring.h"
#include "menu/style/menustyletable.h"
#include "menu/widgets/menustaticwidget.h"

#include <string.h>

class GolFont;
class GolStringTable;

// VTABLE: LEGORACERS 0x004b273c
// SIZE 0x78
class MenuTextLabel : public MenuStaticWidget {
public:
	// SIZE 0x48
	class CreateParams : public MenuWidget::CreateParams {
	public:
		GolStringTable* m_stringTable; // 0x38
		GolFont* m_font;               // 0x3c
		undefined2 m_stringId;         // 0x40
		LegoU32 m_wrapWidth;           // 0x44
	};

	MenuTextLabel();
	void Reset() override;                                 // vtable+0x00
	~MenuTextLabel() override;                             // vtable+0x04
	MenuWidget* DrawSelf(Rect*, Rect*) override;           // vtable+0x38
	virtual void SetString(GolString*, LegoBool32);        // vtable+0x40
	virtual void SetStringByIndex(undefined2, undefined4); // vtable+0x44
	LegoBool32 Create(CreateParams* p_createParams, MenuStyleTable::TextStyle* p_style);
	void WrapText(LegoS32 p_rightMargin);
	void FitToBounds() { m_fitToBounds = TRUE; }
	void SetText(const LegoChar* p_text)
	{
		LegoS32 length = static_cast<LegoS32>(strlen(p_text));
		if (length > 65534) {
			length = 65534;
		}
		delete[] m_ownedText;
		m_ownedText = new undefined2[length + 1];
		for (LegoS32 i = 0; i < length; ++i) {
			m_ownedText[i] = static_cast<LegoU8>(p_text[i]);
		}
		m_ownedText[length] = 0;
		GolString string;
		string.CopyFromBufSelection(&m_ownedText[0], static_cast<undefined2>(length + 1));
		SetString(&string, TRUE);
	}
	void SetFont(GolFont* p_font) { m_font = p_font; }

	// SYNTHETIC: LEGORACERS 0x0046f470
	// MenuTextLabel::`scalar deleting destructor'

protected:
	MenuStyleTable::TextStyle* m_style; // 0x58
	GolStringTable* m_stringTable;      // 0x5c
	GolFont* m_font;                    // 0x60
	GolString m_string;                 // 0x64
	LegoS32 m_wrapWidth;                // 0x70
	LegoBool32 m_wrapped;               // 0x74
	// GolString is a non-owning view; dynamically supplied text must live with the widget.
	undefined2* m_ownedText;
	LegoBool32 m_fitToBounds;
};

#endif // MENUTEXTLABEL_H
