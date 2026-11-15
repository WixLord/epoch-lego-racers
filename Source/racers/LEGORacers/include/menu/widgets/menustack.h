#ifndef MENUSTACK_H
#define MENUSTACK_H

#include "menu/widgets/menustaticwidget.h"

// Reusable flow container. Children keep their existing styles and input handlers.
class MenuStack : public MenuStaticWidget {
public:
	LegoBool32 Create(CreateParams* p_params, LegoBool p_horizontal, LegoS32 p_spacing)
	{
		m_horizontal = p_horizontal;
		m_spacing = p_spacing;
		m_cursor = 0;
		return CreateWidget(p_params);
	}

	void Append(MenuWidget* p_child, LegoS32 p_width, LegoS32 p_height)
	{
		Rect rect = {0, 0, p_width, p_height};
		if (m_horizontal) {
			rect.m_left += m_cursor;
			rect.m_right += m_cursor;
			m_cursor += p_width + m_spacing;
		}
		else {
			rect.m_top += m_cursor;
			rect.m_bottom += m_cursor;
			m_cursor += p_height + m_spacing;
		}
		p_child->SetParent(this);
		p_child->SetRect(&rect);
	}

private:
	LegoBool m_horizontal;
	LegoS32 m_spacing;
	LegoS32 m_cursor;
};

#endif
