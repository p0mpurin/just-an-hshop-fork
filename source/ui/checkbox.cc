/* This file is part of 3hs
 * Copyright (C) 2021-2025 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <ui/checkbox.hh>

#define BORDER_THICKNESS 1.0f/*px*/
#define BORDER_THICKNESS2 (BORDER_THICKNESS*2.0f)

UI_CTHEME_GETTER(color_border, ui::theme::checkbox_border_color)
UI_CTHEME_GETTER(color_check, ui::theme::checkbox_check_color)
UI_SLOTS(ui::CheckBox, color_border, color_check)


void ui::CheckBox::setup(bool isInitialChecked)
{
	this->flags = isInitialChecked ? ui::CheckBox::CHECKED : 0;
	this->w = this->h = 10.0f;
}

void ui::CheckBox::resize(float nw, float nh)
{
	this->ox = this->x + nw;
	this->oy = this->y + nh;
	this->w = nw;
	this->h = nh;
}

void ui::CheckBox::set_y(float y)
{
	this->y = y;
	this->oy = y + this->h;
}

void ui::CheckBox::set_x(float x)
{
	this->x = x;
	this->ox = x + this->w;
}

void ui::CheckBox::set_checked(bool c)
{
	u8 of = this->flags;
	if(c) this->flags |=  ui::CheckBox::CHECKED;
	else  this->flags &= ~ui::CheckBox::CHECKED;
	if(this->flags != of)
		this->on_change_cb(c);
}

bool ui::CheckBox::render(ui::Keys& keys)
{
	/* the checkbox may get pressed here */
	if(ui::is_touched(keys) && keys.touch.px >= this->x && keys.touch.px <= this->ox &&
			keys.touch.py >= this->y && keys.touch.py <= this->oy)
	{
		this->flags ^= ui::CheckBox::CHECKED;
		this->on_change_cb(!!(this->flags & ui::CheckBox::CHECKED));
		ui::set_touch_lock(keys);
	}

	/** checkbox map:
	 * (x,y)   B (x+w,y)
	 *  |---------|
	 * A|         | C
	 *  |---------|
	 * (x,y+h) D (x+w,y+h)
	 */
	C2D_DrawRectSolid(this->x, this->y, this->z, this->w, this->h, this->slots[0]);
	ui::background_rect(this->screen, this->x + BORDER_THICKNESS, this->y + BORDER_THICKNESS, this->z, this->w - BORDER_THICKNESS2, this->h - BORDER_THICKNESS2);

	if(this->flags & ui::CheckBox::CHECKED)
	{
		float ymid = (this->y+this->oy)/2;
		float xmid = (this->x+this->ox)/2;

		/* draw an actual check, a little outside the reported bounds from width() and height() but that's fine */
		C2D_DrawLine(this->x - 2.0f, ymid - 1.0f, this->slots[1], xmid, this->oy - 1.0f, this->slots[1], 2.0f, this->z);
		C2D_DrawLine(xmid, this->oy - 1.0f, this->slots[1], this->ox + 2.0f, this->y - 2.0f, this->slots[1], 2.0f, this->z);
	}

	return true;
}

float ui::CheckBox::height()
{
	return this->h;
}

float ui::CheckBox::width()
{
	return this->w;
}

