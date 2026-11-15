#ifndef MENUBOX_H
#define MENUBOX_H

#include "menu/widgets/menuwidget.h"

#include <string.h>

// Non-owning horizontal/vertical layout. Fixed slots and weighted flexible slots
// can contain widgets, nested boxes, or NULL spacers. All sizes are UI pixels.
class MenuBox : public MenuWidget {
public:
	enum Direction {
		c_horizontal,
		c_vertical
	};
	MenuBox() : m_count(0), m_direction(c_horizontal), m_padding(0), m_gap(0) {}
	~MenuBox() override { Destroy(); }
	LegoBool32 Destroy() override
	{
		while (GetFirstChild()) {
			GetFirstChild()->RemoveFromParent();
		}
		m_count = 0;
		return MenuWidget::Destroy();
	}
	void Create(MenuWidget* p_parent, Direction p_direction, LegoS32 p_padding, LegoS32 p_gap)
	{
		CreateParams params;
		::memset(&params, 0, sizeof(params));
		params.m_parent = p_parent;
		params.m_flags = 1;
		CreateWidget(&params);
		m_direction = p_direction;
		m_padding = p_padding;
		m_gap = p_gap;
	}
	void Add(MenuWidget* p_widget, LegoS32 p_fixedSize = 0, LegoS32 p_weight = 1, LegoFloat p_aspect = 0)
	{
		if (m_count >= 64) {
			return;
		}
		Slot& slot = m_slots[m_count++];
		slot.m_widget = p_widget;
		slot.m_fixedSize = p_fixedSize;
		slot.m_weight = p_weight;
		slot.m_aspect = p_aspect;
		if (p_widget) {
			p_widget->SetParent(this);
		}
	}
	void SetRect(Rect* p_rect) override
	{
		MenuWidget::SetRect(p_rect);
		Arrange();
	}
	void Arrange()
	{
		if (!m_count) {
			return;
		}
		LegoS32 width = m_rect.m_right - m_rect.m_left, height = m_rect.m_bottom - m_rect.m_top;
		LegoS32 available = (m_direction == c_horizontal ? width : height) - 2 * m_padding - (m_count - 1) * m_gap;
		LegoS32 weights = 0;
		for (LegoS32 i = 0; i < m_count; i++) {
			if (m_slots[i].m_fixedSize > 0) {
				available -= m_slots[i].m_fixedSize;
			}
			else {
				weights += m_slots[i].m_weight;
			}
		}
		if (available < 0) {
			available = 0;
		}
		LegoS32 position = m_padding, usedWeight = 0, previous = 0;
		for (LegoS32 i = 0; i < m_count; i++) {
			Slot& slot = m_slots[i];
			LegoS32 size = slot.m_fixedSize;
			if (size <= 0) {
				usedWeight += slot.m_weight;
				LegoS32 next = weights ? available * usedWeight / weights : 0;
				size = next - previous;
				previous = next;
			}
			Rect rect;
			if (m_direction == c_horizontal) {
				rect.m_left = position;
				rect.m_right = position + size;
				rect.m_top = m_padding;
				rect.m_bottom = height - m_padding;
			}
			else {
				rect.m_top = position;
				rect.m_bottom = position + size;
				rect.m_left = m_padding;
				rect.m_right = width - m_padding;
			}
			if (rect.m_right < rect.m_left) {
				rect.m_right = rect.m_left;
			}
			if (rect.m_bottom < rect.m_top) {
				rect.m_bottom = rect.m_top;
			}
			if (slot.m_widget) {
				if (slot.m_aspect > 0) {
					LegoS32 w = rect.m_right - rect.m_left, h = rect.m_bottom - rect.m_top;
					LegoS32 fitW = w, fitH = static_cast<LegoS32>(w / slot.m_aspect);
					if (fitH > h) {
						fitH = h;
						fitW = static_cast<LegoS32>(h * slot.m_aspect);
					}
					rect.m_left += (w - fitW) / 2;
					rect.m_top += (h - fitH) / 2;
					rect.m_right = rect.m_left + fitW;
					rect.m_bottom = rect.m_top + fitH;
				}
				slot.m_widget->SetRect(&rect);
			}
			position += size + m_gap;
		}
	}

private:
	struct Slot {
		MenuWidget* m_widget;
		LegoS32 m_fixedSize, m_weight;
		LegoFloat m_aspect;
	};
	Slot m_slots[64];
	LegoS32 m_count;
	Direction m_direction;
	LegoS32 m_padding, m_gap;
};
#endif
